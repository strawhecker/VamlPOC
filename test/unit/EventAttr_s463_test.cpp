// S463 (dep:S462) — EVENT-ATTRIBUTE wiring for the authored set.
//
// The S322 markup-event wiring extends to the authored-set events:
//   * Window  Loaded / Closing — raised by the Window control (S440 re-base +
//       this S463 raise helper); `Loaded="..."` / `Closing="..."` wire via
//       BindEvents (a real routed handler on the window).
//   * TextBox TextChanged — raised by a REAL TextBox edit (the S402 commit
//       path); `TextChanged="..."` wires + fires on a keystroke.
//   * the RAISED-ATTACHED form `StackPanel.Button.Click="Handler"` — a dotted
//       event-attribute key whose EVENT TAIL ("Click") is the event: the reader
//       routes it (never an unknown-property throw) and BindEvents wires it on
//       the authored-on element; a Click raised on a CHILD Button bubbles to it
//       (the WPF-faithful bubbling attach over the Sprint-8 engine).
//
// Completes the S322 event-hook resolution for the authored surface.
//
// Registers no S463-unique DPs (events are routed identities, not DPs).
// Uses the Shouldly vocabulary. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"          // Window / TextBox / StackPanel / Button
#include "vaml/core/FocusManager.h"     // KeyCode (the TextBox edit key)
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>

using vaml::Button;
using vaml::DependencyObject;
using vaml::RoutedEventArgs;
using vaml::StackPanel;
using vaml::TextBox;
using vaml::UIElement;
using vaml::VamlReader;
using vaml::VamlReaderException;
using vaml::Window;
using vaml::test::ShouldThrow;

// ---------------------------------------------------------------------------
// 1. Window Loaded / Closing wire + raise
// ---------------------------------------------------------------------------

TEST_CASE("S463: Window Loaded=\"...\" wires and raises on the window")
{
    VamlReader reader;
    reader.register_event("Loaded");
    reader.register_event("Closing");
    int loaded = 0;
    int closing = 0;
    reader.bind_class_handler("OnLoaded", [&](RoutedEventArgs&) { ++loaded; });
    reader.bind_class_handler("OnClosing", [&](RoutedEventArgs&) { ++closing; });

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Class=\"App\" Loaded=\"OnLoaded\" Closing=\"OnClosing\"/>");
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(1));

    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(reader.BindEvents()).ShouldBe(2u));

    (void)win->raise_loaded();
    SHOULD(ShouldlyIntegral(loaded).ShouldBe(1));
    (void)win->raise_closing();
    SHOULD(ShouldlyIntegral(closing).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. TextBox TextChanged wires + fires on a real edit
// ---------------------------------------------------------------------------

TEST_CASE("S463: TextBox TextChanged=\"...\" fires on a real keystroke editing commit")
{
    VamlReader reader;
    reader.register_event("TextChanged");
    int changed = 0;
    reader.bind_class_handler("OnTextChanged", [&](RoutedEventArgs&) { ++changed; });

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<TextBox xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"T\" TextChanged=\"OnTextChanged\"/>");
    auto* tb = dynamic_cast<TextBox*>(root.get());
    SHOULD(ShouldlyIntegral(tb != nullptr).ShouldBe(1));

    SHOULD(ShouldlyIntegral(reader.BindEvents()).ShouldBe(1u));

    // A real edit (insert a letter via the S402 handle_key path) raises
    // TextChanged -> the wired markup handler fires.
    (void)tb->handle_key(vaml::KeyCode::A, true);
    SHOULD(ShouldlyIntegral(changed).ShouldBe(1));
    SHOULD(ShouldlyString(tb->text()).ShouldBe(std::string("a")));
}

// ---------------------------------------------------------------------------
// 3. The RAISED-ATTACHED form StackPanel.Button.Click (bubbling attach)
// ---------------------------------------------------------------------------

TEST_CASE("S463: StackPanel.Button.Click wires a bubbling Click on the panel")
{
    VamlReader reader;
    reader.register_event("Click");
    int clicks = 0;
    reader.bind_class_handler("StackPanel_Click",
                              [&](RoutedEventArgs&) { ++clicks; });

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Panel\" StackPanel.Button.Click=\"StackPanel_Click\">"
        "<Button x:Name=\"Btn\"/>"
        "</StackPanel>");

    // The dotted event key must route as an EVENT (tail "Click"), never an
    // unknown-property throw.
    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(reader.BindEvents()).ShouldBe(1u));

    auto* btn = dynamic_cast<Button*>(reader.named("Btn"));
    SHOULD(ShouldlyIntegral(btn != nullptr).ShouldBe(1));

    // A Click raised on the child Button bubbles up to the panel's wired
    // handler (the WPF-faithful raised-attached bubbling attach).
    if (btn)
    {
        (void)btn->raise_click();
    }
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
}
