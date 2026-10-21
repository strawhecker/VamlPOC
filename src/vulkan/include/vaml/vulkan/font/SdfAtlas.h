#pragma once
// S204 — SdfAtlas: the SDF rasterizer (vaml::vulkan::font), dep:S203.
//
// Converts S203's per-glyph COVERAGE bitmaps into signed-distance-field tiles
// in an S202 quad::TextureAtlas, for SOFT-EDGE text sampling. S204 does NOT
// re-rasterize from the font: it consumes the coverage tiles S203 already
// produced (the S203-recorded NO-GAP — S203's bitmaps are S204's explicit SDF
// source; no second rasterizer), so S204 is purely CPU.
//
// MAIN IDEA / APPROACH (the opinionated default recorded at plan time): a
// chamfer distance transform over each glyph's coverage bitmap, normalized so
// each SDF texel carries a signed distance to the glyph edge mapped to
// [0,1] with 0.5 at the edge (packed 0..255). The soft-edge sampling headline
// — the CPU-side value semantic S205's shader mirrors — is `sdf_soft_alpha`:
// the standard linear-AA ramp `clamp((d - (0.5 - aa)) / (2*aa), 0, 1)`.
//
// PURE DECISION SEAMS (headless, ctest-proven, no Vulkan runtime):
//   - sdf_spread()       — half-width of the soft edge, in SDF cells (default 4)
//   - sdf_cells_per_px() — SDF sampling density vs the S203 coverage bitmap
//                           (default 1: the SDF grid == the coverage grid)
//   - coverage_to_sdf(covRGBA, w, h, spread) → a packed SDF tile (0..255 per
//     texel; interior > edge, exterior < edge) using a two-pass chamfer
//     distance transform. Returns an EMPTY raster on degenerate input.
//   - sdf_raster_valid()
//   - sdf_soft_alpha(dist01, aa) → alpha in [0,1] via the linear-AA ramp
//
// The REAL OWNER `SdfAtlas` borrows both an S202 quad::TextureAtlas (for tile
// uploads) and an S203 FontAtlas (for its coverage tiles + metrics): for each
// glyph it computes coverage_to_sdf over S203's coverage, uploads one SDF tile
// via the S202 sub-region upload (only that tile dirties) + flushes, and
// records SdfGlyph { codepoint, tileIndex, metrics, spread }.
//
// NOT here (recorded, not silent): the actual TEXT SAMPLING shader is S205 —
// it consumes SdfAtlas::texture_atlas() (the SDF resource) + the sdf_soft_alpha
// AA contract; S206 batches same-shader text/rect draws from the shared
// descriptor layout; text LAYOUT is S400/S207'ish, out of module scope. NO
// MINIMAL REAL / NOT a stub — the real, permanent, kept-and-extended SDF atlas.
//
// LIFETIMES: the borrowed atlas + FontAtlas must OUTLIVE this object; the
// atlas graphics queue must be idle before upload_sdfs()'s flush().
// THREADING (ADR-018): single-threaded, once-at-import, zero locks.

#include <vaml/vulkan/font/FontAtlas.h>

#include <cstdint>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::font {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// Half-width of the soft edge, in SDF cells (the S204 default: 4). Each SDF
// texel's signed distance is clamped to [-spread, +spread] before packing.
inline constexpr std::uint32_t kSdfDefaultSpread = 4u;

// SDF sampling density relative to S203's coverage bitmap (default 1: the SDF
// grid == the coverage grid). A higher density later (e.g. S400 supersampling)
// is S400's concern, recorded NOT silent.
inline constexpr std::uint32_t kSdfCellsPerPx = 1u;

// A packed SDF raster: ONE texel per (w*h), each value 0..255 where 0 = fully
// outside (distance -spread), 128 = the glyph edge, 255 = fully inside
// (distance +spread). w/h/spread are tagged for the S205 input + the
// "soft-edge" contract.
struct SdfRaster
{
    std::uint32_t                w      = 0;
    std::uint32_t                h      = 0;
    std::uint32_t                spread = kSdfDefaultSpread;
    std::vector<std::uint8_t>    packed;      // size w*h
};

// True iff `r` is a non-empty, internally-consistent SDF raster (packed size
// == w*h, spread > 0).
VAML_VULKAN_EXPORT bool sdf_raster_valid(const SdfRaster& r) noexcept;

