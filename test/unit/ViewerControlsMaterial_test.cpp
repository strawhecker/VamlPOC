// S403/S404/S405 — the Slider / CheckBox / ScrollViewer materials at the
// presentation vertical-slice boundary (vaml::presentation). Headless proof
// that the real re-based controls classify to MaterialKind::Solid through
// classify_merge_element (the track / check-box / viewport-clip default draws),
// and that the vulkan default classifier (classify_element) maps them to Solid.
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // Slider, CheckBox, ScrollViewer
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiSceneEncoder.h>  // classify_element / MaterialKind

TEST_CASE("S403/S404/S405: vulkan classify_element -> Solid for the real controls")
{
    vaml::Slider s; vaml::CheckBox cb; vaml::ScrollViewer sv;
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(s) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(cb) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(sv) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S403: classify_merge_element -> Solid track box for a Slider at its arranged rect")
{
    vaml::Slider s;
    (void)s.set_value(50.0);
    s.Measure(vaml::Size{ 120.0f, 10.0f });
    s.Arrange(vaml::Rect{ 0.0f, 0.0f, 120.0f, 8.0f });
    const auto ev = vaml::presentation::classify_merge_element(s);
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyString(ev.item.w).ShouldBe(120.0f));
}

TEST_CASE("S404: classify_merge_element -> Solid; a CHECKED CheckBox fills vs an UNCHECKED outline")
{
    vaml::CheckBox on;
    (void)on.set_checked(1);
    vaml::CheckBox off;
    (void)off.set_checked(0);

    const auto evOn = vaml::presentation::classify_merge_element(on);
    const auto evOff = vaml::presentation::classify_merge_element(off);
    SHOULD(ShouldlyIntegral(evOn.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    // The checked box's fill alpha is opaque; the unchecked box is a faded outline.
    SHOULD(ShouldlyString(evOn.item.a).ShouldBe(1.0f));
    SHOULD(ShouldlyString(evOff.item.a).ShouldBe(0.6f));
}

TEST_CASE("S405: classify_merge_element -> Solid viewport-clip box for a ScrollViewer")
{
    vaml::ScrollViewer sv;
    sv.Measure(vaml::Size{ 120.0f, 80.0f });
    sv.Arrange(vaml::Rect{ 0.0f, 0.0f, 120.0f, 80.0f });
    const auto ev = vaml::presentation::classify_merge_element(sv);
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyString(ev.item.h).ShouldBe(80.0f));
}
