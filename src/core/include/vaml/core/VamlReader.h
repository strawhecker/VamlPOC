#pragma once
// S091 (dep:S090) — VamlReader: build a LIVE dependency-object tree from a
// parsed (S085) VAML tree, setting properties and linking children to parents.
//
// This is the runtime reader entry point. It composes three completed
// primitives into a real object tree:
//   - S088 resolve_element  — ParsedElement -> live instance (global registry);
//   - S089/S090 ValueConverterRegistry — plain string literal -> typed Value
//     (Int32 / Bool / Color / Thickness; String is identity);
//   - the S039/S043 global DependencyProperty table + S043-attached-aware
//     DependencyObject::Set — attribute name -> stored value (attached keys
//     like "Grid.Row" pass through to Set, which is already attached-aware).
//
// Two entry points:
//   - Load(std::string_view text) — parse_vaml(text) then Load(root).
//   - Load(const ParsedElement&)  — build a live tree from an already-parsed
//     tree (the headline S091 surface).
//
// OWNERSHIP MODEL (Decision D1 of the S091 plan — accepted):
//   The reader retains ownership of every NON-ROOT built object in an internal
//   retention vector and returns the ROOT to the caller. A plain
//   DependencyObject carries no visual-children list (that is S121 — recorded,
//   no overlap, no gap), so retained_objects() is how a consumer / tree-
//   verification step reaches the non-root nodes the reader built. Each built
//   object is owned EXACTLY ONCE: children are owned by `retained_`, the root
//   by the returned unique_ptr. set_parent (S039/S041) is a NON-OWNING linkage;
//   keeping ownership here (rather than in a parent's children container, which
//   S121 will own) avoids dangling while the Load call is live. The durable
//   "tree owns its children" ownership transfers into the UIElement visual tree
//   at S120/S121 — recorded NO-GAP, not a silent backfill.
//
// BUILD ALGORITHM (private recursive build), per ParsedElement:
//   1. RESOLVE -> resolve_element(element) (global registry). Unknown type ->
//      ElementResolutionException naming type + line:col propagates (S088).
//   2. SET PROPS -> for each ParsedAttribute:
//        - directive != None -> SKIP (defer): x:Name/x:Class -> S092, x:Key ->
//          S361, x:Null -> S084. Recorded NO-GAP, NOT silent.
//        - isExtension      -> SKIP (defer to S092 markup-extension
//          resolution). Recorded NO-GAP, NOT silent.
//        - else plain literal -> set:
//            target DP = DependencyProperty::TryGet(attr.key); nullptr ->
//              throw VamlReaderException (unknown-property baseline; S094 owns
//              the full reader error taxonomy). A dotted attached key
//              ("Grid.Row") passes through as the DP name — S043 already makes
//              Set attached-aware by name.
//            convert literal -> Value: ValueType::String -> Value::String
//              (identity); otherwise map boundType() -> converter name
//              (Int32/Bool/Color/Thickness) and ValueConverterRegistry::global()
//              .convert(typeName, literal). Malformed/UnknownType ->
//              ValueConversionException propagates (S089/S090).
//            obj->Set(PropertyId(attr.key), value) — works on MINIMAL REAL seed
//              objects (Button/StackPanel/...) via GLOBAL-registered DPs, which
//              Set resolves even when the object never locally registered them
//              (S043 attached-aware global resolution).
//   3. BUILD CHILDREN -> recurse each child, child->set_parent(obj.get()) (the
//      S039/S041 parent-link + inheritance-scope seed), then move child into
//      retained_. NO owned children list on DependencyObject (that is S121;
//      recorded). Subtree exceptions propagate from the offending child.
//   4. RETURN obj (the ROOT is NOT placed in retained_; each object owned
//      exactly once).
//
// ERROR BASELINE (S091 scope): VamlReaderException : std::runtime_error covers
// the UNRESOLVABLE-PROPERTY case only (an attribute naming a property with no
// global definition). Unknown-element (ElementResolutionException) and
// malformed-value/unknown-type (ValueConversionException) already propagate as
// real structured errors (S088/S089/S090). S094 = the dedicated
// error-taxonomy / reader-suite gate (recorded, NOT silent).
//
// STARTUP: Load() lazily runs register_builtin_elements() +
// register_builtin_converters() ONCE (idempotent guards) so the reader works
// standalone without the host wiring up globals — matches the shared-folder VM
// lane where each test process starts fresh. Documented single-threaded.
//
// MINIMAL REAL? No. VamlReader::Load is delivered COMPLETE for its declared
// scope (resolve + set props + build children + unknown-property baseline) and
// tested; S092 (markup extensions / x:Name) and S094 (full error taxonomy)
// extend by normal feature progression — no sentinel, no silent backfill.

#include "vaml/core/DependencyObject.h" // unique_ptr<DependencyObject>
#include "vaml/core/DependencyProperty.h" // DependencyProperty (convertLiteral decl)
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

struct ParsedElement; // forward-declared (Load(const ParsedElement&) takes a ref)

// S091: the reader's OWN error taxonomy — contains ONLY what the reader can
// itself produce (S048 discipline — no dead enum arms). An attribute may name
// a property that has no global DependencyProperty definition; that is this
// step's one genuinely reader-local error. Everything else the reader can hit
// already propagates as a structured error from a lower layer
// (ElementResolutionException / ValueConversionException).
enum class VamlReaderError
{
    None,          // no reader error
    UnknownProperty // an attribute names a property with no global definition
};

// S091: the reader error — names the offending attribute key. what() renders
// "unknown property '<key>'". Parallel to ElementResolutionException (S088) /
// ValueConversionException (S089) at the reader layer, and the S094-documented
// baseline of the full reader error taxonomy.
class VAML_CORE_EXPORT VamlReaderException : public std::runtime_error
{
public:
    VamlReaderException(VamlReaderError kind, std::string property, std::string what);

    VamlReaderError kind() const noexcept { return kind_; }
    // The offending attribute's key (the property name that had no definition).
    const std::string& property() const noexcept { return property_; }

private:
    VamlReaderError kind_;
    std::string property_;
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
    [[nodiscard]] std::unique_ptr<DependencyObject> Load(const ParsedElement& root);

    // The non-root built objects, in construction order. Each is owned exactly
    // once by this reader; the returned root is separate. Mirrors the
    // ownership-model decision (header comment).
    [[nodiscard]] const std::vector<std::unique_ptr<DependencyObject>>&
    retained_objects() const noexcept { return retained_; }

private:
    // Recursive build (the algorithm in the header comment). Returns the owned
    // live object; appends non-root children to `retained_`. Throws the
    // structured errors described above.
    std::unique_ptr<DependencyObject> build(const ParsedElement& element);

    // Converts a plain string literal into a Value of the DP's bound type.
    // String -> identity; otherwise routes through ValueConverterRegistry::global()
    // via the type-name mapping. Throws ValueConversionException (S089/S090).
    static Value convertLiteral(const DependencyProperty& dp, const std::string& literal);

    std::vector<std::unique_ptr<DependencyObject>> retained_;
};

} // namespace vaml
