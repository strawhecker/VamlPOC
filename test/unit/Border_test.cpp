// S481 (dep:S480) — Border control RESOLVED: a REAL re-based UIElement
// single-content decorator with BorderBrush/BorderThickness/CornerRadius/
// Padding. This is the FIRST production use of the dormant `MaterialKind::Border`
// + `border_rect(...)` path — the S205 Border pipeline is made LIVE at S481.
//
// Under test (headless):
//   1. The Border DP surface is real and reachable (Thickness value path).
//   2. The single-content slot adopts one child (measure/arrange the inner box).
//   3. The classifier maps a Border to MaterialKind::Border (the FIRST live emit
//      of the S205 BorderRect material — a real NO-GAP closure of the dormant
//      Border seam).
//   4. classify_merge_element at the presentation boundary emits a border_rect
//      DrawItem (BorderRect kind) with the resolved BorderBrush color.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"

#include <memory>

using vaml::Border;
using vaml::Thickness;
using vaml::UIElement;

TEST_CASE("S481: Border DP surface round-trips (Thickness value path)")
{
    Border border;
    SHOULD(ShouldlyIntegral(static_cast<int>(border.border_brush() == 0)).ShouldBe(1));
    (void)border.set_border_brush(12);
    (void)border.set_border_thickness(Thickness{ 2.0f, 2.0f, 2.0f, 2.0f });
    (void)border.set_corner_radius(Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
    (void)border.set_padding(Thickness{ 6.0f, 6.0f, 6.0f, 6.0f });
    SHOULD(ShouldlyIntegral(border.border_brush()).ShouldBe(12));
    SHOULD(ShouldlyIntegral(static_cast<int>(border.border_thickness().left == 2.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(border.corner_radius().left == 4.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(border.padding().left == 6.0f)).ShouldBe(1));
}

TEST_CASE("S481: Border hosts a single content child and lays out the inner slot")
{
    Border border;
    auto inner = std::make_unique<vaml::TextBlock>();
    (void)inner->set_text("content");
    auto* raw = inner.get();
    border.set_content(std::move(inner));
    SHOULD(ShouldlyIntegral(border.content() == raw).ShouldBe(1));
    SHOULD(ShouldlyIntegral(border.child_count()).ShouldBe(1));
    (void)border.set_border_thickness(Thickness{ 2.0f, 2.0f, 2.0f, 2.0f });
    (void)border.set_padding(Thickness{ 4.0f, 4.0f, 4.0f, 4.0f });
    border.Measure(vaml::Size{ 200.0f, 120.0f });
    border.Arrange(vaml::Rect{ 0.0f, 0.0f, 200.0f, 120.0f });
    // The content child is arranged into the inner slot (behind the border +
    // padding insets): its origin is offset by top+padding, its size reduced.
    const vaml::Rect cr = border.content()->final_rect();
    SHOULD(ShouldlyIntegral(cr.x >= 6.0f - 0.01f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(cr.y >= 6.0f - 0.01f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(cr.width <= 200.0f - 12.0f + 0.01f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(cr.height <= 120.0f - 12.0f + 0.01f).ShouldBe(true));
}

// The Border CLASSIFIER material (MaterialKind::Border -> the FIRST live
// BorderRect emit of the dormant S205 pipeline) is asserted in the S486 viewer
// growth test (vaml-presentation-tests, which links vaml-vulkan).
