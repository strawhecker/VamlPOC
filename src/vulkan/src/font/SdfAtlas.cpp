// S204 — SdfAtlas implementation: converts S203's coverage bitmaps into signed-
// distance-field tiles (chamfer distance transform over the coverage alpha),
// uploaded into an S202 quad::TextureAtlas one tile per glyph via the S202
// SUB-REGION upload (only that glyph's tile dirties). No re-rasterization —
// S203's coverage tiles are S204's explicit SDF source (the S203 NO-GAP).
//
// The pure seam `coverage_to_sdf` returns a single-channel (1 byte/texel,
// w*h) packed SDF — the headless-computable SDF. The S202 atlas is RGBA8, so
// the SdfAtlas OWNER repacks each SDF texel into an RGBA8 tile with
// R=G=B=A=dist (the quadratic/multichannel expansion the RGBA8 atlas format
// requires) before upload. The shader-meaningful value is identical in all
// four channels.
//
// The SHADER-side soft-edge sampling is NOT here: S205 consumes the SDF atlas
// + the sdf_soft_alpha AA contract. S204 delivers the value semantic (the
// CPU-side proof of what S205's shader mirrors).
//
// THREADING (ADR-018): single-threaded, once-at-import, zero locks.

#include <vaml/vulkan/font/SdfAtlas.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vaml::vulkan::font {

namespace {

// The coverage threshold: a texel at >= 50% coverage counts as inside the glyph
// for the distance transform's edge definition.
constexpr int kCoverageThreshold = 128;
constexpr float kBig = 1e12f;

// The chamfer distance-transform kernel steps (1 texel = 1 SDF cell; the
// spread is expressed in SDF cells).
constexpr float kKernelXY   = 1.00f;   // direct-neighbour step
constexpr float kKernelDiag = 1.41f;  // diagonal step (approx sqrt(2))

inline std::size_t flat(std::uint32_t iw, int x, int y) noexcept
{
    return static_cast<std::size_t>(y) * iw + static_cast<std::size_t>(x);
}

// Classic two-pass chamfer distance transform: `field` is seeded (0 = seed,
// +kBig = far) and, after the two passes, holds the distance of each texel to
// the nearest seed cell. The kernel is symmetric so one forward + one backward
// pass over the masked image is sufficient.
void chamfer_dt(std::vector<float>& field, std::uint32_t w, std::uint32_t h) noexcept
{
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Forward pass (top-left -> bottom-right): pull from already-visited
    // (top / left / top-left / top-right) neighbours.
    for (int y = 0; y < ih; ++y)
    {
        for (int x = 0; x < iw; ++x)
        {
            float& f = field[flat(static_cast<std::uint32_t>(iw), x, y)];
            if (y > 0)          f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x, y - 1)] + kKernelXY);
            if (x > 0)          f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x - 1, y)] + kKernelXY);
            if (x > 0 && y > 0) f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x - 1, y - 1)] + kKernelDiag);
            if (x + 1 < iw && y > 0) f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x + 1, y - 1)] + kKernelDiag);
        }
    }
    // Backward pass (bottom-right -> top-left): pull from (bottom / right /
    // bottom-left / bottom-right) neighbours.
    for (int y = ih - 1; y >= 0; --y)
    {
        for (int x = iw - 1; x >= 0; --x)
        {
            float& f = field[flat(static_cast<std::uint32_t>(iw), x, y)];
            if (y + 1 < ih)         f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x, y + 1)] + kKernelXY);
            if (x + 1 < iw)         f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x + 1, y)] + kKernelXY);
            if (x + 1 < iw && y + 1 < ih) f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x + 1, y + 1)] + kKernelDiag);
            if (x > 0 && y + 1 < ih)     f = std::min(f, field[flat(static_cast<std::uint32_t>(iw), x - 1, y + 1)] + kKernelDiag);
        }
    }
}

// Boxes a signed distance into [-spread, +spread] then maps to a byte with
// 0.5 (128) at the edge (d=0).
inline std::uint8_t pack_dist(float d, float spread) noexcept
{
    d = std::max(-spread, std::min(spread, d));
    const float t = (d / (2.0f * spread)) + 0.5f;
    const float v = std::max(0.0f, std::min(255.0f, t * 255.0f));
    return static_cast<std::uint8_t>(v + 0.5f);
}

} // namespace

bool sdf_raster_valid(const SdfRaster& r) noexcept
{
    return r.w != 0u && r.h != 0u && r.spread != 0u &&
           r.packed.size() == static_cast<std::size_t>(r.w) * r.h;
}

