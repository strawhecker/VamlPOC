// S124 (dep:S123) — Arrange contract + clip:
// Arrange(finalRect); clip bounds honored by render slot.
//
// Under test:
//   1. UIElement::Arrange(arrangeSlot) -> writes final_rect() (the S122 field)
//      via compute_final_rect with the element's own desired_size() (S123
//      Measure), margin(), horizontal_alignment(), vertical_alignment().
//   2. Arrange pairs with Measure: Measure(avail) then Arrange(slot) ->
//      final_rect reflects the measured desired.
//   3. Repeated Arrange with a new slot RECOMPUTES (recompute, not cumulate).
//   4. final_rect() defaults 0 until Arrange; the RENDER SLOT == CLIP BOUNDS
//      (a base element's final_rect is both).
//   5. clip_to_bounds — the S124 rectangular clip op (pure value domain):
//      content ⊆ clip -> unchanged; partial overlap -> clamped; fully
//      contained; disjoint -> empty zero-rect; never escapes the clip, never
//      negative.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"

using vaml::HorizontalAlignment;
using vaml::LayoutSlot;
using vaml::Rect;
using vaml::Size;
using vaml::Thickness;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// Constructs a rect from origin + extent for readable slot literals.
Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// A test-local leaf that carries a real content extent through the S123/124
// seam — the exact pattern S125's Text/panel overrides will use. Its Measured
// DesiredSize drives the Arrange slot so the pairing is concrete, not trivial.
class ContentLeaf : public UIElement
{
public:
    explicit ContentLeaf(Size natural) : natural_(natural) {}

protected:
    Size measure_natural_size(const Size& available) const override
    {
        (void)available;
        return natural_;
    }

private:
    Size natural_;
};

} // namespace

// --- UIElement::Arrange -> final_rect() ----------------------------------------

TEST_CASE("S124: Arrange writes final_rect() — default Stretch fills the slot")
{
    UIElement el;
    // Default Stretch on both axes; desired 5x5 (base has no content) but a
    // Stretch layout fills the slot.
    el.Arrange(rect_of(10.0f, 20.0f, 100.0f, 50.0f));
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(50.0f));
}

