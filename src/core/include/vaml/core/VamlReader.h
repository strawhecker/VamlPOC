#pragma once
// S092 (dep:S091) — VamlReader extensions: (1) records the x:Name registry,
// (2) retains deferred markup-extension values (the S2 tag), and (3) exposes a
// MINIMAL REAL event-hook surface (Sprint 8). Everything S091 delivered is
// unchanged; S092 ADDS the strands described below.
//
// The three S092 strands (see the S092 plan):
//   A. x:Name / x:Class registry — build() now CONSUMES the S083 Name/Class
//      directives (previously skipped): x:Name registers the built object
//      under that name (root or child); x:Class records the authored root
//      class name. The binding of that class name to an owner object/type is
//      deliberately deferred to Sprint 8 (S286 / InitializeComponent-analogue)
//      and to S243 — recorded NO-GAP, not silent. A duplicate x:Name with-in a
//      single Load throws VamlReaderException(DuplicateName).
//   B. Deferred markup extensions (the S2 tag) — an S084-classified isExtension
//      attribute ({Binding Title}) is no longer skipped silently; build()
//      records it as a DeferredExtension (target object, attribute key, and the
//      ORIGINAL source text). The reader does NOT resolve the extension — that
//      is S252/S253's real contract (S2 tag: "resolved by S252"). Retained only,
//      but findable via deferred_extensions().
//   C. Event hooks (Sprint 8) — MINIMAL REAL seed. register_event(name) declares
//      that an attribute key is an EVENT, not a property; build() routes a
//      matching declaration to a DeferredEventHandler (target, eventName,
//      handlerName) instead of attempting a property set. The actual routed-
//      event engine is S320/S322; the S092 surface only defers event wiring
//      deterministically.
//
// S091 continues to own everything not listed above (resolve + literal prop
// set + child linking + unknown-property baseline). S094 still owns the full
// reader error taxonomy; S092 introduces exactly the ONE genuinely-new reader
// error it can produce: DuplicateName (validated by this step's tests).

#include "vaml/core/DependencyObject.h" // unique_ptr<DependencyObject>
#include "vaml/core/DependencyProperty.h" // DependencyProperty (convertLiteral decl)
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

struct ParsedElement; // forward-declared (Load(const ParsedElement&) takes a ref)

// S091: the reader's OWN error taxonomy — contains ONLY what the reader can
// itself produce (S048 discipline — no dead enum arms). An attribute may name
// a property that has no global DependencyProperty definition; that is this
// step's one genuinely reader-local error. Everything else the reader can hit
// already propagates as a structured error from a lower layer
// (ElementResolutionException / ValueConversionException).
//
// S092 adds ONE arm the reader can now genuinely produce: a duplicate x:Name
// registered within a single Load is a real authoring error (WPF-faithful —
// a name scope with a repeated name is an error, fail loud, no silent
// last-wins). No other arm is invented; S094 still owns the full taxonomy.
//
// S094 (dep:S093) — the reader error-taxonomy CONSOLIDATION GATE. The four
// title cases (unknown attr, unknown element, bad type name, malformed value)
// are all already structured + reachable through one realistic reader path,
// and S094 proves them on that path (VamlReader_s094_test.cpp). It also
// resolves the S092-recorded hand-off: `x:Class` is root-only by definition
// (WPF-faithful), so a Class directive on a NON-root element was being recorded
// as root_class_ silently — a silent last-wins hole. S094 makes it a genuine
// reader error via the ONE new arm below. No other arm is invented (the
// unknown-type / malformed-value genres live in their lower layers and
// propagate as-is; duplicate-typed arms there would be duplicate surface).
enum class VamlReaderError
{
    None,          // no reader error
    UnknownProperty, // an attribute names a property with no global definition (S091)
    DuplicateName, // an x:Name was registered more than once in one Load (S092)
    ClassOnNonRoot // an x:Class directive was authored on a NON-root element (S094)
};

// S091: the reader error — names the offending attribute key. what() renders
// "unknown property '<key>'". Parallel to ElementResolutionException (S088) /
// ValueConversionException (S089) at the reader layer, and the S094-documented
// baseline of the full reader error taxonomy.
//
// S092: for DuplicateName, `property`/`name` holds the duplicate x:Name value;
// what() renders "duplicate x:Name '<name>'" (the offending NAME, not the
// attribute key). S094: for ClassOnNonRoot, `property` holds the offending
// x:Class VALUE; what() renders "x:Class on a non-root element '<class>'".
class VAML_CORE_EXPORT VamlReaderException : public std::runtime_error
{
public:
    VamlReaderException(VamlReaderError kind, std::string property, std::string what);

    VamlReaderError kind() const noexcept { return kind_; }
    // The offending attribute's key (unknown-property), the duplicate x:Name
    // value (DuplicateName), or the offending x:Class value (ClassOnNonRoot).
    const std::string& property() const noexcept { return property_; }

private:
    VamlReaderError kind_;
    std::string property_;
};

// S092 (Strand B) — ONE retained deferred markup-extension on a built object.
// `target` is the object the extension was authored on, `key` the attribute
// key (e.g. "Text"), and `source` the ORIGINAL extension source text (e.g.
// "{Binding Title}") retained verbatim — the "S2 tag" so S252/S253 (the
// Binding sprint) can re-parse and bind it. The reader does NOT resolve the
// extension (that is S252's contract); it only makes it findable. Resolved by
// S252 — recorded NO-GAP, not silent.
struct DeferredExtension
{
    const DependencyObject* target = nullptr;
    std::string_view key;    // S140: interned (pool-backed, no owned churn)
    std::string_view source; // S140: interned
};

