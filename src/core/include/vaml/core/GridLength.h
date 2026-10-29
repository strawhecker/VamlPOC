#pragma once
// VAML GridLength — WPF-faithful row/column length (Absolute/Auto/Star).
// S131 (dep:S130_1): Grid row/col definitions; Absolute/Auto/Star resolution.
//
// RESOLVES the S087 `Grid` MINIMAL REAL seed at its own sprint (NO-GAP, not a
// backfill) — this header is the GridLength value-dimension the re-based Grid
// control consumes. It is a pure value type (like Color/Thickness/Geometry),
// NOT a MINIMAL REAL marker: it is complete + real on delivery and kept.
//
// WPF GridLength model: a row/column track is declared as one of three unit
// types — Absolute (a fixed Pixel length), Auto (sized to content), or Star
// (a proportional share of the space REMAINING after all Absolute + Auto
// tracks). The `value` field is the fixed length for Pixel and the weight for
// Star; it is unused for Auto. `resolve_grid_lengths` is the REAL, testable,
// public resolution primitive that turns a set of definitions + auto-measured
// extents plus an available extent into concrete resolved track lengths.

#include "vaml/core/ValueConversionError.h" // ValueConversionException (parse_grid_length)
#include "vaml/core/export.h"                // VAML_CORE_EXPORT
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace vaml {

// Forward declaration so parse_grid_length (declared below, before the full
// struct body) can return GridLength by value.
struct GridLength;

// 11k-3 — parses a Grid track length literal ("Auto" / "auto" / numeric /
// "N*" / "*") into a GridLength, WPF GridLengthConverter-faithful. HOISTED
// PUBLIC here from the 11j-9 file-local copy in VamlReader.cpp (promoted to an
// authorable value-type element resource, and reusable by the Grid-track
// consumer + the value-type resource branch). Malformed input throws
// ValueConversionException{MalformedValue} naming type "GridLength".
VAML_CORE_EXPORT GridLength parse_grid_length(std::string_view literal);

// The track unit kind, stored on a GridLength. Auto=0 / Pixel=1 / Star=2 (a
// stable ordinal so the unit can ride an Int32 value path if ever needed).
enum class GridUnitType : std::uint8_t
{
    Auto  = 0, // sized to the content of the cells in the track
    Pixel = 1, // a fixed Absolute length (value)
    Star  = 2, // a proportional weight (value) of the remaining space
};

// A track length: unit type + value (fixed length for Pixel, weight for Star;
// unused for Auto). Header-only value type with value equality + factories.
struct GridLength
{
    GridUnitType unitType = GridUnitType::Auto;
    float        value    = 0.0f;

    [[nodiscard]] constexpr bool operator==(const GridLength&) const noexcept = default;

    [[nodiscard]] static constexpr GridLength Auto() noexcept
    {
        return GridLength{ GridUnitType::Auto, 0.0f };
    }
    [[nodiscard]] static constexpr GridLength Pixel(float v) noexcept
    {
        return GridLength{ GridUnitType::Pixel, v };
    }
    [[nodiscard]] static constexpr GridLength Star(float w) noexcept
    {
        return GridLength{ GridUnitType::Star, w };
    }
};

// A column definition: its track width. A Grid holds a vector of these via
// column_definitions() (S131).
struct ColumnDefinition
{
    GridLength width = GridLength::Auto();
};

// A row definition: its track height (S131).
struct RowDefinition
{
    GridLength height = GridLength::Auto();
};

// Resolves a set of track definitions against `available` into concrete
// lengths, WPF-faithful:
//   * Absolute (Pixel) tracks -> `value`.
//   * Auto tracks -> the caller-supplied auto-measured extent (`autoMeasured[i]`,
//     i.e. the max content extent of the cells in that track).
//   * Star tracks -> a proportional share (by weight) of the space REMAINING
//     after all Absolute + Auto tracks are taken:
//         remaining = max(0, available - sumAbsolute - sumAuto)
//         star_i     = (weight_i / totalStar) * remaining
//     clamped >= 0; if totalStar == 0 (no star weights) every star resolves 0.
//
// Auto/Space semantics: absolute+auto always win their full extents; star
// shares are clamped so a negative-remaining never yields a negative length.
// Never produces negative geometry (NO-GAP contract, S123 discipline).
inline std::vector<float> resolve_grid_lengths(
    const std::vector<GridLength>& defs,
    const std::vector<float>& autoMeasured,
    float available)
{
    std::vector<float> resolved(defs.size(), 0.0f);
    float sumFixed = 0.0f; // sum of absolute + auto extents
    float totalStar = 0.0f;

    for (std::size_t i = 0; i < defs.size(); ++i)
    {
        switch (defs[i].unitType)
        {
            case GridUnitType::Pixel:
                resolved[i] = std::max(0.0f, defs[i].value);
                sumFixed += resolved[i];
                break;
            case GridUnitType::Auto:
                resolved[i] = (i < autoMeasured.size()) ? std::max(0.0f, autoMeasured[i]) : 0.0f;
                sumFixed += resolved[i];
                break;
            case GridUnitType::Star:
                totalStar += std::max(0.0f, defs[i].value);
                break;
        }
    }

    const float remaining = std::max(0.0f, available - sumFixed);
    if (totalStar > 0.0f)
    {
        const float unit = remaining / totalStar;
        for (std::size_t i = 0; i < defs.size(); ++i)
        {
            if (defs[i].unitType == GridUnitType::Star)
            {
                resolved[i] = std::max(0.0f, std::max(0.0f, defs[i].value) * unit);
            }
        }
    }
    // Else (totalStar == 0): every star track stays 0.

    return resolved;
}

} // namespace vaml
