// VAML CommandManager — the static routed-command table + re-query hub
// (WPF System.Windows.Input.CommandManager). 11n-2.
//
// The static function-local statics follow the S043 "global via Meyers-static"
// pattern (like the AutomationProperties registration). ADR-018: ZERO new
// locks — all access is affinity-thread on the single UI thread.

#include "vaml/core/CommandManager.h"

#include <utility>

namespace vaml {

namespace {
// The routed-command table + requery handler list live as function-local
// statics (Meyers-guard, constructed on first use). Non-owning command
// pointers (app/framework lifetime — the CommandManager never deletes them).
std::vector<CommandManager::Entry>& command_table()
{
    static std::vector<CommandManager::Entry> table;
    return table;
}
} // namespace

VAML_CORE_EXPORT
void CommandManager::RegisterCommand(std::string name, ICommand* command)
{
    if (command == nullptr || name.empty())
        return;
    auto& table = command_table();
    for (const auto& e : table)
    {
        if (e.name == name)
        {
            return; // idempotent: first registration wins (app owns the command)
        }
    }
    table.push_back(Entry{std::move(name), command});
}

VAML_CORE_EXPORT
ICommand* CommandManager::FindCommand(const std::string& name)
{
    for (const auto& e : command_table())
    {
        if (e.name == name)
            return e.command;
    }
    return nullptr;
}

VAML_CORE_EXPORT
CommandManager::RequeryToken CommandManager::AddRequerySuggested(RequeryHandler handler)
{
    RequeryToken t = next_token();
    handlers().emplace_back(t, std::move(handler));
    next_token() = t + 1;
    return t;
}

VAML_CORE_EXPORT
void CommandManager::RemoveRequerySuggested(RequeryToken token)
{
    auto& hs = handlers();
    for (auto it = hs.begin(); it != hs.end(); ++it)
    {
        if (it->first == token)
        {
            hs.erase(it);
            return;
        }
    }
}

VAML_CORE_EXPORT
void CommandManager::InvalidateRequerySuggested()
{
    // copy-before-invoke: a handler that (un)subscribes mid-fire is safe.
    const auto copy = handlers();
    for (const auto& [t, h] : copy)
    {
        if (h)
            h();
    }
}

// The requery handler list + token counter (function-local statics).
std::vector<std::pair<CommandManager::RequeryToken, CommandManager::RequeryHandler>>&
CommandManager::handlers()
{
    static std::vector<std::pair<RequeryToken, RequeryHandler>> list;
    return list;
}

CommandManager::RequeryToken& CommandManager::next_token()
{
    static RequeryToken t = 1;
    return t;
}

} // namespace vaml
