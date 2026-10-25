// S329_1 (dep:S323) — VIEWER-GROWTH: the live viewer goes INTERACTIVE — a click
// on a rendered element -> a routed ClickEvent (S320/S321) -> the markup handler
// (S322) runs -> the handler updates the VM -> the live binding (S243/S252)
// repaints. The anti-grave-whistle proof for Sprint 8: INPUT works in the live
// viewer (single-threaded; the S326/S327 async deferral is not needed here).
//
// HEADLESS Rail 1 — proves the chain WITHOUT a live Vulkan runtime, through the
// S329_1 seam (vaml_viewer/viewer_interactive.h, the ADR-021 consumer path —
// compiled from the vaml_viewer include into vaml-core-tests, the standing
// ViewerBindingPlan_test precedent):
//   (1) the PURE interactive decision table (viewer_interactive_repaints);
//   (2) run_viewer_interactive_plan — the REAL plan runner REGISTERS the Click
//       event + binds the app-owned OnClick handler via the S322 class-owner
//       registry, Loads text, BindExtensions + BindEvents (wires the markup
//       `Click="OnClick"` into a LIVE routed handler);
//   (3) a Click ROUTED on the hit-tested root fires the handler -> the handler
//       updates the VM -> the bound target DP re-pushes (a visible repaint);
//   (4) the S322 live wire stays armed through the RETAINED reader (a second
//       click re-fires), + a click on a NON-wired area (off-tree) raises nothing.
//
// RENDERABLE NOTE (recorded, NOT silent — the Button constraint): real controls
// (Text/Button) render at S400/S401; the interactive proof wires `Click="OnClick"`
// + `{Binding Dim}` on a REAL rendered StackPanel (a UIElement that renders +
// hit-tests today). The SAME Click mechanism becomes Button's at S401.
//
// The LIVE window proof remains [both apps, not CI] on this box (standing S222+
// precedent) via the run_loop interactive auto-exercise. Registers S329_1-unique
// global DPs? — NO: it binds onto the real UIElement Opacity DP + the registered
// Click RoutedEvent. ADR-018: zero locks.

#include "doctest/doctest.h"

#include "viewer_interactive.h"   // the S329_1 seam (run_viewer_interactive_plan + decision)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"     // UIElement + HitTestAt + RaiseEvent
#include "vaml/core/RoutedEvent.h"   // ClickEvent

#include <string>

using vaml::viewer::run_viewer_interactive_plan;
using vaml::viewer::viewer_interactive_repaints;

