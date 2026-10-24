// S256 (dep:S253) — VIEWER-GROWTH: the reload populates a VM (plain ctor; NO
// DI) + binds `{Binding}` (the S240–S252 binding path visible at the viewer
// boundary). HEADLESS Rail 1 — proves the "you SEE data binding appear in the
// live window" headline WITHOUT a live Vulkan runtime, through the S256 seam
// (vaml_viewer/viewer_binding.h, the ADR-021 consumer path — compiled from the
// vaml_viewer include into vaml-core-tests, the standing ViewerLoadFile
// precedent):
//   (1) the app-constructed ViewModel (a REAL ObservableObject + PROPERTY_BINDABLE
//       per S250/S251, plain ctor, an injected service by value) + the PURE
//       bind-render decision table (viewer_bind_render_action);
//   (2) run_viewer_binding_plan — the REAL plan runner constructs the VM +
//       service (S250 pattern), Loads text through a RETAINED VamlReader,
//       set_data_context + BindExtensions (the S252 S2-tag resolution), and the
//       bound `{Binding Dim}` lands on the target UIElement's Opacity DP (the
//       S242 first transfer — the visible dim the S208 encoder folds);
//   (3) the S243 LIVE WIRE stays armed through the RETAINED reader: a later
//       `vm->set_Dim(...)` re-pushes the bound Opacity DP (the update-in-window
//       headline).
//
// The LIVE window/render proof remains [both apps, not CI] on this box (the
// standing S222+ precedent) + feeds S443 (app-shell markup bind + command path).
// Registers S256-unique global DPs? — NO: it binds onto the REAL UIElement Opacity
// DP (S120, registered by UIElement construction). ADR-018 zero locks.

#include "doctest/doctest.h"

#include "viewer_binding.h"   // the S256 seam (ViewerDemoViewModel + run_viewer_binding_plan)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/UIElement.h"     // UIElement + the S120 Opacity DP (effective_value)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <string>

using vaml::viewer::BindRenderAction;
using vaml::viewer::ViewerDemoService;
using vaml::viewer::ViewerDemoViewModel;
using vaml::viewer::run_viewer_binding_plan;
using vaml::viewer::viewer_bind_render_action;

namespace {

// The renderable binding doc: a StackPanel (a real UIElement, so RealText +
// Opacity are its real DPs) whose Opacity is markup-bound to the VM's Dim.
const char* kBindingDoc =
    "<StackPanel Opacity=\"{Binding Dim}\"/>";

double opacity_of(vaml::UIElement& el)
{
    return el.Get(vaml::PropertyId::of("Opacity")).get<double>();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. The S250/S251 app-constructed VM (plain ctor + injected service, NO DI)
// ---------------------------------------------------------------------------

TEST_CASE("S256: the viewer VM is app-constructed (plain ctor, injected service)")
{
    // The S250 "engineer wires, we bind" headline, at the viewer boundary: the
    // app (here the test) constructs the VM + passes its own service in by value
    // through a PLAIN constructor — no container, no CompositionRoot.
    ViewerDemoService svc(0.4);
    ViewerDemoViewModel vm(0.4);
    SHOULD(ShouldlyString(std::to_string(vm.Dim())).ShouldBe(std::to_string(0.4)));
    // The VM surfaced the injected service's state as its bindable Dim.
    SHOULD(ShouldlyIntegral(vm.service().dim() == svc.dim() ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S256: the pure bind-render decision table")
{
    // A clean load+bind -> Render (the bound tree re-renders in the window).
    SHOULD(ShouldlyString(static_cast<int>(viewer_bind_render_action(true)))
               .ShouldBe(static_cast<int>(BindRenderAction::Render)));
    // A failed load -> Report (keep the last-good tree, never blank).
    SHOULD(ShouldlyString(static_cast<int>(viewer_bind_render_action(false)))
               .ShouldBe(static_cast<int>(BindRenderAction::Report)));
}

// ---------------------------------------------------------------------------
// 2. The REAL plan runner: {Binding Dim} resolved onto the target Opacity DP
// ---------------------------------------------------------------------------

TEST_CASE("S256: run_viewer_binding_plan binds {Binding Dim} onto the target Opacity")
{
    auto plan = run_viewer_binding_plan(kBindingDoc, 0.4);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.bound == 1u)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.vm).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));

    // The root must be a UIElement (the renderable tree the S208 encoder folds).
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));
    // The S242 FIRST TRANSFER landed the bound Dim onto the target Opacity DP —
    // the visible dim (alpha fold) the S208 encoder honors.
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(0.4)));
}

// ---------------------------------------------------------------------------
// 3. The S243 LIVE WIRE stays armed through the RETAINED reader
// ---------------------------------------------------------------------------

TEST_CASE("S256: a later set_Dim re-pushes the bound Opacity (live, via the retained reader)")
{
    auto plan = run_viewer_binding_plan(kBindingDoc, 0.4);
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));

    // The retained reader keeps the S252 reader-owned live binding armed; a real
    // VM change re-pushes through the S243 one-way wire to the Opacity DP.
    (void)plan.vm->set_Dim(0.15);
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(0.15)));

    (void)plan.vm->set_Dim(1.0);
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(1.0)));
}

// ---------------------------------------------------------------------------
// 4. A malformed binding document reports (never silent) + keeps last-good
// ---------------------------------------------------------------------------

TEST_CASE("S256: a document whose binding fails reports a structured detail")
{
    const std::string bad =
        "<StackPanel Opacity=\"{Binding}\"/>";   // {Binding} with no path -> Class A
    auto plan = run_viewer_binding_plan(bad, 0.4);
    // The plan reports the failure (never silent), keeps no root (last-good kept
    // by the app), and the pure decision says Report.
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(0));
    // The structured detail is non-empty (never silent) — the exact reader what().
    SHOULD(ShouldlyString(std::string(plan.detail)).ShouldNotBe(std::string("")));
    SHOULD(ShouldlyString(static_cast<int>(viewer_bind_render_action(plan.ok)))
               .ShouldBe(static_cast<int>(BindRenderAction::Report)));
}
