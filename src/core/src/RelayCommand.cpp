// S246 — RelayCommand: the concrete ICommand implementation.
// See RelayCommand.h.

#include "vaml/core/RelayCommand.h"

#include <algorithm>

namespace vaml {

void RelayCommand::Execute(const Value& parameter)
{
    // A moved-from RelayCommand has no action: Execute is a safe no-op.
    if (action_)
    {
        action_(parameter);
    }
}

bool RelayCommand::CanExecute(const Value& parameter) const
{
    // A command with no predicate (or a moved-from command) is always
    // executable — the WPF RelayCommand default. Side-effect-free.
    return predicate_ ? predicate_(parameter) : true;
}

ICommand::CanExecuteChangedToken
RelayCommand::add_can_execute_changed(ICommand::CanExecuteChangedHandler handler)
{
    const ICommand::CanExecuteChangedToken token = next_token_++;
    subs_.push_back(Sub{ token, std::move(handler) });
    return token;
}

bool RelayCommand::remove_can_execute_changed(ICommand::CanExecuteChangedToken token)
{
    const auto it = std::find_if(subs_.begin(), subs_.end(),
                                 [token](const Sub& s) { return s.token == token; });
    if (it != subs_.end())
    {
        subs_.erase(it);
        return true;
    }
    return false;
}

void RelayCommand::NotifyCanExecuteChanged()
{
    // Fires in REGISTRATION order (snapshot-before-invoke — the S244
    // discipline); a re-entrant add/remove during a raise lands in the same
    // list but is not visited again this pass.
    for (const Sub& s : subs_)
    {
        s.handler();
    }
}

} // namespace vaml
