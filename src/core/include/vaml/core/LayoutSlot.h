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
#include "vaml/core/Transform.h"  // S842a — Matrix2D (find_maximal_area_local_space_rect)
#include "vaml/core/export.h"

#include <cstdint>
#include <limits>

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
    Left    = 0, // child sits at the left; width = desired (may overflow + clip)
    Center  = 1, // child centered; width = desired (may overflow + clip)
    Right   = 2, // child sits at the right; width = desired (may overflow + clip)
    Stretch = 3, // child fills the slot, but keeps desired (clips) if larger
};

// WPF-faithful vertical alignment (same Int32-value-path convention).
// S11p re-pointing: ORDINALS NOW MATCH WPF (FrameworkElement.cs: `VerticalAlignment
// { Top=0, Center=1, Bottom=2, Stretch=3 }`). Default expressed symbolically
// (VerticalAlignment::Stretch), never a bare 0.
enum class VerticalAlignment : std::uint8_t
{
    Top     = 0, // child sits at the top; height = desired (may overflow + clip)
    Center  = 1, // child centered; height = desired (may overflow + clip)
    Bottom  = 2, // child sits at the bottom; height = desired (may overflow + clip)
    Stretch = 3, // child fills the slot, but keeps desired (clips) if larger
};

// ===========================================================================
// S842a — OVERRIDE-SEAM FOUNDATION supporting value types (WPF-faithful)
// ===========================================================================
// These are the WPF FrameworkElement/UIElement layout helpers the seam split
// (S842b) consumes. Purely additive: nothing here is wired into Measure/Arrange
// yet. Ground truth = E:\GitRepos\wpf (FrameworkElement.cs / UIElement.cs).

// WPF `DpiScale` (UIElement/Visual DpiScale): the ratio of the screen DPI to
// the layout DPI on each axis. Headless VAML is 96-DPI => {1.0, 1.0}.
struct DpiScale
{
    double dpiScaleX = 1.0;
    double dpiScaleY = 1.0;
};

// The WPF `GetDpi()` analogue: headless layout is always 1:1 (96 DPI). A future
// real-DPI host overrides the value handed to RoundLayout*; the algorithm is
// unchanged.
[[nodiscard]] inline DpiScale get_dpi() noexcept { return {}; }

// WPF `FrameworkElement.MinMax` (FE.cs:4036-4066) — the four derived folds that
// combine Width/Height (NaN=>auto) with Min/MaxWidth/Height. Built from the six
// FE layout DPs (the caller passes the resolved DP values). The derived fields
// (minWidth/maxWidth/minHeight/maxHeight) are the folded result.
struct MinMax
{
    MinMax(double feWidth, double feHeight,
           double feMinWidth, double feMinHeight,
           double feMaxWidth, double feMaxHeight) noexcept;

    double minWidth  = 0.0;
    double maxWidth  = std::numeric_limits<double>::infinity();
    double minHeight = 0.0;
    double maxHeight = std::numeric_limits<double>::infinity();
};

// WPF `UIElement.RoundLayoutValue` (UIElement.cs:1079-1104): DPI-aware rounding
// for UseLayoutRounding. If dpiScale is not ~1.0, round(value*dpi)/dpi and
// RESTORE the original on NaN/±Inf/MaxValue; otherwise round to the nearest
// integer (round-half-to-even, matching .NET Math.Round).
[[nodiscard]] VAML_CORE_EXPORT double round_layout_value(double value, double dpiScale) noexcept;

// WPF `RoundLayoutSize` (per-component, X uses dpiScaleX, Y uses dpiScaleY).
[[nodiscard]] VAML_CORE_EXPORT Size round_layout_size(Size size, DpiScale dpi) noexcept;

// WPF `RoundLayoutRect` (UIElement.cs:1106-1116): round X/Width with dpiScaleX,
// Y/Height with dpiScaleY.
[[nodiscard]] VAML_CORE_EXPORT Rect round_layout_rect(Rect rect, DpiScale dpi) noexcept;

