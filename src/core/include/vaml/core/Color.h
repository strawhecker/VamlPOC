#pragma once
// VAML Color — 8-bit RGBA.
// S089 (dep:S088): RESOLVES the S030 MINIMAL REAL marker — Color now carries
// its full string-parsing semantics (parse_color, "#RRGGBB"/"#AARRGGBB" ->
// Color) and the sentinel tag is removed here per the resolution contract.
// This resolution is part of S089's own feature step, not a backfill (NO-GAP).

#include "vaml/core/ValueConversionError.h"
#include "vaml/core/export.h"

#include <cstdint>
#include <string_view>

namespace vaml {

// Premultiplied-convention-free, deterministic 8-bit RGBA color.
// Components are stored in R,G,B,A order (byte 0 = red); parse_color maps hex
// nibbles straight to bytes with no byte-order ambiguity.
struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0; // 255 = fully opaque

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;
};

// S089: parses a color from a string. Accepted forms:
//   - "#RRGGBB"   (3 bytes; alpha defaults to 255 = opaque)
//   - "#AARRGGBB" (4 bytes; alpha honored)
//   - the leading '#' is OPTIONAL (accepted either way)
// Hex digits are case-insensitive; 6 digits map to R,G,B in that order, 8
// digits to A,R,G,B (WPF-compatible argb). A malformed literal (wrong length,
// a non-hex digit, or empty) throws ValueConversionException with kind()
// MalformedValue and type name "Color". Named colors are explicitly OUT of
// scope for S089 (recorded NO-GAP: named-color tables are a later
// brush/resource step, not this sprint).
VAML_CORE_EXPORT Color parse_color(std::string_view literal);

} // namespace vaml
