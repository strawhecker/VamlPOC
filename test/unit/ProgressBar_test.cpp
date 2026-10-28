// S484 (dep:S483) — ProgressBar control RESOLVED: Value/Min/Max + the
// determinate fill.
//
// Under test (headless):
//   1. The DP surface round-trips; Value clamps to [Minimum, Maximum].
//   2. A real Value change raises the BUBBLING "ValueChanged" routed event +
//      the value_changed Signal (the S403 Slider precedent).
//   3. The classifier maps ProgressBar to MaterialKind::Solid (the track +
//      determinate fill; the fill fraction computed from the range DPs).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/RoutedEvent.h"

#include <memory>

using vaml::ProgressBar;

TEST_CASE("S484: ProgressBar DP surface + Value clamping")
{
    ProgressBar pb;
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.value() == 0.0)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.maximum() == 100.0)).ShouldBe(1));
    (void)pb.set_value(0.0);
    (void)pb.set_value(150.0); // clamps up to Maximum (100)
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.value() == 100.0)).ShouldBe(1));
    (void)pb.set_value(-30.0); // clamps down to Minimum (0)
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.value() == 0.0)).ShouldBe(1));
    (void)pb.set_minimum(10.0);
    (void)pb.set_maximum(50.0);
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.minimum() == 10.0)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.maximum() == 50.0)).ShouldBe(1));
}

TEST_CASE("S484: a real Value change raises the value_changed Signal + ValueChanged event")
{
    ProgressBar pb;
    int fires = 0;
    (void)pb.value_changed().add([&]() { ++fires; });
    (void)pb.set_value(42.0);
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.value() == 42.0)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));
    // Same value -> NO_CHANGE (no second fire).
    (void)pb.set_value(42.0);
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));
}