// WPF `LayoutDoubleUtil` (FE.cs:4649-4665 area) — the FIXED-epsilon (0.00000153)
// layout compares, distinct from the relative DoubleUtil. `less_than` is the
// WPF `LessThan` used by ArrangeCore (FE.cs:4645 `LayoutDoubleUtil.LessThan`).
namespace layout_double_util
{
[[nodiscard]] inline bool are_close(double value1, double value2) noexcept
{
    constexpr double eps = 0.00000153; // WPF LayoutDoubleUtil.eps
    if (value1 == value2)
    {
        return true;
    }
    const double diff = value1 - value2;
    return (diff < eps) && (diff > -eps);
}

[[nodiscard]] inline bool less_than(double value1, double value2) noexcept
{
    return (value1 < value2) && !are_close(value1, value2);
}
} // namespace layout_double_util

// WPF `FrameworkElement.FindMaximalAreaLocalSpaceRect` (FE.cs:4099-4282) — used
// only when a non-identity LayoutTransform is set. Given the transform applied
// to the child, finds (in axis-aligned LOCAL space) the largest rectangle that,
// after transform, fits within `transformSpaceBounds`. Full faithful port: zero
// guard, +∞ guard, HasInverse guard, the b/c-zero branch, the a/d-zero branch,
// and the general branch with intercept-halving + the expandFactor correction.
[[nodiscard]] VAML_CORE_EXPORT Size find_maximal_area_local_space_rect(
    const Matrix2D& transform, Size transformSpaceBounds) noexcept;

// The arrange slot a parent allocates to a child, AFTER margins (WPF-faithful:
// the slot is what remains once the child's Margin is removed from the
// parent's available bounds).
struct LayoutSlot
{
    Rect bounds;
};

// The outcome of compute_final_rect: the (possibly OVERSIZED) arranged rect, the
// WPF NeedsClipBounds mirror, and the WPF GetLayoutClip SLOT-CLIENT rect.
// Struct member names follow the WPF source (`Rect`/`NeedsClipBounds`) — no
// invented public name.
struct arrange_result
{
    Rect rect;        // the arranged rect in slot/parent space — MAY exceed the slot
    bool needs_clip;  // mirror of WPF FrameworkElement.NeedsClipBounds
    // S833 (dep:S832): the WPF GetLayoutClip rectangular clip result — the
    // LAYOUT-SLOT CLIENT window (`layoutSlotSize - Margin`, alignment-offset
    // compensated, so in world coords it is `{slot.x+ml, slot.y+mt, inset, inset}`),
    // NOT the (possibly oversized) `rect`. FrameworkElement.GetLayoutClip
    // (FE.cs:4862-4960) returns `Rect(-offset, clippingSize)` where
    // `clippingSize = layoutSlotSize - Margin`; WPF has the slot because
    // `ensureClip(finalRect.Size)` (UIElement.cs:884) hands it the arrange slot.
    // Meaningful when `needs_clip` is true OR the element's S834 `ClipToBounds`
    // DP is true (the forced-mode case: the layout slot is clipped even when the
    // content fits, WPF FE.cs:4874/4917-4923). The renderer (S208) emits a
    // scissor to THIS rect when `needs_clip || ClipToBounds`, never to `rect`
    // (which would be a no-op for the overflow case).
    Rect clip_rect;
};

