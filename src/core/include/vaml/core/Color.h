#pragma once
// VAML Color — 8-bit RGBA.
// S030 MINIMAL REAL: seeds the true Color interface (storage + byte order).
// String/value parsing ("#RRGGBB" -> Color) and the full converter semantics
// are owned by S089 (value-converter registry). Do NOT extend silently.
// [MINIMAL REAL] Color — RESOLVED BY S089. Do NOT extend silently; do NOT backfill.

#include <cstdint>

namespace vaml {

// Premultiplied-convention-free, deterministic 8-bit RGBA color.
// Components are stored in R,G,B,A order (byte 0 = red) so S089's "#RRGGBB"
// parser maps hex nibbles straight to bytes with no byte-order ambiguity.
struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;
};

} // namespace vaml
