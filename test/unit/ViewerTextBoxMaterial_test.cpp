// S402 (dep:S401) — TextBox material at the presentation vertical-slice boundary
// (vaml::presentation). Headless proof that a REAL S402 TextBox control gets the
// default TextBox material (MaterialKind::Solid — the S402 edit-field fill box)
// through classify_merge_element, at its arranged final_rect.
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // TextBox (S402)
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>           // Size, Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiScene.h>       // DrawItemKind

TEST_CASE("S402: classify_merge_element -> Solid material for a TextBox at its arranged rect")
{
    vaml::TextBox tb;
    (void)tb.set_text("edit");
    tb.Measure(vaml::Size{ 120.0f, 30.0f });
    tb.Arrange(vaml::Rect{ 2.0f, 4.0f, 80.0f, 22.0f });

    const vaml::vulkan::ui::ElementVisual ev = vaml::presentation::classify_merge_element(tb);

    // A TextBox is a real control that renders its edit-field fill box (Solid).
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ev.item.kind == vaml::vulkan::ui::DrawItemKind::SolidRect).ShouldBe(true));
    SHOULD(ShouldlyString(ev.item.x).ShouldBe(2.0f));
    SHOULD(ShouldlyString(ev.item.y).ShouldBe(4.0f));
    SHOULD(ShouldlyString(ev.item.w).ShouldBe(80.0f));
    SHOULD(ShouldlyString(ev.item.h).ShouldBe(22.0f));
}
