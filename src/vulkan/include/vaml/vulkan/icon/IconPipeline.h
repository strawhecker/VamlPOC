#pragma once
// S202_1 — the ICON asset pipeline (single quad engine; per ADR-015).
//
// Mandate (ADR-015, recorded): PNG and SVG are BOTH source asset formats, each
// rasterized ONCE into the S202 TextureAtlas exactly as fonts are (S203/S204)
// — no vector-on-screen, no second renderer. Output tagged with its texel count
// for best-fit selection (S213_1).
//
// This module provides the IMPORT/RASTERIZATION half: a packaged PNG or SVG
// source is rasterized ONCE into RGBA8, tagged with its texel count (w*h), and
// uploaded as a tile of the EXISTING S202 quad::TextureAtlas. The result is
// drawn by the UNCHANGED S200/S201 quad pipeline (QuadPipeline/QuadScene sample
// the atlas; a quad's UVs select its tile). There is NO second renderer and NO
// vector-on-screen path — the render path is untouched by S202_1.
//
// NOT here (NO-GAP recorded in the S202_1 plan, not silent):
//   - S203/S204: glyph atlas + SDF rasterizer (icon import only; fonts are a
//     separate token; "exactly as fonts are" is a MODEL statement, not scope).
//   - S205/S205_1: shared descriptor/shader layout + monochrome TINT-at-sample.
//     S202_1 rasterizes full-color pixels; shading-time tinting is S205_1.
//   - S213_1: the DPI best-fit SELECTION POLICY. S202_1 only PRODUCES the tagged
//     inputs (per-slot texelCount + the size family) that S213_1 consumes.
//   - S360_1 / S400_1 / S528_1: IconKey→resource wiring, the Icon control, and
//     the pixel-snapshot gallery.
//   - The deliberate fallback GLYPH + its structured diagnostic: S202_1 surfaces
//     a decode/raster failure as a NON-CRASHING structured marker
//     (icon_error_kind); the fallback GLYPH itself is S400_1's (the Icon
//     control's) job — recorded, never backfilled as a stub.
//
// STRUCTURAL SECURITY RULE (ADR-015): the importer consumes ONLY from the
// packaged IconSourceSet (bounded key → IconSource). A raw path / URI / script is
// STRUCTURALLY unrepresentable in this API — there is no path-taking entry point.
//
// LIFETIMES: a `quad::TextureAtlas` may be borrowed by IconAtlas; it must outlive
// the IconAtlas. CPU raster + import is single-threaded and once-at-import
// (ADR-018: zero new locks).

#include <vaml/vulkan/quad/TextureAtlas.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::icon {

// ---------------------------------------------------------------------------
// Source formats
// ---------------------------------------------------------------------------

// The TWO S202_1 source-asset formats. PNG is decoded (stb_image); SVG is
// rasterized (nanosvg) at a requested size. Both produce RGBA8.
enum class IconFormat
{
    Png,
    Svg,
};

// A packaged icon asset: a format tag + the asset's raw bytes. NEVER an
// arbitrary path — this is the ADR-015 structural boundary (the importer only
// ever consumes from IconSourceSet; a path/URI/script is unrepresentable).
struct IconSource
{
    IconFormat             format = IconFormat::Png;
    std::vector<std::uint8_t> bytes;
};

// A bounded key → IconSource map. The ONLY thing the importer may consume.
using IconSourceSet = std::unordered_map<std::uint32_t, IconSource>;

// ---------------------------------------------------------------------------
// Pure decision seams (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// Structured, NON-CRASHING failure marker (ADR-015 NO-GAP: the decode/raster
// path never crashes on bad input — it reports). None == success.
enum class IconErrorKind
{
    None,
    Malformed,    // source bytes did not decode into a valid image/svg
    Unsupported,  // a format the installed rasterizer cannot produce
    OutOfSet,     // the requested key is not a member of the IconSourceSet
};

// The rasterized-once output, tagged with its texel count for S213_1 best-fit
// selection. rgba is tightly-packed R8G8B8A8 (w*h*4); texelCount == w*h.
struct IconRaster
{
    std::uint32_t             w          = 0;
    std::uint32_t             h          = 0;
    std::vector<std::uint8_t> rgba;              // w*h*4, tightly packed
    std::uint32_t             texelCount = 0;    // == w*h (the S213_1 best-fit tag)
};

// True iff `r` describes a valid, non-empty raster. Pure + headless-testable.
VAML_VULKAN_EXPORT bool icon_raster_valid(const IconRaster& r) noexcept;

// The ADR-015 structural-rule seam: true iff `key` is a member of `set`; on
// success copies the packaged source into `out` (the ONLY source a consumer may
// submit to raster_icon/IconAtlas). A false return is the OutOfSet rejection —
// a caller maps it to IconErrorKind::OutOfSet. Pure + headless-testable.
VAML_VULKAN_EXPORT bool source_by_key(const IconSourceSet& set, std::uint32_t key,
                                      IconSource& out) noexcept;

// Rasterizes `source` ONCE into RGBA8 at sizeW×sizeH. PNG: decoded then
// resampled (stb_image). SVG: rasterized at the requested size (nanosvg).
// Returns true and fills `out` on success; false + `out` left untouched on
// failure. On failure the kind is reported via outError (nullptr tolerated),
// so a caller can distinguish Malformed / Unsupported. Pure + headless-testable
// (no device). This is the "rasterized ONCE" seam — callers cache the result and
// never re-rasterize per-frame.
VAML_VULKAN_EXPORT bool raster_icon(const IconSource& source,
                                    std::uint32_t sizeW, std::uint32_t sizeH,
                                    IconRaster& out,
                                    IconErrorKind* outError = nullptr) noexcept;

} // namespace vaml::vulkan::icon
