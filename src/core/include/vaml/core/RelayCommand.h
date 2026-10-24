#pragma once
// VAML RelayCommand — the concrete ICommand implementation (WPF
// System.Windows.Input.RelayCommand / ICommand + RelayCommand<object>).
// S246 (dep:S245). Sprint 6 third token.
//
// This is the REAL permanent core surface for the command primitive — NOT a
// STUB / NOT MINIMAL REAL (complete on delivery). S246_1/consumers (the S253
// `{Command}` binding markup, the S441 app-shell) invoke + query through the
// ICommand interface; RelayCommand is the default concrete command.
//
// SHAPE (WPF-faithful + WPF-NOTATION-faithful):
//   - constructor requires an ExecuteAction (`std::function<void(const Value&)`)
//     — the whole point of a relay command;
//   - optional CanExecutePredicate (`std::function<bool(const Value&)>`); when
//     absent/null, CanExecute returns TRUE (WPF RelayCommand default);
//   - Execute(parameter) invokes the action with the parameter;
//   - CanExecute(parameter) runs the predicate (side-effect-free);
//   - NotifyCanExecuteChanged() raises the CanExecuteChanged event in
//     REGISTRATION order to every registered handler (the S244 snapshot /
//     S241 registration-order discipline). A consumer raises it when the
//     command's executability may have changed, then re-queries CanExecute.
//
// MOVE-ONLY (owns a handler list + is virtual-rooted per ICommand): the S170
// move-only RAII precedent. Copy deleted; move ctor/assign transfer the action,
// predicate, subscriber list, and next-token counter; a moved-from is INERT
// (its Execute/CanExecute are safe no-ops — no action/predicate — and its
// handler list is emptied, so the dtor never touches stale state).
//
// ADR-018: ZERO new locks (single-threaded, registration-ordered; mirrors
// ObservableObject S241 / DependencyObject S041 subscriber lists).

#include "vaml/core/ICommand.h"

#include <functional>
#include <utility>
#include <vector>

namespace vaml {

class VAML_CORE_EXPORT RelayCommand final : public ICommand
{
public:
    using ExecuteAction       = std::function<void(const Value&)>;
    using CanExecutePredicate = std::function<bool(const Value&)>;

    // REQUIRED action. `predicate` defaults to empty (= always executable).
    explicit RelayCommand(ExecuteAction action, CanExecutePredicate predicate = {})
        : action_(std::move(action)), predicate_(std::move(predicate))
    {
    }
    ~RelayCommand() override = default;

    RelayCommand(const RelayCommand&) = delete;
    RelayCommand& operator=(const RelayCommand&) = delete;

    RelayCommand(RelayCommand&&) noexcept = default;
    RelayCommand& operator=(RelayCommand&&) noexcept = default;

    // --- ICommand ---------------------------------------------------------
    void Execute(const Value& parameter) override;
    [[nodiscard]] bool CanExecute(const Value& parameter) const override;

    [[nodiscard]] ICommand::CanExecuteChangedToken
    add_can_execute_changed(ICommand::CanExecuteChangedHandler handler) override;
    bool remove_can_execute_changed(ICommand::CanExecuteChangedToken token) override;

    // --- RelayCommand-specific -------------------------------------------
    // Raises the CanExecuteChanged event in registration order. A consumer
    // calls this (e.g. from an ObservableObject property setter's change hook)
    // when the command's executability may have changed.
    void NotifyCanExecuteChanged();

private:
    ExecuteAction       action_;
    CanExecutePredicate predicate_;

    struct Sub
    {
        ICommand::CanExecuteChangedToken  token;
        ICommand::CanExecuteChangedHandler handler;
    };
    std::vector<Sub> subs_;
    std::size_t     next_token_ = 1;
};

} // namespace vaml
