// S400_1 (dep:S400) — Icon material at the presentation vertical-slice boundary
// (vaml::presentation). Headless proof that a REAL S400_1 Icon control gets a
// MONOCHROME material (not a solid fill) through the default vertical-slice
// classifier, and that the headless classifier's rendered item is the S400_1
// DELIBERATE FALLBACK GLYPH (a visible marker at the icon's arranged box — the
// honest headless outcome when there is no live device IconAtlas; the real
// atlas-driven quad is the [s400_1-live]/S409_1 path, recorded NO-GAP not
// silent). Also proves the vulkan default classifier (classify_element) maps an
// Icon -> MaterialKind::Monochrome (the S207 material authority).
//
// Links vaml-presentation (PUBLIC-links vaml-vulkan -> vaml-core).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>        // Icon (S400_1)
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>       // Size, Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element / default_visual_of
#include <vaml/vulkan/ui/UiSceneEncoder.h>  // classify_element / MaterialKind
#include <vaml/vulkan/ui/UiScene.h>   // DrawItemKind

TEST_CASE("S400_1: vulkan classify_element -> MaterialKind::Monochrome for an Icon")
{
    vaml::Icon ic;
    const auto kind = vaml::vulkan::ui::classify_element(ic);
    // An Icon is a real control that contributes a MONOCHROME material (the
    // S207 material authority; the shader path is chosen by the DP).
    SHOULD(ShouldlyIntegral(kind == vaml::vulkan::ui::MaterialKind::Monochrome).ShouldBe(true));
}

TEST_CASE("S400_1: classify_merge_element -> Monochrome + fallback glyph for an Icon")
{
    // Arrange a real Icon so it has a final_rect (the render slot).
    vaml::Icon ic;
    (void)ic.set_width(24.0);
    (void)ic.set_height(24.0);
    ic.Measure(vaml::Size{ 100.0f, 100.0f });
    ic.Arrange(vaml::Rect{ 3.0f, 5.0f, 24.0f, 24.0f });
    const vaml::vulkan::ui::ElementVisual ev = vaml::presentation::classify_merge_element(ic);

    // The vertical-slice classifier emits the MONOCHROME material (an Icon is a
    // real control, not a solid-fill panel).
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Monochrome).ShouldBe(true));
    // The headless item is the S400_1 fallback glyph (a solid box at the
    // arranged origin) — the honest headless render with no live device IconAtlas.
    SHOULD(ShouldlyIntegral(ev.item.kind == vaml::vulkan::ui::DrawItemKind::SolidRect).ShouldBe(true));
    SHOULD(ShouldlyString(ev.item.x).ShouldBe(3.0f));
    SHOULD(ShouldlyString(ev.item.y).ShouldBe(5.0f));
    SHOULD(ShouldlyString(ev.item.w).ShouldBe(24.0f));
    SHOULD(ShouldlyString(ev.item.h).ShouldBe(24.0f));
}

TEST_CASE("S400_1: default_visual_of wires the Icon material (the encoder seam)")
{
    vaml::Icon ic;
    (void)ic.set_width(16.0);
    (void)ic.set_height(16.0);
    ic.Measure(vaml::Size{ 50.0f, 50.0f });
    ic.Arrange(vaml::Rect{ 0.0f, 0.0f, 16.0f, 16.0f });

    const auto visualOf = vaml::presentation::default_visual_of();
    const vaml::vulkan::ui::ElementVisual ev = visualOf(ic);
    SHOULD(ShouldlyIntegral(ev.kind == vaml::vulkan::ui::MaterialKind::Monochrome).ShouldBe(true));
}
