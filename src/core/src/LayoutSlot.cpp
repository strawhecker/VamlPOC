// S122 — LayoutSlot::compute_final_rect out-of-line definition.
//
// The REAL alignment algorithm S124's Arrange will consume. WPF-faithful:
// the LayoutSlot is the arrange bounds AFTER the child's Margin is removed;
// the child's DesiredSize is aligned inside that slot by HorizontalAlignment/
// VerticalAlignment to yield the final rect.

#include "vaml/core/LayoutSlot.h"

#include <algorithm>
#include <cmath>

namespace vaml {

VAML_CORE_EXPORT
arrange_result compute_final_rect(const LayoutSlot& slot,
                                  Size desired,
                                  Thickness margin,
                                  HorizontalAlignment hAlign,
                                  VerticalAlignment vAlign)
{
    // 1. Inset the slot by margin -> the "clientSize" window (FE.cs:4569-4579).
    //    Margins clamp to >= 0 (a margin larger than the slot cannot drive a
    //    negative available / negative child size).
    const float ml = std::max(0.0f, margin.left);
    const float mt = std::max(0.0f, margin.top);
    const float mr = std::max(0.0f, margin.right);
    const float mb = std::max(0.0f, margin.bottom);
    const float insetW = std::max(0.0f, slot.bounds.width - ml - mr);
    const float insetH = std::max(0.0f, slot.bounds.height - mt - mb);

    // 2. S130_1 (WPF ArrangeCore-faithful): recover the element's inner desired
    //    extent by stripping the margin out of the margin-inflated DesiredSize
    //    (WPF `innerDesiredSize = DesiredSize - margin`). This stored value IS
    //    WPF's UnclippedDesiredSize (divergence #1 resolved at S832).
    float innerW = std::max(0.0f, desired.width - ml - mr);
    float innerH = std::max(0.0f, desired.height - mt - mb);

    // S123 invariant: Size::Infinite() (the unbounded-measure sentinel) never
    // escapes into geometry — guard a non-finite inner desired to the window.
    if (!std::isfinite(innerW)) innerW = insetW;
    if (!std::isfinite(innerH)) innerH = insetH;

    // 3. Arrange size per axis (S832 / FE.cs:4597-4624). A non-Stretch child is
    //    arranged at its UNCLIPPED desired size VERBATIM (it may OVERFLOW the
    //    slot). A Stretch child fills the slot, but if the slot is SMALLER than
    //    the unclipped desired it keeps the desired size (child-wins) rather
    //    than shrinking below what its MeasureOverride accepted.
    const bool hStretch = (hAlign == HorizontalAlignment::Stretch);
    const bool vStretch = (vAlign == VerticalAlignment::Stretch);
    const float sizeW = hStretch ? std::max(insetW, innerW) : innerW;
    const float sizeH = vStretch ? std::max(insetH, innerH) : innerH;

    // 5. NeedsClipBounds (FE.cs:4596/4603/4756-4762). For VAML's void
    //    ArrangeOverride (ink == arrange size) the post-override legs reduce to
    //    "the clientSize window is smaller than the unclipped desired on an
    //    axis" — i.e. the arranged child exceeds its slot.
    const bool needs_clip = (insetW < innerW) || (insetH < innerH);

    // 4. ComputeAlignmentOffset(clientSize, arrangeSize) (FE.cs:4805-4849).
    //    Center/Stretch take half the leftover; Right/Bottom the full leftover.
    //    When the child OVERFLOWS (size > inset) these offsets go NEGATIVE,
    //    centering/right-placing the oversized child — WPF-faithful. Stretch
    //    degenerates to Left/Top (offset 0) once it overflows
    //    (FE.cs:4811-4827).
    float ox = 0.0f;
    if (hAlign == HorizontalAlignment::Center)
    {
        ox = (insetW - sizeW) * 0.5f;
    }
    else if (hAlign == HorizontalAlignment::Right)
    {
        ox = insetW - sizeW;
    }
    float oy = 0.0f;
    if (vAlign == VerticalAlignment::Center)
    {
        oy = (insetH - sizeH) * 0.5f;
    }
    else if (vAlign == VerticalAlignment::Bottom)
    {
        oy = insetH - sizeH;
    }

    // Re-add slot.origin + margin.top/left (FE.cs:4772-4775). The arranged rect
    // may exceed the slot; the S833 conditional clip scissors it when needs_clip.
    // S833 (dep:S832): also return the WPF GetLayoutClip SLOT-CLIENT rect — the
    // clippingSize window (`slot - Margin`) at the slot origin + margin, in world
    // coords (FrameworkElement.GetLayoutClip returns `Rect(-offset, clippingSize)`,
    // FE.cs:4916-4929; the -offset maps world back to `slot.x+ml, slot.y+mt`).
    // This is the clip BOUNDS the renderer scissors to when needs_clip — NOT the
    // (possibly oversized) arranged rect (clipping it to itself would be a no-op).
    return { { slot.bounds.x + ml + ox,
               slot.bounds.y + mt + oy,
               sizeW,
               sizeH },
             needs_clip,
             { slot.bounds.x + ml,
               slot.bounds.y + mt,
               insetW,
               insetH } };
}

VAML_CORE_EXPORT
Size compute_measured_desired(Size natural)
{
    // S827/S828 (WPF-fidelity drift closure): Measure is ALIGNMENT-AGNOSTIC and
    // does NOT clamp to the offered size. WPF MeasureCore returns
    // `MeasureOverride(content)` (FrameworkElement.cs:~4385); the Stretch-vs-
    // wrap fill decision lives in ArrangeCore (`FE.cs:4617-4623`), applied by
    // compute_final_rect. So this finalizer simply returns the element's
    // un-clamped content extent: `natural` per axis, guarded `>= 0`. DesiredSize
    // may EXCEED `available` (WPF's "soft constraint" / unclippedDesiredSize,
    // UIElement.cs:541-551); the render slot now follows the S832 (11ad)
    // child-wins-then-clip contract in compute_final_rect (the child may exceed
    // its slot and reports needs_clip; the clip scissor is the deferred S833).
    //
    // There is NO "Stretch + finite -> available (fill)" branch and NO invented
    // "Stretch + infinite -> wrap" branch (the removed drift). `natural` is
    // finite (MeasureOverride / explicit Width-Height are finite), so a measured
    // size is always finite + non-negative.
    //
    // [WPF DIVERGENCE] #1 — the single un-clipped store. WPF keeps TWO values:
    // the parent-facing, CLAMPED `_desiredSize` (FrameworkElement.MeasureCore
    // FE.cs:4435-4500: `clippedDesiredWidth = desiredSize.Width + margin; if
    // (clippedDesiredWidth > availableSize.Width) { clippedDesiredWidth =
    // availableSize.Width; clipped = true; } ... return new
    // Size(Math.Max(0, clippedDesiredWidth), ...)`) plus the separate
    // `UnclippedDesiredSize` (FE.cs:4473-4491) that ArrangeCore consumes. What
    // VAML stores in `UIElement::desired_size_` IS WPF's UnclippedDesiredSize;
    // the clamped pair + the `clipped` flag feeding NeedsClipBounds are absent.
    // Consequence: no parent can observe an over-constrained child as clamped,
    // and `DesiredSize`-driven parents (Stack.cs:566-600 maxes the CLAMPED
    // child size) see the larger figure. RESOLVED BY S832 (Sprint 11ad — the
    // arrange-side child-wins-then-clip landed; compute_final_rect now arranges
    // the child at this unclipped size and reports needs_clip; the deferred S833
    // scissor honors it). Do NOT backfill a measure-side clamp here.
    return { std::max(0.0f, natural.width), std::max(0.0f, natural.height) };
}

VAML_CORE_EXPORT
Rect clip_to_bounds(const Rect& content, const Rect& clipBounds)
{
    // The S124 rectangular clip op. Intersect `content` with `clipBounds`,
    // clamped so the result is always a sub-rect of clipBounds (never escapes
    // it) and never has negative geometry. A disjoint intersection yields an
    // EMPTY zero-rect (nothing renders) — the same "no negative geometry from
    // the layout/arrange contract" guarantee the fit functions uphold.
    const float cl = clipBounds.left();
    const float ct = clipBounds.top();
    const float cr = clipBounds.right();
    const float cb = clipBounds.bottom();

    const float left   = std::max(content.left(), cl);
    const float top    = std::max(content.top(), ct);
    const float right  = std::min(content.right(), cr);
    const float bottom = std::min(content.bottom(), cb);

    // An empty (disjoint) intersection is an empty zero-rect at the clip's
    // top-left — never a negative width/height, never a rect outside the clip.
    const float w = std::max(0.0f, right - left);
    const float h = std::max(0.0f, bottom - top);
    return { left, top, w, h };
}

} // namespace vaml
