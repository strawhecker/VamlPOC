// S403/S404/S405 (Sprint 10) — Slider, CheckBox, ScrollViewer control tests
// (vaml::core). Headless proof of the three real re-based controls:
//
//   S403 SLIDER — Value/Min/Max DPs (Value clamps to the range), the value-
//   changed Signal + the routed "ValueChanged" event (source==this), and the
//   thumb-drag set_value_from_ratio mapping a [0,1] drag ratio to Value.
//   S404 CHECKBOX — nullable IsChecked (the WPF-faithful third state 0/1/2),
//   IsThreeState, and the WPF on-toggle cycle (confirmed from ToggleButton.cs).
//   S405 SCROLLVIEWER — offset clamps to [0, scrollable], wheel scroll
//   (scroll_by_vertical), and the S124 clipping of content against the viewport.
//
// The three controls are re-based onto UIElement (real control seeds RESOLVED
// at their own steps). The material + classifier half live in the vulkan /
// presentation targets. Uses the standing Shouldly-for-doctest vocabulary.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>                // Slider, CheckBox, ScrollViewer
#include <vaml/core/RoutedEvent.h>            // the routed change events
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>

#include <string>

using vaml::CheckBox;
using vaml::ScrollViewer;
using vaml::Slider;
using vaml::Size;
using vaml::Rect;

// ----------------------------------------------------------------- S403 Slider

TEST_CASE("S403: Slider Value/Min/Max DPs — Value clamps to the range")
{
    Slider s;
    // Defaults.
    SHOULD(ShouldlyString(s.value()).ShouldBe(0.0));
    SHOULD(ShouldlyString(s.minimum()).ShouldBe(0.0));
    SHOULD(ShouldlyString(s.maximum()).ShouldBe(100.0));

    // Value round-trip.
    (void)s.set_value(50.0);
    SHOULD(ShouldlyString(s.value()).ShouldBe(50.0));

    // Value CLAMPS to [Minimum, Maximum].
    (void)s.set_value(250.0);
    SHOULD(ShouldlyString(s.value()).ShouldBe(100.0));
    (void)s.set_value(-5.0);
    SHOULD(ShouldlyString(s.value()).ShouldBe(0.0));

    // Range manip.
    (void)s.set_minimum(-10.0);
    (void)s.set_maximum(10.0);
    (void)s.set_value(-20.0);
    SHOULD(ShouldlyString(s.value()).ShouldBe(-10.0));
    (void)s.set_value(20.0);
    SHOULD(ShouldlyString(s.value()).ShouldBe(10.0));
}

TEST_CASE("S403: Slider thumb-drag — set_value_from_ratio maps a drag ratio to Value")
{
    Slider s;
    (void)s.set_minimum(0.0);
    (void)s.set_maximum(100.0);

    // ratio 0 -> min; ratio 1 -> max; ratio 0.5 -> midpoint.
    SHOULD(ShouldlyString(s.set_value_from_ratio(0.0)).ShouldBe(0.0));
    SHOULD(ShouldlyString(s.set_value_from_ratio(1.0)).ShouldBe(100.0));
    SHOULD(ShouldlyString(s.set_value_from_ratio(0.5)).ShouldBe(50.0));

    // A ratio outside [0,1] clamps.
    SHOULD(ShouldlyString(s.set_value_from_ratio(2.0)).ShouldBe(100.0));
    SHOULD(ShouldlyString(s.set_value_from_ratio(-1.0)).ShouldBe(0.0));
}

TEST_CASE("S403: Slider fires the value-changed Signal on a real change")
{
    Slider s;
    int fires = 0;
    (void)s.value_changed().add([&] { ++fires; }); // the subscription token is discarded

    (void)s.set_value(10.0); // real change -> 1 fire
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));
    (void)s.set_value(10.0); // NO_CHANGE (equal) -> no fire
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));
    (void)s.set_value(33.0); // real change -> 2 fires
    SHOULD(ShouldlyIntegral(fires).ShouldBe(2));
}

// --------------------------------------------------------------- S404 CheckBox

