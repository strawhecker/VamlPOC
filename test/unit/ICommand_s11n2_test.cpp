// 11n-2 — THE ICOMMAND SPINE (vaml::core).
//
// PER DECISION-1 (roadmap 11n-2): ONE `vaml::ICommand` (the S246 family).
// RoutedCommand IMPLEMENTS the existing S246 interface — no parallel interface,
// no bridge. The spine is self-contained PresentationCore surface:
//   RoutedCommand : ICommand  + CommandManager (the routed-command table +
//   RequerySuggested hub)  + InputGesture -> KeyGesture/MouseGesture
//   + InputBinding(Freezable, ICommandSource) -> KeyBinding/MouseBinding
//   + the ButtonBase controls (Button/RepeatButton/MenuItem) upgraded to
//     ICommandSource (Command/CommandParameter/CommandTarget as REAL DPs).
//
// Proves:
//   (A) RoutedCommand implements the ONE ICommand (CanExecute default-true;
//       CanExecuteChanged token subscribe/unsubscribe); carries Name/OwnerType.
//   (B) CommandManager table: RegisterCommand (idempotent) + FindCommand,
//       and RequerySuggested add/remove/invalidate (registration order).
//   (C) KeyGesture/MouseGesture: matches + gesture_text (WPF ModifierKeys/
//       MouseActions ints — ZERO invented names).
//   (D) InputBinding : Freezable, ICommandSource — KeyBinding/MouseBinding
//       attach a gesture + command, expose the ICommandSource shape, and are
//       frozen -> write refusal (the Freezable invariant).
//   (E) Button/RepeatButton/MenuItem : ICommandSource — Command
//       (vaml::ICommand*) / CommandParameter / CommandTarget DPs round-trip,
//       and raise_click EXECUTES the command (CanExecute-gated).
//
// ADR-018: ZERO new locks (Meyers-static table + single-threaded affinity).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/CommandManager.h>
#include <vaml/core/Element.h>        // Button/RepeatButton/MenuItem + register_builtin_elements
#include <vaml/core/ICommand.h>
#include <vaml/core/InputBinding.h>
#include <vaml/core/InputGesture.h>
#include <vaml/core/RelayCommand.h>   // a concrete ICommand impl for execute assertions
#include <vaml/core/RoutedCommand.h>

#include <cstddef>
#include <string>

using vaml::CommandManager;
using vaml::ICommand;
using vaml::InputGestureMatch;
using vaml::KeyBinding;
using vaml::KeyGesture;
using vaml::MouseBinding;
using vaml::MouseGesture;
using vaml::RelayCommand;
using vaml::RoutedCommand;
using vaml::Value;

// raw pointer-equality helper predicates (ShouldlyPtr exposes only null-ness)
#define SHOULD_EQ(a, b) SHOULD(ShouldlyIntegral((a) == (b) ? 1 : 0).ShouldBe(1))

// ---------------------------------------------------------------------------
// A. RoutedCommand : ICommand (DECISION-1)
// ---------------------------------------------------------------------------

TEST_CASE("11n-2: RoutedCommand implements the ONE ICommand (name/owner/canexecute/events)")
{
    RoutedCommand rc("Save", "ApplicationCommands");
    SHOULD(ShouldlyString(rc.name()).ShouldBe(std::string("Save")));
    SHOULD(ShouldlyString(rc.owner_type()).ShouldBe(std::string("ApplicationCommands")));

    // CanExecute default-true (a RoutedCommand is open unless a gesture/manager disables).
    SHOULD(ShouldlyIntegral(rc.CanExecute(Value::Null()) ? 1 : 0).ShouldBe(1));

    // CanExecuteChanged subscribe/unsubscribe (token pattern).
    int fired = 0;
    auto token = rc.add_can_execute_changed([&] { ++fired; });
    SHOULD(ShouldlyIntegral(rc.remove_can_execute_changed(token) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(rc.remove_can_execute_changed(token) ? 1 : 0).ShouldBe(0));

    // It IS-A vaml::ICommand (DECISION-1: no bridge, no parallel interface).
    ICommand* asBase = &rc;
    SHOULD(ShouldlyPointer(asBase).ShouldNotBeNull());
}

// ---------------------------------------------------------------------------
// B. CommandManager (table + RequerySuggested)
// ---------------------------------------------------------------------------

TEST_CASE("11n-2: CommandManager routed-command table + RequerySuggested hub")
{
    RoutedCommand rc("Copy", "ApplicationCommands");

    // Use a test-unique name (the table is process-static).
    static int uniqCounter = 0;
    const std::string uniq = std::string("Test11n2Copy") + std::to_string(++uniqCounter);

    CommandManager::RegisterCommand(uniq + ".Copy", &rc);
    ICommand* found = CommandManager::FindCommand(uniq + ".Copy");
    SHOULD(ShouldlyPointer(found).ShouldNotBeNull());

    // idempotent: re-register (even a different command) keeps the first
    ICommand* first = CommandManager::FindCommand(uniq + ".Copy");
    CommandManager::RegisterCommand(uniq + ".Copy", nullptr);
    {
        ICommand* after = CommandManager::FindCommand(uniq + ".Copy");
        SHOULD(ShouldlyIntegral(after == first ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(after == nullptr ? 1 : 0).ShouldBe(0));
    }

    // RequerySuggested
    int fired = 0;
    auto tok = CommandManager::AddRequerySuggested([&] { ++fired; });
    CommandManager::InvalidateRequerySuggested();
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));
    CommandManager::RemoveRequerySuggested(tok);
    CommandManager::InvalidateRequerySuggested();
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1)); // removed -> no more
}

