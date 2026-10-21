// S203 — FontAtlas implementation: embeds a compact TrueType default font
// (DejaVu Sans subset, ASCII 32–126), parses it with stb_truetype, and
// rasterizes each printable-ASCII glyph's coverage bitmap ONCE into an S202
// quad::TextureAtlas tile. The S202 exact-grid layout is REUSED as the glyph
// atlas grid (1 glyph : 1 tile, codepoint order), so there is no second atlas
// layout — the recorded S203 decision, not a re-implementation.
//
// The PDF/GPU consumer samples texture_atlas() and selects a glyph via its
// tile UVs with the UNCHANGED S200/S201 quad pipeline. S203 adds no renderer
// and no text layout. The coverage bitmaps produced here are S204's explicit
// SDF source (S204 converts them to SDF — no second rasterizer).
//
// THREADING (ADR-018): single-threaded, once-at-import, zero locks. The
// embedded font blob is static/process-lifetime (interning-equivalent).
//
// The stb_truetype implementation (STB_TRUETYPE_IMPLEMENTATION) is defined
// exactly once in TruetypeImpl.cpp; this TU includes the header WITHOUT it
// (the VMA single-TU discipline).

#include <vaml/vulkan/font/FontAtlas.h>

#include "EmbeddedFont_ascii.h"

#include <stb_truetype.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace vaml::vulkan::font {

namespace {

// Validates a non-zero, >=95-tile S202 layout that the glyph atlas can reuse.
bool usable_layout(const quad::AtlasLayout& layout) noexcept
{
    return quad::atlas_layout_valid(layout) && quad::atlas_tile_count(layout) >= kAsciiGlyphCount;
}

} // namespace

std::vector<std::uint32_t> ascii_codepoints() noexcept
{
    std::vector<std::uint32_t> cps;
    cps.reserve(kAsciiGlyphCount);
    for (int cp = kAsciiFirst; cp <= kAsciiLast; ++cp)
        cps.push_back(static_cast<std::uint32_t>(cp));
    return cps;
}

bool glyph_metrics_valid(const GlyphMetrics& m) noexcept
{
    return m.advance > 0.0f && m.width >= 0 && m.height >= 0;
}

std::pair<std::uint32_t, std::uint32_t> glyph_cell_size(const quad::AtlasLayout& layout) noexcept
{
    if (!quad::atlas_layout_valid(layout))
        return {0u, 0u};
    return {layout.tileW, layout.tileH};
}

std::uint32_t glyph_slot_index(std::uint32_t codepoint) noexcept
{
    if (codepoint < static_cast<std::uint32_t>(kAsciiFirst) ||
        codepoint > static_cast<std::uint32_t>(kAsciiLast))
        return kNoSlot;
    return codepoint - static_cast<std::uint32_t>(kAsciiFirst);
}

std::span<const std::uint8_t> embedded_font_bytes() noexcept
{
    return std::span<const std::uint8_t>(kEmbeddedFontAscii, kEmbeddedFontAsciiBytes);
}

bool embedded_font_valid() noexcept
{
    const auto bytes = embedded_font_bytes();
    stbtt_fontinfo info{};
    // stbtt_InitFont's 3rd arg is `fontstart` (the byte OFFSET of the font within
    // the data), 0 for a standalone font — NOT the length. Returns nonzero on a
    // successful parse with the required tables (cmap/head/hhea/hmtx) present.
    return stbtt_InitFont(&info, static_cast<const unsigned char*>(bytes.data()), 0) != 0;
}

namespace {

// Rasterizes `codepoint`'s coverage into an RGBA8 tile (alpha = coverage,
// RGB = white) sized tileW×tileH, glyph ink-centred within the tile. Returns
// false if the font/scale can't be applied (degenerate) or glyph is empty.
bool rasterize_coverage_tile(const stbtt_fontinfo& font, std::uint32_t codepoint,
                             std::uint32_t tileW, std::uint32_t tileH,
                             std::vector<std::uint8_t>& outRGBA)
{
    if (tileW == 0u || tileH == 0u)
        return false;

    // Scale the em so a typical glyph fits inside the square cell with margin.
    const float pixelHeight = static_cast<float>(tileH) * 0.9f;
    const float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
    if (scale <= 0.0f)
        return false;

    // Allocate an exact glyph-bitmap buffer via the box info, render the glyph
    // into it (single channel, tightly packed at its own size), then blit the
    // ink CENTRED into the tile. stbtt_GetCodepointBitmap renders a bitmap of
    // exactly the glyph's ink-box size at (ix0,iy0) — the simplest correct path.
    int gw = 0, gh = 0, gx = 0, gy = 0;
    unsigned char* gbm = stbtt_GetCodepointBitmap(&font, scale, scale,
                                                  static_cast<int>(codepoint),
                                                  &gw, &gh, &gx, &gy);
    if (!gbm || gw <= 0 || gh <= 0)
        return false;  // empty glyph (should not happen for the printable set)

    // Tightly-packed RGBA8 tile (alpha = coverage, RGB white), glyph ink centred.
    outRGBA.assign(static_cast<std::size_t>(tileW) * tileH * 4u, 0u);
    std::uint8_t* px = outRGBA.data();
    const int offX = (static_cast<int>(tileW) - gw) / 2;
    const int offY = (static_cast<int>(tileH) - gh) / 2;
    for (int y = 0; y < gh; ++y)
    {
        for (int x = 0; x < gw; ++x)
        {
            const int tx = offX + x;
            const int ty = offY + y;
            if (tx < 0 || ty < 0 || tx >= static_cast<int>(tileW) ||
                ty >= static_cast<int>(tileH))
                continue;
            const std::uint8_t cov = gbm[static_cast<std::size_t>(y) * gw + x];
            if (cov == 0u)
                continue;
            std::uint8_t* d = px + (static_cast<std::size_t>(ty) * tileW + tx) * 4u;
            d[0] = 255u;
            d[1] = 255u;
            d[2] = 255u;
            d[3] = cov;
        }
    }
    stbtt_FreeBitmap(gbm, nullptr);
    return true;
}

} // namespace

