#pragma once
// VAML Thickness — four edge insets in layout units (floats).
// S089 (dep:S088): RESOLVES the S030 MINIMAL REAL marker — Thickness now
// carries its full comma-list string-parsing semantics (parse_thickness,
// "10,20" -> 10,20,10,20) and the sentinel tag is removed here per the
// resolution contract. This resolution is part of S089's own feature step, not
// a backfill (NO-GAP).

#include "vaml/core/ValueConversionError.h"
#include "vaml/core/export.h"

#include <string_view>

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

// S089: parses a thickness from a WPF-faithful comma-list string:
//   - 1 value  = uniform on all four edges   ("10"     -> 10,10,10,10)
//   - 2 values = (left, top); right/bottom MIRRORED
//                            ("10,20"   -> 10,20,10,20)
//   - 4 values = left, top, right, bottom explicit
//                            ("1,2,3,4" -> 1,2,3,4)
// Each token is parsed as a real float (whole-string). A malformed literal
// (empty, a count other than 1/2/4, or a non-numeric token) throws
// ValueConversionException with kind() MalformedValue and type name
// "Thickness". No other count (e.g. 3) is supported — mirrors WPF.
VAML_CORE_EXPORT Thickness parse_thickness(std::string_view literal);

} // namespace vaml
