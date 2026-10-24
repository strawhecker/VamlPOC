#pragma once
// S285 (dep:S284) — Runtime VBamlReader: bytes → DependencyObject tree
// (InitializeComponent-style).
//
// The compiled-path ("S7 reader") runtime. A .vbaml byte stream (produced by
// vamlcc / compile_to_bytes, S283) is decoded via the S282 serializer and
// materialized into a LIVE DependencyObject tree — the compiled analogue of
// the S2 text VamlReader (S091) / InitializeComponent. It lives in vaml-tools
// (NOT core): it consumes the tools-layer schema + serializer, and vaml-core
// cannot depend on vaml-tools (tools → core only). It is the release path's
// single source of truth (see S288 parity).
//
// This header owns the S285 → S286 deferred surface contract:
//   * S285 — Load(bytes): decode (S282) → build the object graph
//     "InitializeComponent-style": instantiate each element via
//     ElementFactoryRegistry::global().create(typeToken) (S087), parent-link
//     children (set_parent, S039/S091), reader-owned `retained_` for non-root,
//     root returned + root_class() (x:Class). Literal props are set:
//     non-binding/non-extension attr → resolve DP via
//     DependencyProperty::TryGet (S043 attached-aware dotted keys) + convert
//     via ValueConverterRegistry::global() (S089) + target->Set(PropertyId, v).
//   * S286 — deferred surfaces: x:Name/x:Key registries, {Binding}
//     DeferredExtensions, event attach (register_event → DeferredEventHandler),
//     and the full error taxonomy (DuplicateName/ClassOnNonRoot/UnknownProperty).
//     S285 deliberately does NOT author those; the scope split is recorded at
//     S285 (NO-GAP, not silent).
//
// RESOLUTION SEMANTICS (S285): each element's type is resolved via the S087
// factory registry (unknown type → VBamlError::UnknownType, naming the element
// id). Each attribute whose key resolves to a DependencyProperty is converted
// via the S089 value registry (a malformed literal → VbamlMalformedValue) and
// Set on the target. Binding / extension / event-shaped keys are S286's step.
//
// ERROR MODEL (S285) — Load(bytes) throws VBamlException (a structured,
// element-id-carrying error) on any failure: malformed buffer (the S282
// VbamlMalformedBuffer, mapped to VBamlError::MalformedBuffer), unknown element
// type, unknown property, malformed value. No silent failures.
//
// PURE + SINGLE-THREADED (ADR-018: zero new locks) — Load is a pure function of
// the byte stream + the S2 registries. ADR-021-consumable: headless tests in
// vaml-tools-tests drive the real reader (never a spawned subprocess).

#include "vaml/tools/export.h"

#include "vaml/core/DependencyObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml::tools {

class VAML_TOOLS_EXPORT VBamlException;

// S285: the compiled reader's OWN error taxonomy — contains ONLY what the
// compiled path can itself produce (S048 discipline — no dead enum arms).
// The malformed-buffer genre propagates from the S282 serializer (mapped to
// MalformedBuffer here); unknown-type / unknown-property / malformed-value are
// the compiled analogues of the S2 reader/resolver/converter errors.
enum class VBamlError
{
    None,            // no reader error
    MalformedBuffer, // the .vbaml byte stream is structurally malformed (S282)
    UnknownType,     // an element id/type has no factory in the S087 type table
    UnknownProperty, // an attribute names a property with no global definition
    MalformedValue,  // a literal could not be converted to its DP's bound type
    // S286 additions (declared here so the enum is forward-stable):
    DuplicateName,   // an x:Name was registered more than once in one Load (S286)
    ClassOnNonRoot,  // an x:Class directive was authored on a NON-root (S286)
};

// S285: a structured compiled-reader error naming the offending element-id.
// what() renders a human-readable message naming the construct; `detail` holds
// the offending attribute key / x:Name value / x:Class value (empty when not
// applicable) — the mirror of VamlReaderException::property() (S092/S094).
class VAML_TOOLS_EXPORT VBamlException : public std::runtime_error
{
public:
    VBamlException(VBamlError kind, std::string what, std::size_t elementId,
                   std::string detail = std::string());

