#pragma once
// VAML Thickness — four edge insets in layout units (floats).
// S030 MINIMAL REAL: seeds the true Thickness interface (storage semantics).
// String/value parsing ("10,20,10,20" -> Thickness) and the full converter
// semantics are owned by S089 (value-converter registry). Do NOT extend
// silently.
// [MINIMAL REAL] Thickness — RESOLVED BY S089. Do NOT extend silently; do NOT backfill.

namespace vaml {

// Edge insets: left, top, right, bottom (top-left origin, layout units).
// Stored as floats to match layout arithmetic precision across the property
// system and measure/arrange passes (S3).
struct Thickness
{
    float left   = 0.0f;
    float top    = 0.0f;
    float right  = 0.0f;
    float bottom = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Thickness&) const noexcept = default;
};

} // namespace vaml
