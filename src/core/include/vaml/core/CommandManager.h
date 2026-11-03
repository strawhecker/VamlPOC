#pragma once
// VAML CommandManager — the static routed-command table + re-query hub
// (WPF System.Windows.Input.CommandManager). 11n-2.
//
// WPF-faithful responsibilities (verified from
// E:\GitRepos\wpf\src\...\Input\Command\CommandManager.cs):
//   1. the ROUTED-COMMAND TABLE: a static store mapping a routed-command
//      NAME to its RoutedCommand instance (WPF registers well-known routed
//      commands like ApplicationCommands.Copy/Paste; VAML registers app +
//      framework routed commands by name);
//   2. RequerySuggested: the global "re-query your CanExecute" signal — when
//      it fires, every command consumer (a ButtonBase : ICommandSource)
//      re-queries its Command.CanExecute. In the single-affinity-thread model
//      this is an on-thread notification, not background scheduling.
//
// PER DECISION-1: the table stores `ICommand*` (the ONE S246 interface), so a
// registered name may resolve to a RoutedCommand OR any other concrete
// ICommand (e.g. a RelayCommand) uniformly. CommandManager never owns the
// commands (non-owning pointers; app/framework lifetime).

#include "vaml/core/ICommand.h"
#include "vaml/core/RoutedCommand.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

class VAML_CORE_EXPORT CommandManager
{
public:
    // --- Routed-command table ----------------------------------------------
    // Registers a command under a NAME (idempotent: first registration wins,
    // the app owns the command — mirroring VamlReader::register_command).
    // Accepts any ICommand* (RoutedCommand or otherwise) per DECISION-1.
    static void RegisterCommand(std::string name, ICommand* command);
    // Resolves a name -> ICommand* (nullptr when unregistered).
    [[nodiscard]] static ICommand* FindCommand(const std::string& name);

    // --- RequerySuggested ---------------------------------------------------
    // The global "re-query your CanExecute" signal. Fires every registered
    // handler, which re-queries its command's CanExecute. ZERO new locks —
    // single-threaded affinity (S043-static patterns: fired on the affinity
    // thread; the token pattern for safe (un)subscription).
    using RequeryHandler = std::function<void()>;
    using RequeryToken   = std::size_t;

    // Adds a handler; returns a token. Fires in registration order.
    [[nodiscard]] static RequeryToken AddRequerySuggested(RequeryHandler handler);
    static void RemoveRequerySuggested(RequeryToken token);

    // Raises RequerySuggested (the "something changed, re-query" kick). Called
    // after a command's input gesture / state changes on the affinity thread.
    static void InvalidateRequerySuggested();

    // A command table row (name -> non-owning ICommand*). Public so the
    // .cpp static accessors + tests can read entries.
    struct Entry
    {
        std::string name;
        ICommand*   command;
    };

private:
    static std::vector<Entry>& table();
    static std::vector<std::pair<RequeryToken, RequeryHandler>>& handlers();
    static RequeryToken& next_token();
};

} // namespace vaml
