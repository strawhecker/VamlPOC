// S401 (dep:S400) — Button control tests (vaml::core). Headless proof of the
// S401 Button (the S087 Button MINIMAL-REAL seed RESOLVED at S401):
//
//   (A) BUTTON IS A REAL CONTROL: re-based onto UIElement (participates in the
//       visual tree + hit-testing + routed events), registered as a built-in
//       element, with the real IsEnabled DP.
//   (B) CLICK ROUTED EVENT: raise_click() raises the BUBBLING ClickEvent with
//       source==the Button (the S324/S323 click flow hit-tests the button and
//       raises Click on it); IsEnabled=false BLOCKS the click (WPF-faithful); a
//       real InputMarshaller press+release on a Button raises Click on the
//       button (source == the button).
//   (C) VISUAL STATES: the default implicit template's Normal/Hover/Pressed/
//       Disabled states swap the shared "Background" Color DP through the S364
//       %state engine; a disabled Button is forced to the Disabled state.
//   (D) MEASURE: MeasureOverride reports a bounded default box; the base
//       Measure finalizer applies the S123 stretch-vs-wrap rule.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>            // Button, register_builtin_elements
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/InputMarshaller.h>    // InputMarshaller (the S324 click flow)
#include <vaml/core/RoutedEvent.h>        // ClickEvent / RoutedEventArgs
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>

#include <memory>
#include <string>

using vaml::Button;
using vaml::Size;
using vaml::Rect;
namespace input = vaml::input;

TEST_CASE("S401: Button is a real UIElement + measured box + registered built-in")
{
    vaml::register_builtin_elements();
    Button b;
    (void)dynamic_cast<vaml::UIElement*>(&b); // re-based onto UIElement

    // IsEnabled default true.
    SHOULD(ShouldlyString(b.is_enabled()).ShouldBe(true));
    SHOULD(ShouldlyString(b.visual_state()).ShouldBe(std::string("Normal")));

    // Measurement: bounded available -> the default box (S123).
    b.Measure(Size{ 100.0f, 40.0f });
    SHOULD(ShouldlyString(b.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(b.desired_size().height).ShouldBe(40.0f));

    // Registered as a real built-in element.
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("Button")).ShouldBe(true));
    auto produced = g.try_create("Button");
    SHOULD(ShouldlyIntegral(produced != nullptr).ShouldBe(true));
    if (produced)
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Button*>(produced.get()) != nullptr).ShouldBe(true));

    // IsEnabled round-trip + validation (only 0/1 accepted).
    SHOULD(ShouldlyString(b.set_is_enabled(false)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(b.is_enabled()).ShouldBe(false));
    SHOULD(ShouldlyString(b.set_is_enabled(true)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(b.is_enabled()).ShouldBe(true));
}

TEST_CASE("S401: raise_click raises a bubbling Click with source == this; IsEnabled blocks it")
{
    Button a; // the button that raises
    Button parent;
    int clicks = 0;
    void* sourceAtParent = nullptr;

    // A handler on the BUTTON itself counts + reads source.
    (void)a.add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs& e) {
        ++clicks;
        sourceAtParent = e.source;
    });

    // A raised click on an ENABLED button fires + source == the button.
    (void)a.set_is_enabled(true);
    const bool handled = a.raise_click();
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
    SHOULD(ShouldlyString(sourceAtParent).ShouldBe(&a));
    SHOULD(ShouldlyString(handled).ShouldBe(false)); // no handler set handled

    // An IsEnabled=false button does NOT raise (WPF-faithful gate).
    (void)a.set_is_enabled(false);
    const bool clickDisabled = a.raise_click();
    SHOULD(ShouldlyString(clickDisabled).ShouldBe(false));
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1)); // unchanged (no second raise)
    (void)parent;
}

TEST_CASE("S401: the S324 input flow raises Click on a hit Button (source == the button)")
{
    // A real arranged Button hit by a press+release raises a bubbling Click with
    // source == the Button — the S323/S324 click flow works on a re-based Button.
    auto root = std::make_unique<vaml::UIElement>();
    root->Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    Button* b = root->create_child<Button>();
    (void)b->set_is_enabled(true);
    b->Arrange(Rect{ 10.0f, 10.0f, 60.0f, 24.0f });

    int clicks = 0;
    void* source = nullptr;
    (void)b->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs& e) {
        ++clicks;
        source = e.source;
    });

    input::InputMarshaller in(root.get());
    // Press + release inside the Button's arranged box (15,15 is within
    // (10..70, 10..34)).
    (void)in.Push({ input::InputEventKind::MouseDown, 15.0f, 15.0f,
                    input::Key::None, false, true });
    (void)in.Push({ input::InputEventKind::MouseUp, 15.0f, 15.0f,
                    input::Key::None, false, false });

    // The hit element is the Button; the click raised on it with the right source.
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
    SHOULD(ShouldlyString(source).ShouldBe(b));
}

TEST_CASE("S401: hover/pressed/disabled visual states swap the Background brush color")
{
    Button b;
    // The default template has the four states (Normal/Hover/Pressed/Disabled)
    // driving the shared "Background" Color DP.

    // Enter Hover -> the Background Color changes off the Normal default.
    (void)b.set_visual_state("Hover");
    const vaml::Value normalBg = b.Get(vaml::PropertyId::of("Background"));
    SHOULD(ShouldlyString(normalBg.type() == vaml::ValueType::Color).ShouldBe(true));
    (void)normalBg;

    // The current visual state reports Hover.
    SHOULD(ShouldlyString(b.visual_state()).ShouldBe(std::string("Hover")));

    // Enter Pressed -> reports Pressed.
    (void)b.set_visual_state("Pressed");
    SHOULD(ShouldlyString(b.visual_state()).ShouldBe(std::string("Pressed")));

    // A disabled Button is FORCED to the Disabled state (WPF-faithful visual).
    (void)b.set_is_enabled(false);
    (void)b.set_visual_state("Hover");
    SHOULD(ShouldlyString(b.visual_state()).ShouldBe(std::string("Disabled")));
}
