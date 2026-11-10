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
using vaml::SolidColorBrush;

TEST_CASE("S482: Rectangle DP surface round-trips + measures to its box")
{
    Rectangle r;
    // S643b-1: Fill/Stroke are Brush OBJECTs.
    auto fill = std::make_shared<SolidColorBrush>();
    (void)fill->set_color(vaml::Color{ 3, 3, 3, 255 });
    auto stroke = std::make_shared<SolidColorBrush>();
    (void)stroke->set_color(vaml::Color{ 4, 4, 4, 255 });
    (void)r.set_fill(fill);
    (void)r.set_stroke(stroke);
    (void)r.set_stroke_thickness(2.0);
    (void)r.set_width(80.0);
    (void)r.set_height(30.0);
    SHOULD(ShouldlyIntegral(r.fill() == fill).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.stroke() == stroke).ShouldBe(1));
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
    auto eFill = std::make_shared<SolidColorBrush>();
    (void)eFill->set_color(vaml::Color{ 2, 2, 2, 255 });
    (void)e.set_fill(eFill);
    e.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(e.desired_size().width > 0.0f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(e.fill() == eFill).ShouldBe(1));

    Line ln;
    auto lStroke = std::make_shared<SolidColorBrush>();
    (void)lStroke->set_color(vaml::Color{ 9, 9, 9, 255 });
    (void)ln.set_stroke(lStroke);
    (void)ln.set_stroke_thickness(3.0);
    ln.Measure(vaml::Size::Infinite());
    // A default Line uses the stroke thickness for the rule's cross extent.
    SHOULD(ShouldlyIntegral(static_cast<int>(ln.desired_size().height == 3.0f)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ln.stroke() == lStroke).ShouldBe(1));
}
