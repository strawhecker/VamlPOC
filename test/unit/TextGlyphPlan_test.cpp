// S400 (dep:S368) — TextGlyphPlan + the Text material (vaml::vulkan::ui).
// Headless proof of the RENDER side of the S400 TextBlock control (11k-1
// re-binds the S087 `Text` name):
//
//   (A) TEXT_GLYPH_UV — the S202 exact-grid UV derivation (a glyph tile -> its
//       atlas sub-rect), the pure seam that maps a TextLayoutPlan glyph to the
//       SDF atlas quad UVs.
//   (B) BUILD_TEXT_GLYPH_ITEMS — the full emission: one text_glyph DrawItem per
//       PLACED + RENDERABLE glyph, positioned at the layout's (x,y) + sized the
//       atlas cell, sampling the derived UVs, tinted by the resolved foreground.
//       Spaces / unmapped / degenerate-atlas glyphs emit nothing.
//   (C) MATERIAL — a REAL TextBlock classifies to MaterialKind::Text through
//       classify_element (the S207 default classifier), so the encoder emits
//       TextGlyph material.
//
// This target links vaml-vulkan, which PUBLIC-links vaml-core, so the core
// TextBlock + TextLayout is reachable. The presentation classifier (which emits
// the vertical-slice TextGlyph) lives in vaml-presentation-tests.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>        // Text (S400)
#include <vaml/core/TextLayout.h>     // text_layout_plan / default_text_metric
#include <vaml/vulkan/ui/TextGlyphPlan.h> // text_glyph_uv / build_text_glyph_items
#include <vaml/vulkan/ui/UiSceneEncoder.h> // classify_element / MaterialKind
#include <vaml/vulkan/ui/UiScene.h>   // DrawItem / text_glyph

#include <cstdint>
#include <vector>

namespace vml = vaml;
using namespace vaml::vulkan::ui;

namespace {

// A realistic S202 exact-grid layout: a 256x128 atlas in 4 cols x 2 rows ->
// 8 tiles of 64x64 each. The S203 glyph atlas reuses this grid (1 glyph : 1
// tile, codepoint order), so tile 0 = the top-left 64x64 cell.
const vaml::vulkan::quad::AtlasLayout kLayout =
    vaml::vulkan::quad::atlas_layout(256, 128, 4, 2);

} // namespace

TEST_CASE("S400: text_glyph_uv — exact-grid tile -> atlas sub-rect")
{
    // Tile 0 -> top-left: u0=0, v0=0, u1=cellW, v1=cellH.
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    const bool ok0 = text_glyph_uv(kLayout, 0u, u0, v0, u1, v1);
    SHOULD(ShouldlyString(ok0).ShouldBe(true));
    SHOULD(ShouldlyString(u0).ShouldBe(0.0f));
    SHOULD(ShouldlyString(v0).ShouldBe(0.0f));
    // cellW = 64/256 = 0.25; cellH = 64/128 = 0.5.
    SHOULD(ShouldlyString(u1).ShouldBe(0.25f));
    SHOULD(ShouldlyString(v1).ShouldBe(0.5f));

    // Tile 5 -> 4 cols: col=1, row=1 -> u0=0.25, v0=0.5.
    const bool ok5 = text_glyph_uv(kLayout, 5u, u0, v0, u1, v1);
    SHOULD(ShouldlyString(ok5).ShouldBe(true));
    SHOULD(ShouldlyString(u0).ShouldBe(0.25f));
    SHOULD(ShouldlyString(v0).ShouldBe(0.5f));
    SHOULD(ShouldlyString(u1).ShouldBe(0.5f));
    SHOULD(ShouldlyString(v1).ShouldBe(1.0f));

    // Tile 7 -> col=3,row=1 -> u0=0.75.
    const bool ok7 = text_glyph_uv(kLayout, 7u, u0, v0, u1, v1);
    SHOULD(ShouldlyString(ok7).ShouldBe(true));
    SHOULD(ShouldlyString(u0).ShouldBe(0.75f));

    // Out of range (tile 8 >= tile_count 8) -> false, UVs untouched.
    float x0 = 9.0f;
    const bool bad = text_glyph_uv(kLayout, 8u, x0, v0, u1, v1);
    SHOULD(ShouldlyString(bad).ShouldBe(false));
    SHOULD(ShouldlyString(x0).ShouldBe(9.0f));
}

