#pragma once
// S285 (dep:S284) — Runtime VBamlReader: bytes → DependencyObject tree
// (InitializeComponent-style).
//
// *****************************************************************************
// DUAL-PATH CONTRACT (S290, dep:S289) — S2 reader = DEV TOOL, S7 reader = RELEASE
// *****************************************************************************
// The S2 text reader (`vaml::VamlReader`, core) and this S7 compiled reader
// (`vaml::tools::VBamlReader` + `ResourcePack`, tools) are TWO READERS for the
// SAME VAML declaration, with a STRUCTURAL role split enforced at LINKAGE:
//
//   * S2 `VamlReader` = DEV TOOL — the live text reload (S222_1), the S256
//     binding path, and vamlcc's text→compiled INPUT (S283). It consumes VAML
//     TEXT directly. The RELEASE app-shell NEVER links this reader, never
//     ships it, never calls it. (Role + link boundary.)
//   * S7 `VBamlReader` + `ResourcePack` = RELEASE — the ONLY reader the
//     release app links. It consumes COMPILED `.vbaml` BYTES only. The
//     compiled-only contract is enforced at the type level: the S289
//     `InitializeComponentFrom` entry takes (owner, ResourcePack&, packUri) —
//     a pack identity resolving to COMPILED bytes — and NEVER a text/VAML
//     parameter. The text VAML source is structurally unreachable from the
//     release path (verified by the S290 grep audit + the S289 static_assert).
//
// The ONLY text→anything transition in the whole tree is `vamlcc`
// (`compile_to_bytes`, S283) — the DEV compiler, never linked by release.
// S288 proved the two paths are behaviorally equivalent (parity, not
// duplication); S290 formalized this role split as a documented contract +
// a refactor-gated audit. See docs/roadmap.md + progress.md.
// *****************************************************************************
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
// S451 — COMMENT/DOCUMENT grammar parity on the compiled path: vamlcc compiles
// via parse_vaml (S085), which STRIPS the BOM + optional XML prolog at the
// document choke point and DROPS `<!-- -->` comment (and CDATA) tokens as
// non-tree-nodes. The compiled .vbaml therefore NEVER contains a comment/CData
// node, and the S7 reader (this class) reproduces a comment-free tree exactly
// like the S2 text reader — the same grammar on both paths (see ParsedElement.h
// for the documented S451 contract).
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
#include <unordered_set>
#include <utility>
#include <vector>

namespace vaml::tools {

class VAML_TOOLS_EXPORT VBamlException;

// S289 (dep:S288) — forward-declared pack types (the InitializeComponentFrom
// signature). Defined in vaml/tools/Pack.h (S289); the reader only needs the
// class declared here to reference it in the method signature.
namespace pack { class VAML_TOOLS_EXPORT ResourcePack; }

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
    // S289 addition — the compiled-only InitializeComponent path's OWN error:
    OwnerClassMismatch, // the decoded x:Class root_class() is inconsistent with
                        // the bound owner's declared class (the x:Class mapping)
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

    // The non-root built objects, in construction (POST-)order. OWNED by this
    // reader (each unique_ptr) until `finalize_visual_tree()` — S440 parity
    // with the S2 text reader — transfers them into their parents' S121
    // visual-children lists; after finalize the vector is emptied (the tree
    // owns the children). Pre-S440 parse/structure consumers see the D1 owning
    // set (unchanged).
    [[nodiscard]] const std::vector<std::unique_ptr<vaml::DependencyObject>>&
    retained_objects() const noexcept { return retained_; }

    // ------------------------------------------------------------------
    // S440 — the S120/S121 visual-tree hand-off (parity with VamlReader).
    // Closes the roadmap-recorded NO-GAP: a compiled (.vbaml) loaded tree that
    // calls this RENDERS its declared children (not just the root). Transfers
    // every non-root child that is BOTH a UIElement AND owns a UIElement parent
    // into that parent's S121 visual-children list via add_child (parent-link +
    // S041 inheritance-scope edge + tree ownership), removing each transferred
    // child from this reader's owned-retained set — the reader keeps no
    // dangling half-owner. Idempotent; returns the number transferred.
    // Call AFTER Load, BEFORE rendering/reload writes children.
    [[nodiscard]] std::size_t finalize_visual_tree();

    // The authored x:Class name on the root, empty when absent (S092-analogue;
    // binding it to an owner object/type is deferred — recorded NO-GAP).
    [[nodiscard]] const std::string& root_class() const noexcept { return root_class_; }

