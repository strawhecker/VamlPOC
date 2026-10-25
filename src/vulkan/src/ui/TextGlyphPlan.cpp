// S400 — TextGlyphPlan implementation (vaml::vulkan::ui). The RENDER-side pure
// seam: maps the CORE text_layout_plan (already word-wrapped) into S206
// DrawItem glyph quads with the REAL S202 exact-grid atlas UVs. See
// TextGlyphPlan.h for the split + NO-GAP. Pure + headless-testable.

#include <vaml/vulkan/ui/TextGlyphPlan.h>

#include <vaml/vulkan/quad/TextureAtlas.h> // the S202 UV derivation + Color4
#include <vaml/vulkan/ui/UiPipeline.h>     // Color4 (vaml::vulkan::ui)

#include <algorithm>
#include <cstdint>

namespace vaml::vulkan::ui {

bool text_glyph_uv(const quad::AtlasLayout& layout, std::uint32_t tileIndex,
                   float& u0, float& v0, float& u1, float& v1) noexcept
{
    if (!quad::atlas_layout_valid(layout))
        return false;
    const std::uint32_t n = quad::atlas_tile_count(layout);
    if (tileIndex >= n)
        return false;

    // Exact-grid: tile (col,row) with a uniform cell. Row-major (tile 0 = the
    // top-left tile; advance left-to-right then top-to-bottom).
    const std::uint32_t cols = layout.cols;
    const std::uint32_t col  = tileIndex % cols;
    const std::uint32_t row  = tileIndex / cols;
    const float cellW = static_cast<float>(layout.tileW) / static_cast<float>(layout.atlasW);
    const float cellH = static_cast<float>(layout.tileH) / static_cast<float>(layout.atlasH);

    u0 = static_cast<float>(col) * cellW;
    v0 = static_cast<float>(row) * cellH;
    u1 = u0 + cellW;
    v1 = v0 + cellH;
    return true;
}

std::size_t build_text_glyph_items(const vaml::TextLayoutPlan& plan, float baseY,
                                   const quad::AtlasLayout& layout,
                                   const TextGlyphRenderMetrics& metrics,
                                   const vaml::Color& color,
                                   std::vector<DrawItem>& out) noexcept
{
    std::size_t before = out.size();
    if (plan.lines.empty() || !metrics)
        return 0;

    // The atlas cell size -> the glyph quad extent (the SDF glyph's ink box is
    // the atlas cell). A degenerate (zero) cell means no glyph can be drawn —
    // but we still honor the layout's advances (a space / empty glyph occupies
    // its advance without a quad).
    const float cellW = static_cast<float>(layout.tileW);
    const float cellH = static_cast<float>(layout.tileH);
    const bool  canDraw = cellW > 0.0f && cellH > 0.0f;

    // A Color -> the straight-alpha Color4 the text_glyph shader samples. VAML
    // Color is premultiplied-free 8-bit RGBA; the SDF text quad's alpha is the
    // SOFT-EDGE ramp (the sampled SDF * the tint alpha), so the color's own
    // alpha is the base tint opacity.
    const float r = static_cast<float>(color.r) / 255.0f;
    const float g = static_cast<float>(color.g) / 255.0f;
    const float b = static_cast<float>(color.b) / 255.0f;
    const float a = static_cast<float>(color.a) / 255.0f;

    // Iterate the wrapped lines; each line's glyphs carry their OWN (x,y)
    // placement computed by the CORE layout (y = lineIndex * lineHeight when
    // created). `baseY` shifts the whole block (e.g. to the element's arranged
    // origin), so the final drawn Y = baseY + placement.y.
    for (const auto& line : plan.lines)
    {
        for (const auto& gp : line.glyphs)
        {
            if (!gp.render)
                continue; // a space / non-painting glyph occupies advance, no quad
            const TextGlyphRender tr = metrics(gp.codepoint);
            if (!tr.hasGlyph || !canDraw)
                continue;

            float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
            if (!text_glyph_uv(layout, tr.tileIndex, u0, v0, u1, v1))
                continue;

            // The SDF glyph quad: positioned at the glyph's layout (x, y) shifted
            // by baseY, sized the atlas cell (cellW×cellH). `aa` = the S204 soft-
            // edge ramp half-width (the SDF default soft edge).
            out.push_back(text_glyph(gp.x, baseY + gp.y, cellW, cellH,
                                     u0, v0, u1, v1,
                                     Color4{ r, g, b, a }, 0.15f));
        }
    }

    return out.size() - before;
}

} // namespace vaml::vulkan::ui