    [[nodiscard]] VBamlError kind() const noexcept { return kind_; }
    // The 0-based element id (== document-order index) that caused the failure,
    // or kNoElement when the error is buffer-level (no element yet).
    [[nodiscard]] std::size_t element_id() const noexcept { return elementId_; }
    // The offending attribute key / x:Name value / x:Class value (empty when
    // not applicable).
    [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

private:
    VBamlError kind_;
    std::size_t elementId_;
    std::string detail_;
};

// kNoElement: sentinel element-id for buffer-level / pre-element errors.
inline constexpr std::size_t kNoElement = static_cast<std::size_t>(-1);

// S285: ONE retained non-root built object is exposed via retained_objects().
// Mirror of the S2 reader ownership model (S091): every non-root node is owned
// exactly once by this reader; the returned root is separate; parent-link via
// set_parent (S039/S041).
//
// S286 (Strand A) — ONE deferred markup-extension record (the compiled analogue
// of the S2 reader's DeferredExtension, S092 Strand B). `target` is the object
// the extension was authored on, `key` the attribute key, `source` the ORIGINAL
// {Binding ...} source text retained verbatim (the "S2 tag"); the compiled
// reader does NOT resolve bindings — that is the release consumer / S252
// analogue (recorded NO-GAP → S288).
//
// S286 (Strand C) — ONE deferred event-handler record (the compiled analogue of
// the S2 reader's DeferredEventHandler, S092 Strand C). `target` is the object
// the event was authored on, `eventName` the registered event name, `handlerName`
// the authored handler name. S322 resolves each record against the x:Class owner
// to wire the real routed event handler.
struct VBamlDeferredExtension
{
    const vaml::DependencyObject* target = nullptr;
    std::string_view key;
    std::string_view source;
};

struct VBamlDeferredEventHandler
{
    const vaml::DependencyObject* target = nullptr;
    std::string_view eventName;
    std::string_view handlerName;
};

class VAML_TOOLS_EXPORT VBamlReader
{
public:
    VBamlReader();
    ~VBamlReader();

    VBamlReader(const VBamlReader&) = delete;
    VBamlReader& operator=(const VBamlReader&) = delete;

    // S285 — Loads a .vbaml byte stream into a live DependencyObject tree
    // (InitializeComponent-style). Decodes (S282) then builds the object graph
    // via the S087 factory registry + S089 converter registry; literal props
    // set, children parent-linked, non-root nodes retained_. The returned root
    // is owned by the caller; the retained nodes are owned by this reader.
    // See the header comment for the S285 vs S286 scope split.
    [[nodiscard]] std::unique_ptr<vaml::DependencyObject>
    Load(const std::vector<std::uint8_t>& bytes);

    // The non-root built objects, in construction order. Each is owned exactly
    // once by this reader. (The returned root is separate.)
    [[nodiscard]] const std::vector<std::unique_ptr<vaml::DependencyObject>>&
    retained_objects() const noexcept { return retained_; }

    // The authored x:Class name on the root, empty when absent (S092-analogue;
    // binding it to an owner object/type is deferred — recorded NO-GAP).
    [[nodiscard]] const std::string& root_class() const noexcept { return root_class_; }

    // ------------------------------------------------------------------
    // S286 strands (declared now so the surface is forward-complete; the text
    // path and tests use them after S285 wire-up. See the S285 header note.)
    // ------------------------------------------------------------------

    // Strand A — x:Name registry (mirrors VamlReader::named/names/name_count).
    [[nodiscard]] vaml::DependencyObject* named(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t name_count() const noexcept;
    [[nodiscard]] const std::vector<std::pair<std::string_view, vaml::DependencyObject*>>&
    names() const noexcept;

    // Strand B — deferred {Binding} extensions (the compiled S2 tag).
    [[nodiscard]] const std::vector<VBamlDeferredExtension>& deferred_extensions() const noexcept;

    // Strand C — event attach (mirrors VamlReader::register_event).
    void register_event(std::string_view eventName);
    [[nodiscard]] const std::vector<VBamlDeferredEventHandler>&
    deferred_event_handlers() const noexcept;

private:
    std::vector<std::unique_ptr<vaml::DependencyObject>> retained_;
    std::string root_class_;

    // S286 registries (wired at S286; declared so the reader is forward-stable).
    std::vector<std::pair<std::string_view, vaml::DependencyObject*>> names_;
    std::vector<VBamlDeferredExtension> deferred_extensions_;
    std::vector<std::string> event_names_;
    std::vector<VBamlDeferredEventHandler> deferred_event_handlers_;
};

} // namespace vaml::tools
