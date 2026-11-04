// S595a (dep:S595) — VIEWER-GROWTH: `IsHitTestVisible` renders the
// ScreenCaptureOverlay-shaped click-pass-through surface (headless Rail 1,
// vaml-presentation-tests — it links vaml-presentation so the S212 encoder +
// the classifier arms are provable). COMPANION to the CORE surface TU
// (ViewerHitTest_s595a_test.cpp, vaml-core-tests): this proves the end-to-end
// viewer-growth path against the REAL surfaces.
//
//   1. LOAD: the ScreenCaptureOverlay-shaped overlay doc loads + finalizes; the
//      DimLayer reads is_hit_test_visible()==false; the capture rectangle is
//      authored near-transparent.
//   2. RENDER-PLAN: the S212 encoder emits the overlay draw items — the overlay
//      STILL RENDERS even though the DimLayer is non-hit-test-visible (the S595a
//      decoupling).
//   3. PASS-THROUGH/CAPTURE toggling: in the IDLE mode a HitTestAt at the
//      capture area returns the UNDERLYING interactive element (pass-through);
//      flipping the capture element to IsHitTestVisible=true (ACTIVE) makes
//      HitTestAt return the overlay element itself (capture/"handled"). The flip
//      is a plain property change in the app's own code (ZERO library policy).

#include "viewer_growth_s595a.h"   // the S595a viewer-growth seam (vaml_viewer, ADR-021)

#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice / default_visual_of
#include <vaml/vulkan/ui/UiScene.h>         // DrawItem
#include <vaml/core/Element.h>              // Window, StackPanel, Rectangle

#include "doctest/doctest.h"

#include <vector>

using namespace vaml::viewer;
using vaml::vulkan::ui::DrawItem;

namespace {

// The authoritative ScreenCaptureOverlay-shaped overlay doc (also on disk as
// sample.s595a.xaml). The capture + dim layers carry explicit Width/Height so
// their layout boxes are REAL (the base UIElement Width/Height DPs — S122);
// the dim layer authors IsHitTestVisible="false" (S595a).
const char* kOverlayDoc = R"(
<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root">
  <StackPanel>
    <StackPanel x:Name="CaptureRect" Width="120" Height="90"/>
    <StackPanel x:Name="DimLayer" Width="120" Height="40" IsHitTestVisible="false"/>
  </StackPanel>
</Window>
)";

} // namespace

TEST_CASE("S595a LOAD: overlay doc loads + finalizes; the DimLayer is not hit-test-visible")
{
    ViewerGrowthS595aPlanResult plan = run_viewer_growth_s595a_plan(kOverlayDoc);
    CAPTURE(plan.detail);
    REQUIRE(plan.ok);
    REQUIRE(plan.root != nullptr);
    REQUIRE(plan.dimLayer != nullptr);
    // The dim layer's authored IsHitTestVisible=false resolved (the S595a surface).
    CHECK(plan.dimLayer->is_hit_test_visible() == false);
    // The reader is retained.
    CHECK(plan.reader != nullptr);
}

TEST_CASE("S595a RENDER-PLAN: the overlay STILL RENDERS (non-hit-test-visible is not hidden)")
{
    ViewerGrowthS595aPlanResult plan = run_viewer_growth_s595a_plan(kOverlayDoc);
    REQUIRE(plan.ok);

    std::vector<DrawItem> out;
    const std::size_t n = viewer_growth_s595a_render_plan(plan.root.get(), out, 320.0f, 240.0f);
    // The Window + StackPanel + CaptureRect + DimLayer all RENDER (Solid fills) —
    // the DimLayer being IsHitTestVisible=false does NOT hide it (the S595a
    // decoupling: renders but does not intercept input). 4+ items.
    CHECK(n >= 4u);
    for (const DrawItem& item : out)
        CHECK(item.w > 0.0f);
}

TEST_CASE("S595a PASS-THROUGH/CAPTURE toggle (idle -> underlying element; active -> the overlay)")
{
    ViewerGrowthS595aPlanResult plan = run_viewer_growth_s595a_plan(kOverlayDoc);
    REQUIRE(plan.ok);
    auto* root = dynamic_cast<vaml::UIElement*>(plan.root.get());
    REQUIRE(root != nullptr);

    // Arrange the tree so final_rects are real (the encode render-plan already
    // arranges; the hit-test uses the same final_rects).
    std::vector<DrawItem> out;
    (void)viewer_growth_s595a_render_plan(plan.root.get(), out, 320.0f, 240.0f);

    // Locate the capture rectangle (the app-side interactable overlay chrome).
    auto* capture = dynamic_cast<vaml::UIElement*>(plan.reader->named("CaptureRect"));
    REQUIRE(capture != nullptr);
    const float cx = capture->final_rect().x + capture->final_rect().width / 2.0f;
    const float cy = capture->final_rect().y + capture->final_rect().height / 2.0f;

    // IDLE: the capture rectangle is IsHitTestVisible=false (the app's idle
    // mode) -> a HitTestAt at its coords SKIPS it and returns the UNDERLYING
    // interactive element (the StackPanel / Window — pass-through).
    (void)capture->set_is_hit_test_visible(false);
    CHECK(capture->is_hit_test_visible() == false);
    vaml::UIElement* idleHit = viewer_growth_s595a_hit(root, cx, cy);
    CHECK(idleHit != capture);   // pass-through (NOT the overlay)

    // ACTIVE: flip the SAME element to is_hit_test_visible=true (a plain app-side
    // property change, ZERO library policy) -> HitTestAt now returns the overlay
    // element itself (capture/"handled").
    (void)capture->set_is_hit_test_visible(true);
    vaml::UIElement* activeHit = viewer_growth_s595a_hit(root, cx, cy);
    CHECK(activeHit == capture); // captured by the overlay
}
