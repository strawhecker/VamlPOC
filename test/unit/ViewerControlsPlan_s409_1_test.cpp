// S409_1 (dep:S408) — VIEWER-GROWTH: the live viewer hosts the REAL S10 control
// set (Text/Icon/Button/TextBox/Slider/CheckBox/ScrollViewer) render +
// interactive via the generic renderer. Headless proof that the viewer's
// default render seam (`classify_merge_element`) emits a REAL material for EACH
// S10 control (the "same for S10+" commitment — the viewer's generic renderer
// draws the control set as it lands). The dedicated S409_1 sample.vaml is the
// [both-apps] live-display proof; this headless seam proves the viewer HOSTS
// every control through the real classifier.
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>                // the S10 controls
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>
#include <vaml/presentation/VamlRenderer.h>   // classify_merge_element (the viewer's generic render seam)
#include <vaml/vulkan/ui/UiScene.h>           // MaterialKind / ElementVisual
#include <vaml/vulkan/ui/UiSceneEncoder.h>    // classify_element

// Every S10 control renders through the viewer's generic classifier to a REAL
// material (Not None) at its arranged box — the generic renderer hosts them all.
TEST_CASE("S409_1: the viewer's generic classifier renders EVERY S10 control to a real material")
{
    vaml::Text     text;
    vaml::Button   button;
    vaml::TextBox  textbox;
    vaml::Slider   slider;
    vaml::CheckBox checkbox;
    vaml::ScrollViewer scroll;
    vaml::Icon     icon;

    // Give each an arranged box (the render slot).
    const vaml::Size slot{ 100.0f, 30.0f };
    text.Measure(slot);    text.Arrange(vaml::Rect{ 0, 0, 80, 20 });
    button.Measure(slot);  button.Arrange(vaml::Rect{ 0, 0, 80, 24 });
    textbox.Measure(slot); textbox.Arrange(vaml::Rect{ 0, 0, 80, 22 });
    slider.Measure(slot);  slider.Arrange(vaml::Rect{ 0, 0, 80, 8 });
    checkbox.Measure(slot); checkbox.Arrange(vaml::Rect{ 0, 0, 16, 16 });
    scroll.Measure(slot);  scroll.Arrange(vaml::Rect{ 0, 0, 80, 60 });
    icon.Measure(slot);    icon.Arrange(vaml::Rect{ 0, 0, 20, 20 });

    const vaml::vulkan::ui::ElementVisual t = vaml::presentation::classify_merge_element(text);
    const vaml::vulkan::ui::ElementVisual b = vaml::presentation::classify_merge_element(button);
    const vaml::vulkan::ui::ElementVisual tb = vaml::presentation::classify_merge_element(textbox);
    const vaml::vulkan::ui::ElementVisual s = vaml::presentation::classify_merge_element(slider);
    const vaml::vulkan::ui::ElementVisual c = vaml::presentation::classify_merge_element(checkbox);
    const vaml::vulkan::ui::ElementVisual sv = vaml::presentation::classify_merge_element(scroll);
    const vaml::vulkan::ui::ElementVisual ic = vaml::presentation::classify_merge_element(icon);

    // The generic renderer maps each to a real (non-void) control material.
    SHOULD(ShouldlyIntegral(t.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));   // Text
    SHOULD(ShouldlyIntegral(b.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));   // Button
    SHOULD(ShouldlyIntegral(tb.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));  // TextBox
    SHOULD(ShouldlyIntegral(s.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));   // Slider
    SHOULD(ShouldlyIntegral(c.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));   // CheckBox
    SHOULD(ShouldlyIntegral(sv.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));  // ScrollViewer
    SHOULD(ShouldlyIntegral(ic.kind != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));  // Icon
}

// The vulkan default classifier (the S207 material authority) also recognizes
// every S10 control.
TEST_CASE("S409_1: the S207 classifier recognizes every S10 control's material")
{
    vaml::Text t; vaml::Button b; vaml::TextBox tb; vaml::Slider s;
    vaml::CheckBox c; vaml::ScrollViewer sv; vaml::Icon ic;
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(t) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(b) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(tb) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(s) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(c) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(sv) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(ic) != vaml::vulkan::ui::MaterialKind::None).ShouldBe(true));
}
