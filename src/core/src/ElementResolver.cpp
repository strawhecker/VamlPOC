// S088 (dep:S087) — ElementResolver out-of-line definitions. Resolves a
// ParsedElement into a live DependencyObject via the S087 ElementFactoryRegistry,
// mapping an unknown type into a structured, line-carrying error.

#include "vaml/core/ElementResolver.h"

#include "vaml/core/ParsedElement.h"

#include <utility>

namespace vaml {

ElementResolutionException::ElementResolutionException(ElementResolutionError kind,
                                                       std::string typeName,
                                                       std::string what,
                                                       std::size_t line,
                                                       std::size_t col)
    : std::runtime_error(what + " (line " + std::to_string(line) + ", column " +
                         std::to_string(col) + ")")
    , kind_(kind)
    , type_name_(std::move(typeName))
    , line_(line)
    , col_(col)
{
}

namespace {

// S088: the type-name to feed the registry. Prefer local_name (the S082 "name
// sans prefix:") so `<local:Button>` resolves "Button"; fall back to the full
// tagName when local_name is empty.
std::string_view resolutionKey(const ParsedElement& element)
{
    if (!element.local_name.empty())
    {
        return element.local_name;
    }
    return element.tagName;
}

} // namespace

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> resolve_element(const ParsedElement& element,
                                                  const ElementFactoryRegistry& registry)
{
    const std::string_view key = resolutionKey(element);

    // Ask the registry for the factory, mapping "no such type" into the
    // structured reader error naming the type + line:col. Only
    // ElementNotFound (an absent/empty key) becomes UnknownType; a throwing
    // factory's construction exception propagates untouched (S087 contract —
    // see header).
    if (!registry.is_registered(key))
    {
        std::string name(key);
        throw ElementResolutionException(
            ElementResolutionError::UnknownType, name,
            "unknown element type '<" + name + ">'", element.line, element.col);
    }

    return registry.create(key);
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> resolve_element(const ParsedElement& element)
{
    return resolve_element(element, ElementFactoryRegistry::global());
}

} // namespace vaml
