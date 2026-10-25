// S401 (dep:S400) — Button material at the presentation vertical-slice boundary
// (vaml::presentation). Headless proof that a REAL S401 Button control gets the
// default Button material (MaterialKind::Solid — the S401 default fill) through
// classify_merge_element, at its arranged final_rect, honoring the visual-state
// Background color + IsEnabled (a disabled Button's fill is faded).
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // Button (S401)
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>           // Size, Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiScene.h>       // DrawItemKind

TEST_CASE("S401: classify_merge_element -> Solid material for a Button at its arranged rect")
{
    vaml::Button b;
    b.Measure(vaml::Size{ 100.0f, 40.0f });
    b.Arrange(vaml::Rect{ 5.0f, 7.0f, 60.0f, 24.0f });

    const vaml::vulkan::ui::ElementVisual ev = vaml::presentation::classify_merge_element(b);

    // A Button is a real control that renders its default fill (Solid).
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ev.item.kind == vaml::vulkan::ui::DrawItemKind::SolidRect).ShouldBe(true));
    // At the arranged origin + extent.
    SHOULD(ShouldlyString(ev.item.x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(ev.item.y).ShouldBe(7.0f));
    SHOULD(ShouldlyString(ev.item.w).ShouldBe(60.0f));
    SHOULD(ShouldlyString(ev.item.h).ShouldBe(24.0f));
}

TEST_CASE("S401: a disabled Button renders a faded fill (IsEnabled honored)")
{
    vaml::Button enabled;
    vaml::Button disabled;
    (void)disabled.set_is_enabled(false);

    const auto evEnabled = vaml::presentation::classify_merge_element(enabled);
    const auto evDisabled = vaml::presentation::classify_merge_element(disabled);

    // The disabled Button's fill alpha is faded (0.55) vs the enabled (1.0).
    SHOULD(ShouldlyString(evEnabled.item.a).ShouldBe(1.0f));
    SHOULD(ShouldlyString(evDisabled.item.a).ShouldBe(0.55f));
}
