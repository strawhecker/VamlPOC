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

    // 2. Child size: Stretch fills the available slot; Left/Center/Right and
    //    Top/Center/Bottom pinch to at most desired (clamped to available).
    const bool hStretch = (hAlign == HorizontalAlignment::Stretch);
    const bool vStretch = (vAlign == VerticalAlignment::Stretch);
    const float childW = hStretch ? availW : std::min(desired.width, availW);
    const float childH = vStretch ? availH : std::min(desired.height, availH);

    // 3. Offset by alignment: Center=(available - child)/2 (floor toward 0 via
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
Size compute_measured_desired(Size natural,
                              Size available,
                              HorizontalAlignment hAlign,
                              VerticalAlignment vAlign)
{
    // The infinity-aware stretch-vs-wrap finalizer S123's Measure consumes.
    //
    // Guard the two axes against the unbounded sentinel (Size::Infinite())
    // BEFORE any math so no axis can report infinity as a DesiredSize, and so
    // the "wrap-to-content" choices below are taken on the infinite axis.
    //
    // Axis resolved from natural/available:
    //   * Stretch (default) + finite available -> available (fill).
    //   * Stretch + infinite available         -> natural (WRAP: cannot stretch
    //     into an unbounded axis).
    //   * Non-Stretch + finite available       -> min(natural, available)
    //     (wrap/clamp to the bounded slot).
    //   * Non-Stretch + infinite available     -> natural (min(x, inf)==x).
    const bool availWInfinite = std::isinf(available.width);
    const bool availHInfinite = std::isinf(available.height);

    const bool hStretch = (hAlign == HorizontalAlignment::Stretch);
    const bool vStretch = (vAlign == VerticalAlignment::Stretch);

    float width = 0.0f;
    if (hStretch && !availWInfinite)
    {
        width = available.width;
    }
    else if (hStretch && availWInfinite)
    {
        width = natural.width; // no unbounded fill -> wraps to content
    }
    else
    {
        width = (availWInfinite) ? natural.width
                                 : std::min(natural.width, available.width);
    }

    float height = 0.0f;
    if (vStretch && !availHInfinite)
    {
        height = available.height;
    }
    else if (vStretch && availHInfinite)
    {
        height = natural.height; // no unbounded fill -> wraps to content
    }
    else
    {
        height = (availHInfinite) ? natural.height
                                  : std::min(natural.height, available.height);
    }

    // A measured DesiredSize is always finite + non-negative (NO negative
    // geometry / NO infinity escapes the measure contract).
    const float outW = std::max(0.0f, width); // inf already replaced above
    const float outH = std::max(0.0f, height);
    return { outW, outH };
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
