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
Rect compute_final_rect(const LayoutSlot& slot,
                        Size desired,
                        Thickness margin,
                        HorizontalAlignment hAlign,
                        VerticalAlignment vAlign)
{
    // 1. Inset the slot by margin. Margins clamp to >= 0 (a margin larger than
    //    the slot cannot drive a negative available / negative child size).
    const float ml = std::max(0.0f, margin.left);
    const float mt = std::max(0.0f, margin.top);
    const float mr = std::max(0.0f, margin.right);
    const float mb = std::max(0.0f, margin.bottom);
    const float availW = std::max(0.0f, slot.bounds.width - ml - mr);
    const float availH = std::max(0.0f, slot.bounds.height - mt - mb);

    // 2. S130_1 (WPF ArrangeCore-faithful): DesiredSize is now MARGIN-INFLATED
    //    (Measure adds margin back, so it propagates into a parent panel's
    //    DesiredSize). Before aligning, strip the margin OUT to recover the
    //    element's inner desired extent — otherwise the margin would be counted
    //    TWICE (once in the inflated desired, once in the inset slot). This is
    //    WPF's `innerDesiredSize = DesiredSize - margin` inside ArrangeCore.
    const float innerW = std::max(0.0f, desired.width - ml - mr);
    const float innerH = std::max(0.0f, desired.height - mt - mb);

    // 3. Child size: Stretch fills the available slot; Left/Center/Right and
    //    Top/Center/Bottom pinch to at most the INNER desired (clamped to
    //    available).
    //
    // [WPF DIVERGENCE] #2/#3 — no NeedsClipBounds + non-Stretch pinned to
    // `avail`. WPF FrameworkElement.ArrangeCore (FE.cs:4589-4625) does NOT
    // pinch: it GROWS the arrange size to `unclippedDesiredSize` when the slot
    // is smaller (`if (arrangeSize.Width < unclippedDesiredSize.Width) {
    // NeedsClipBounds = true; arrangeSize.Width = unclippedDesiredSize.Width; }`)
    // and arranges a non-Stretch child at `unclippedDesiredSize` VERBATIM
    // (`if (HorizontalAlignment != Stretch) arrangeSize.Width =
    // unclippedDesiredSize.Width;`), i.e. a Left-aligned oversize child
    // OVERFLOWS its slot unclipped (GetLayoutClip returns null when
    // ClipToBounds is false, UIElement.cs:3322-3333). VAML instead clamps to
    // `avail` (std::min above), so an oversize non-Stretch child is PINNED
    // rather than overflowing, and a slot smaller than the unclipped desired is
    // never marked NeedsClipBounds. RESOLVED BY S832 (Sprint 11ad — the S124
    // arrange-contract + clip drift closure). Do NOT paper over here; do NOT
    // assert oversized-overflow as WPF-true until S832 lands.
    const bool hStretch = (hAlign == HorizontalAlignment::Stretch);
    const bool vStretch = (vAlign == VerticalAlignment::Stretch);
    const float childW = hStretch ? availW : std::min(innerW, availW);
    const float childH = vStretch ? availH : std::min(innerH, availH);

    // 4. Offset by alignment: Center=(available - child)/2 (floor toward 0 via
    //    max(0,...) for the integer-division-free positive case); Right/Bottom
    //    = available - child (>= 0 because child <= available).
    float ox = 0.0f;
    if (hAlign == HorizontalAlignment::Center)
    {
        ox = (availW - childW) * 0.5f;
    }
    else if (hAlign == HorizontalAlignment::Right)
    {
        ox = availW - childW;
    }
    float oy = 0.0f;
    if (vAlign == VerticalAlignment::Center)
    {
        oy = (availH - childH) * 0.5f;
    }
    else if (vAlign == VerticalAlignment::Bottom)
    {
        oy = availH - childH;
    }

    return { slot.bounds.x + ml + ox,
             slot.bounds.y + mt + oy,
             childW,
             childH };
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
    // UIElement.cs:541-551); the render slot stays parent-bounded by
    // compute_final_rect until S832 (11ad) adds the arrange-side clip.
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
    // child size) see the larger figure. RESOLVED BY S832 (Sprint 11ad). Do NOT
    // backfill a clamp here — the store, the flag and the arrange-side clip land
    // together or not at all.
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
