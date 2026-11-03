#pragma once
// VAML ICommandSource — the command-invoking touchpoint interface
// (WPF System.Windows.Input.ICommandSource). 11n-2 (dep: S246 ICommand).
//
// This is the UNCOVERED ANCHOR 11n-2 lands on (DAG: ICommandSource — the
// ButtonBase touchpoint). WPF-faithful shape (verified from
// E:\GitRepos\wpf\src\...\Command\ICommandSource.cs):
//   ICommand       Command         { get; }   // the command executed on invoke
//   object         CommandParameter{ get; }   // the parameter passed to Execute
//   IInputElement  CommandTarget   { get; }   // the target routed to
//
// PER DECISION-1: there is ONE `vaml::ICommand` (the existing S246 interface).
// `ICommandSource::Command` is typed `vaml::ICommand*` (the real command
// object), never a parallel interface and never a bridge. CommandParameter is
// a `Value` (the VAML value domain — supersedes the S464 string form; a
// string literal stays representable as Value::String, so existing authored
// string parameters keep working). CommandTarget is `DependencyObject*`
// (the VAML target element).
//
// ADR-018: ZERO new locks — an ICommandSource is a command-holding control,
// mutated/queried on its affinity thread (single-threaded, S037/S041/S143).

#include "vaml/core/ICommand.h"      // vaml::ICommand
#include "vaml/core/Value.h"         // vaml::Value
#include "vaml/core/export.h"

#include <cstddef>

namespace vaml {

class DependencyObject;   // forward decl (CommandTarget is DependencyObject*)

// The command-invoking touchpoint interface (WPF System.Windows.Input
// ICommandSource). Implemented by the ButtonBase controls (Button,
// RepeatButton, ToggleButton, MenuItem) — the anchor 11n-2 upgrades them to
// a REAL command-holding object.
class VAML_CORE_EXPORT ICommandSource
{
public:
    virtual ~ICommandSource() = default;

    // The command executed when the source is invoked ("pressed").
    // Types: vaml::ICommand* (the ONE S246 command interface). Nullable.
    [[nodiscard]] virtual vaml::ICommand* source_command() const noexcept = 0;

    // The parameter passed to the command's Execute/CanExecute. A Value (the
    // VAML value domain); Value::Null() when none authored.
    [[nodiscard]] virtual vaml::Value source_command_parameter() const = 0;

    // The target element the command is routed to. Nullable (the source
    // itself routes when null — WPF defaults CommandTarget to the focused
    // element; VAML defaults to the source).
    [[nodiscard]] virtual DependencyObject* source_command_target() const noexcept = 0;
};

} // namespace vaml