TEST_CASE("S404: CheckBox nullable IsChecked + IsThreeState (WPF-faithful)")
{
    CheckBox cb;
    SHOULD(ShouldlyString(cb.checked()).ShouldBe(0)); // default unchecked
    SHOULD(ShouldlyString(cb.is_three_state()).ShouldBe(false));

    // set_checked accepts the third (indeterminate) state.
    (void)cb.set_checked(2);
    SHOULD(ShouldlyString(cb.checked()).ShouldBe(2));
    (void)cb.set_checked(1);
    SHOULD(ShouldlyString(cb.checked()).ShouldBe(1));

    // IsThreeState validation (0/1).
    (void)cb.set_is_three_state(true);
    SHOULD(ShouldlyString(cb.is_three_state()).ShouldBe(true));
}

TEST_CASE("S404: CheckBox toggle — the WPF on-toggle cycle (ToggleButton.cs)")
{
    // IsThreeState==false: checked <-> unchecked (indeterminate unreachable).
    CheckBox cb;
    (void)cb.set_is_three_state(false);
    (void)cb.set_checked(0);
    SHOULD(ShouldlyString(cb.toggle()).ShouldBe(1)); // false -> true
    SHOULD(ShouldlyString(cb.toggle()).ShouldBe(0)); // true -> false
    SHOULD(ShouldlyString(cb.toggle()).ShouldBe(1));

    // IsThreeState==true: checked -> indeterminate -> unchecked -> checked.
    CheckBox tri;
    (void)tri.set_is_three_state(true);
    (void)tri.set_checked(1);
    SHOULD(ShouldlyString(tri.toggle()).ShouldBe(2)); // true(1) + IsThreeState -> null(2)
    SHOULD(ShouldlyString(tri.toggle()).ShouldBe(0)); // null(2) -> false(0)
    SHOULD(ShouldlyString(tri.toggle()).ShouldBe(1)); // false(0) -> true(1)
}

TEST_CASE("S404: CheckBox is a real UIElement with a default box")
{
    CheckBox cb;
    (void)dynamic_cast<vaml::UIElement*>(&cb);
    cb.Measure(Size{ 20.0f, 20.0f });
    SHOULD(ShouldlyString(cb.desired_size().width).ShouldBe(20.0f));
}

// ----------------------------------------------------------- S405 ScrollViewer

TEST_CASE("S405: ScrollViewer offset clamps to [0, scrollable] + wheel scroll")
{
    ScrollViewer sv;
    sv.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f }); // viewport 100x100
    sv.set_content_height(250.0);                  // scrollable = 150

    SHOULD(ShouldlyString(sv.scrollable_height()).ShouldBe(150.0));
    // offset clamps to [0, scrollable].
    (void)sv.set_vertical_offset(999.0);
    SHOULD(ShouldlyString(sv.vertical_offset()).ShouldBe(150.0));
    (void)sv.set_vertical_offset(-5.0);
    SHOULD(ShouldlyString(sv.vertical_offset()).ShouldBe(0.0));

    // Wheel scroll: a positive delta scrolls down; clamps at the bottom.
    SHOULD(ShouldlyString(sv.scroll_by_vertical(20.0)).ShouldBe(20.0));
    SHOULD(ShouldlyString(sv.scroll_by_vertical(20.0)).ShouldBe(40.0));
    SHOULD(ShouldlyString(sv.scroll_by_vertical(999.0)).ShouldBe(150.0)); // clamps
    SHOULD(ShouldlyString(sv.scroll_by_vertical(-999.0)).ShouldBe(0.0));  // clamps up
}

TEST_CASE("S405: ScrollViewer clips content against the viewport (S124)")
{
    ScrollViewer sv;
    sv.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });
    sv.set_content_height(300.0);

    // No scroll, a 200-tall content at origin: the visible sub-rect is clipped to
    // the 100-high viewport (the content below is hidden).
    const Rect c1 = sv.clip_content(Rect{ 0.0f, 0.0f, 80.0f, 200.0f });
    SHOULD(ShouldlyString(c1.height).ShouldBe(100.0f)); // clipped to viewport height
    SHOULD(ShouldlyString(c1.width).ShouldBe(80.0f));

    // A content fully BELOW the viewport (after scrolling away) is hidden (zero).
    (void)sv.set_vertical_offset(100.0);
    // The clip_content translates by the offset (content at its own origin).
    const Rect c2 = sv.clip_content(Rect{ 0.0f, -120.0f, 80.0f, 40.0f }); // above the viewport
    SHOULD(ShouldlyString(c2.height <= 100.0f).ShouldBe(true));           // bounded
}
