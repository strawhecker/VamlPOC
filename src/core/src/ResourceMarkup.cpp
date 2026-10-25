// S361 (dep:S360) — resource-markup parse + the Static/Dynamic resolution
// seams. See ResourceMarkup.h.

#include "vaml/core/ResourceMarkup.h"

#include <cstddef>
#include <string_view>

namespace vaml {

namespace {

// Extracts the resource KEY from a `{StaticResource Key}` / `{DynamicResource Key}`
// source. `name` is the resource-markup name (e.g. "StaticResource"). On a
// well-formed reference the key is the trimmed non-empty token after the name
// (up to the closing '}'); otherwise the call sets *malformed and returns an
// empty established key.
[[nodiscard]] std::string_view resource_key_from(std::string_view source,
                                                 std::string_view name,
                                                 bool* malformed)
{
    // source looks like "{StaticResource Key}" / "{DynamicResource Key}".
    // Find `name` inside the braces, then take the token after it up to the '}'.
    const std::size_t namePos = source.find(name);
    if (namePos == std::string_view::npos)
    {
        *malformed = false; // not a resource markup at all
        return {};
    }
    std::size_t start = namePos + name.size();
    // Skip whitespace after the resource name.
    while (start < source.size() && (source[start] == ' ' || source[start] == '\t'))
    {
        ++start;
    }
    // The key runs to the closing '}' (trim trailing whitespace).
    if (start >= source.size() || source[start] == '}')
    {
        *malformed = true; // empty key: "{StaticResource }"
        return {};
    }
    const std::size_t closeBrace = source.rfind('}');
    std::size_t end = closeBrace;
    while (end > start && (source[end - 1] == ' ' || source[end - 1] == '\t'))
    {
        --end;
    }
    if (end <= start)
    {
        *malformed = true; // empty/whitespace key
        return {};
    }
    return source.substr(start, end - start);
}

} // namespace

VAML_CORE_EXPORT
ResourceMarkup parse_resource_markup(std::string_view source) noexcept
{
    ResourceMarkup out;

    // A well-formed resource reference starts with '{' and names one of the
    // resource-markup keywords. Anything else is NOT this layer's markup.
    if (source.size() < 3 || source[0] != '{')
    {
        return out; // kind None, not recognized, not malformed (a non-resource extension)
    }

    constexpr std::string_view kStatic = "StaticResource";
    constexpr std::string_view kDynamic = "DynamicResource";
    if (source.find(kStatic) != std::string_view::npos)
    {
        out.kind = ResourceReferenceKind::Static;
        out.recognized = true;
        out.key = resource_key_from(source, kStatic, &out.malformed);
        return out;
    }
    if (source.find(kDynamic) != std::string_view::npos)
    {
        out.kind = ResourceReferenceKind::Dynamic;
        out.recognized = true;
        out.key = resource_key_from(source, kDynamic, &out.malformed);
        return out;
    }
    return out; // not a resource reference (a binding / x:Null / bare extension)
}

VAML_CORE_EXPORT
bool static_resource_markup(std::string_view source) noexcept
{
    const ResourceMarkup m = parse_resource_markup(source);
    return m.recognized && m.kind == ResourceReferenceKind::Static && !m.malformed;
}

VAML_CORE_EXPORT
bool dynamic_resource_markup(std::string_view source) noexcept
{
    const ResourceMarkup m = parse_resource_markup(source);
    return m.recognized && m.kind == ResourceReferenceKind::Dynamic && !m.malformed;
}

} // namespace vaml
