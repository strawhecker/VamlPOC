// S089 (dep:S088) — Color string parsing. RESOLVES the S030 Color MINIMAL REAL
// marker: parse_color gives Color its full "#RRGGBB"/"#AARRGGBB" parsing
// semantics. The sentinel is removed (resolution is part of this feature step,
// not a backfill — NO-GAP).

#include "vaml/core/Color.h"

namespace vaml {

namespace {

// Maps one ASCII hex nibble to its value, or -1 if not a hex digit.
// Case-insensitive per the Color.h contract.
int hexNibble(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

[[noreturn]] void throw_bad_color(std::string_view literal)
{
    throw ValueConversionException(
        ValueConversionError::MalformedValue, std::string("Color"), std::string(literal),
        "cannot convert '" + std::string(literal) + "' to Color (MalformedValue)");
}

} // namespace

VAML_CORE_EXPORT
Color parse_color(std::string_view literal)
{
    // Optional leading '#'. With it: 6 digits (RRGGBB, opaque alpha) or 8
    // digits (AARRGGBB); without it the same digit counts.
    std::size_t start = 0;
    if (!literal.empty() && literal[0] == '#')
    {
        start = 1;
    }

    const std::size_t digits = literal.size() - start;
    if (digits != 6 && digits != 8)
    {
        throw_bad_color(literal);
    }

    // Accumulate each byte as exactly two hex nibbles (first nibble high).
    std::uint8_t rgba[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < digits; ++i)
    {
        const int hi = hexNibble(literal[start + i]);
        if (hi < 0)
        {
            throw_bad_color(literal);
        }
        rgba[i / 2] = static_cast<std::uint8_t>((rgba[i / 2] << 4) | static_cast<std::uint8_t>(hi));
    }

    if (digits == 6)
    {
        // "#RRGGBB" -> RGB; alpha defaults to fully opaque.
        return Color{rgba[0], rgba[1], rgba[2], 255};
    }
    // "#AARRGGBB" -> A,R,G,B in that order (WPF-compatible argb).
    return Color{rgba[1], rgba[2], rgba[3], rgba[0]};
}

} // namespace vaml
