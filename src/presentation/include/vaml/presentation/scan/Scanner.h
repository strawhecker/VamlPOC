#pragma once
// S641 — THE UI-COMPLIANCE SCANNER — Scanner.h (scrape + compliance_compare + ScanReport).
//
// Sprint 11r (S641): the scanner makes the compare LIVE — render an authored
// `.xaml` through the REAL retained render path -> S640a `ImageBuffer` -> SCRAPE
// the rendered UI for compliance (element/draw-item presence, coordinates,
// pixel/color at declared locations) -> COMPARE to the expected outcome
// DECLARED IN that `.xaml`.
//
// The `ScanReport` carries the pixel/element grid samples + the per-datum
// PASS/FAIL detail; `compliance_compare(actualScan, expectedDecl)` returns the
// PASS/FAIL (per-datum detail) — NEVER a bare global bit.
//
// SPLIT (the standing S162/S200/S212 precedent): the SCRAPE + COMPARE are PURE +
// headless (consume an already-filled `Impl::ImageBuffer` + an already-arranged
// core tree + a parsed `ScanDeclaration`), so the scanner's decision layer is
// ctest-provable on BOTH CI lanes with NO live Vulkan runtime. The REAL GPU
// render that FILLS the ImageBuffer is the S640a `RenderTarget::render_to`
// [both apps, not CI] lane — the offline scrape/compare here is fed by whatever
// fills the buffer (GPU here, WPF emitter's .bgra there, a synthetic buffer in
// ctest).
//
// METADATA READ (baked): the compare consumes the emitter-deposited metadata
// (quad kinds + tolerance) from the sidecar as a SECONDARY oracle; even if S642
// refines the metadata shape, S641 READS whatever the emitter deposits TODAY —
// the metadata reader is forward-compatible, so a later gap-close never strands
// it. The in-XAML `SCAN DECLARATION` remains the PRIMARY oracle.
//
// COLOR COMPARISON: the sample's declared-location color is compared to the
// ImageBuffer's BGRA pixel via a per-channel tolerance (explicitly also proven,
// never silent). A pixel row/col that falls outside the buffer is a MissingRegion
// datum (not a silent pass).
//
// ZERO LOCKS (ADR-018) + ZERO MINIMAL REAL — permanent ADR-021 surface.

#include <vaml/presentation/scan/ScanDeclaration.h>
#include <vaml/presentation/scan/EmitterMetadata.h>  // S642: catalogue-driven tolerance merge
#include <vaml/presentation/RenderTarget.h> // vaml::presentation::Impl::ImageBuffer

#include <vaml/core/UIElement.h>            // vaml::UIElement (the arranged core tree)

#include <cstdint>
#include <string>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation::scan {

// The outcome of ONE datum check — never a global bit.
enum class DatumStatus
{
    Pass,
    Fail,
    Missing,    // a declared region could not be found / was out of frame
};

// ONE per-datum detail line of the ScanReport.
struct ScanDatum
{
    std::string  quad;    // the named quad this datum belongs to
    std::string  what;    // the datum's identity (e.g. "element-present", "pixel@(x,y)")
    DatumStatus  status   = DatumStatus::Fail;
    double       actual   = 0.0;  // measured
    double       expected = 0.0;  // the declared/expected value (tolerance as raw metric)
    double       tolerance = 0.0; // the per-quad jointolerance actually used
    std::string  detail;         // free-form human/LLM-readable note (never empty on fail)
};

// The scrape + compare RESULT. Holds the per-datum grid + the aggregate.
struct ScanReport
{
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::vector<ScanDatum> data;   // the per-datum PASS/FAIL detail (authoritative)
    int pass_count = 0;
    int fail_count = 0;
    int missing_count = 0;

    [[nodiscard]] bool pass() const noexcept { return fail_count == 0 && missing_count == 0; }
    [[nodiscard]] int total() const noexcept { return pass_count + fail_count + missing_count; }
};

// The SCRAPED actual scan: the element/draw-item grid + the sampled pixels.
// `element_present(name, rect)` records a named element with its arranged
// frame-space rect; `pixel(x, y)` samples the ImageBuffer. `compliance_compare`
// consumes this as `actualScan`.
//
// The scrape itself is what closes the "element presence + coordinates" ask:
// caller drives the arranged core tree walk (vaml::UIElement::walk -> presence +
// final_rect) and the pixel grid (from the Impl::ImageBuffer) into this.
struct ScrapeGrid
{
    // A sampled element's presence + arranged rect (frame space).
    struct ElementSample
    {
        std::string name;
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    };

    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::vector<ElementSample> elements;   // named elements sampled (presence + rect)
    std::vector<std::uint8_t>  bgra;       // tight w*h*4 BGRA pixels (row 0 = top)

    // Samples a BGRA pixel -> (r,g,b,a); returns {0,0,0,0} out of frame.
    [[nodiscard]] std::uint32_t pixel(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] std::uint8_t pixel_r(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] std::uint8_t pixel_g(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] std::uint8_t pixel_b(std::uint32_t x, std::uint32_t y) const noexcept;
};

// Builds a ScanReport by comparing the SCRAPED actual grid to the DECLARED
// expected outcome (the in-XAML ScanDeclaration) + the (secondary,
// forward-compatible) emitter-sidecar metadata. Returns per-datum
// PASS/FAIL/Missing detail. `actual` is the authoritative pixel/element scrape
// source; `expected` the oracle. `sidecarKinds` maps a quad name -> an
// emitter-declared kind (when the declaration omits one) for the tolerance
// merge. PURE + headless (both CI lanes).
VAML_PRESENTATION_EXPORT ScanReport
compliance_compare(const ScrapeGrid& actual,
                   const ScanDeclaration& expected,
                   const std::vector<std::pair<std::string, QuadKind>>& sidecarKinds = {});

// S642 (metadata fine-tune — provisional): the CATALOGUE-DRIVEN compare overload.
// The emitter sidecar (EmitterMetadata) carries the SYNC-TOKEN capability id per
// quad + the catalogue-driven per-quad tolerance; this overload resolves each quad's
// effective tolerance through the capability catalogue (the declared per-quad
// `tolerance` in the SCAN DECLARATION STILL wins; else the sidecar's catalogue
// tolerance; else the built-in kind default). PROVISIONAL until S643's screens prove
// the quads carry the tolerance info the compare needs. PURE + headless.
VAML_PRESENTATION_EXPORT ScanReport
compliance_compare_catalogued(const ScrapeGrid& actual,
                              const ScanDeclaration& expected,
                              const EmitterMetadata& sidecar);

// A per-quad PIXEL-GRID helper used by the compare: counts the non-empty
// (non-transparent / non-blank) pixels in a frame rect, and returns how many
// rows/cols actually carry ink — the scaffold behind the text `auto-locate` +
// tolerance-band and the icon presence + the `full-missing-content` detection.
// PURE + headless.
struct QuadGrid
{
    std::uint32_t sample_x = 0, sample_y = 0, sample_w = 0, sample_h = 0;
    std::uint32_t ink_pixels   = 0;   // non-blank pixels inside the (clamped) rect
    std::uint32_t total_pixels = 0;
    float  centroid_x = -1.0f, centroid_y = -1.0f;  // ink centroid (-1 when no ink)
    bool   has_ink = false;
};
VAML_PRESENTATION_EXPORT QuadGrid
sample_quad(const ScrapeGrid& actual, float x, float y, float w, float h);

} // namespace vaml::presentation::scan
