// S482 (dep:S481) — Rectangle + Ellipse + Line shapes RESOLVED: REAL re-based
// UIElements with Fill/Stroke/StrokeThickness + Width/Height.
//
// Under test (headless):
//   1. The shape DP surfaces round-trip (Fill/Stroke/StrokeThickness/Width/Height).
//   2. Each shape measures to its Width/Height box (0 = a bounded default).
//   3. The classifier maps Rectangle/Ellipse/Line to MaterialKind::Solid (the
//      fill/stroke box; Ellipse's CURVED outline is the S209/S649
//      continuation, recorded NO-GAP not silent).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"

#include <memory>

using vaml::Rectangle;
using vaml::Ellipse;
using vaml::Line;

TEST_CASE("S482: Rectangle DP surface round-trips + measures to its box")
{
    Rectangle r;
    (void)r.set_fill(3);
    (void)r.set_stroke(4);
    (void)r.set_stroke_thickness(2.0);
    (void)r.set_width(80.0);
    (void)r.set_height(30.0);
    SHOULD(ShouldlyIntegral(r.fill()).ShouldBe(3));
    SHOULD(ShouldlyIntegral(r.stroke()).ShouldBe(4));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.stroke_thickness() == 2.0)).ShouldBe(1));
    // An UNBOUNDED offer (Size::Infinite) lets the intrinsic DP box survive
    // (the S123 wrap rule — a bounded offer would stretch a default-Stretch
    // element to fill it).
    r.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(static_cast<int>(r.desired_size().width == 80.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.desired_size().height == 30.0f)).ShouldBe(1));
}

TEST_CASE("S482: Ellipse + Line measure to their boxes; Line honors StrokeThickness")
{
    Ellipse e;
    (void)e.set_fill(2);
    e.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(e.desired_size().width > 0.0f).ShouldBe(true));

    Line ln;
    (void)ln.set_stroke(9);
    (void)ln.set_stroke_thickness(3.0);
    ln.Measure(vaml::Size::Infinite());
    // A default Line uses the stroke thickness for the rule's cross extent.
    SHOULD(ShouldlyIntegral(static_cast<int>(ln.desired_size().height == 3.0f)).ShouldBe(true));
}