SdfRaster coverage_to_sdf(const std::uint8_t* covRGBA,
                          std::uint32_t w, std::uint32_t h,
                          std::uint32_t spread) noexcept
{
    SdfRaster out;
    if (!covRGBA || w == 0u || h == 0u || spread == 0u)
        return out;
    out.w      = w;
    out.h      = h;
    out.spread = spread;

    const std::size_t n = static_cast<std::size_t>(w) * h;
    const float fSpread = static_cast<float>(spread);

    // Two distance fields, one away from the INSIDE seeds, one away from the
    // OUTSIDE seeds, so every texel learns its signed distance to the edge.
    std::vector<float> distToInside(n, 0.0f);
    std::vector<float> distToOutside(n, 0.0f);
    for (std::size_t i = 0; i < n; ++i)
    {
        const bool inside = covRGBA[i * 4u + 3u] >= kCoverageThreshold;
        distToInside[i]   =  inside ? 0.0f  : kBig;  // seeds = inside texels
        distToOutside[i]  = !inside ? 0.0f  : kBig;   // seeds = outside texels
    }
    chamfer_dt(distToInside,  w, h);   // dist of an outside texel to nearest inside
    chamfer_dt(distToOutside, w, h);   // dist of an inside texel to nearest outside

    // Signed distance: positive inside (toward the interior), negative outside.
    // Build the packed 1-channel field.
    out.packed.resize(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const bool inside = covRGBA[i * 4u + 3u] >= kCoverageThreshold;
        const float d = inside ? distToOutside[i] : -distToInside[i];
        out.packed[i] = pack_dist(d, fSpread);
    }
    return out;
}

float sdf_soft_alpha(float dist01, float aa) noexcept
{
    if (!(aa > 0.0f))
        return 0.0f;
    const float d = std::max(0.0f, std::min(1.0f, dist01));
    // linear AA ramp centred at the 0.5 edge with half-width `aa`.
    const float alpha = (d - (0.5f - aa)) / (2.0f * aa);
    return std::max(0.0f, std::min(1.0f, alpha));
}

SdfAtlas::SdfAtlas(quad::TextureAtlas& atlas, const FontAtlas& font) noexcept
    : atlas_(&atlas)
    , font_(&font)
{
}

bool SdfAtlas::build() noexcept
{
    if (!font_ || !font_->loaded())
        return false;
    const quad::AtlasLayout layout = atlas_->layout();
    if (!quad::atlas_layout_valid(layout) ||
        quad::atlas_tile_count(layout) < kAsciiGlyphCount)
        return false;

    slots_.assign(kAsciiGlyphCount, SdfGlyph{});
    sdfTiles_.clear();
    built_ = false;
    uploaded_ = false;

    const std::uint32_t tileW = layout.tileW;
    const std::uint32_t tileH = layout.tileH;
    const std::size_t tileTexels = static_cast<std::size_t>(tileW) * tileH;

    for (std::uint32_t cp = static_cast<std::uint32_t>(kAsciiFirst);
         cp <= static_cast<std::uint32_t>(kAsciiLast); ++cp)
    {
        const std::uint32_t idx = cp - static_cast<std::uint32_t>(kAsciiFirst);
        const GlyphSlot gs = font_->slot_for(cp);
        if (!gs.hasCoverage)
            continue;  // leave this slot empty

        const std::span<const std::uint8_t> cov = font_->coverage_tile(cp);
        if (cov.empty() || cov.size() < tileTexels * 4u)
            continue;

        const SdfRaster sdf = coverage_to_sdf(cov.data(), tileW, tileH, spread_);
        if (!sdf_raster_valid(sdf))
            continue;

        // Repack the 1-channel SDF as an RGBA8 tile (R=G=B=A=dist) for the
        // S202 RGBA8 atlas format; the value is identical in all channels.
        std::vector<std::uint8_t> rgba8(tileTexels * 4u, 0u);
        for (std::size_t i = 0; i < tileTexels; ++i)
        {
            const std::uint8_t v = sdf.packed[i];
            rgba8[i * 4u + 0u] = v;
            rgba8[i * 4u + 1u] = v;
            rgba8[i * 4u + 2u] = v;
            rgba8[i * 4u + 3u] = v;
        }

        SdfGlyph& sg = slots_[idx];
        sg.codepoint = cp;
        sg.tileIndex = glyph_slot_index(cp);
        sg.metrics   = gs.metrics;
        sg.spread    = spread_;
        sg.hasSdf    = true;
        sdfTiles_.push_back(std::move(rgba8));
    }

    built_ = true;
    return true;
}

std::uint32_t SdfAtlas::upload_sdfs() noexcept
{
    if (!built_ || uploaded_)
        return 0u;
    const std::size_t expectedBytes =
        static_cast<std::size_t>(quad::atlas_tile_bytes(atlas_->layout()));
    std::uint32_t uploaded = 0u;
    std::size_t sdfIdx = 0u;
    for (std::uint32_t i = 0; i < kAsciiGlyphCount; ++i)
    {
        if (!slots_[i].hasSdf || sdfIdx >= sdfTiles_.size())
            continue;
        const std::vector<std::uint8_t>& sdf = sdfTiles_[sdfIdx++];
        if (sdf.size() != expectedBytes)
            continue;
        if (atlas_->upload_tile(slots_[i].tileIndex, sdf))
            ++uploaded;
    }
    atlas_->flush();
    uploaded_ = true;
    return uploaded;
}

SdfGlyph SdfAtlas::slot_for(std::uint32_t codepoint) const noexcept
{
    const std::uint32_t idx = glyph_slot_index(codepoint);
    if (idx == kNoSlot || idx >= slots_.size())
        return {};
    return slots_[idx];
}

bool SdfAtlas::has_slot(std::uint32_t codepoint) const noexcept
{
    const SdfGlyph s = slot_for(codepoint);
    return s.hasSdf;
}

} // namespace vaml::vulkan::font
