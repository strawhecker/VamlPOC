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
//
// S11p (WPF-fidelity re-pointing) — ORDINALS NOW MATCH the WPF source exactly
// (FrameworkElement.cs: `HorizontalAlignment { Left=0, Center=1, Right=2,
// Stretch=3 }`). Previously vaml had `Stretch=0` (an invented re-ordering that
// tied the DP default's bare Int32(0) to Stretch — the S11p drift we removed).
// All consumers compare by NAME (`hAlign == HorizontalAlignment::Stretch`), so
// re-pointing only changes the numeric slots; the DP default is now expressed
// SYMBOLICALLY (HorizontalAlignment::Stretch), never as a bare 0.
enum class HorizontalAlignment : std::uint8_t
{
    Left    = 0, // child sits at the left; width = desired (clamped)
    Center  = 1, // child centered; width = desired (clamped)
    Right   = 2, // child sits at the right; width = desired (clamped)
    Stretch = 3, // child fills the available slot (default)
};

// WPF-faithful vertical alignment (same Int32-value-path convention).
// S11p re-pointing: ORDINALS NOW MATCH WPF (FrameworkElement.cs: `VerticalAlignment
// { Top=0, Center=1, Bottom=2, Stretch=3 }`). Default expressed symbolically
// (VerticalAlignment::Stretch), never a bare 0.
enum class VerticalAlignment : std::uint8_t
{
    Top     = 0, // child sits at the top; height = desired (clamped)
    Center  = 1, // child centered; height = desired (clamped)
    Bottom  = 2, // child sits at the bottom; height = desired (clamped)
    Stretch = 3, // child fills the available slot (default)
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

// Computes a child's MEASURED DesiredSize from its un-finalized content extent
// (`natural`) against the `available` bounds, honoring the S123 infinity-aware
// stretch-vs-wrap contract. S123's UIElement::Measure calls this to finalize
// its DesiredSize; S125/S129/S131 panels reuse it when measuring children.
// WPF-faithful + infinity-aware (INFINITE available = Size::Infinite(),
// the "unbounded axis" sentinel S123 consumes):
//   * Stretch axis + FINITE available   -> DesiredSize = available (fills).
//   * Stretch axis + INFINITE available -> DesiredSize = natural (cannot
//     stretch to infinity, so the axis WRAPS to content).
//   * Non-Stretch axis (Left/Center/Right / Top/Center/Bottom) + FINITE
//     available -> DesiredSize = min(natural, available) (wrap/clamp).
//   * Non-Stretch axis + INFINITE available -> DesiredSize = natural
//     (min(x, infinity) == x).
// Never negative: the result is clamped >= 0 and `Size::Infinite()` never
// escapes into a DesiredSize (a measured size is always finite).
VAML_CORE_EXPORT Size compute_measured_desired(Size natural,
                                               Size available,
                                               HorizontalAlignment hAlign,
                                               VerticalAlignment vAlign);

// The S124 rectangular CLIP op — "clip bounds honored by render slot".
// Returns `content ∩ clipBounds` (the clipped content rect), clamped so the
// result is ALWAYS a sub-rect of `clipBounds`, never escapes it, and never has
// negative geometry:
//   * content ⊆ clipBounds   -> returned unchanged (fully visible).
//   * partial overlap         -> returned clamped to clipBounds (the overflowing
//                               edges are cut to the clip edge).
//   * disjoint (no overlap)   -> an EMPTY zero-rect (nothing renders).
// The clip bounds are the element's render slot — S124's final_rect() — so the
// renderer (S208) clips each element to its own final_rect(), and ScrollViewer
// (S405) clips a scrolled viewport via the same op. This is REAL rectangular
// clipping (the value-domain precedent of compute_final_rect/compute_measured_
// desired). It is NOT arbitrary-geometry clipping: Geometry.h today only has
// Size/Rect, and WPF's arbitrary Clip Geometry is not representable until the
// renderer introduces geometry (S208). Recorded NO-GAP, not silent — the
// rectangular render-slot clip is the S124 scope; arbitrary-geometry clip is
// S208's. Consumes `Rect` from Geometry.h.
VAML_CORE_EXPORT Rect clip_to_bounds(const Rect& content, const Rect& clipBounds);

} // namespace vaml
