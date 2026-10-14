#pragma once
// S088 (dep:S087) — ElementResolver: resolve a ParsedElement into a live
// DependencyObject instance; an unknown type becomes a STRUCTURED error naming
// the type AND the source line/column.
//
// This is the reader-boundary resolution primitive. It sits at the boundary
// between the S085 parsed tree and the S087 ElementFactoryRegistry:
//   * It consumes a ParsedElement (whose tagName/local_name carry the type
//     name and whose line/col carry the source position — S088's extension).
//   * It asks the registry for a factory keyed on the element's type name.
//   * It maps the registry's ElementNotFound into ElementResolutionException —
//     a line-carrying reader error ("unknown element type '<name>' (line N,
//     column M)") — while LEAVING the ElementFactoryRegistry itself free of any
//     source-location concept (the S087-recorded NO-GAP: the registry only
//     throws ElementNotFound; S088 is where that throw is mapped into the
//     structured reader error).
//
// SCOPE BOUNDARY (NO-GAP, not silent): S088 delivers the SINGLE-ELEMENT
// resolution primitive + the structured error, exactly its headline. It does
// NOT build the child hierarchy / set properties — that is S091's step
// (VamlReader::Load builds children, sets props), which will consume
// resolve_element. No dead tree-walk is added now.
//
// Factory-vs-unknown error split (S087 contract preserved):
//   * ElementNotFound (no factory for the name) -> ElementResolutionException
//     with kind() == ElementResolutionError::UnknownType.
//   * A factory that EXISTS but throws during construction (e.g. a control's
//     ctor failing) PROPAGATES its construction exception — it is NOT swallowed
//     into UnknownType. Construction failure is a live, different error; only
//     the "no such type" case is the reader's structured unknown-type error.
//
// Resolution key: the element's local_name (the S082 "element name sans
// prefix:"), falling back to tagName when local_name is empty. So a prefixed
// `<local:Button>` resolves "Button" — the registry is a flat type-name table,
// and the prefix (namespace) portion is not part of the registry key at this
// step (full namespace-aware type resolution is a later concern, recorded not
// silent). An empty final key (empty tag => no type at all) is reported as
// UnknownType.
//
// MINIMAL REAL? No. ElementResolver is delivered complete for its declared
// scope (resolve + structured error + tests). S091 extends by normal feature
// progression — no sentinel, no silent backfill.

#include "vaml/core/DependencyObject.h"       // unique_ptr<DependencyObject>
#include "vaml/core/ElementFactoryRegistry.h" // registry + ElementFactory
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vaml {

struct ParsedElement; // resolved type is forward-declared (resolve_element takes a ref)

// S088: the resolver's error taxonomy. Contains ONLY what S088 can actually
// produce (S048 discipline — no dead enum arms).
enum class ElementResolutionError
{
    None,       // no resolution error
    UnknownType // the element's type name has no registered factory
};

// S088: the structured resolution error — names the type AND the 1-based
// source line:col where the offending element was authored. Parallel to
// XmlParseException (S085) / XmlScanException (S081) at the reader layer.
// what() renders "unknown element type '<name>' (line N, column M)".
class ElementResolutionException : public std::runtime_error
{
public:
    ElementResolutionException(ElementResolutionError kind, std::string typeName,
                               std::string what, std::size_t line, std::size_t col);

    ElementResolutionError kind() const noexcept { return kind_; }
    // The offending element's type name (local_name / tagName, as resolved).
    const std::string& type_name() const noexcept { return type_name_; }
    // 1-based source position of the element's opening '<' (>= 1 always).
    std::size_t line() const noexcept { return line_; }
    std::size_t col() const noexcept { return col_; }

private:
    ElementResolutionError kind_;
    std::string type_name_;
    std::size_t line_;
    std::size_t col_;
};

// S088: resolves a single ParsedElement into a live instance via `registry`.
// The key is the element's local_name (fallback tagName); a key with no
// registered factory -> ElementResolutionException(UnknownType) naming the
// type + the element's line:col. A throwing factory propagates. See the header
// comment for the scope boundary.
VAML_CORE_EXPORT std::unique_ptr<DependencyObject> resolve_element(
    const ParsedElement& element, const ElementFactoryRegistry& registry);

// S088: convenience overload resolving against ElementFactoryRegistry::global()
// — the path the S091 Runtime VamlReader consumes once startup registration has
// run. Behavior identical to the registry-parameter overload otherwise.
VAML_CORE_EXPORT std::unique_ptr<DependencyObject> resolve_element(const ParsedElement& element);

} // namespace vaml
