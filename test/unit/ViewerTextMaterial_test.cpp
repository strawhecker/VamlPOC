// S400 (dep:S368) — Text material at the presentation vertical-slice boundary
// (11k-1: the REAL TextBlock control re-binds the S087 `Text` name)
// (vaml::presentation). Headless proof that the REAL S400 TextBlock control
// gets a TEXT glyph material (not a solid fill) through the default
// vertical-slice classifier the viewer + S409_1 consume: classify_merge_element
// maps a TextBlock to MaterialKind::Text + a TextGlyph DrawItem at the
// element's arranged origin.
//
// This target links vaml-presentation (which PUBLIC-links vaml-vulkan, which
// PUBLIC-links vaml-core), so the core TextBlock + TextLayout + the vulkan
// DrawItem types are all reachable.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>        // TextBlock (S400, 11k-1)
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>       // Size, Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element / default_visual_of
#include <vaml/vulkan/ui/UiScene.h>   // DrawItemKind

TEST_CASE("S400: classify_merge_element -> TextGlyph material for a TextBlock element")
{
    // Arrange a real TextBlock so it has a final_rect (the render slot).
    vaml::TextBlock t;
    (void)t.set_text("Hi");
    (void)t.set_font_size(16.0);
    t.Measure(vaml::Size{ 200.0f, 100.0f });
    t.Arrange(vaml::Rect{ 7.0f, 9.0f, 100.0f, 20.0f });

    const vaml::vulkan::ui::ElementVisual ev = vaml::presentation::classify_merge_element(t);

    // The vertical-slice classifier emits the TEXT material (not a solid fill).
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Text).ShouldBe(true));
    // The item is a TextGlyph (the SDF text quad kind).
    SHOULD(ShouldlyIntegral(ev.item.kind == vaml::vulkan::ui::DrawItemKind::TextGlyph).ShouldBe(true));
    // Originates at the element's arranged origin (the render slot's top-left).
    SHOULD(ShouldlyString(ev.item.x).ShouldBe(7.0f));
    SHOULD(ShouldlyString(ev.item.y).ShouldBe(9.0f));
    // The default (unresolved) foreground -> opaque black -> RGB (0,0,0), opaque.
    SHOULD(ShouldlyString(ev.item.r).ShouldBe(0.0f));
    SHOULD(ShouldlyString(ev.item.g).ShouldBe(0.0f));
    SHOULD(ShouldlyString(ev.item.b).ShouldBe(0.0f));
}

TEST_CASE("S400: default_visual_of wires classify_merge_element (the encoder seam)")
{
    // The viewer's default VisualOf seam IS classify_merge_element (a TextBlock
    // maps to the Text material through the encoder path the renderer feeds).
    vaml::TextBlock t;
    (void)t.set_text("X");
    (void)t.set_font_size(16.0);
    t.Measure(vaml::Size{ 200.0f, 100.0f });
    t.Arrange(vaml::Rect{ 0.0f, 0.0f, 50.0f, 20.0f });

    const auto visualOf = vaml::presentation::default_visual_of();
    const vaml::vulkan::ui::ElementVisual ev = visualOf(t);
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Text).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ev.item.kind == vaml::vulkan::ui::DrawItemKind::TextGlyph).ShouldBe(true));
}
