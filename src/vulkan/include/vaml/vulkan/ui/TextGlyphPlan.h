#pragma once
// VAML TextGlyphPlan — the S400 RENDER-side pure seam (vaml::vulkan::ui):
// converts a CORE text_layout_plan (already word-wrapped) into the S206
// DrawItem glyph-quad list, with the REAL per-glyph atlas UV mapping.
//
// S400 split (ADR-011 / ADR-021 kept): the text layout + wrap engine lives in
// CORE (vaml::core::TextLayout, headless + device-free); this vulkan surface is
// the RENDER half — it maps each placed glyph to its SDF-atlas sub-rect UVs and
// emits `text_glyph` DrawItems the S206 batcher consumes. It is PURE +
// HEADLESS-TESTABLE: it needs no Vulkan runtime, only an S202 atlas layout (for
// the exact-grid UV derivation) + a per-glyph UV provider.
//
// UV DERIVATION (the S202 exact-grid contract): the glyph atlas reuses the S202
// tile grid (1 glyph : 1 tile, codepoint order — the S203 decision, kept by
// S204). A glyph's tile index -> its UV sub-rect is
//   tileW/tileH = the atlas grid's cell; tile (col,row) maps to
//   u0 = col*cellW; v0 = row*cellH; u1 = u0+cellW; v1 = v0+cellH  (UV in [0,1]).
// The SdfAtlas/FontAtlas + S202 TextureAtlas layout supply this mapping; this
// seam derives it from the layout + a slot (tile index).
//
// HEADLESS GLYPH BASIS: `TextGlyphRender` carries per-codepoint { advance,
// tileW, tileH, tileIndex } — the REAL render-time values an SdfAtlas provides.
// A consumer may feed a TOY provider (the core tests) or the REAL atlas (the
// live viewer / S409_1). The seam emits one text_glyph DrawItem per PLACED +
// RENDERABLE glyph (a space renders nothing), positioned at the layout's
// (x,y) scaled to the atlas cell, using the text's resolved foreground Color.
//
// NO-GAP (recorded, not silent): the REAL SdfAtlas/FontAtlas SAMPLING (the
// live device-backed glyph uploads + sampling) is S204/S205 (already shipped);
// this seam supplies the QUAD EMISSION the S205 SDF shader samples. The live
// [s400-live] device proof / the S409_1 viewer growth feed the real atlas here.

#include <vaml/vulkan/quad/TextureAtlas.h>  // AtlasLayout (the S202 exact-grid UV source)
#include <vaml/vulkan/ui/UiScene.h>         // DrawItem, text_glyph

#include <vaml/core/Color.h>                // Color (the resolved tint)
#include <vaml/core/TextLayout.h>           // TextLayoutPlan (the wrapped layout)

#include <cstdint>
#include <functional>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// One glyph's RENDER datum (the S400 render basis): its SDF-atlas tile index
// (via the S203 1:1 codepoint->tile mapping; kNoSlot for an un-mappable
// codepoint) + the atlas CELL SIZE + the REAL horizontal advance (pixels).
struct TextGlyphRender
{
    float          advance   = 0.0f;
    std::uint32_t  tileIndex = 0xFFFFFFFFu;  // the S203 glyph_slot_index; kNoSlot = unmapped
    bool           hasGlyph  = false;         // a coverage-bearing SDF tile exists
    float          cellW     = 0.0f;          // the SDF atlas cell width (px)
    float          cellH     = 0.0f;          // the SDF atlas cell height (px)
};

// The per-codepoint render-metrics provider. A REAL consumer reads an
// SdfAtlas (slot_for → tile + cell) + a FontAtlas (metrics); a TOY provider
// drives the headless tests. `render` in the returned datum governs whether a
// quad is emitted (a space / unmapped glyph -> hasGlyph false -> no quad).
using TextGlyphRenderMetrics =
    std::function<TextGlyphRender(std::uint32_t codepoint)>;

// Derives a glyph's atlas UV sub-rect from the S202 exact-grid layout + a tile
// index. Returns true + writes (u0,v0,u1,v1) on a valid layout + in-range
// tile; returns false (UVs untouched) otherwise. Pure + headless-testable.
VAML_VULKAN_EXPORT bool text_glyph_uv(const quad::AtlasLayout& layout,
                                      std::uint32_t tileIndex,
                                      float& u0, float& v0,
                                      float& u1, float& v1) noexcept;

// Emits one text_glyph DrawItem per PLACED + RENDERABLE glyph in `plan`: the
// quad is drawn at the layout's (x,y) sized (cellW, cellH) [the SDF atlas cell
// is the glyph's ink box], sampling the atlas sub-rect derived via
// text_glyph_uv(layout, render.tileIndex), ultratinted by `color`. A glyph
// whose provider returns hasGlyph=false OR whose UV derivation fails emits
// nothing (it still occupies its advance). Returns the number of DrawItems
// appended. Pure + headless-testable.
VAML_VULKAN_EXPORT std::size_t
build_text_glyph_items(const vaml::TextLayoutPlan& plan, float baseY,
                       const quad::AtlasLayout& layout,
                       const TextGlyphRenderMetrics& metrics,
                       const vaml::Color& color,
                       std::vector<DrawItem>& out) noexcept;

} // namespace vaml::vulkan::ui
