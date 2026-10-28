// S470/S471/S472/S473 (Sprint 11c) — the DockPanel + selection controls
// materials at the presentation vertical-slice boundary. Headless proof that
// each real re-based control classifies to MaterialKind::Solid through
// classify_merge_element (DockPanel fill / RadioButton check-glyph / ComboBox
// field / ListBox-ListView fill), and that the vulkan default classifier
// (classify_element) maps them all to Solid.
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // DockPanel, RadioButton, ComboBox, ListBox, ListView
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiSceneEncoder.h>  // classify_element / MaterialKind

TEST_CASE("S11c: vulkan classify_element -> Solid for DockPanel + selection controls")
{
    vaml::DockPanel dock; vaml::RadioButton radio; vaml::ComboBox cb;
    vaml::ListBox lb; vaml::ListView listview;
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(dock) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(radio) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(cb) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(lb) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(listview) ==
                            vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S11c: classify_merge_element -> Solid for DockPanel + selection controls")
{
    // Arrange each into a slot so the render rect is real.
    const vaml::Size slot{ 100.0f, 40.0f };
    vaml::DockPanel dock; vaml::RadioButton radio; vaml::ComboBox cb;
    vaml::ListBox lb; vaml::ListView listview2;
    dock.Measure(slot);  dock.Arrange(vaml::Rect{ 0,0,100,60 });
    radio.Measure(slot);    radio.Arrange(vaml::Rect{ 0,0,16,16 });
    cb.Measure(slot);    cb.Arrange(vaml::Rect{ 0,0,100,22 });
    lb.Measure(slot);    lb.Arrange(vaml::Rect{ 0,0,100,80 });
    listview2.Measure(slot); listview2.Arrange(vaml::Rect{ 0,0,100,80 });

    const auto d = vaml::presentation::classify_merge_element(dock);
    const auto r = vaml::presentation::classify_merge_element(radio);
    const auto c = vaml::presentation::classify_merge_element(cb);
    const auto l = vaml::presentation::classify_merge_element(lb);

    SHOULD(ShouldlyIntegral(d.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyString(d.item.w).ShouldBe(100.0f));
    SHOULD(ShouldlyIntegral(r.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(c.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(l.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S11c: a CHECKED RadioButton fills opaque vs an unchecked outline")
{
    vaml::RadioButton on;
    (void)on.set_checked(true);
    vaml::RadioButton off;
    const auto evOn = vaml::presentation::classify_merge_element(on);
    const auto evOff = vaml::presentation::classify_merge_element(off);
    SHOULD(ShouldlyIntegral(evOn.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyString(evOn.item.a).ShouldBe(1.0f));
    SHOULD(ShouldlyString(evOff.item.a).ShouldBe(0.6f));
}
