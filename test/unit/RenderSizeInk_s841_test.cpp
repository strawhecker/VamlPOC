// S841 (dep:S840) — RENDER-SIZE / INK fidelity (D3, the material S208-touching
// close): the slot / ink / anchor triad made explicit.
//
// WPF ground truth (Rule-2 source audit, E:\GitRepos\wpf):
//   * FrameworkElement.ArrangeCore (FE.cs:4697, 4720-4721) — `Size innerInkSize =
//     ArrangeOverride(arrangeSize); RenderSize = innerInkSize;` — the INK is the
//     ArrangeOverride RETURN, distinct from the arrange slot.
//   * FrameworkElement.ArrangeOverride(Size finalSize) RETURNS the passed size by
//     default (FE.cs:5111-5113) — so ink == slot by default (the backward-
//     compatible safety net).
//   * The anchor (layout offset) is set separately via SetLayoutOffset (FE.cs:4787-
//     4793), tracked distinct from RenderSize — the render is placed at the offset,
//     never folded into RenderSize.
//   * GetLayoutClip consumes the INK (`Size inkSize = this.RenderSize;` FE.cs:4874)
//     for the `clippingSize < inkSize` trigger (FE.cs:4917-4923) — an ink larger
//     than the slot forces a clip; an ink smaller/equal does not.
//   * Hit-testing is against the RenderSize box (the ink), not the arrange slot.
//
// Under test:
//   1. Default (base) element: render_size() == final_rect() size, layout_offset()
//      == final_rect() origin, ink_rect() == final_rect() (ink == slot).
//   2. A SHRINKING ink (ArrangeOverride returns half the slot): final_rect() stays
//      the SLOT (unchanged), render_size()/ink_rect() reflect the ink, laid at the
//      same anchor; needs_clip() stays false (content fits).
//   3. A GROWING ink (ArrangeOverride returns slot+extent): ink larger than the slot
//      forces needs_clip() true via the ink leg; final_rect() stays the slot.
//   4. Hit-test reads the INK box: a point inside the slot but OUTSIDE a shrunk ink
//      does NOT hit; a point inside the ink does.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/FrameworkElement.h"

using vaml::Rect;
using vaml::Size;
using vaml::UIElement;

namespace {

Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// An element whose ArrangeOverride returns HALF the arrange slot — a distinct,
// SMALLER ink over an unchanged slot (the "ink inside the slot" case; content
// fits, so needs_clip stays false).
class ShrinkInkLeaf : public vaml::FrameworkElement
{
protected:
    Size ArrangeOverride(Size finalSize) override
    {
        return Size{ finalSize.width * 0.5f, finalSize.height * 0.5f };
    }
};

// An element whose ArrangeOverride returns the slot GROWN by a fixed extent — an
// ink LARGER than the slot. WPF forces a clip when clientSize < inkSize
// (FE.cs:4917-4923), so needs_clip() must fire from the ink leg.
class GrowInkLeaf : public vaml::FrameworkElement
{
protected:
    Size ArrangeOverride(Size finalSize) override
    {
        return Size{ finalSize.width + 10.0f, finalSize.height + 10.0f };
    }
};

} // namespace

// --- 1. Default: ink == slot (the backward-compatible safety net) ---------------

TEST_CASE("S841: default element — render_size() == final_rect() size and ink_rect() == final_rect()")
{
    UIElement el;
    el.Arrange(rect_of(10.0f, 20.0f, 100.0f, 50.0f));

    // The slot (final_rect) is unchanged by S841.
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(50.0f));

    // The base ArrangeOverride returns the passed size -> ink == slot.
    SHOULD(ShouldlyString(el.render_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.render_size().height).ShouldBe(50.0f));

    // The anchor == the arrange-slot origin.
    SHOULD(ShouldlyString(el.layout_offset().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.layout_offset().y).ShouldBe(20.0f));

    // ink_rect composes anchor + ink -> identical to final_rect for the default.
    SHOULD(ShouldlyString(el.ink_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.ink_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(el.ink_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.ink_rect().height).ShouldBe(50.0f));
}

// --- 2. A shrinking ink over an UNCHANGED slot ---------------------------------

TEST_CASE("S841: shrinking ink — final_rect() stays the slot, render_size()/ink_rect() reflect the ink")
{
    ShrinkInkLeaf el;
    el.Arrange(rect_of(5.0f, 6.0f, 80.0f, 40.0f));

    // final_rect is the arrange SLOT — unchanged by the differing ink.
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(80.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(40.0f));

    // The ink == the ArrangeOverride return (half the slot).
    SHOULD(ShouldlyString(el.render_size().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(el.render_size().height).ShouldBe(20.0f));

    // The anchor is still the slot origin (offset tracked distinct from the ink).
    SHOULD(ShouldlyString(el.layout_offset().x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(el.layout_offset().y).ShouldBe(6.0f));

    // ink_rect = anchor + ink.
    SHOULD(ShouldlyString(el.ink_rect().x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(el.ink_rect().y).ShouldBe(6.0f));
    SHOULD(ShouldlyString(el.ink_rect().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(el.ink_rect().height).ShouldBe(20.0f));

    // Content fits (ink < slot) -> no clip (WPF clippingSize < inkSize is false).
    SHOULD(ShouldlyString(el.needs_clip()).ShouldBe(false));
}

// --- 3. A growing ink forces a clip via the ink leg ----------------------------

TEST_CASE("S841: growing ink — needs_clip() fires from the ink leg (slot smaller than ink)")
{
    GrowInkLeaf el;
    el.Arrange(rect_of(0.0f, 0.0f, 100.0f, 50.0f));

    // The slot is unchanged.
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(50.0f));

    // The ink grew beyond the slot.
    SHOULD(ShouldlyString(el.render_size().width).ShouldBe(110.0f));
    SHOULD(ShouldlyString(el.render_size().height).ShouldBe(60.0f));
    SHOULD(ShouldlyString(el.ink_rect().width).ShouldBe(110.0f));
    SHOULD(ShouldlyString(el.ink_rect().height).ShouldBe(60.0f));

    // clientSize (100x50) < ink (110x60) -> WPF clips. The needs_clip ink leg
    // must fire even though the arrange-slot squeeze (ar.needs_clip) is false.
    SHOULD(ShouldlyString(el.needs_clip()).ShouldBe(true));
}

// --- 4. Hit-test reads the INK box --------------------------------------------

TEST_CASE("S841: hit-test reads the ink box — a point in the slot but outside a shrunk ink misses")
{
    ShrinkInkLeaf el;
    el.Arrange(rect_of(0.0f, 0.0f, 100.0f, 80.0f)); // ink = 50x40 at (0,0)

    // Inside the ink -> hits this element.
    CHECK(el.HitTestAt(10.0f, 10.0f) == &el);

    // Inside the SLOT but OUTSIDE the ink (x=90,y=70) -> does NOT hit (the ink
    // box, not the slot, is the hit geometry).
    CHECK(el.HitTestAt(90.0f, 70.0f) == nullptr);
}
