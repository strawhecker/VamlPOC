// S089 (dep:S088) — Thickness string parsing. RESOLVES the S030 Thickness
// MINIMAL REAL marker: parse_thickness gives Thickness its full comma-list
// parsing semantics. The sentinel is removed (resolution is part of this
// feature step, not a backfill — NO-GAP).

#include "vaml/core/Thickness.h"

#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

namespace {

[[noreturn]] void throw_bad_thickness(std::string_view literal)
{
    throw ValueConversionException(
        ValueConversionError::MalformedValue, std::string("Thickness"), std::string(literal),
        "cannot convert '" + std::string(literal) + "' to Thickness (MalformedValue)");
}

// Splits the literal on ','. A leading/trailing/doubled comma yields an EMPTY
// token (marked by an empty element in the result), which the caller treats as
// malformed. An empty literal yields an empty vector.
std::vector<std::string_view> split_comma(std::string_view s)
{
    std::vector<std::string_view> parts;
    std::size_t begin = 0;
    while (true)
    {
        const std::size_t comma = s.find(',', begin);
        const std::size_t end = comma == std::string_view::npos ? s.size() : comma;
        parts.push_back(s.substr(begin, end - begin));
        if (comma == std::string_view::npos)
        {
            break;
        }
        begin = comma + 1;
    }
    return parts;
}

// Whole-string float parse: EVERY character must be part of the number (no
// leading/trailing whitespace — from_chars does not skip it, and a trailing
// character marks malformed). Returns true on a full, valid float parse; on
// success only, `out` is written.
bool parse_float(std::string_view s, float& out) noexcept
{
    if (s.empty())
    {
        return false;
    }
    const char* const first = s.data();
    const char* const last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc() && ptr == last;
}

} // namespace

VAML_CORE_EXPORT
Thickness parse_thickness(std::string_view literal)
{
    const std::vector<std::string_view> parts = split_comma(literal);
    if (parts.empty())
    {
        throw_bad_thickness(literal);
    }

    const std::size_t count = parts.size();

    // 1: uniform on all four edges ("10" -> 10,10,10,10).
    if (count == 1)
    {
        float uniform = 0.0f;
        if (!parse_float(parts[0], uniform))
        {
            throw_bad_thickness(literal);
        }
        return Thickness{uniform, uniform, uniform, uniform};
    }

    // 2: (left, top) with right/bottom MIRRORED ("10,20" -> 10,20,10,20).
    if (count == 2)
    {
        float l = 0.0f, t = 0.0f;
        if (!parse_float(parts[0], l) || !parse_float(parts[1], t))
        {
            throw_bad_thickness(literal);
        }
        return Thickness{l, t, l, t};
    }

    // 4: left, top, right, bottom explicit ("1,2,3,4" -> 1,2,3,4).
    if (count == 4)
    {
        float l = 0.0f, t = 0.0f, r = 0.0f, b = 0.0f;
        if (!parse_float(parts[0], l) || !parse_float(parts[1], t) ||
            !parse_float(parts[2], r) || !parse_float(parts[3], b))
        {
            throw_bad_thickness(literal);
        }
        return Thickness{l, t, r, b};
    }

    // Any other count (0, 3, 5+) is malformed — WPF accepts only 1/2/4.
    throw_bad_thickness(literal);
}

} // namespace vaml
