// S511 (dep:S510) — EFFECTS: the vaml/core model (DropShadowEffect + BlurEffect)
// + the pure shadow offset/color seams, + the UIElement::set_effect side-table.
//
// WPF-FAITHFUL (rev 3, D0-REVISED user-accepted): UIElement.Effect is rendered
// to an OFFSCREEN surface and blitted back through a shader. The CORE here owns
// the model + the pure value seams; the REAL offscreen blur/blit render arm is
// the vaml-vulkan UiSceneEncoder (verified in UiSceneEncoder_effect_test.cpp).
//
// Validation (the S025 rejection precedent): negative BlurRadius/ShadowDepth/
// Radius and out-of-[0,1] Effect.Opacity are REJECTED (store unchanged).
// ADR-018: ZERO new locks (plain field setters; out-of-line side-table).
//
// Headless (Rail 1, ADR-021). doctest main from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Effect.h"
#include "vaml/core/Element.h"  // StackPanel (a UIElement to attach an effect to)
#include "vaml/core/UIElement.h"

#include <cmath>
#include <memory>

using vaml::BlurEffect;
using vaml::Color;
using vaml::DropShadowEffect;
using vaml::Effect;
using vaml::SetResult;
using vaml::StackPanel;

namespace {

[[nodiscard]] bool feq(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

TEST_CASE("S511: DropShadowEffect defaults are WPF-faithful")
{
    DropShadowEffect e;
    const Color blackOpaque{ 0, 0, 0, 255 };
    CHECK(e.color() == blackOpaque);                    // Black opaque
    CHECK(feq(e.blur_radius(), 5.0));
    CHECK(feq(e.shadow_depth(), 5.0));
    CHECK(feq(e.direction(), 315.0));                   // default upper-left light
    CHECK(feq(e.opacity(), 1.0));
    const std::string name(e.effect_name());
    CHECK(name == "DropShadowEffect");
}

TEST_CASE("S511: DropShadowEffect validates + REJECTS negative/out-of-range")
{
    DropShadowEffect e;
    CHECK(e.set_blur_radius(-1.0) == SetResult::REJECTED);
    CHECK(feq(e.blur_radius(), 5.0));                    // unchanged
    CHECK(e.set_shadow_depth(-0.5) == SetResult::REJECTED);
    CHECK(feq(e.shadow_depth(), 5.0));
    CHECK(e.set_opacity(1.5) == SetResult::REJECTED);
    CHECK(feq(e.opacity(), 1.0));
    CHECK(e.set_opacity(-0.1) == SetResult::REJECTED);
    CHECK(e.set_direction(std::numeric_limits<double>::infinity()) == SetResult::REJECTED);

    // Valid setters APPLY.
    const Color redHalf{ 255, 0, 0, 128 };
    CHECK(e.set_color(redHalf) == SetResult::APPLIED);
    CHECK(e.set_blur_radius(3.0) == SetResult::APPLIED);
    CHECK(e.set_opacity(0.5) == SetResult::APPLIED);
    CHECK(e.color() == redHalf);
    CHECK(feq(e.blur_radius(), 3.0));
    CHECK(feq(e.opacity(), 0.5));
}

TEST_CASE("S511: BlurEffect validates + REJECTS negative radius")
{
    BlurEffect e;
    CHECK(feq(e.radius(), 5.0));                         // WPF default
    CHECK(e.set_radius(-2.0) == SetResult::REJECTED);
    CHECK(feq(e.radius(), 5.0));
    CHECK(e.set_radius(8.0) == SetResult::APPLIED);
    CHECK(feq(e.radius(), 8.0));
    CHECK(std::string(e.effect_name()) == "BlurEffect");
}

TEST_CASE("S511: drop_shadow_offset matches the WPF default upper-left light")
{
    double dx = 0, dy = 0;
    const double pi = 3.141592653589793;

    // Default Direction=315 -> shadow cast toward the LOWER-RIGHT (+x,+y).
    vaml::drop_shadow_offset(315.0, 5.0, dx, dy);
    const double expDx315 = -5.0 * std::sin(315.0 * pi / 180.0);
    const double expDy315 =  5.0 * std::cos(315.0 * pi / 180.0);
    const bool ok315dx = feq(dx, expDx315, 1e-9);
    CHECK(ok315dx);
    const bool ok315dy = feq(dy, expDy315, 1e-9);
    CHECK(ok315dy);
    // Both + for the lower-right cast.
    CHECK(dx > 0.0);
    CHECK(dy > 0.0);

    // Direction=0 (light from above) casts STRAIGHT DOWN (+y).
    vaml::drop_shadow_offset(0.0, 2.0, dx, dy);
    CHECK(feq(dx, 0.0));
    CHECK(feq(dy, 2.0));

    // Direction=90 (light from the right) casts LEFT (-x).
    vaml::drop_shadow_offset(90.0, 4.0, dx, dy);
    CHECK(feq(dx, -4.0));
    const bool okDy90 = feq(dy, 0.0, 1e-9);
    CHECK(okDy90);
}

TEST_CASE("S511: drop_shadow_color applies Effect.Opacity to the alpha")
{
    // Opaque black at Opacity 0.5 -> alpha 128.
    const Color c = drop_shadow_color(Color{ 0, 0, 0, 255 }, 0.5);
    CHECK(c.r == 0);
    CHECK(c.g == 0);
    CHECK(c.b == 0);
    CHECK(c.a == 128);

    // Mid-alpha at Opacity 1.0 -> unchanged.
    const Color c2 = drop_shadow_color(Color{ 10, 20, 30, 100 }, 1.0);
    CHECK(c2.a == 100);
    CHECK(c2.r == 10);
    CHECK(c2.g == 20);
    CHECK(c2.b == 30);

    // Clamp: alpha 200 at Opacity 1.0 stays 200; at Opacity 0.0 -> 0.
    const Color hi = drop_shadow_color(Color{ 1, 2, 3, 200 }, 1.0);
    CHECK(hi.a == 200);
    const Color lo = drop_shadow_color(Color{ 1, 2, 3, 200 }, 0.0);
    CHECK(lo.a == 0);
}

TEST_CASE("S511: UIElement::set_effect stores + clears the out-of-line effect")
{
    vaml::register_builtin_elements();
    StackPanel host;

    // No effect by default.
    CHECK(host.effect() == nullptr);

    auto ds = std::make_unique<DropShadowEffect>();
    (void)ds->set_blur_radius(3.0);
    host.set_effect(std::move(ds));
    REQUIRE(host.effect() != nullptr);
    const std::string dsName(host.effect()->effect_name());
    CHECK(dsName == "DropShadowEffect");
    const double dsBlur = static_cast<const DropShadowEffect*>(host.effect())->blur_radius();
    CHECK(feq(dsBlur, 3.0));

    // Replace with a BlurEffect.
    host.set_effect(std::make_unique<BlurEffect>());
    REQUIRE(host.effect() != nullptr);
    const std::string blName(host.effect()->effect_name());
    CHECK(blName == "BlurEffect");

    // Clear (nullptr) removes the side-table entry.
    host.set_effect(nullptr);
    CHECK(host.effect() == nullptr);
}
