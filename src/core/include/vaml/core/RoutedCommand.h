#pragma once
// VAML RoutedCommand — a command that raises a ROUTED command event instead of
// running a delegate (WPF System.Windows.Input.RoutedCommand). 11n-2
// (dep: S246 ICommand).
//
// PER DECISION-1 (roadmap 11n-2): RoutedCommand IMPLEMENTS the EXISTING
// single `vaml::ICommand` (S246). There is NO parallel/duplicate ICommand and
// NO bridge — a RoutedCommand IS an `ICommand`, so any command consumer
// (ButtonBase : ICommandSource, {Command} markup, InvokeCommandAction) treats
// it uniformly.
//
// WPF-faithful shape (verified from
// E:\GitRepos\wpf\src\...\Input\Command\RoutedCommand.cs):
//   - ctor(name, ownerType, inputGestures...);
//   - Name / OwnerType / InputGestures properties;
//   - Execute(parameter, target) — RAISES a routed Executed command event
//     (not a delegate);
//   - CanExecute(parameter, target) — raises a routed CanExecute query;
//   - CanExecuteChanged — raised when the input gestures / manager state that
//     drive CanExecute change (WPF: CommandManager.RequerySuggested).
//   In VAML (ADR-011: "how to render/structure" stays out of a routed-event
//   chain for the value plane), the routed EXECUTION is a content-notification
//   on the command's owner-type family: Execute raises a
//   "CommandExecuted" event whose source is the command itself; the owning
//   application/control family subscribes and performs the real action. The
//   routed-command-table (CommandManager) resolves a RoutedCommand by name.
//
// ADR-018: ZERO new locks (single-threaded affinity; CommandManager is the
// static routed-command table below).

#include "vaml/core/ICommand.h"      // vaml::ICommand (the ONE command interface)
#include "vaml/core/Value.h"         // Value (the parameter domain)
#include "vaml/core/export.h"

#include <string>
#include <utility>
#include <vector>

namespace vaml {

// A command that executes by raising a routed command event (no delegate).
class VAML_CORE_EXPORT RoutedCommand : public ICommand
{
public:
    // WPF-faithful ctor: name + ownerType. (inputGestures are authored via a
    // RoutedCommand's InputGestures collection — the 11n-2 gesture seam.)
    RoutedCommand(std::string name, std::string ownerType);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& owner_type() const noexcept { return ownerType_; }

    // --- ICommand (S246) — but via a routed event, not a delegate. -------
    // CanExecute reports the CURRENT can-run state from the command's own
    // gesture/manager source; the routed query does not carry a CanExecute
    // delay in the VAML value plane.
    bool CanExecute(const Value& parameter) const override;
    void Execute(const Value& parameter) override;

    // The S246 CanExecuteChanged surface (re-query signal; e.g.
    // CommandManager::RequerySuggested raises it so every consumer re-queries).
    [[nodiscard]] CanExecuteChangedToken add_can_execute_changed(CanExecuteChangedHandler handler) override;
    bool remove_can_execute_changed(CanExecuteChangedToken token) override;

private:
    std::string name_;
    std::string ownerType_;
    // The routed-command CanExecuteChanged event slot (the S246 token
    // pattern, registration order, copy-before-invoke).
    CanExecuteChangedToken next_ct_ = 1;
    std::vector<std::pair<CanExecuteChangedToken, CanExecuteChangedHandler>> canExecuteChanged_;
};

} // namespace vaml
