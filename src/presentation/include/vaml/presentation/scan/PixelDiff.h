#pragma once
// S641 — THE UI-COMPLIANCE SCANNER — PixelDiff.h (GIMP-style diff + tolerance tool).
//
// The colored per-pixel diff image a human or the LLM interprets, + the
// auto-locate-text tolerance band, + the `full-missing-content` detection
// (one blank, one present -> full-missing-content). The diff is written as a
// REAL PNG via the vendored stb_image_write seam (self-contained, no external
// tool). PNG bit-match (S.Deferred-PNG) stays the RECORDED upgrade path; the
// structured scan is authoritative NOW.
//
// COLOR CODING:
//   * unchanged pixels  -> grey (the actual's color, desaturated toward neutral)
//   * differing pixels   -> HOT (magenta/red) where the two frames differ beyond tolerance
//   * missing-content    -> red REGION (a blank region where ink was expected)
//
// Pure + headless (both CI lanes): the diff math + band + classification are
// ctestable; writing the PNG bytes is done through `pixel_diff_encode_png` (the
// stb_image_write seam — encoding is pure data-munging, no window/GPU).
//
// ZERO LOCKS (ADR-018) + ZERO MINIMAL REAL.

#include <vaml/presentation/RenderTarget.h> // Impl::ImageBuffer

#include <cstdint>
#include <string>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation::scan {

// The per-pixel diff classification (the color-coding input).
enum class DiffPixel
{
    Unchanged,      // grey  (within tolerance)
    Different,      // HOT   (beyond tolerance)
    MissingContent, // RED   (blank region where ink was expected)
};

// The diff of two BGRA frames at the SAME dims. `image` holds RGBA bytes (row 0
// = top) ready to encode as a PNG; `classifications` per pixel; the aggregate
// counters. The RAW frames are retained (actual_bgra / expected_bgra) so a
// per-quad TOLERANCE BAND can re-evaluate pixels declared inside a quad region
// (the auto-locate-text band). The diff runs standalone AND declaration-aware.
struct PixelDiff
{
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> image;       // RGBA diff image (w*h*4)
    std::vector<DiffPixel>    classifications; // w*h
    std::vector<std::uint8_t> actual_bgra;    // retained raw actual frame (w*h*4)
    std::vector<std::uint8_t> expected_bgra;  // retained raw expected frame (w*h*4)
    std::uint64_t unchanged = 0;
    std::uint64_t different = 0;
    std::uint64_t missing   = 0;
};

// Computes a GIMP-style per-pixel diff between the actual and expected frames
// (SAME dims; a size mismatch -> a Truncated/different classification for the
// whole — never a silent pass). `tolerance` is the per-channel deviation (0..1,
// relative to 255). Regions that are blank in `actual` but inked in `expected`
// (beyond tolerance) classify as MissingContent.
VAML_PRESENTATION_EXPORT PixelDiff
pixel_diff(const Impl::ImageBuffer& actual, const Impl::ImageBuffer& expected,
           double tolerance);

// Auto-locate + tolerance-band classification over a declared quad: RE-evaluates
// the pixels inside the declared band against the RETAINED raw frames — a blank
// actual region where ink was expected (beyond tolerance) is flagged
// MissingContent, differing pixels stay HOT. This is the roadmap's "one blank,
// one present" -> full-missing-content catch at the QUAD level. Pure.
VAML_PRESENTATION_EXPORT void
classify_quad(PixelDiff& diff, float x, float y, float w, float h, double tolerance);

// Encodes the diff image as a PNG byte buffer (via stb_image_write). Returns
// false (never silent) if encoding fails.
VAML_PRESENTATION_EXPORT bool pixel_diff_encode_png(const PixelDiff& diff,
                                                    std::vector<std::uint8_t>& outPng);

// Convenience: writes a diff PNG to `path`. Returns false (never silent) on
// write/encode failure.
VAML_PRESENTATION_EXPORT bool pixel_diff_write_png(const PixelDiff& diff, const std::string& path);

} // namespace vaml::presentation::scan