TEST_CASE("S124: Arrange writes final_rect() — non-Stretch alignment pins inside the slot")
{
    // Non-Stretch (Center) pins the MEASURED desired inside the slot. Use a
    // content leaf whose desired (100x30, measured from 100x30 natural) is
    // centered in a 400x200 slot: x = (400-100)/2 = 150, y = (200-30)/2 = 85.
    ContentLeaf leaf{ Size{ 100.0f, 30.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Center);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Center);
    leaf.Measure(Size{ 400.0f, 200.0f });
    leaf.Arrange(rect_of(0.0f, 0.0f, 400.0f, 200.0f));
    SHOULD(ShouldlyString(leaf.final_rect().x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(leaf.final_rect().y).ShouldBe(85.0f));
    SHOULD(ShouldlyString(leaf.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.final_rect().height).ShouldBe(30.0f));
}

TEST_CASE("S124: Arrange honors Margin (origin advances; slot shrinks)")
{
    UIElement el;
    (void)el.set_margin(Thickness{ 10.0f, 20.0f, 5.0f, 5.0f });
    // Slot 100x50 at origin, margins 10/20/5/5: available = 85x25, origin
    // advances to (10,20); Stretch fills the remaining.
    el.Arrange(rect_of(0.0f, 0.0f, 100.0f, 50.0f));
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(85.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(25.0f));
}

TEST_CASE("S124: Arrange pairs with Measure — final_rect reflects the measured desired")
{
    // Measure a non-Stretch content leaf, then Arrange it into a larger slot:
    // the arranged rect reflects the MEASURED desired_size() (not the original
    // natural extent in a way that would ignore Measure).
    ContentLeaf leaf{ Size{ 30.0f, 40.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Center);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Center);
    leaf.Measure(Size{ 200.0f, 100.0f }); // non-Stretch keeps min(natural, avail) = natural
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(40.0f));
    leaf.Arrange(rect_of(0.0f, 0.0f, 200.0f, 100.0f));
    // Arranged from the MEASURED desired (30x40) centered in 200x100:
    // x = (200-30)/2 = 85, y = (100-40)/2 = 30.
    SHOULD(ShouldlyString(leaf.final_rect().x).ShouldBe(85.0f));
    SHOULD(ShouldlyString(leaf.final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(leaf.final_rect().width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(leaf.final_rect().height).ShouldBe(40.0f));
}

TEST_CASE("S124: repeated Arrange with a changed slot recomputes final_rect()")
{
    UIElement el;
    el.Arrange(rect_of(0.0f, 0.0f, 100.0f, 50.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(50.0f));
    // Re-arrange with a new slot; the field is OVERWRITTEN (recompute, never
    // cumulate — the same discipline Measure/S123 upholds).
    el.Arrange(rect_of(5.0f, 10.0f, 30.0f, 30.0f));
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(10.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(30.0f));
}

TEST_CASE("S124: final_rect() defaults 0 until Arrange — render slot == clip bounds")
{
    UIElement el;
    // Default (before any Arrange pass): zero rect.
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(0.0f));

    // After Arrange, final_rect() IS the render slot — the same extent that
    // clip honors (the renderer draws the element into (and clips it to) this
    // rect at S208). A base element's content fits any slot, so the clip ==
    // the full render slot: clipping the slot to itself leaves it unchanged.
    el.Arrange(rect_of(5.0f, 6.0f, 70.0f, 40.0f));
    const Rect slot = el.final_rect();
    const Rect clipped = vaml::clip_to_bounds(slot, slot);
    SHOULD(ShouldlyString(clipped.x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(clipped.y).ShouldBe(6.0f));
    SHOULD(ShouldlyString(clipped.width).ShouldBe(70.0f));
    SHOULD(ShouldlyString(clipped.height).ShouldBe(40.0f));
}

// --- clip_to_bounds (pure value domain) -----------------------------------------

TEST_CASE("S124: clip_to_bounds — content inside the clip is returned unchanged")
{
    // Content fully within clipBounds -> unchanged (fully visible).
    const Rect r = vaml::clip_to_bounds(
        rect_of(10.0f, 10.0f, 20.0f, 20.0f), rect_of(0.0f, 0.0f, 100.0f, 100.0f));
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(20.0f));
}

TEST_CASE("S124: clip_to_bounds — content equal to the clip is unchanged")
{
    const Rect r = vaml::clip_to_bounds(
        rect_of(0.0f, 0.0f, 50.0f, 40.0f), rect_of(0.0f, 0.0f, 50.0f, 40.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(40.0f));
}

TEST_CASE("S124: clip_to_bounds — partial overflow is clamped to the clip edge")
{
    // Content bleeds past the clip's right edge (and bottom): the visible
    // portion is clamped to the clip, cutting the overflowing extent.
    const Rect r = vaml::clip_to_bounds(
        rect_of(0.0f, 0.0f, 120.0f, 60.0f), rect_of(0.0f, 0.0f, 100.0f, 50.0f));
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(50.0f));
}

TEST_CASE("S124: clip_to_bounds — content offset above/left of the clip is pulled in")
{
    // Content extends before the clip's left/top edges: the visible portion
    // starts at the clip origin (origin clamped INWARD to the clip).
    const Rect r = vaml::clip_to_bounds(
        rect_of(-20.0f, -10.0f, 60.0f, 40.0f), rect_of(0.0f, 0.0f, 100.0f, 100.0f));
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
}

TEST_CASE("S124: clip_to_bounds — disjoint content yields an EMPTY zero-rect")
{
    // Content entirely to the right of / below the clip: no overlap -> empty
    // zero-rect (nothing renders), never negative geometry, never outside.
    const Rect r = vaml::clip_to_bounds(
        rect_of(200.0f, 200.0f, 20.0f, 20.0f), rect_of(0.0f, 0.0f, 100.0f, 100.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.x >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(r.y >= 0.0f).ShouldBe(true));
}

TEST_CASE("S124: clip_to_bounds — result never escapes the clip, never negative")
{
    // Whatever the input, the clipped result is always a sub-rect of clipBounds
    // (origin >= clip origin, extent <= clip extent, all non-negative).
    const Rect clip = rect_of(10.0f, 20.0f, 50.0f, 30.0f);
    const Rect r = vaml::clip_to_bounds(rect_of(-100.0f, -100.0f, 1000.0f, 1000.0f), clip);
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(r.right() <= clip.right()).ShouldBe(true));
    SHOULD(ShouldlyString(r.bottom() <= clip.bottom()).ShouldBe(true));
}