// Computes a child's final rect inside `slot` given its DesiredSize, Margin,
// and alignments — the WPF-faithful `FrameworkElement.ArrangeCore` geometry
// (FE.cs:4546-4775, `ComputeAlignmentOffset` FE.cs:4805-4849). The REAL
// keep+extend algorithm S124's Arrange calls (not a stub).
//
// S832 (Sprint 11ad — ARRANGE-SQUEEZE FIDELITY): the child is NEVER arranged
// smaller than what its own MeasureOverride accepted. A child whose content
// exceeds its slot keeps its UNCLIPPED desired size and is clipped LATER
// (`NeedsClipBounds`, consumed by the deferred S833 clip) — matching WPF
// ArrangeCore (FE.cs:4597-4606: `if (arrangeSize.Width < unclippedDesiredSize.Width)
// { NeedsClipBounds = true; arrangeSize.Width = unclippedDesiredSize.Width; }`).
//   1. inset the slot by margin -> the "clientSize" window (clamped >= 0);
//   2. innerDesired = max(0, desired - margin) — the S130_1 margin strip; the
//      stored DesiredSize IS WPF's UnclippedDesiredSize (divergence #1);
//   3. arrange size per axis: non-Stretch => innerDesired VERBATIM (may overflow,
//      FE.cs:4617-4623); Stretch => max(inset, innerDesired) — fills the slot,
//      but keeps innerDesired (and clips) when the slot is smaller
//      (FE.cs:4597-4624);
//   4. offset = ComputeAlignmentOffset(clientSize, arrangeSize) (Center/Stretch
//      half the leftover; Right/Bottom the full leftover; Stretch degenerates to
//      Left/Top once the child overflows — FE.cs:4811-4827), then re-add
//      `slot.origin + margin.top/left` (FE.cs:4772-4775);
//   5. needs_clip = (inset < innerDesired) on either axis. For VAML's `void`
//      ArrangeOverride (ink == arrange size), this equals WPF's post-override
//      `clientSize < clippedInkSize` (FE.cs:4756-4762) — the arrange-side legs
//      reduce to `inset < innerDesired`.
//   6. clip_rect = the slot-client window `{slot.x+ml, slot.y+mt, inset, inset}`
//      — the WPF GetLayoutClip rectangular result (RESOLVED BY S833; see the
//      arrange_result field doc). The trigger to APPLY it is `needs_clip` OR the
//      S834 `ClipToBounds` DP (the renderer clips when `needs_clip ||
//      ClipToBounds`, WPF FE.cs:4874).
// S833 RESOLVED the deferred scissor: `arrange_result.clip_rect` is the value
// the S208 renderer scissors to when `needs_clip`; the render slot `rect` is
// NEVER clipped to itself (that would be a no-op for an overflowing child).
// Margins clamp to >= 0 so a margin larger than the slot cannot drive negative
// geometry (NO negative geometry escapes the layout contract).
VAML_CORE_EXPORT arrange_result compute_final_rect(const LayoutSlot& slot,
                                                   Size desired,
                                                   Thickness margin,
                                                   HorizontalAlignment hAlign,
                                                   VerticalAlignment vAlign);

// Computes a child's MEASURED DesiredSize from its un-finalized content extent
// (`natural`). S123's UIElement::Measure calls this to finalize its DesiredSize;
// S125/S129/S131 panels reuse it when measuring children.
//
// S827/S828 (WPF-fidelity drift closure): Measure is ALIGNMENT-AGNOSTIC and
// does NOT clamp to the offered size. WPF's MeasureCore simply returns
// `MeasureOverride(content)` (FrameworkElement.cs:~4385) — the Stretch-vs-wrap
// decision is an ARRANGE concern (ArrangeCore, `FE.cs:4617-4623`), NOT a
// Measure one. So this finalizer returns the element's un-clamped content
// extent: `natural` per axis, guarded `>= 0`. DesiredSize may therefore EXCEED
// `available` (WPF's recorded "soft constraint" / unclippedDesiredSize,
// UIElement.cs:541-551) — the final render slot then follows the S832 (11ad)
// child-wins-then-clip contract in compute_final_rect (the child may exceed its
// slot and report needs_clip; the clip itself is the deferred S833 scissor).
//
// There is NO "Stretch + finite -> available (fill)" and NO invented
// "Stretch + infinite -> wrap" special-case: those were the drift S827/S828
// removed. A measured size is always finite and never negative (the input
// `natural` is finite; the result is clamped >= 0).
VAML_CORE_EXPORT Size compute_measured_desired(Size natural);

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
//
// S833 (conditional render-slot clip): the TRIGGER to apply this op at the
// renderer changed, NOT the math. A fitting element produces NO clip (unchanged
// S206/S207 batching); only a `needs_clip` element (the S832 NeedsClipBounds
// mirror, whose clip bounds are the SLOT CLIENT from `arrange_result.clip_rect`)
// or a `ClipToBounds` element (S834) is clipped. This op stays the pure
// value-domain intersect it always was.
VAML_CORE_EXPORT Rect clip_to_bounds(const Rect& content, const Rect& clipBounds);

} // namespace vaml