TEST_CASE("S400: build_text_glyph_items — one quad per rendered glyph, atlas-cell sized + tinted UV")
{
    // Layout "AB": two glyphs, a toy LAYOUT provider advancing 64px each (so the
    // placed x position matches the 64px atlas cell) + the toy RENDER provider
    // maps every printable to tile 0 (64x64 cell).
    const vml::TextLayoutPlan plan = vml::text_layout_plan(
        "AB", 16.0f, 200.0f,
        [](std::uint32_t cp) -> vml::TextGlyphMetric {
            if (cp == 32u) return { 32.0f, false };
            return { 64.0f, true };
        });

    std::vector<DrawItem> out;
    const std::size_t n = build_text_glyph_items(plan, /*baseY=*/0.0f, kLayout,
        [](std::uint32_t cp) -> TextGlyphRender {
            if (cp == 32u) return TextGlyphRender{ 32.0f, 0xFFFFFFFFu, false, 64.0f, 64.0f };
            return TextGlyphRender{ 64.0f, 0u, true, 64.0f, 64.0f };
        },
        vml::Color{ 255, 0, 0, 255 }, out);

    SHOULD(ShouldlyIntegral(n).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(out.size()).ShouldBe(2u));
    // Both are TextGlyph kind, at (0,0) and (64,0), 64x64, full-tinted alpha.
    SHOULD(ShouldlyIntegral(out[0].kind == DrawItemKind::TextGlyph).ShouldBe(true));
    SHOULD(ShouldlyString(out[0].x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(out[0].y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(out[0].w).ShouldBe(64.0f));
    SHOULD(ShouldlyString(out[0].h).ShouldBe(64.0f));
    SHOULD(ShouldlyString(out[0].r).ShouldBe(1.0f));  // 255/255
    SHOULD(ShouldlyString(out[1].x).ShouldBe(64.0f));
    SHOULD(ShouldlyString(out[1].y).ShouldBe(0.0f));
}

TEST_CASE("S400: build_text_glyph_items — spaces emit nothing; unmapped/degen atlas emit nothing")
{
    // "A B": A, space (render=false -> no quad), B -> 2 queries, 2 rendered, but
    // only 2 quads (the space advances without a quad).
    const vml::TextLayoutPlan plan = vml::text_layout_plan(
        "A B", 16.0f, 200.0f,
        [](std::uint32_t cp) -> vml::TextGlyphMetric {
            if (cp == 32u) return { 32.0f, false };
            return { 64.0f, true };
        });

    std::vector<DrawItem> out;
    const std::size_t n = build_text_glyph_items(plan, 0.0f, kLayout,
        [](std::uint32_t cp) -> TextGlyphRender {
            if (cp == 32u) return TextGlyphRender{ 32.0f, 0xFFFFFFFFu, false, 64.0f, 64.0f };
            return TextGlyphRender{ 64.0f, 0u, true, 64.0f, 64.0f };
        },
        vml::Color{ 0, 0, 0, 255 }, out);
    // A and B render (2 quads); the space emits nothing.
    SHOULD(ShouldlyIntegral(n).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(out.size()).ShouldBe(2u));

    // A degenerate atlas (no valid layout) emits nothing even for renderable glyphs.
    std::vector<DrawItem> out2;
    const vaml::vulkan::quad::AtlasLayout emptyLayout =
        vaml::vulkan::quad::atlas_layout(0, 0, 0, 0);
    const std::size_t n2 = build_text_glyph_items(plan, 0.0f, emptyLayout,
        [](std::uint32_t) -> TextGlyphRender { return TextGlyphRender{ 64.0f, 0u, true, 0.0f, 0.0f }; },
        vml::Color{ 0, 0, 0, 255 }, out2);
    SHOULD(ShouldlyIntegral(n2).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(out2.size()).ShouldBe(0u));
}

TEST_CASE("S400: classify_element -> MaterialKind::Text for a real TextBlock")
{
    // The S207 default classifier recognizes the REAL S400 TextBlock control
    // (11k-1 re-binds the S087 Text name) -> Text material (the encoder's "draw
    // list by material" authority for the SDF text).
    vml::TextBlock t;
    const auto kind = classify_element(t);
    SHOULD(ShouldlyIntegral(kind == MaterialKind::Text).ShouldBe(true));
}
