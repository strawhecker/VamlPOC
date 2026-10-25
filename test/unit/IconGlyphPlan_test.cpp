// S400_1 (dep:S400) — Icon control DP surface + the render seam (vaml::core +
// vaml::vulkan::icon). Headless proof of:
//
//   (A) ICON CONTROL DP SURFACE (core — the S400_1 title):
//       Source/IconKey/TintMode/Foreground/Width/Height/Stretch DPs round-trip,
//       Width/Height validated >= 0, the control is a real UIElement (re-based),
//       registered as a built-in element.
//   (B) TINT MODE -> RENDER PATH: icon_tint_to_path(Preserve) -> PreserveQuad,
//       Monochrome -> Monochrome (the S400_1 title mapping, NO-GAP in S205_1).
//   (C) BEST-FIT + QUAD (IconGlyphPlan, pure): an unresolved/missing asset ->
//       the DELIBERATE fallback glyph + a STRUCTURED diagnostic (NEVER silent,
//       NEVER a crash); a resolved populated slot -> the icon quad via the
//       S213_1 best-fit; the Monochrome path carries the resolved tint; the
//       S213_1 select_best_fit + physical_n_for drive the icon side.
//
// This target links vaml-vulkan (PUBLIC-links vaml-core), so both the Icon
// control + the IconGlyphPlan are reachable. build_icon_draw_item is PURE over
// the caller-supplied candidate family + hasSlot (the S213_1/IconAtlas
// convention the live [s400_1-live] path feeds from a real device atlas), so
// no GPU TextureAtlas/IconAtlas is constructed here.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>                    // Icon, IconTintMode, IconStretch, register_builtin_elements
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>
#include <vaml/core/ElementFactoryRegistry.h>     // find "Icon" (built-in)
#include <vaml/core/BindingDiagnostics.h>         // the S250_1 structured-diagnostic channel
#include <vaml/vulkan/icon/IconGlyphPlan.h>       // build_icon_draw_item / icon_tint_to_path / IconGlyphInput
#include <vaml/vulkan/icon/IconBestFit.h>         // IconBestFitCache / select_best_fit / physical_n_for / IconCandidate
#include <vaml/vulkan/ui/UiScene.h>               // DrawItem

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vml = vaml;
using vaml::Color;
using vaml::Icon;
using vaml::IconStretch;
using vaml::IconTintMode;
using vaml::Rect;

namespace {

// A toy S202 exact-grid layout: a 128x128 atlas in 4x4 -> 16 tiles of 32x32.
const vaml::vulkan::quad::AtlasLayout kLayout =
    vaml::vulkan::quad::atlas_layout(128, 128, 4, 4);

// The S213_1 size family for a resolved icon (all square, the S202_1 contract).
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
// (A) Icon DP surface (core)
// ---------------------------------------------------------------------------

TEST_CASE("S400_1: Icon is a real UIElement + DP defaults + registered built-in")
{
    vaml::register_builtin_elements();
    Icon ic;
    (void)dynamic_cast<vaml::UIElement*>(&ic); // re-based onto UIElement

    // DP defaults.
    SHOULD(ShouldlyString(ic.icon_key()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(ic.source()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(ic.tint_mode()).ShouldBe(static_cast<std::int32_t>(IconTintMode::Preserve)));
    SHOULD(ShouldlyString(ic.foreground()).ShouldBe(0));
    SHOULD(ShouldlyString(ic.width()).ShouldBe(0.0));
    SHOULD(ShouldlyString(ic.height()).ShouldBe(0.0));
    SHOULD(ShouldlyString(ic.stretch()).ShouldBe(static_cast<std::int32_t>(IconStretch::None)));

    // Round-trip each DP.
    SHOULD(ShouldlyString(ic.set_icon_key("save")).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.icon_key()).ShouldBe(std::string("save")));
    SHOULD(ShouldlyString(ic.set_source("inline-asset")).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.source()).ShouldBe(std::string("inline-asset")));
    SHOULD(ShouldlyString(ic.set_tint_mode(static_cast<std::int32_t>(IconTintMode::Monochrome))).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.tint_mode()).ShouldBe(static_cast<std::int32_t>(IconTintMode::Monochrome)));
    SHOULD(ShouldlyString(ic.set_foreground(9)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.foreground()).ShouldBe(9));
    SHOULD(ShouldlyString(ic.set_width(48.0)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.width()).ShouldBe(48.0));
    SHOULD(ShouldlyString(ic.set_height(32.0)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.height()).ShouldBe(32.0));
    SHOULD(ShouldlyString(ic.set_stretch(static_cast<std::int32_t>(IconStretch::Uniform))).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(ic.stretch()).ShouldBe(static_cast<std::int32_t>(IconStretch::Uniform)));

    // Registered as a real built-in element (the factory resolves "Icon").
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("Icon")).ShouldBe(true));
    auto produced = g.try_create("Icon");
    SHOULD(ShouldlyIntegral(produced != nullptr).ShouldBe(true));
    if (produced)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Icon*>(produced.get()) != nullptr).ShouldBe(true));
    }
}