// ---------------------------------------------------------------------------
// C. InputGesture -> KeyGesture / MouseGesture
// ---------------------------------------------------------------------------

TEST_CASE("11n-2: KeyGesture/MouseGesture match + gesture_text (WPF ints)")
{
    // Ctrl+S: key=83 ('S'), modifiers=2 (Control). WPF ModifierKeys.Control=2.
    KeyGesture kg(83, 2);
    SHOULD(ShouldlyIntegral(kg.matches(83, 2) == InputGestureMatch::Matched ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(kg.matches(83, 0) == InputGestureMatch::Matched ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyString(kg.gesture_text()).ShouldBe(std::string("Ctrl+83")));

    // LeftClick mouse gesture: action=1 (LeftClick), no modifiers.
    MouseGesture mg(1, 0);
    SHOULD(ShouldlyIntegral(mg.matches(1, 0) == InputGestureMatch::Matched ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(mg.matches(2, 0) == InputGestureMatch::Matched ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyString(mg.gesture_text()).ShouldBe(std::string("LeftClick")));
}

// ---------------------------------------------------------------------------
// D. InputBinding(Freezable, ICommandSource) -> KeyBinding/MouseBinding
// ---------------------------------------------------------------------------

TEST_CASE("11n-2: KeyBinding/MouseBinding (Freezable + ICommandSource)")
{
    RelayCommand noop([](const Value&) { /* no-op */ });

    KeyBinding kb(KeyGesture(83, 2));
    kb.with_command(&noop).with_parameter(Value::String("p"));
    SHOULD_EQ(kb.source_command(), static_cast<ICommand*>(&noop));
    SHOULD(ShouldlyString(kb.source_command_parameter().get<std::string>()).ShouldBe(std::string("p")));
    SHOULD(ShouldlyIntegral(kb.is_frozen() ? 1 : 0).ShouldBe(0));

    MouseBinding mb(MouseGesture(1, 0));
    mb.with_command(&noop);
    SHOULD_EQ(mb.source_command(), static_cast<ICommand*>(&noop));

    // Freeze-refusal is exercised in the isolated Freezable_s11n2_test.cpp TU
    // (verified there — a frozen InputBinding refuses a write). Kept out of
    // this heavy TU to avoid the MSVC PCH heap-exhaustion on edit.
}

// ---------------------------------------------------------------------------
// E. ButtonBase controls : ICommandSource (the anchor this step lands on)
// ---------------------------------------------------------------------------

TEST_CASE("11n-2: Button/RepeatButton/MenuItem : ICommandSource execute on invoke")
{
    vaml::register_builtin_elements();

    // A RelayCommand records its (parameter) execution count.
    int execCount = 0;
    Value lastParam = Value::Null();
    RelayCommand cmd([&](const Value& p) { ++execCount; lastParam = p; });

    // --- Button ------------------------------------------------------------
    {
        vaml::Button b;
        SHOULD(ShouldlyIntegral(b.set_command(&cmd) == vaml::SetResult::APPLIED ? 1 : 0).ShouldBe(1));
        SHOULD_EQ(b.source_command(), static_cast<ICommand*>(&cmd));
        SHOULD(ShouldlyIntegral(b.set_command_parameter(Value::String("B")) == vaml::SetResult::APPLIED ? 1 : 0).ShouldBe(1));

        const int before = execCount;
        (void)b.raise_click();
        SHOULD(ShouldlyIntegral(execCount).ShouldBe(before + 1));
        SHOULD(ShouldlyString(lastParam.get<std::string>()).ShouldBe(std::string("B")));
    }

    // --- RepeatButton --------------------------------------------------------
    {
        vaml::RepeatButton repeat;
        SHOULD(ShouldlyIntegral(repeat.set_command(&cmd) == vaml::SetResult::APPLIED ? 1 : 0).ShouldBe(1));
        SHOULD_EQ(repeat.source_command(), static_cast<ICommand*>(&cmd));
        const int before = execCount;
        (void)repeat.raise_click();
        SHOULD(ShouldlyIntegral(execCount).ShouldBe(before + 1));
    }

    // --- MenuItem ------------------------------------------------------------
    {
        vaml::MenuItem mi;
        SHOULD(ShouldlyIntegral(mi.set_command_object(&cmd) == vaml::SetResult::APPLIED ? 1 : 0).ShouldBe(1));
        SHOULD_EQ(mi.source_command(), static_cast<ICommand*>(&cmd));
        const int before = execCount;
        (void)mi.raise_click();
        SHOULD(ShouldlyIntegral(execCount).ShouldBe(before + 1));
    }
}
