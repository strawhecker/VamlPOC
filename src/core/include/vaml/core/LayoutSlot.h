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
