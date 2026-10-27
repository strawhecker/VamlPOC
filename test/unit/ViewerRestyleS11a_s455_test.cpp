// S455 (dep:S454) — VIEWER-GROWTH(11a): the RESOURCE/STYLE authoring grammar
// (S453 + S454) driving a LIVE re-skin. The S369 "you see live re-skin" seam
// (vaml_viewer/viewer_restyle.h) is extended to re-drive the headline through
// the AUTHORED style grammar: a styled .vaml that authors a ResourceDictionary +
// an implicit Style (with authored <Setter>s) is re-Loaded and the implicit style
// is RE-APPLIED by TARGET TYPE (apply_implicit_style), re-skining the live target.
//
// Under test:
//   1. run_viewer_style_restyle_plan on a styled doc (authored dict + implicit
//      Style) -> ok=true, reskinned=true, the authored Background SETTER applied
//      onto the named target (the re-skin proof, driven from the fresh grammar).
//   2. Editing the authored Background color + re-Load re-skis to the new color.
//   3. A doc with NO authored implicit style -> ok but reskinned=false (an
//      explicit-style/absent-style target is NOT re-skinned — never silent).
//   4. Missing target -> ok=true but reskinned=false (readable, never a crash).
//   5. Bad markup -> ok=false + structured detail (never silent).
//
// Registers S455-unique DPs (Background is globally registered by the brush/
// template engine). Uses the Shouldly vocabulary; doctest main from smoke_test.
// Lives in vaml-tools-tests (compiles the REAL viewer_restyle.h seam, ADR-021).

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
using vaml::PropertyId;
using vaml::UIElement;
using vaml::Value;
using vaml::viewer::ViewerStyleRestylePlanResult;
using vaml::viewer::run_viewer_style_restyle_plan;

namespace {

// A styled .vaml authored with the S453/S454 grammar: a ResourceDictionary +
// an IMPLICIT <Style TargetType="StackPanel"> whose authored <Setter> re-skis
// the target's Background by target type. The root StackPanel is the re-skin
// target (it owns the authored dict, so apply_implicit_style resolves it).
const std::string kStyleDoc =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"S369StyleTarget\">"
    "<StackPanel.Resources>"
    "<Style TargetType=\"StackPanel\">"
    "<Setter Property=\"Background\" Value=\"#FF0000\"/>"
    "</Style>"
    "</StackPanel.Resources>"
    "</StackPanel>";

// The same styled doc with an EDITED authored Background (red -> green) — the
// external-edit-then-reload that re-skis the live window.
const std::string kStyleDocGreen =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"S369StyleTarget\">"
    "<StackPanel.Resources>"
    "<Style TargetType=\"StackPanel\">"
    "<Setter Property=\"Background\" Value=\"#00FF00\"/>"
    "</Style>"
    "</StackPanel.Resources>"
    "</StackPanel>";

// A doc with NO implicit style (only an explicitly-keyed style — which must NOT
// auto-apply). The target owns the dict but the authored style is explicit.
const std::string kNoImplicitDoc =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"S369StyleTarget\">"
    "<StackPanel.Resources>"
    "<Style x:Key=\"Explicit\" TargetType=\"StackPanel\">"
    "<Setter Property=\"Background\" Value=\"#FF0000\"/>"
    "</Style>"
    "</StackPanel.Resources>"
    "</StackPanel>";

} // namespace

TEST_CASE("S455: an authored implicit Style re-skis the live target by target type")
{
    const ViewerStyleRestylePlanResult r = run_viewer_style_restyle_plan(kStyleDoc);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.reskinned)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.applied > 0u)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr).ShouldBe(true));

    // The authored <Setter Property="Background" Value="#FF0000"/> landed on the
    // target (re-skin through the fresh style-authoring grammar).
    auto* element = dynamic_cast<UIElement*>(r.reader->named("S369StyleTarget"));
    SHOULD(ShouldlyIntegral(element != nullptr).ShouldBe(true));
    if (element)
    {
        SHOULD(ShouldlyString(element->Get(PropertyId::of("Background")))
                   .ShouldBe(Value::Color(Color{ 255, 0, 0, 255 })));
    }
}

TEST_CASE("S455: an authored-Background edit + re-Load re-skis the target red -> green")
{
    const ViewerStyleRestylePlanResult first = run_viewer_style_restyle_plan(kStyleDoc);
    auto* e1 = dynamic_cast<UIElement*>(first.reader->named("S369StyleTarget"));
    SHOULD(ShouldlyIntegral(e1 != nullptr).ShouldBe(true));
    if (e1)
    {
        SHOULD(ShouldlyString(e1->Get(PropertyId::of("Background")))
                   .ShouldBe(Value::Color(Color{ 255, 0, 0, 255 })));
    }

    // The edited authored style (#FF0000 -> #00FF00) re-Drives the same plan.
    const ViewerStyleRestylePlanResult second = run_viewer_style_restyle_plan(kStyleDocGreen);
    SHOULD(ShouldlyIntegral(static_cast<int>(second.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(second.reskinned)).ShouldBe(1));
    auto* e2 = dynamic_cast<UIElement*>(second.reader->named("S369StyleTarget"));
    SHOULD(ShouldlyIntegral(e2 != nullptr).ShouldBe(true));
    if (e2)
    {
        SHOULD(ShouldlyString(e2->Get(PropertyId::of("Background")))
                   .ShouldBe(Value::Color(Color{ 0, 255, 0, 255 })));
    }
}

TEST_CASE("S455: a doc with only an EXPLICIT style does NOT auto-re-skin (implicit only, never silent)")
{
    const ViewerStyleRestylePlanResult r = run_viewer_style_restyle_plan(kNoImplicitDoc);
    // The document LOADED (ok=true) but the explicit style must NOT auto-apply:
    // reskinned=false, applied=0. (explicit-vs-implicit on the re-skin seam — an
    // explicitly-keyed style re-skis only via an explicit apply_style, never on
    // the implicit reload path.)
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.reskinned)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.applied)).ShouldBe(0u));
}

TEST_CASE("S455: a missing re-skin target is readable — ok but reskinned=false (never a crash)")
{
    const std::string noTarget =
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style TargetType=\"StackPanel\"><Setter Property=\"Background\" Value=\"#FF0000\"/></Style>"
        "</StackPanel.Resources>"
        "</StackPanel>";
    const ViewerStyleRestylePlanResult r = run_viewer_style_restyle_plan(noTarget);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));        // tree loaded
    SHOULD(ShouldlyIntegral(static_cast<int>(r.reskinned)).ShouldBe(0)); // nothing re-skinned
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));              // reason reported
}

TEST_CASE("S455: bad markup fails readably — ok=false + structured detail (never silent)")
{
    const ViewerStyleRestylePlanResult r = run_viewer_style_restyle_plan("<Broken");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));        // failed load
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));              // structured detail
}
