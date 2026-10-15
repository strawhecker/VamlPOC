#pragma once
// VAML layout slot + alignment — the arrange-slot geometry and the alignment
// algorithm that S124's Arrange consumes (S122: LayoutSlot + final rect +
// DesiredSize + Margin + alignment).
//
// WPF-faithful model: a parent allocates a LayoutSlot (the arrange bounds, in
// the parent's own space) to each child AFTER removing the child's Margin;
// the child's DesiredSize (from Measure) is then aligned inside that slot by
// HorizontalAlignment/VerticalAlignment to produce the child's final rect.
// `compute_final_rect` is the REAL keep+extend algorithm — kept into S124's
// Arrange, NOT a stub.

#include "vaml/core/Geometry.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/export.h"

#include <cstdint>

namespace vaml {

// WPF-faithful horizontal alignment, stored on the HorizontalAlignment DP via
// the Int32 value path (the enum rides Int32, same pattern as Visibility).
enum class HorizontalAlignment : std::uint8_t
{
    Stretch = 0, // child fills the available slot (default)
    Left    = 1, // child sits at the left; width = desired (clamped)
    Center  = 2, // child centered; width = desired (clamped)
    Right   = 3, // child sits at the right; width = desired (clamped)
};

// WPF-faithful vertical alignment (same Int32-value-path convention).
enum class VerticalAlignment : std::uint8_t
{
    Stretch = 0, // child fills the available slot (default)
    Top     = 1, // child sits at the top; height = desired (clamped)
    Center  = 2, // child centered; height = desired (clamped)
    Bottom  = 3, // child sits at the bottom; height = desired (clamped)
};

// The arrange slot a parent allocates to a child, AFTER margins (WPF-faithful:
// the slot is what remains once the child's Margin is removed from the
// parent's available bounds).
struct LayoutSlot
{
    Rect bounds;
};

// Computes a child's final rect inside `slot` given its DesiredSize, Margin,
// and alignments. The REAL algorithm S124's Arrange will call (kept + extended
// into Arrange; MINIMAL-REAL-kept semantics, not a stub):
//   1. inset the slot by margin (left/top add; right/bottom subtract);
//   2. child size = available (Stretch) or min(desired, available);
//   3. x/y offset by alignment: Center=(available - child)/2, Right/Bottom =
//      available - child; Stretch already fills.
// Margins are clamped to >= 0 so a margin larger than the slot cannot drive a
// negative child size (NO negative geometry from the layout contract).
VAML_CORE_EXPORT Rect compute_final_rect(const LayoutSlot& slot,
                                         Size desired,
                                         Thickness margin,
                                         HorizontalAlignment hAlign,
                                         VerticalAlignment vAlign);

} // namespace vaml