TEST_CASE("S400_1: Icon Width/Height validated >= 0 (negative REJECTED)")
{
    Icon ic;
    SHOULD(ShouldlyString(ic.set_width(-5.0)).ShouldBe(vaml::SetResult::REJECTED));
    SHOULD(ShouldlyString(ic.set_height(-3.0)).ShouldBe(vaml::SetResult::REJECTED));
    // Unchanged (default 0 = auto).
    SHOULD(ShouldlyString(ic.width()).ShouldBe(0.0));
    SHOULD(ShouldlyString(ic.height()).ShouldBe(0.0));
}

// ---------------------------------------------------------------------------
// (B) TintMode -> render path
// ---------------------------------------------------------------------------

TEST_CASE("S400_1: icon_tint_to_path maps TintMode to the render path")
{
    SHOULD(ShouldlyIntegral(vaml::vulkan::icon::icon_tint_to_path(IconTintMode::Preserve) ==
                            vaml::vulkan::icon::IconRenderPath::PreserveQuad).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::icon::icon_tint_to_path(IconTintMode::Monochrome) ==
                            vaml::vulkan::icon::IconRenderPath::Monochrome).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (C) IconGlyphPlan: fallback + diagnostic; resolved best-fit quad
// ---------------------------------------------------------------------------

TEST_CASE("S400_1: build_icon_draw_item — unresolved asset -> fallback glyph + structured diagnostic")
{
    SinkGuard g; // captures the S250_1 diagnostic channel

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::IconGlyphInput in;
    in.logicalBox = Rect{ 0.0f, 0.0f, 20.0f, 20.0f };
    in.assetResolved = false; // unresolved

    const auto r = vaml::vulkan::icon::build_icon_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout, Color{ 0, 0, 0, 255 });

    // The fallback outcome + a EMITTED structured diagnostic (NEVER silent).
    SHOULD(ShouldlyString(r.ok).ShouldBe(false));
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(1u));
    // The diagnostic detail names the unresolved asset.
    SHOULD(ShouldlyString(g.sink.lastDetail.find("unresolved") != std::string::npos).ShouldBe(true));
    // The fallback glyph occupies the icon's logical box.
    SHOULD(ShouldlyString(r.item.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.item.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.item.w).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.item.h).ShouldBe(20.0f));
}

TEST_CASE("S400_1: build_icon_draw_item — resolved asset -> the icon quad via the S213_1 best-fit")
{
    SinkGuard g; // proves NO diagnostic on the resolved path (emission only on failure)

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::IconGlyphInput in;
    in.logicalBox = Rect{ 4.0f, 6.0f, 24.0f, 24.0f }; // side 24 -> N = 24 (dpiScale 1)
    in.assetResolved = true;
    in.assetKey      = 0;
    in.dpiScale      = 1.0f;
    in.tint          = IconTintMode::Monochrome;
    in.foregroundBrushId = 5;

    const auto r = vaml::vulkan::icon::build_icon_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout, Color{ 200, 30, 10, 255 });

    // A resolved quad (NOT the fallback) at the logical box, NO diagnostic.
    SHOULD(ShouldlyString(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(0u));
    // The Monochrome path carries the resolved tint (R=200/255 -> ~0.784).
    SHOULD(ShouldlyString(r.item.x).ShouldBe(4.0f));
    SHOULD(ShouldlyString(r.item.y).ShouldBe(6.0f));
    SHOULD(ShouldlyString(r.item.w).ShouldBe(24.0f));
    SHOULD(ShouldlyString(r.item.h).ShouldBe(24.0f));
    // side 24 -> best-fit selects the 32x32 candidate? N=24 -> largest ≤ 24 is the
    // 16x16 (index 0 of the family), so the chosen tile = 1. The slot reports it.
    SHOULD(ShouldlyIntegral(r.slotTile).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(r.targetN).ShouldBe(16u));
    // The tint: R 200/255 ≈ 0.784.
    SHOULD(ShouldlyString(r.item.r).ShouldBe(200.0f / 255.0f));
}

TEST_CASE("S400_1: build_icon_draw_item — degenerate logical box -> fallback (no crash)")
{
    SinkGuard g;

    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::IconGlyphInput in;
    in.logicalBox = Rect{ 0.0f, 0.0f, 0.0f, 0.0f }; // zero box
    in.assetResolved = true;

    const auto r = vaml::vulkan::icon::build_icon_draw_item(
        in, toyFamily(), /*hasSlot=*/true, cache, kLayout, Color{ 0, 0, 0, 255 });
    // A zero box -> fallback + diagnostic, never a crash.
    SHOULD(ShouldlyString(r.isFallback).ShouldBe(true));
    SHOULD(ShouldlyString(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.sink.emitted).ShouldBe(1u));
}

TEST_CASE("S400_1: S213_1 best-fit select + physical_n_for drive the icon side")
{
    // The S213_1 best-fit decision is the icon's render side computation:
    // physical N = logical side × dpiScale; select picks the largest set ≤ N.
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
