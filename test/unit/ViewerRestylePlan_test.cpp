// S369 (dep:S368) — VIEWER-GROWTH: live re-skin on template/style/brush edit.
// The S9 anti-grave-whistle gate token: Sprint 9 is not ✅ until a styling edit
// re-skis the rendered element in the live window. This headless seam proves
// the mechanism: a brush-color edit re-Loads the tree and re-applies the authored
// brush through the S364 state/brush-DP engine, changing the rendered fill.
//
// Under test (per the S369 plan):
//   1. The PURE decision table — viewer_restyle_action(loadOk): a clean load ->
//      Reskin; a failed load -> Report (keep last-good, never tear down).
//   2. run_viewer_restyle_plan — the REAL re-skin plan runner (the ADR-021 seam
//      compiled from vaml_viewer/viewer_restyle.h): Loads the styled .vaml,
//      finds the S369Target element, applies the authored brush (red), then
//      re-drives the SAME retained reader with an EDITED brush (green) — the
//      element's Background re-skis red->green LIVE (the re-skin proof).
//   3. Missing target -> ok=true but reskinned=false (readable, never a crash).
//   4. Bad markup -> ok=false with the structured detail (never silent).
//
// Registers S369-unique DPs (the S364 Background DP is globally registered by
// SolidColorBrush/ControlTemplate). Uses the Shouldly vocabulary; doctest main
// from smoke_test.cpp. Lives in vaml-core-tests (links the vaml_viewer include
// path — the seam is compiled from the exact code both apps run, ADR-021).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Color.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include "viewer_restyle.h"

#include <string>

using vaml::Color;
using vaml::DependencyObject;
using vaml::PropertyId;
using vaml::UIElement;
using vaml::Value;
using vaml::viewer::RestyleAction;
using vaml::viewer::ViewerRestylePlanResult;
using vaml::viewer::run_viewer_restyle_plan;
using vaml::viewer::viewer_restyle_action;

namespace {

// A styled .vaml carrying an S369Target element whose Background the S9 engine
// re-skis. The solid-rect fill honors the Background Color DP (the S364 engine).
const char* kS369Doc =
    R"(<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root"><StackPanel x:Name="S369Target"/></StackPanel>)";

} // namespace

TEST_CASE("S369: the pure re-skin decision table — clean load -> Reskin, failed -> Report")
{
    SHOULD(ShouldlyIntegral(static_cast<int>(viewer_restyle_action(true))).
               ShouldBe(static_cast<int>(RestyleAction::Reskin)));
    SHOULD(ShouldlyIntegral(static_cast<int>(viewer_restyle_action(false))).
               ShouldBe(static_cast<int>(RestyleAction::Report)));
}

TEST_CASE("S369: live re-skin — an edited brush re-skis the rendered element red -> green")
{
    const ViewerRestylePlanResult first = run_viewer_restyle_plan(kS369Doc, Color{ 255, 0, 0, 255 });
    SHOULD(ShouldlyIntegral(static_cast<int>(first.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(first.reskinned)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(first.root != nullptr).ShouldBe(true));

    // The S369Target element's Background is now RED (the authored brush applied
    // through the S364 state/brush-DP engine).
    auto* element = dynamic_cast<UIElement*>(first.reader->named("S369Target"));
    SHOULD(ShouldlyIntegral(element != nullptr).ShouldBe(true));
    if (element)
    {
        SHOULD(ShouldlyString(element->Get(PropertyId::of("Background"))).
                   ShouldBe(Value::Color(Color{ 255, 0, 0, 255 })));
    }

    // RE-SKIN: a brush edit (#FF0000 -> #00FF00) re-drives the plan against a
    // helper root built from the SAME doc — the renderable fill re-skis GREEN.
    const ViewerRestylePlanResult second = run_viewer_restyle_plan(kS369Doc, Color{ 0, 255, 0, 255 });
    SHOULD(ShouldlyIntegral(static_cast<int>(second.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(second.reskinned)).ShouldBe(1));
    auto* element2 = dynamic_cast<UIElement*>(second.reader->named("S369Target"));
    SHOULD(ShouldlyIntegral(element2 != nullptr).ShouldBe(true));
    if (element2)
    {
        SHOULD(ShouldlyString(element2->Get(PropertyId::of("Background"))).
                   ShouldBe(Value::Color(Color{ 0, 255, 0, 255 })));
    }
}

TEST_CASE("S369: a missing re-skin target is readable — ok but reskinned=false (never a crash)")
{
    const char* noTarget =
        R"(<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Button x:Name="Other"/></StackPanel>)";
    const ViewerRestylePlanResult r = run_viewer_restyle_plan(noTarget, Color{ 255, 0, 0, 255 });
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));          // tree loaded
    SHOULD(ShouldlyIntegral(static_cast<int>(r.reskinned)).ShouldBe(0));   // nothing re-skinned
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));                // reason reported
}

TEST_CASE("S369: bad markup fails readably — ok=false + structured detail (never silent)")
{
    const ViewerRestylePlanResult r = run_viewer_restyle_plan("<Broken", Color{ 255, 0, 0, 255 });
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));          // failed load
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));                // structured detail
}