// S092 (Strand C) — ONE deferred event handler on a built object. `target` is
// the object the event was authored on, `eventName` the registered event name
// (e.g. "Click"), and `handlerName` the authored handler name (e.g. "OnClick").
// S322 resolves each record against the x:Class owner registry to wire the real
// routed event handler (S320 RoutedEvent primitive). MINIMAL REAL seed —
// resolved by S322; Do NOT extend silently.
struct DeferredEventHandler
{
    const DependencyObject* target = nullptr;
    std::string_view eventName;   // S140: interned
    std::string_view handlerName; // S140: interned
};

class VAML_CORE_EXPORT VamlReader
{
public:
    VamlReader();
    ~VamlReader();

    VamlReader(const VamlReader&) = delete;
    VamlReader& operator=(const VamlReader&) = delete;

    // Parses `text` and builds a live tree — equivalent to parse_vaml(text)
    // then Load(root). Parse failures throw XmlScanException (S081) /
    // XmlParseException (S085); resolution/conversion failures propagate their
    // structured errors (see the header comment).
    [[nodiscard]] std::unique_ptr<DependencyObject> Load(std::string_view text);

    // Builds a live tree from an already-parsed root. The headline S091
    // surface. See the header comment for the algorithm + ownership model.
    // S092: also records the x:Name registry (Strand A), deferred markup
    // extensions (Strand B) and deferred event handlers (Strand C).
    [[nodiscard]] std::unique_ptr<DependencyObject> Load(const ParsedElement& root);

    // The non-root built objects, in construction order. Each is owned exactly
    // once by this reader; the returned root is separate. Mirrors the
    // ownership-model decision (header comment).
    [[nodiscard]] const std::vector<std::unique_ptr<DependencyObject>>&
    retained_objects() const noexcept { return retained_; }

    // ------------------------------------------------------------------
    // S092 Strand A — x:Name registry (REAL, tested, permanent).
    // ------------------------------------------------------------------

    // Returns the built object registered under `name`, or nullptr if no such
    // x:Name was recorded. Works for the root (a root carrying x:Name is
    // registered) and every retained child. NAMES WERE CLEARED at the start of
    // the most recent Load — this reflects the CURRENT read.
    [[nodiscard]] DependencyObject* named(std::string_view name) const noexcept;

    // Number of currently-registered names (0 after a Load with no x:Name).
    [[nodiscard]] std::size_t name_count() const noexcept;

    // Registered x:Name entries in REGISTRATION (document) order: (name, obj).
    // Keys are owned std::string copies (the documented store discipline —
    // never the PropertyId view).
    [[nodiscard]] const std::vector<std::pair<std::string, DependencyObject*>>&
    names() const noexcept;

    // The authored x:Class name on the root, empty when absent. S092 records
    // ONLY the authored name; binding it to an owner object/type is deferred
    // (Sprint 8 / S286 / InitializeComponent-analogue). Recorded NO-GAP.
    [[nodiscard]] const std::string& root_class() const noexcept;

    // ------------------------------------------------------------------
    // S092 Strand B — deferred markup extensions (the S2 tag). Resolved by
    // S252. Retained only; never converted/set here.
    // ------------------------------------------------------------------
    [[nodiscard]] const std::vector<DeferredExtension>& deferred_extensions() const noexcept;

    // ------------------------------------------------------------------
    // S092 Strand C — event hooks (MINIMAL REAL; resolved by S322, Sprint 8).
    // ------------------------------------------------------------------
    // [MINIMAL REAL] VamlReader event hooks — RESOLVED BY S322 (Sprint 8).
    // Do NOT extend silently; do NOT backfill.

    // Declares that `eventName` is an EVENT, not a property. An attribute whose
    // key exactly matches a registered event name is routed to a deferred-event
    // record in build() (never a property set / unknown-property throw). The
    // host (future x:Class owner / control type, via CompositionRoot later)
    // calls this before Load. Idempotent.
    void register_event(std::string_view eventName);

    // The deferred event handlers recorded by the most recent Load, document
    // order. MINIMAL REAL seed — consumed/resolved by S322.
    [[nodiscard]] const std::vector<DeferredEventHandler>&
    deferred_event_handlers() const noexcept;

private:
    // Recursive build (the algorithm in the header comment). Returns the owned
    // live object; appends non-root children to `retained_`. Throws the
    // structured errors described above. S094: `depth` is 0 for the root,
    // incremented per recursion level; it gates the ClassOnNonRoot rule
    // (x:Class is root-only — a Class directive at depth != 0 throws).
    std::unique_ptr<DependencyObject> build(const ParsedElement& element, std::size_t depth);

    // Converts a plain string literal into a Value of the DP's bound type.
    // String -> identity; otherwise routes through ValueConverterRegistry::global()
    // via the type-name mapping. Throws ValueConversionException (S089/S090).
    static Value convertLiteral(const DependencyProperty& dp, std::string_view literal);

    std::vector<std::unique_ptr<DependencyObject>> retained_;

    // S092 Strand A — x:Name registry (insertion-ordered; owned keys).
    std::vector<std::pair<std::string, DependencyObject*>> names_;
    std::string root_class_;

    // S092 Strand B — deferred markup extensions (document order).
    std::vector<DeferredExtension> deferred_extensions_;

    // S092 Strand C — registered event names + deferred event handlers.
    std::vector<std::string> event_names_;
    std::vector<DeferredEventHandler> deferred_event_handlers_;
};

} // namespace vaml
