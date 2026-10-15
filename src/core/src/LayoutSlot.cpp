// S122 — LayoutSlot::compute_final_rect out-of-line definition.
//
// The REAL alignment algorithm S124's Arrange will consume. WPF-faithful:
// the LayoutSlot is the arrange bounds AFTER the child's Margin is removed;
// the child's DesiredSize is aligned inside that slot by HorizontalAlignment/
// VerticalAlignment to yield the final rect.

#include "vaml/core/LayoutSlot.h"

#include <algorithm>

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

} // namespace vaml