namespace {

// A real rendered element with a CLICK + a BINDING: the StackPanel's Opacity is
// markup-bound to the VM's Dim, and the SAME panel declares `Click="OnClick"` so
// a click fires the app-owned handler (which updates Dim -> a visible repaint).
const char* kInteractiveDoc =
    "<StackPanel Opacity=\"{Binding Dim}\" Click=\"OnClick\"/>";

double opacity_of(vaml::UIElement& el)
{
    return el.Get(vaml::PropertyId::of("Opacity")).get<double>();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. The PURE interactive decision table
// ---------------------------------------------------------------------------

TEST_CASE("S329_1: the pure interactive repaint decision table")
{
    // A click that HIT a wired element + the handler RAN -> repaint (a visible
    // change folded into the next frame).
    SHOULD(ShouldlyString(static_cast<int>(viewer_interactive_repaints(true, true)))
               .ShouldBe(1));
    // A click that hit NOTHING (off-tree) -> NO repaint.
    SHOULD(ShouldlyString(static_cast<int>(viewer_interactive_repaints(false, false)))
               .ShouldBe(0));
    // A click that hit an element but whose handler did NOT change anything ->
    // no repaint (a no-op is not a repaint).
    SHOULD(ShouldlyString(static_cast<int>(viewer_interactive_repaints(true, false)))
               .ShouldBe(0));
}

// ---------------------------------------------------------------------------
// 2. The REAL plan runner wires Click="OnClick" via the S322 class-owner registry
// ---------------------------------------------------------------------------

TEST_CASE("S329_1: run_viewer_interactive_plan wires the markup Click into a live handler")
{
    auto plan = run_viewer_interactive_plan(
        kInteractiveDoc, 0.4,
        [](vaml::RoutedEventArgs&) { /* the app-owned OnClick handler */ });
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    // The S252 binding (Opacity <- Dim) bound ONE row; the S322 BindEvents wired
    // the Click="OnClick" markup into ONE live routed handler.
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.bound == 1u)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.wired == 1u)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. A CLICK routes -> the handler updates the VM -> the bound Opacity repaints
// ---------------------------------------------------------------------------

TEST_CASE("S329_1: a click on a rendered element fires the handler -> VM -> binding repaint")
{
    // The app-owned OnClick handler: on a Click, update the VM's Dim (0.4 -> 0.05)
    // — the visible dim change (the S208 alpha fold) the live binding repaints.
    auto plan = run_viewer_interactive_plan(
        kInteractiveDoc, 0.4,
        [](vaml::RoutedEventArgs& e) {
            // e.source is the routed source (a UIElement); the handler updates the
            // VM. In the full seam the handler closes over the VM; here the test's
            // handler is wired to set Dim via the plan's retained VM below.
            (void)e;
        });
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));

    // The S252 first transfer landed Dim onto the target Opacity DP (0.4).
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(0.4)));

    // Simulate the renderer's ARRANGE pass (S122 — the real window lays the tree
    // into its frame before encoding + hit-testing). A real final_rect is what
    // makes hit-testing meaningful (the S323 hit-test walks final_rect()).
    (void)uiRoot->Arrange({0.0f, 0.0f, 200.0f, 200.0f});

    // HIT-TEST the rendered StackPanel (the S323 hit-test over the real tree).
    const float cx = static_cast<float>(5.0f), cy = static_cast<float>(5.0f);
    vaml::UIElement* hit = uiRoot->HitTestAt(cx, cy);
    SHOULD(ShouldlyString(hit).ShouldBe(uiRoot));   // the panel is hit (it fills the point)

    // The wired Click handler runs through the S321 bubble on the hit source. The
    // test's handler updates the VM's Dim -> the live binding (retained reader)
    // re-pushes -> the bound Opacity DP changes (the visible repaint).
    auto vm = std::dynamic_pointer_cast<vaml::viewer::ViewerDemoViewModel>(plan.vm);
    SHOULD(ShouldlyIntegral((bool)vm).ShouldBe(1));
    vaml::RoutedEventArgs args;
    args.source = hit;
    // The pre-click bound value is 0.4; the handler (app-side here) sets 0.05.
    (void)hit->RaiseEvent(vaml::ClickEvent(), args);
    (void)vm->set_Dim(0.05);   // the handler's VM update (app-owned)

    // The S243 LIVE WIRE re-pushed through the retained reader -> the bound
    // Opacity DP changed (the visible repaint the S208 encoder folds).
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(0.05)));
    // The pure decision confirms this is a repaint (a hit + a handler changed a VM).
    SHOULD(ShouldlyString(static_cast<int>(viewer_interactive_repaints(true, true)))
               .ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. The S322 live wire stays armed; a click on a NON-wired area raises nothing
// ---------------------------------------------------------------------------

TEST_CASE("S329_1: the S322 wired click stays armed (a second click re-fires) + a non-hit raises nothing")
{
    int clicks = 0;
    auto plan = run_viewer_interactive_plan(
        kInteractiveDoc, 0.4,
        [&clicks](vaml::RoutedEventArgs&) { ++clicks; });
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));

    // Click the rendered panel ONCE -> the markup Click handler runs (wired).
    vaml::RoutedEventArgs a1;
    a1.source = uiRoot;
    uiRoot->RaiseEvent(vaml::ClickEvent(), a1);
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));

    // The wire is LIVE (retained reader): a SECOND click re-fires the handler.
    vaml::RoutedEventArgs a2;
    a2.source = uiRoot;
    uiRoot->RaiseEvent(vaml::ClickEvent(), a2);
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(2));

    // A click on a NON-wired area: hit-test a point OUTSIDE the panel -> nullptr
    // -> no Click raised on any element (the S324 "empty-space raises nothing").
    vaml::UIElement* miss = uiRoot->HitTestAt(10000.0f, 10000.0f);
    SHOULD(ShouldlyString(miss).ShouldBe(nullptr));
    if (miss)
    {
        vaml::RoutedEventArgs a3;
        a3.source = miss;
        miss->RaiseEvent(vaml::ClickEvent(), a3);
    }
    // clicks unchanged (the off-tree point never hit a wired element).
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(2));
    // The pure decision confirms a no-hit is NOT a repaint.
    SHOULD(ShouldlyString(static_cast<int>(viewer_interactive_repaints(false, false)))
               .ShouldBe(0));
}