FontAtlas::FontAtlas(quad::TextureAtlas& atlas) noexcept
    : atlas_(&atlas)
{
}

bool FontAtlas::load() noexcept
{
    // Reset any prior state so a failed re-load leaves nothing stale.
    slots_.assign(kAsciiGlyphCount, GlyphSlot{});
    loaded_ = false;

    const quad::AtlasLayout layout = atlas_->layout();
    if (!usable_layout(layout))
        return false;
    const std::uint32_t tileW = layout.tileW;
    const std::uint32_t tileH = layout.tileH;

    const auto bytes = embedded_font_bytes();
    stbtt_fontinfo font{};
    if (!stbtt_InitFont(&font, static_cast<const unsigned char*>(bytes.data()), 0))
        return false;
    const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(tileH) * 0.9f);
    if (scale <= 0.0f)
        return false;

    for (std::uint32_t cp = static_cast<std::uint32_t>(kAsciiFirst);
         cp <= static_cast<std::uint32_t>(kAsciiLast); ++cp)
    {
        const std::uint32_t idx = cp - static_cast<std::uint32_t>(kAsciiFirst);
        GlyphSlot& slot = slots_[idx];
        slot.codepoint = cp;
        slot.tileIndex = glyph_slot_index(cp);  // == idx for the printable range

        // Horizontal metrics at our scale.
        int advanceW = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font, static_cast<int>(cp), &advanceW, &lsb);
        slot.metrics.advance = static_cast<float>(advanceW) * scale;
        slot.metrics.leftBearing = static_cast<int>(static_cast<float>(lsb) * scale);

        // Ink bounding box (font units) -> pixels.
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBox(&font, static_cast<int>(cp), &x0, &y0, &x1, &y1);
        slot.metrics.topBearing = static_cast<int>(static_cast<float>(y1) * scale);
        slot.metrics.width  = static_cast<int>(static_cast<float>(x1 - x0) * scale);
        slot.metrics.height = static_cast<int>(static_cast<float>(y1 - y0) * scale);

        // Rasterize the coverage tile (drives hasCoverage).
        std::vector<std::uint8_t> tileRGBA;
        if (rasterize_coverage_tile(font, cp, tileW, tileH, tileRGBA))
        {
            slot.hasCoverage = true;
            coverageTiles_.push_back(std::move(tileRGBA));
        }
    }

    loaded_ = true;
    return true;
}

std::uint32_t FontAtlas::upload_glyphs() noexcept
{
    if (!loaded_)
        return 0;

    const std::uint32_t expectedBytes = quad::atlas_tile_bytes(atlas_->layout());
    std::uint32_t uploaded = 0u;
    std::size_t covIdx = 0u;
    for (std::uint32_t i = 0; i < kAsciiGlyphCount; ++i)
    {
        const GlyphSlot& slot = slots_[i];
        if (!slot.hasCoverage || covIdx >= coverageTiles_.size())
            continue;
        const std::vector<std::uint8_t>& bytes = coverageTiles_[covIdx++];
        if (bytes.size() != expectedBytes)
            continue;
        // tileIndex == i for the printable range (1:1 glyph -> tile contract).
        if (atlas_->upload_tile(slot.tileIndex, bytes))
            ++uploaded;
    }
    atlas_->flush();
    return uploaded;
}

GlyphSlot FontAtlas::slot_for(std::uint32_t codepoint) const noexcept
{
    const std::uint32_t idx = glyph_slot_index(codepoint);
    if (idx == kNoSlot || idx >= slots_.size())
        return {};
    return slots_[idx];
}

bool FontAtlas::has_slot(std::uint32_t codepoint) const noexcept
{
    const GlyphSlot s = slot_for(codepoint);
    return s.hasCoverage && s.codepoint != 0u;
}

std::span<const std::uint8_t> FontAtlas::coverage_tile(std::uint32_t codepoint) const noexcept
{
    const std::uint32_t idx = glyph_slot_index(codepoint);
    if (idx == kNoSlot || idx >= slots_.size() || !slots_[idx].hasCoverage)
        return {};
    // Coverage tiles are stored in codepoint order (index == codepoint-32).
    if (idx >= coverageTiles_.size())
        return {};
    return coverageTiles_[idx];
}

} // namespace vaml::vulkan::font
