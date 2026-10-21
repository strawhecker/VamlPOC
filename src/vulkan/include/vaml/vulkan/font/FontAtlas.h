#pragma once
// S203 — FontAtlas: the embedded bitmap-font loader + ASCII 32–126 glyph atlas
// build (vaml::vulkan::font), dep:S202. This is the "exactly as fonts are"
// model S202_1 referenced: a real TrueType default font (embedded, the S165
// embedded-SPIR-V precedent) is parsed by stb_truetype and each printable
// ASCII 32–126 glyph is rasterized ONCE into an S202 quad::TextureAtlas tile,
// via the S202 SUB-REGION UPLOAD (only the glyph's own tile dirties).
//
// The S202 atlas grid is reused as the glyph atlas grid (the recorded S203
// decision): each of the 95 glyphs maps 1:1 to an atlas tile via
// glyph_slot_index(codepoint) — no re-authored grid math, no second atlas
// layout. The atlas is drawn by the UNCHANGED S200/S201 quad pipeline: a
// consumer samples texture_atlas() and selects a glyph tile with quad UVs.
// S203 adds NO renderer, NO text layout, NO on-screen path.
//
// PURE DECISION SEAMS (headless, ctest-proven, no Vulkan runtime):
//   - kAsciiFirst = 32 / kAsciiLast = 126 / kAsciiGlyphCount = 95
//   - ascii_codepoints() → the 95 codepoints (ordered)
//   - GlyphMetrics { advance; leftBearing, topBearing, width, height } +
//     glyph_metrics_valid() (the per-glyph metric record a text layout needs)
//   - glyph_cell_size() and glyph_slot_index(cp) — derived ONLY from the
//     S202 exact-grid atlas layout (atlas_layout_valid / atlas_tile_region),
//     reused, not re-implemented.
//   - embedded_font_bytes() → span<const uint8_t> of the checked-in blob;
//     embedded_font_valid() (parses via stbtt_InitFont with NO device — the
//     headless "the embedded font loads" proof).
//
// The REAL OWNER `FontAtlas` — minimal Vulkan handles, borrows the S202
// quad::TextureAtlas (mirrors icon::IconAtlas): load() parses the embedded
// font + computes metrics + rasterizes each glyph's coverage bitmap ONCE into
// an RGBA8 tile; upload_glyphs() uploads each tile (S202 sub-region
// invalidation) + flushes. Records GlyphSlot { codepoint, tileIndex, metrics }.
//
// LIFETIMES: the borrowed atlas must OUTLIVE this object (and its graphics
// queue must be idle before upload_glyphs()'s flush()). The embedded font blob
// is static/process-lifetime (interning-equivalent).
//
// NOT here (recorded, not silent): the SDF conversion of these coverage
// bitmaps is S204 (its explicit SDF source — no second rasterizer); the soft
// text sampling shader is S205; the text LAYOUT (advance run, wrapping) is
// S400/S207'ish, out of this module's scope. NO MINIMAL REAL / NOT a stub —
// this is the real, permanent, kept-and-extended glyph atlas.

#include <vaml/vulkan/quad/TextureAtlas.h>

#include <cstdint>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::font {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The printable-ASCII glyph range the embedded default font covers (S203).
// kAsciiFirst = ' ' (32), kAsciiLast = '~' (126), kAsciiGlyphCount = 95.
inline constexpr int kAsciiFirst       = 32;
inline constexpr int kAsciiLast        = 126;
inline constexpr std::uint32_t kAsciiGlyphCount = 95u;

// The 95 printable-ASCII codepoints in ASCENDING order (32..126 inclusive).
// Pure + headless-testable.
VAML_VULKAN_EXPORT std::vector<std::uint32_t> ascii_codepoints() noexcept;

// One glyph's advance + ink-bounding-box metrics (font units scaled to the
// raster size at load time). advance is in pixels (the horizontal advance the
// text layout consumes); leftBearing/topBearing/width/height describe the
// glyph's ink bbox relative to its origin/baseline (pixels).
struct GlyphMetrics
{
    float      advance       = 0.0f;
    int        leftBearing   = 0;
    int        topBearing    = 0;
    int        width         = 0;
    int        height        = 0;
};

// True iff `m` is a sane, non-negative ink bbox with a strictly-positive
// advance (a glyph a text layout can actually place).
VAML_VULKAN_EXPORT bool glyph_metrics_valid(const GlyphMetrics& m) noexcept;

// The glyph atlas cell size — DERIVED from the borrowed S202 layout (each tile
// is tileW×tileH pixels). Requires a valid atlas layout; {0,0} otherwise.
VAML_VULKAN_EXPORT std::pair<std::uint32_t, std::uint32_t>
glyph_cell_size(const quad::AtlasLayout& layout) noexcept;

