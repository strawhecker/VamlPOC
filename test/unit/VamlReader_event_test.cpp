// S322 (dep:S321) — markup event wiring: Click="OnClick" -> handler on the
// x:Class owner via the S322 class-owner registry + the S320/S321 routed-event
// engine. RESOLVES the S092 event-hook MINIMAL REAL seed (the sentinel is
// removed at this step; register_event + deferred-event records are now the
// REAL surface feeding BindEvents).
//
// The event is wired on a REAL rendered UIElement Panel (StackPanel — a real
// S129 UIElement). Button/Text are S401/S400 MINIMAL REAL DependencyObject
// seeds (not UIElement yet), so they don't hold routed handlers until their
// control sprints — the RECORDED decision (not silent): S322 wiring +
// S329_1 interactive proof operate on real rendered UIElements; the same
// mechanism becomes Button's at S401.
//
// Validates, with the Shouldly vocabulary:
//   1. register_event("Click") + a Click="OnClick" markup attr -> a deferred
//      record (S092 parity, re-proven).
//   2. bind_class_handler("OnClick", handler) + BindEvents() -> the target
//      StackPanel gains a LIVE routed handler that fires on RaiseEvent (the
//      S321 engine) and bubbles.
//   3. The handler resolves by (class, handlerName) — the x:Class-owner
//      registry.
//   4. A handlerName with no registered handler -> VamlReaderException(
//      NoEventHandler) naming the class + handler (Class A, never silent).
//   5. bind_class_handler is idempotent (first wins).
//   6. The S092 event-hook MINIMAL REAL sentinel is GONE (grep — the
//      register_event/deferred surface is the kept+extended real one).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Element.h"            // StackPanel + register_builtin_elements
#include "vaml/core/RoutedEvent.h"        // RoutedEventArgs / RoutedEventHandler / ClickEvent
#include "vaml/core/UIElement.h"          // add_routed_handler / RaiseEvent
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>

using vaml::DependencyObject;
using vaml::RoutedEventArgs;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::test::ShouldThrow;

TEST_CASE("S322: register_event + BindEvents wires Click=\"OnClick\" to a live routed handler")
{
    VamlReader reader;
    reader.register_event("Click");

    // The app registers its named handler (S322 class-owner registry, ADR-006
    // app-owned composition — no DI).
    int handled = 0;
    reader.bind_class_handler("OnClick", [&](RoutedEventArgs&) { ++handled; });

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Class=\"MyWindow\" Click=\"OnClick\"/>");

    // S092 parity: the markup event produced ONE deferred record.
    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(1u));

    // BindEvents wires it -> the target StackPanel gains a live route handler.
    const std::size_t wired = reader.BindEvents();
    SHOULD(ShouldlyIntegral(wired).ShouldBe(1u));

    // The root is the StackPanel; raising a Click on it fires the markup
    // handler through the S321 engine.
    auto* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    RoutedEventArgs args;
    args.source = panel;
    panel->RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(handled).ShouldBe(1));

    // The handler BUBBLES: a Click raised on a CHILD of the panel reaches the
    // panel's markup handler (the S321 parent-chain route) — proving the wire
    // is a REAL routed handler, not a one-off.
    auto* child = panel->create_child<UIElement>();
    RoutedEventArgs args2;
    args2.source = child;
    child->RaiseEvent(vaml::ClickEvent(), args2);
    SHOULD(ShouldlyIntegral(handled).ShouldBe(2));
}

TEST_CASE("S322: a handlerName with no registered handler throws NoEventHandler (Class A)")
{
    VamlReader reader;
    reader.register_event("Click");
    (void)reader.Load("<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                      "x:Class=\"MyWindow\" Click=\"OnMissing\"/>");
    SHOULD(ShouldThrow<VamlReaderException>([&] { (void)reader.BindEvents(); }));
}

TEST_CASE("S322: NoEventHandler names the class + handler")
{
    VamlReader reader;
    reader.register_event("Click");
    (void)reader.Load("<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                      "x:Class=\"MyWindow\" Click=\"OnMissing\"/>");
    bool threw = false;
    try
    {
        (void)reader.BindEvents();
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::NoEventHandler)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("OnMissing")));
        SHOULD(ShouldlyString(std::string(e.what()).find("OnMissing") != std::string::npos).ShouldBe(true));
        SHOULD(ShouldlyString(std::string(e.what()).find("MyWindow") != std::string::npos).ShouldBe(true));
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(1));
}

TEST_CASE("S322: bind_class_handler is idempotent (first wins)")
{
    VamlReader reader;
    reader.register_event("Click");

    int first = 0;
    int second = 0;
    reader.bind_class_handler("OnClick", [&](RoutedEventArgs&) { ++first; });
    reader.bind_class_handler("OnClick", [&](RoutedEventArgs&) { ++second; }); // ignored

    std::unique_ptr<DependencyObject> root = reader.Load("<StackPanel Click=\"OnClick\"/>");
    (void)reader.BindEvents();

    auto* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));
    RoutedEventArgs args;
    args.source = panel;
    panel->RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(first).ShouldBe(1));
    SHOULD(ShouldlyIntegral(second).ShouldBe(0)); // the first registration won
}

TEST_CASE("S322: an unregistered event key is an unknown property (S092 parity, never silent)")
{
    VamlReader reader;
    reader.register_event("Click");
    reader.bind_class_handler("OnFoo", [&](RoutedEventArgs&) {});
    bool threw = false;
    try
    {
        // "FooClick" is NOT a registered event AND has no DP -> UnknownProperty.
        (void)reader.Load("<StackPanel FooClick=\"OnFoo\"/>");
    }
    catch (const VamlReaderException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(1));
}