// The SDF computation primitive over a COVERAGE bitmap (S203's tile, tightly
// packed RGBA8 whose ALPHA channel is the glyph coverage 0..255). Computes a
// per-texel signed distance to the glyph edge (chamfer two-pass distance
// transform, threshold at 50% coverage) and packs it to 0..255 with `spread`
// cells of soft edge. Returns an EMPTY raster (w/h/spread == 0) on degenerate
// input (w/h == 0 or spread == 0). Pure + headless-testable — this is the
// "glyphs → SDF into atlas" core.
VAML_VULKAN_EXPORT SdfRaster coverage_to_sdf(const std::uint8_t* covRGBA,
                                             std::uint32_t w, std::uint32_t h,
                                             std::uint32_t spread) noexcept;

// 8-bit convenience overload taking the raw bytes directly.
[[nodiscard]] inline SdfRaster coverage_to_sdf(const std::vector<std::uint8_t>& covRGBA,
                                               std::uint32_t w, std::uint32_t h,
                                               std::uint32_t spread) noexcept
{
    if (covRGBA.empty())
        return {};
    return coverage_to_sdf(covRGBA.data(), w, h, spread);
}

// The SOFT-EDGE SAMPLING headline: maps an SDF texel's [0,1] distance (0.5 =
// edge) to a coverage alpha in [0,1] via the standard linear-AA ramp
//   clamp((d - (0.5 - aa)) / (2*aa), 0, 1)
// Fully-inside → 1, fully-outside → 0, half-edge → ~0.5, and a LARGER `aa`
// WIDENS the transition (softer edge). This is the value semantic S205's text
// shader mirrors. `dist01` is clipped to [0,1]; `aa` must be > 0.
VAML_VULKAN_EXPORT float sdf_soft_alpha(float dist01, float aa) noexcept;

// ---------------------------------------------------------------------------
// Real SDF-atlas owner (lightweight value; no Vulkan handles of its own)
// ---------------------------------------------------------------------------

// One populated SDF atlas slot: which S202 tile the glyph's SDF occupies + the
// glyph metrics (carried through) + the spread used.
struct SdfGlyph
{
    std::uint32_t codepoint   = 0;
    std::uint32_t tileIndex   = 0;
    GlyphMetrics  metrics     {};
    std::uint32_t spread      = kSdfDefaultSpread;
    bool          hasSdf      = false;
};

// The real S204 owner. Lightweight value borrowing a quad::TextureAtlas + a
// FontAtlas; owns the SDF slot map. No Vulkan handles of its own.
class VAML_VULKAN_EXPORT SdfAtlas
{
public:
    // Borrows `atlas` for tile uploads + `font` for its coverage-tile SDF
    // source + metrics. Both must outlive this object.
    SdfAtlas(quad::TextureAtlas& atlas, const FontAtlas& font) noexcept;

    // For each glyph of the source S203 FontAtlas, computes SDF tiles over
    // sdf_spread default, uploads them (S202 sub-region upload, one tile per
    // glyph), flushes, and records	SdfGlyph. Returns false (no state change)
    // when the source has no coverage / the atlas is unusable. Idempotent.
    [[nodiscard]] bool build() noexcept;

    // Flushes any pending SDF tiles to the GPU (S202 sub-region invalidation;
    // each tile dirties only its own flag). Returns tiles uploaded (0 if none).
    std::uint32_t upload_sdfs() noexcept;

    // The slot record for `codepoint` (32..126); empty if never built.
    [[nodiscard]] SdfGlyph slot_for(std::uint32_t codepoint) const noexcept;
    [[nodiscard]] bool has_slot(std::uint32_t codepoint) const noexcept;
    [[nodiscard]] bool built() const noexcept { return built_; }

    // The atlas all SDF glyphs are drawn from (the S205 text shader's sampled
    // resource). Forwards: stable after construction.
    [[nodiscard]] quad::TextureAtlas& atlas() noexcept { return *atlas_; }
    [[nodiscard]] const quad::TextureAtlas& atlas() const noexcept { return *atlas_; }

    // The spread used (kSdfDefaultSpread unless overridden at build time).
    [[nodiscard]] std::uint32_t spread() const noexcept { return spread_; }

private:
    quad::TextureAtlas*  atlas_ = nullptr;
    const FontAtlas*     font_  = nullptr;
    std::uint32_t        spread_ = kSdfDefaultSpread;
    std::vector<SdfGlyph> slots_;            // indexed by (codepoint-32)
    std::vector<std::vector<std::uint8_t>> sdfTiles_;  // the RGBA8 SDF tiles
    bool                 built_  = false;
    bool                 uploaded_ = false;
};

} // namespace vaml::vulkan::font