// Maps an ASCII codepoint (32..126) to its 1:1 atlas tile index. The S203
// glyph atlas REUSES the S202 exact-grid layout: the 95 glyphs fill the first
// 95 tiles in codepoint order (tile i = codepoint (32 + i)). Requires a valid
// layout with at least 95 tiles; returns kNoSlot otherwise / cp out of range.
VAML_VULKAN_EXPORT std::uint32_t glyph_slot_index(std::uint32_t codepoint) noexcept;
inline constexpr std::uint32_t kNoSlot = 0xFFFFFFFFu;

// The embedded default font bytes (span over the checked-in blob). Static.
VAML_VULKAN_EXPORT std::span<const std::uint8_t> embedded_font_bytes() noexcept;

// True iff the embedded font bytes parse as a valid TrueType font (stbtt_InitFont
// succeeds with no device — the headless "the embedded font loads" proof).
VAML_VULKAN_EXPORT bool embedded_font_valid() noexcept;

// ---------------------------------------------------------------------------
// Real glyph-atlas owner (lightweight value; no Vulkan handles of its own)
// ---------------------------------------------------------------------------

// One populated glyph atlas slot: which S202 tile the glyph occupies + its
// metrics.
struct GlyphSlot
{
    std::uint32_t codepoint      = 0;
    std::uint32_t tileIndex      = 0;
    GlyphMetrics  metrics        {};
    bool          hasCoverage    = false;  // a real coverage bitmap was rasterized
};

// The real S203 owner. A lightweight value that borrows a quad::TextureAtlas
// (which itself is move-only) and owns the glyph slot map. NOT move-only by
// value semantics: it holds no Vulkan handles, only the borrowed pointer +
// CPU bookkeeping. Constructed directly.
class VAML_VULKAN_EXPORT FontAtlas
{
public:
    // Borrows `atlas` for glyph tile uploads. `atlas` must outlive this object
    // and may be over ANY valid S202 exact-grid layout with >= 95 tiles.
    explicit FontAtlas(quad::TextureAtlas& atlas) noexcept;

    // Parses the EMBEDDED default font (embedded_font_bytes) with stb_truetype
    // and, for each printable-ASCII 32..126 codepoint, computes GlyphMetrics +
    // rasterizes the glyph's COVERAGE bitmap ONCE into an RGBA8 tile (alpha =
    // coverage, color white) sized to fill the S202 atlas cell. Records the
    // GlyphSlot for each glyph. Returns false (WITHOUT corrupting state) when
    // the embedded font can't be parsed, or the atlas layout is invalid / has
    // < 95 tiles. Slots are NOT flushed to the GPU until upload_glyphs().
    [[nodiscard]] bool load() noexcept;

    // Uploads every glyph tile into its S202 atlas slot (each upload_tile
    // dirties ONLY that glyph's tile — S202 sub-region invalidation) and
    // flushes the dirty set to the GPU. Returns the number of tiles uploaded
    // (0 if load() hasn't succeeded / nothing to flush). Idempotent: repeated
    // calls after a successful flush() upload nothing (no dirty tiles).
    std::uint32_t upload_glyphs() noexcept;

    // The slot record for `codepoint` (32..126); empty if never loaded.
    [[nodiscard]] GlyphSlot slot_for(std::uint32_t codepoint) const noexcept;

    // True iff `codepoint` (32..126) has a populated, coverage-bearing slot.
    [[nodiscard]] bool has_slot(std::uint32_t codepoint) const noexcept;

    // The atlas all glyphs are drawn from (sampled by the unchanged S200/S201
    // pipeline). Forwards: stable after construction.
    [[nodiscard]] quad::TextureAtlas& atlas() noexcept { return *atlas_; }
    [[nodiscard]] const quad::TextureAtlas& atlas() const noexcept { return *atlas_; }

    // The rasterized RGBA8 coverage tile for `codepoint` (alpha = glyph
    // coverage, RGB white) — the S204 SDF SOURCE. Empty span if not loaded /
    // the glyph has no coverage. S204 converts THESE bitmaps to SDF (no second
    // rasterizer), per the S203-recorded NO-GAP.
    [[nodiscard]] std::span<const std::uint8_t> coverage_tile(std::uint32_t codepoint) const noexcept;

    // Number of distinct glyph slots populated so far (95 on a successful load).
    [[nodiscard]] std::uint32_t slot_count() const noexcept { return static_cast<std::uint32_t>(slots_.size()); }

    // True iff load() has succeeded and the slots are ready for upload.
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

private:
    quad::TextureAtlas*      atlas_ = nullptr;
    std::vector<GlyphSlot>   slots_;       // indexed by (codepoint-32)
    std::vector<std::vector<std::uint8_t>> coverageTiles_;  // the rasterized RGBA8 tiles
    bool                     loaded_ = false;
};

} // namespace vaml::vulkan::font
