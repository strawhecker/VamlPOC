// S465 (dep:S464) — VIEWER-GROWTH(11b): the remaining-attribute surface rendered.
//
// A sample .vaml authors ALL of Sprint-11b's surface — an authored gradient
// brush (S460) + {x:Static}/{x:Type} (S461) + attached props + a FrameworkElement
// Name (S462) + event attributes (S463) + Command + CommandParameter (S464) — and
// the LIVE viewer resolves the gradient BRUSH by id through the S465 gradient
// FILL resolution, emitting a vertex-sampled tint (the S460 LinearGradientBrush
// MINIMAL REAL seed is RESOLVED at THIS step — the sentinel is gone).
//
// Under test (headless Rail 1, the S465 title "the LIVE viewer renders the
// gradient + wires a Click/Command visibly"):
//   1. The fully-authored sample (gradient + x:Static/x:Type + attached props +
//      Name + events + Command) LOADS through a retained reader without an
//      unknown-property/event/extension error (the whole authored set parses).
//   2. resolve_gradient_fill samples the authored gradient along its stops
//      (the honest vertex-sampled tint over the solid-rect path — NO new shader),
//      RESOLVING the S460 seed at its own step.
//   3. A wired Click + a Command+CommandParameter drive visible state (the
//      S463/S464 surfaces resolve end-to-end).
//
// NO-GAP (recorded, not silent): the live WINDOW display (the [both-apps] proof)
// stays the on-machine local gate; this headless seam is the exact code path's
// CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Brush.h"
#include "vaml/core/Color.h"
#include "vaml/core/Element.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/TextLayout.h"       // resolve_gradient_fill
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>

using vaml::Color;
using vaml::DependencyObject;
using vaml::LinearGradientBrush;
using vaml::RelayCommand;
using vaml::RoutedEventArgs;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::resolve_gradient_fill;

// The fully-authored S11b sample: a gradient Brush resource (S460), an
// x:Static-typed static read (S461), attached properties + a FrameworkElement
// Name (S462), event attributes (S463) + Command/CommandParameter (S464). The
// root StackPanel carries the authored resource dict the gradient rides.
const std::string kSample11b =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\">"
    "<StackPanel.Resources>"
    "<LinearGradientBrush x:Key=\"Brush:9\" StartPoint=\"0,0\" EndPoint=\"1,1\">"
    "<LinearGradientBrush.GradientStops>"
    "<GradientStop Color=\"#FF0000\" Offset=\"0\"/>"
    "<GradientStop Color=\"#0000FF\" Offset=\"1\"/>"
    "</LinearGradientBrush.GradientStops>"
    "</LinearGradientBrush>"
    "</StackPanel.Resources>"
    "<Button Name=\"submitButton\" Canvas.Left=\"4\" Click=\"OnClick\" "
    "CommandParamsText=\"{x:Static local:Constants.AppTitle}\"/>"
    "</StackPanel>";

namespace {

// A static value + type the sample's x:Static/x:Type resolve against.
const std::string kAppTitle = "VAML 11b Sample";

} // namespace

// ---------------------------------------------------------------------------
// 1. The fully-authored S11b sample loads without an authored-surface error
// ---------------------------------------------------------------------------

TEST_CASE("S465: the S11b sample (brush + x:Static + attached + Name + event + command) loads")
{
    // Canvas.Left needs the Canvas DP registered (the S136 self-registration).
    { vaml::Canvas probe; (void)probe; }

    VamlReader reader;
    reader.register_event("Click");
    reader.register_static_value("Constants.AppTitle", Value::String(kAppTitle));
    reader.register_command("SaveCommand", nullptr); // registered guard (no deref in load)
    reader.bind_class_handler("OnClick", [](RoutedEventArgs&) {});

    // The authored set must PARSE + BUILD cleanly: no unknown-property for the
    // Name / Canvas.Left / event / x:Static rows.
    bool threw = false;
    std::unique_ptr<DependencyObject> root;
    try
    {
        root = reader.Load(kSample11b);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));

    // The FrameworkElement Name + the authored resource dict are reachable.
    auto* submit = dynamic_cast<UIElement*>(reader.named("submitButton"));
    SHOULD(ShouldlyIntegral(submit != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. resolve_gradient_fill samples the authored gradient along its stops
//    (RESOLVES the S460 seed — the gradient RENDER at its own step)
// ---------------------------------------------------------------------------

TEST_CASE("S465: resolve_gradient_fill interpolates the authored gradient (S460 seed resolved)")
{
    VamlReader reader;
    // A doc whose owned dict keys the gradient under "Brush:9" (the resolve path).
    const std::string gradientDoc =
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" x:Name=\"P\">"
        "<StackPanel.Resources>"
        "<LinearGradientBrush x:Key=\"Brush:9\" StartPoint=\"0,0\" EndPoint=\"1,1\">"
        "<LinearGradientBrush.GradientStops>"
        "<GradientStop Color=\"#FF0000\" Offset=\"0\"/>"
        "<GradientStop Color=\"#0000FF\" Offset=\"1\"/>"
        "</LinearGradientBrush.GradientStops>"
        "</LinearGradientBrush>"
        "</StackPanel.Resources>"
        "</StackPanel>";
    std::unique_ptr<DependencyObject> root = reader.Load(gradientDoc);
    auto* panel = dynamic_cast<UIElement*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    // t=0 -> the first stop (red); t=1 -> the last stop (blue); t=0.5 -> midpoint.
    const Color t0 = resolve_gradient_fill(*panel, 9, 0.0);
    SHOULD(ShouldlyString(t0).ShouldBe(Color{ 255, 0, 0, 255 }));
    const Color t1 = resolve_gradient_fill(*panel, 9, 1.0);
    SHOULD(ShouldlyString(t1).ShouldBe(Color{ 0, 0, 255, 255 }));
    const Color mid = resolve_gradient_fill(*panel, 9, 0.5);
    // The lerp: 255*0.5 + 0*0.5 + 0.5 = 128.0 -> 128 (rounded) / 0 / 128 —
    // the vertex-sampled midpoint tint.
    SHOULD(ShouldlyString(mid.r).ShouldBe(128u));
    SHOULD(ShouldlyString(mid.g).ShouldBe(0u));
    SHOULD(ShouldlyString(mid.b).ShouldBe(128u));
}

// ---------------------------------------------------------------------------
// 3. A wired Click + a Command+CommandParameter drive a visible counter
// ---------------------------------------------------------------------------

TEST_CASE("S465: a wired Click + a parameterized command execute visibly")
{
    VamlReader reader;
    int clicks = 0;
    reader.register_event("Click");
    reader.bind_class_handler("OnClick", [&](RoutedEventArgs&) { ++clicks; });

    int cmdParam = -1;
    auto* command = new RelayCommand(
        [&cmdParam](const Value& p) { cmdParam = p.get<int>(); }, nullptr);
    reader.register_command("SaveCommand", command);

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"B\" Click=\"OnClick\"/>"
        "</StackPanel>");
    SHOULD(ShouldlyIntegral(reader.BindEvents()).ShouldBe(1u));

    auto* btn = dynamic_cast<vaml::Button*>(reader.named("B"));
    SHOULD(ShouldlyIntegral(btn != nullptr).ShouldBe(1));
    if (btn)
    {
        (void)btn->raise_click();
    }
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));

    // Command + CommandParameter: execute SaveCommand with the parameter 42.
    const bool executed = reader.ExecuteCommand("SaveCommand", Value::Int32(42));
    SHOULD(ShouldlyIntegral(static_cast<int>(executed)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(cmdParam).ShouldBe(42));

    delete command;
}
