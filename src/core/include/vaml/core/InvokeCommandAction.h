#pragma once
// VAML InvokeCommandAction — the Microsoft.Xaml.Behaviors command action
// (S502, Sprint 11f). WPF/GROUND TRUTH (Microsoft.Xaml.Behaviors
// InvokeCommandAction.cs): an `InvokeCommandAction : TriggerAction` whose
// `invoke` executes a bound `ICommand` with the trigger's parameter. This is
// the S11k-5 EventTrigger Action payload resolved at 11f — the interactivity
// `i:` surface, NOT the Style.Triggers BeginStoryboard family (that stays the
// S559 NO-GAP token).
//
// Surface (WPF-faithful):
//   - `Command` (ICommand*) — the command to execute (resolved by the app/reader
//     through the S253 command registry; non-owning, the app owns it).
//   - `CommandParameter` (Value) — the parameter passed to Execute (WPF
//     `CommandParameter`); a Null default passes Value::Null().
//   - `invoke(parameter)` — when a command is set AND CanExecute(parameter)
//     (WPF-faithful gate), executes `Command->Execute(parameter)` (or the
//     CommandParameter when it overrides the trigger parameter).
//
// Real + permanent (NOT MINIMAL REAL, NOT a stub). ADR-018: zero locks.

#include "vaml/core/TriggerAction.h"
#include "vaml/core/Value.h"

namespace vaml {

class VAML_CORE_EXPORT InvokeCommandAction final : public TriggerAction
{
public:
    InvokeCommandAction() = default;

    // The command to execute (NON-OWNING — the app owns it; the S253
    // app-owned command registry). nullptr = no command (invoke is a no-op).
    void set_command(ICommand* command) noexcept { command_ = command; }
    [[nodiscard]] ICommand* command() const noexcept { return command_; }

    // The command parameter override. A Null (default) means the invoke uses
    // the trigger's passed parameter; a non-Null override wins.
    void set_command_parameter(Value parameter) { commandParameter_ = std::move(parameter); }
    [[nodiscard]] const Value& command_parameter() const noexcept { return commandParameter_; }

    // Invoke: when a command is set AND CanExecute(parameter) (WPF-faithful),
    // executes it. The effective parameter is the CommandParameter when it is
    // non-Null, else the trigger's `parameter`. A command with no predicate has
    // CanExecute==true (the RelayCommand default), so it always executes.
    void invoke(const Value& parameter) override;

private:
    ICommand* command_ = nullptr;
    Value     commandParameter_ = Value::Null();
};

} // namespace vaml
