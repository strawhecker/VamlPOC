// S502 — InvokeCommandAction: execute a bound ICommand with a parameter on invoke.

#include "vaml/core/InvokeCommandAction.h"

namespace vaml {

void InvokeCommandAction::invoke(const Value& parameter)
{
    if (command_ == nullptr)
    {
        return; // no command to execute — a no-op (WPF-faithful for a null Command)
    }
    // The effective parameter: a non-Null CommandParameter override wins;
    // otherwise the trigger's passed parameter (WPF-faithful precedence).
    const Value& effective =
        (commandParameter_.type() != ValueType::Null) ? commandParameter_ : parameter;
    if (command_->CanExecute(effective))
    {
        command_->Execute(effective);
    }
}

} // namespace vaml
