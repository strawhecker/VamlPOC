// S640 (dep:S622) — IMAGE + SOURCE — the driver's explicit FIRST step for
// Sprint 11r. Headless proof (Rail 1, vaml-vulkan-tests) of the Image
// control's render seam (vaml::vulkan::icon::ImageGlyphPlan) + the BARE Source
// DP surface. Mirror of IconGlyphPlan_test (S400_1), specialized for Image.
//
// Under test (NO live device — build_image_draw_item is PURE over the
// caller-supplied candidate family + hasSlot, the S213_1/IconAtlas convention):
//
//   (A) BARE SOURCE DP SURFACE (core): the Image control's BARE `Source` DP
//       round-trips via set_source/source() (the S640 headline — pre-S640 only
//       the dotted "Image.Source" was registered, so `Source="..."` authored
//       markup threw `unknown property 'Source'`; the reader resolves a bare
//       attribute key by its BARE name). Image is a real UIElement + a
//       registered built-in + IconKey/Stretch ride the S400_1 machinery.
//   (B) RENDER SEAM (ImageGlyphPlan, pure): an unresolved/missing asset ->
//       the DELIBERATE fallback glyph + a STRUCTURED diagnostic (NEVER silent,
//       NEVER a crash); a resolved populated slot -> the full-color image quad
//       via the S213_1 best-fit (opaque white tint = the asset's own RGB, the
//       Preserve path — no Monochrome tint for Image); a degenerate logical box
//       -> fallback (no crash).
//   (C) BEST-FIT: the S213_1 select_best_fit + physical_n_for drive the image
//       side exactly as they drive Icon (the shared pipeline, ADR-015).
//
// The Image→atlas→quad ACTUALLY DRAWN on the rendered surface is the headless
// seam's quad (build_image_draw_item returns the positioned full-color quad +
// the chosen slot); the DEVICE IconAtlas upload + the S209 texture-binding of
// the atlas descriptor set + the REAL last_framebuffer_scale() sampling are the
// live [s640-live] path (both apps, not CI) — recorded NO-GAP, never silent.
// The S641 scanner then confirms the image's rect + non-default pixels on the
// rendered surface.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>                    // Image, IconStretch, register_builtin_elements
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>
#include <vaml/core/ElementFactoryRegistry.h>     // find "Image" (built-in)
#include <vaml/core/BindingDiagnostics.h>         // the S250_1 structured-diagnostic channel
#include <vaml/vulkan/icon/ImageGlyphPlan.h>      // build_image_draw_item / ImageGlyphInput
#include <vaml/vulkan/icon/IconBestFit.h>         // IconBestFitCache / select_best_fit / physical_n_for / IconCandidate
#include <vaml/vulkan/ui/UiScene.h>               // DrawItem

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using vaml::Color;
using vaml::Image;
using vaml::IconStretch;
using vaml::Rect;

namespace {

// A toy S202 exact-grid layout: a 128x128 atlas in 4x4 -> 16 tiles of 32x32.
const vaml::vulkan::quad::AtlasLayout kLayout =
    vaml::vulkan::quad::atlas_layout(128, 128, 4, 4);

// The S213_1 size family for a resolved image (all square, the S202_1 icon
// contract — Image rides the S400_1/S202_1 icon-asset pipeline).
std::vector<vaml::vulkan::icon::IconCandidate> toyFamily()
{
    return {
        { 1, 16, 16, 256 },   // tile 1, 16x16
        { 2, 32, 32, 1024 },  // tile 2, 32x32
        { 3, 48, 48, 2304 },  // tile 3, 48x48
    };
}

// A capturing diagnostic sink proving emission (ADR-015 NEVER silent).
struct CaptureSink : vaml::IBindingDiagnostics
{
    std::size_t emitted = 0;
    std::string lastDetail;
    void emit(const vaml::BindingDiagnostic& d) override
    {
        ++emitted;
        lastDetail = d.detail;
    }
};

// Scoped sink install/restore (teardown hygiene — the S9 cleanup guard).
struct SinkGuard
{
    CaptureSink sink;
    SinkGuard() { (void)vaml::set_binding_diagnostics(sink); }
    ~SinkGuard() { (void)vaml::set_binding_diagnostics(vaml::default_binding_diagnostics()); }
};

} // namespace

// ---------------------------------------------------------------------------
// (A) BARE Source DP surface (core — the S640 headline)
// ---------------------------------------------------------------------------

TEST_CASE("S640: Image is a real UIElement + BARE Source DP round-trips + registered built-in")
{
    vaml::register_builtin_elements();
    Image im;
    (void)dynamic_cast<vaml::UIElement*>(&im); // re-based onto UIElement

    // DP defaults: bare Source "" (the S640 registration), IconKey "", Stretch None.
    SHOULD(ShouldlyString(im.source()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(im.icon_key()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(im.stretch()).ShouldBe(static_cast<std::int32_t>(IconStretch::None)));

    // The S640 headline: the BARE `Source` name round-trips (this is what
    // authored `Source="..."` hits — the reader resolves a bare attribute key by
    // its BARE name; pre-S640 the dotted "Image.Source" was the only registration).
    SHOULD(ShouldlyString(im.set_source("face-blowing-a-kiss-64.svg")).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(im.source()).ShouldBe(std::string("face-blowing-a-kiss-64.svg")));
    SHOULD(ShouldlyString(im.set_icon_key("bliss")).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(im.icon_key()).ShouldBe(std::string("bliss")));
    SHOULD(ShouldlyString(im.set_stretch(static_cast<std::int32_t>(IconStretch::Uniform))).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(im.stretch()).ShouldBe(static_cast<std::int32_t>(IconStretch::Uniform)));

    // Registered as a real built-in element (the factory resolves "Image").
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("Image")).ShouldBe(true));
    auto produced = g.try_create("Image");
    SHOULD(ShouldlyIntegral(produced != nullptr).ShouldBe(true));
    if (produced)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Image*>(produced.get()) != nullptr).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// (B) ImageGlyphPlan: fallback + diagnostic; resolved full-color quad
