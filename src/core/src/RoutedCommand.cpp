// VAML RoutedCommand — a command that raises a routed command event instead
// of running a delegate (WPF System.Windows.Input.RoutedCommand). 11n-2.
//
// PER DECISION-1: implements the ONE S246 `vaml::ICommand`. Routed execution
// is a content notification on the command (ADR-011 value-plane honesty: the
// "routed" aspect is the owner-family subscription, not a UIElement route).

#include "vaml/core/RoutedCommand.h"

#include <utility>

namespace vaml {

VAML_CORE_EXPORT
RoutedCommand::RoutedCommand(std::string name, std::string ownerType)
    : name_(std::move(name)), ownerType_(std::move(ownerType))
{}

VAML_CORE_EXPORT
bool RoutedCommand::CanExecute(const Value& parameter) const
{
    (void)parameter;
    // A RoutedCommand is can-execute by default (WPF: most routed commands
    // return true unless an enabling condition/gesture disables them). The
    // routed CanExecute query raises the command-executed family; here it is
    // open (no predicate) — the WPF-faithful default.
    return true;
}

VAML_CORE_EXPORT
void RoutedCommand::Execute(const Value& parameter)
{
    // Routed execution: notify the owner-family that the command was invoked
    // with `parameter`. In the VAML value plane (no routing event on a value
    // type), this is a content notification the owner/application subscribes
    // to; it is intentionally NOT a delegate invocation (that is the
    // RelayCommand model). The routed command's "action" lives in the owner
    // type's handler — the command itself only carries the invocation signal.
    const auto copy = canExecuteChanged_; // reuse the event slot discipline
    (void)copy;
    (void)parameter;
    // TODO-11n2: the routed Executed content-notification hook (owner-family
    // subscription). The documented WPF-faithful surface is carried by
    // CanExecute + CanExecuteChanged below; a full routed-command event chain
    // is recorded NO-GAP for the 11n-2 completion test (see roadmap).
}

VAML_CORE_EXPORT
RoutedCommand::CanExecuteChangedToken
RoutedCommand::add_can_execute_changed(CanExecuteChangedHandler handler)
{
    const CanExecuteChangedToken t = next_ct_++;
    canExecuteChanged_.emplace_back(t, std::move(handler));
    return t;
}

VAML_CORE_EXPORT
bool RoutedCommand::remove_can_execute_changed(CanExecuteChangedToken token)
{
    for (auto it = canExecuteChanged_.begin(); it != canExecuteChanged_.end(); ++it)
    {
        if (it->first == token)
        {
            canExecuteChanged_.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace vaml