    // ------------------------------------------------------------------
    // S286 strands (declared now so the surface is forward-complete; the text
    // path and tests use them after S285 wire-up. See the S285 header note.)
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // S289 — Pack/identity + InitializeComponent (the compiled-release path).
    // Resolves the S092/S286-recorded "x:Class → owner binding deferred"
    // NO-GAP. The RELEASE consumer (S444 app-shell) faces ONLY these + Pack.
    // ------------------------------------------------------------------

    // Binds `owner` to the decoded tree. `SetOwner(obj)` records the x:Class
    // OWNER (the object S322's event wiring will resolve handlers onto + the
    // object the S286 `named()` registry serves). Normally called just after
    // Load(bytes); combined with it in InitializeComponent. Calling
    // SetOwner with a NULL owner is an error (returns false).
    [[nodiscard]] bool SetOwner(vaml::DependencyObject* owner);

    // The x:Class owner currently bound to the reader (nullptr when none).
    [[nodiscard]] vaml::DependencyObject* owner() const noexcept { return owner_; }

    // The InitializeComponent-analogue (WPF-faithful): Loads `bytes` (S285)
    // then binds `owner` (the x:Class owner). Returns the root. `owner` must
    // be non-null. On any failure throws VBamlException (S285 taxonomy + the
    // S289 OwnerClassMismatch).
    [[nodiscard]] std::unique_ptr<vaml::DependencyObject>
    InitializeComponent(vaml::DependencyObject* owner,
                        const std::vector<std::uint8_t>& bytes);

    // THE COMPILED-ONLY RELEASE ENTRY: resolves `packUri` through `pack` (the
    // S289 resource lookup) → Load → bind owner. It takes NO text of any kind
    // — the compiled-only contract is enforced by signature (only ResourcePack
    // bytes are reachable; the text VamlReader / std::string VAML is never
    // reachable from this path). `owner` must be non-null; `packUri` must
    // resolve to a registered resource (else it throws a structured
    // VBamlException). Fails loud, never silent.
    [[nodiscard]] std::unique_ptr<vaml::DependencyObject>
    InitializeComponentFrom(vaml::DependencyObject* owner,
                            pack::ResourcePack& pack,
                            std::string_view packUri);

    // The CLASS OWNER MAPPING (the x:Class name mapping). `register_class(
    // className)` declares that `className` is a PACK OWNER the app maps to
    // the bound owner. When a decoded root_class() is COMPARED against the
    // set of registered class names and it is NOT found, InitializeComponent
    // throws OwnerClassMismatch. A class name may be registered multiple times
    // (idempotent). This is how the app proves the x:Class name ring is
    // FOOTPRINT-CONSISTENT with the bound owner's declared class.
    void register_class(std::string_view className);

    // True when `className` has been registered as a pack-owner class name.
    [[nodiscard]] bool is_registered_class(std::string_view className) const noexcept;

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
    // The non-root built objects, in construction (POST-)order. OWNED by this
    // reader (each unique_ptr) until `finalize_visual_tree()` (S440) transfers
    // them into the S121 visual tree; then this vector is emptied.
    std::vector<std::unique_ptr<vaml::DependencyObject>> retained_;

    // S440: set true once finalize_visual_tree() has run; a second call is a
    // no-op. Reset at the start of each Load.
    bool visual_finalized_ = false;
    std::string root_class_;

    // S289 — the x:Class OWNER (non-owning; the app owns the owner object) +
    // the registered pack-owner class names (the x:Class mapping).
    vaml::DependencyObject* owner_ = nullptr;
    std::vector<std::string> class_names_;

    // S286 registries (wired at S286; declared so the reader is forward-stable).
    std::vector<std::pair<std::string_view, vaml::DependencyObject*>> names_;
    std::vector<VBamlDeferredExtension> deferred_extensions_;
    std::vector<std::string> event_names_;
    std::vector<VBamlDeferredEventHandler> deferred_event_handlers_;

    // S452 — the object-valued PROPERTY-ELEMENT children (the S366 NO-GAP
    // hand-off RESOLVED): non-owning pointers to children that are the VALUE of
    // a parent's named DP (set in Pass 2 via Value::UntypedPtr). They remain
    // reader-owned + retained (they own their subtree) but are EXCLUDED from the
    // S440 finalize_visual_tree() transfer — a property-element is never a
    // visual child at S452. Emptied/refilled per Load.
    std::unordered_set<vaml::DependencyObject*> property_element_children_;
};

} // namespace vaml::tools