// ---------------------------------------------------------------------------

TEST_CASE("S640: build_image_draw_item — unresolved asset -> fallback glyph + structured diagnostic")
{
    SinkGuard g; // captures the S250_1 diagnostic channel

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::ImageGlyphInput in;
    in.logicalBox = Rect{ 0.0f, 0.0f, 20.0f, 20.0f };
    in.source     = "missing.svg";
    in.assetResolved = false; // unresolved

    const auto r = vaml::vulkan::icon::build_image_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout);

    // The fallback outcome + an EMITTED structured diagnostic (NEVER silent).
    SHOULD(ShouldlyString(r.ok).ShouldBe(false));
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(1u));
    // The diagnostic detail names the unresolved image source.
    SHOULD(ShouldlyString(g.sink.lastDetail.find("unresolved") != std::string::npos).ShouldBe(true));
    // The fallback glyph occupies the image's logical box.
    SHOULD(ShouldlyString(r.item.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.item.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.item.w).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.item.h).ShouldBe(20.0f));
}

TEST_CASE("S640: build_image_draw_item — resolved asset -> the full-color image quad via the S213_1 best-fit")
{
    SinkGuard g; // proves NO diagnostic on the resolved path (emission only on failure)

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::ImageGlyphInput in;
    in.logicalBox    = Rect{ 4.0f, 6.0f, 24.0f, 24.0f }; // side 24 -> N = 24 (dpiScale 1)
    in.source        = "face-blowing-a-kiss-36.svg";
    in.assetResolved = true;
    in.assetKey      = 0;
    in.dpiScale      = 1.0f;

    const auto r = vaml::vulkan::icon::build_image_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout);

    // A resolved quad (NOT the fallback) at the logical box, NO diagnostic.
    SHOULD(ShouldlyString(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(0u));
    // The full-color Path: an opaque white tint (1,1,1,1) so the asset's own RGB
    // shows through — Image has no Monochrome tint (the S400_1 Preserve path).
    SHOULD(ShouldlyString(r.item.x).ShouldBe(4.0f));
    SHOULD(ShouldlyString(r.item.y).ShouldBe(6.0f));
    SHOULD(ShouldlyString(r.item.w).ShouldBe(24.0f));
    SHOULD(ShouldlyString(r.item.h).ShouldBe(24.0f));
    SHOULD(ShouldlyString(r.item.r).ShouldBe(1.0f));
    SHOULD(ShouldlyString(r.item.g).ShouldBe(1.0f));
    SHOULD(ShouldlyString(r.item.b).ShouldBe(1.0f));
    // side 24 -> best-fit selects the largest ≤ 24: the 16x16 (index 0 of the
    // family), so the chosen tile = 1. The slot reports it.
    SHOULD(ShouldlyIntegral(r.slotTile).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(r.targetN).ShouldBe(16u));
}

TEST_CASE("S640: build_image_draw_item — degenerate logical box -> fallback (no crash)")
{
    SinkGuard g;

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::ImageGlyphInput in;
    in.logicalBox    = Rect{ 0.0f, 0.0f, 0.0f, 0.0f }; // zero box
    in.assetResolved = true;

    const auto r = vaml::vulkan::icon::build_image_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout);
    // A zero box -> fallback + diagnostic, never a crash.
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(true));
    SHOULD(ShouldlyString(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// (C) S213_1 best-fit select + physical_n_for drive the image side
// ---------------------------------------------------------------------------

TEST_CASE("S640: S213_1 best-fit select + physical_n_for drive the image side")
{
    // The S213_1 best-fit decision is the image's render side computation:
    // physical N = logical side x dpiScale; select picks the largest set ≤ N.
    // Image rides the SAME pipeline as Icon (ADR-015 — one quad engine).
    const std::uint32_t n = vaml::vulkan::icon::physical_n_for(20.0f, 2.0f);
    SHOULD(ShouldlyIntegral(n).ShouldBe(40u));

    std::vector<vaml::vulkan::icon::IconCandidate> fam;
    fam.push_back({ 1, 16, 16, 256 });  // tile 1, 16x16
    fam.push_back({ 2, 32, 32, 1024 }); // tile 2, 32x32
    fam.push_back({ 3, 48, 48, 2304 }); // tile 3, 48x48

    // N=40 -> the largest candidate ≤ 40 is the 32x32 (index 1).
    const auto best = vaml::vulkan::icon::select_best_fit(
        vaml::vulkan::icon::ScalingPolicy::None, fam, 40u);
    SHOULD(ShouldlyIntegral(best.has_value()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(best->candidateIndex).ShouldBe(1));
    // targetN = the chosen candidate's side (32) under None (no resample).
    SHOULD(ShouldlyIntegral(best->resampleToN).ShouldBe(false));
    SHOULD(ShouldlyIntegral(best->targetN).ShouldBe(32u));
}
