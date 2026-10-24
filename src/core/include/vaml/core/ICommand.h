#pragma once
// VAML ICommand — the ICommand-analogue interface (WPF System.Windows.Input.ICommand).
// S246 (dep:S245). Sprint 6 third token.
//
// This is the REAL permanent core surface for the command primitive that the
// MVVM layer (ObservableObject S241 + RelayCommand S246) and the future
// `{Command DoIt}` binding markup (S253) build on — NOT a STUB / NOT MINIMAL
// REAL (complete on delivery).
//
// WPF-faithful ICommand shape, carried as `Value` (A "parameter" matches the
// ObservableObject / binding value domain):
//   - CanExecute(parameter)      — the QUERY (side-effect-free, const);
//   - Execute(parameter)         — the INVOKE;
//   - CanExecuteChanged          — the event a consumer re-queries on (WPF puts
//     the event on ICommand, so ANY command consumer re-queries uniformly).
//
// The CanExecuteChanged event uses the codebase-established TOKEN-SUBSCRIPTION
// pattern (exactly like ObservableObject S241 / DependencyObject S041):
// `add_can_execute_changed(handler) -> token`, `remove_can_execute_changed(token)`,
// fired in REGISTRATION order. This is a strict superset of the iterator-handle
// subscribe/unsubscribe/raise style seen in the external `delegate.h` reference
// (Accupulse project — a cross-repo import, deliberately NOT adopted): token
// removal is safe under re-entrant removal and never dangles an iterator.
//
// ADR-018: ZERO new locks. A command is single-threaded (mutated + queried on
// its owner thread); cross-thread marshalling is S211/S326 territory, not this
// type's.
//
// The pure interface is MOVABLE-consumer-friendly (RelayCommand is the S246
// concrete implementation; it is move-only per the S170 move-only RAII
// precedent). The interface itself is non-copyable (delete copy) — a command is
// a polymorphic owned handle, never copied by value.

#include "vaml/core/Value.h"        // Value
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>

namespace vaml {

// The ICommand-analogue interface.
class VAML_CORE_EXPORT ICommand
{
public:
    // The CanExecuteChanged event handler signature: a parameterless callback
    // invoked when a command SHOULD re-query its CanExecute.
    using CanExecuteChangedHandler = std::function<void()>;
    using CanExecuteChangedToken   = std::size_t;

    virtual ~ICommand() = default;

    // A command is a polymorphic owned handle: never copied by value.
    ICommand(const ICommand&) = delete;
    ICommand& operator=(const ICommand&) = delete;

    // Derived concrete commands are default-constructible (the base's default
    // ctor was suppressed by the deleted copy ctor, so re-declare it here —
    // protected, since ICommand is abstract and only ever a base).
protected:
    ICommand() = default;
    ICommand(ICommand&&) noexcept = default;
    ICommand& operator=(ICommand&&) noexcept = default;

public:

    // --- The invoke -------------------------------------------------------
    // Executes the command with the given parameter. Pure virtual; a concrete
    // command (RelayCommand) invokes its action.
    virtual void Execute(const Value& parameter) = 0;

    // --- The query --------------------------------------------------------
    // Pure + side-effect-free: returns whether the command CAN execute with
    // the given parameter. A command with no predicate returns true.
    [[nodiscard]] virtual bool CanExecute(const Value& parameter) const = 0;

    // --- The CanExecuteChanged event --------------------------------------
    // Registers a handler; returns a removal token. Fired in registration
    // order. A consumer listens, then re-queries CanExecute on a raise.
    [[nodiscard]] virtual CanExecuteChangedToken add_can_execute_changed(CanExecuteChangedHandler handler) = 0;

    // Removes the handler identified by `token`. Returns true when a handler
    // was actually removed; false for an unknown/expired token (no-op).
    virtual bool remove_can_execute_changed(CanExecuteChangedToken token) = 0;
};

} // namespace vaml
