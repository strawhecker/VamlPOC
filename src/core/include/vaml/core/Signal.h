#pragma once
// VAML Signal<T> — the typed multi-cast signal (S320, Sprint 8, dep:S292).
//
// THE ADR-005 EVENT PRIMITIVE. A `vaml::Signal<Ret(Args...)>` is a typed
// multi-cast callback list with TOKEN-based subscription (add -> token,
// remove(token) -> bool), fired in REGISTRATION ORDER, with a REENTRANCY-SAFE
// fire: a handler may add/remove handlers or destroy the signal while a fire
// is in flight without corrupting the iteration. This is the surface the
// routed-event engine (S321) and the markup wiring (S322) are built on.
//
// DESIGN (recorded — the ADR-005 "Signal<T> with token-based removal" headline):
//   - A handler is a `std::function` (type-erased per the ADR-005 "hot paths
//     avoid type-erasure ... NOT this v1 signal" line — see below).
//   - Registration is APPEND-ONLY: add() pushes at the back and returns a
//     MONOTONIC non-reused token; remove(token) erases by token, preserving
//     the order of what remains (a re-add appends at the end, never resurrects
//     the old position). zero-cost when empty (no allocation for a brand-new
//     signal).
//   - REENTRANCY (the S320 contract's headline): `fire` COPIES the current
//     handler list before invoking (copy-before-invoke, the S041/S244/S245
//     discipline) and guards against the signal being DESTROYED mid-fire via
//     an in-flight counter. Consequences, all deterministic + tested:
//       * a handler REMOVING ITSELF or ANOTHER during a fire: the copy is
//         iterated, so the removal only affects the live list (a later re-fire),
//         never the in-flight iteration — the copy is a snapshot. Removal of a
//         handler that ALREADY ran in this fire simply won't be in the next.
//       * a handler ADDING during a fire: the add lands in the live list and
//         fires on the NEXT fire, never the current one (the snapshot already
//         took the current list) — deterministic, no double-invoke.
//       * a handler DESTROYING the signal mid-fire: the in-flight guard keeps a
//         temporary copy of the handlers alive (the fire's working list is
//         moved out), so the iteration completes safely; the destroyed signal
//         object is no longer usable (UB if touched after — the caller's duty).
//   - ADR-005 says "hot paths avoid type-erasure (typed function pointers /
//     std::move_only_function)". RECORDED DECISION (not silent): v1 uses
//     `std::function` for the handler type. The routed-event / input paths in
//     this sprint are NOT hot GPU-frame paths, and the S320 headline is the
//     reentrancy + token contract, not the last-drop of type-erasure cost.
//     A move_only_function (immovable-captured) signal is a later refinement
//     (recorded NO-GAP; ADR-010-style roadmap note) — never a silent deferral.
//   - ADR-018: ZERO new locks. A signal is used on ONE thread (the UI/affinity
//     thread); cross-thread event marshalling is the Dispatcher's job — DEFERRED
//     to Sprint 11a (the multithreaded-UI construction); no Dispatcher in S8.
//
// FUTURE (ADR-008 — RoutedEvent, the WPF-faithful routed event, IS shipped in
// this sprint family): RoutedEvent.h (S320) + the UIElement routed-handler +
// RaiseEvent bubbling/tunneling engine (S321) build directly on Signal<T>.

#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace vaml {

// A monotonic, never-reused subscription token. add() returns one; remove(token)
// consumes it. Not zero (0 == "no token").
using SignalToken = std::size_t;

// The typed multi-cast signal: Ret(Args...). Move-only RAII; moved-from is
// INERT (fire is a safe no-op, add/remove still work but on the empty moved-to
// target OR the inert moved-from — the caller owns not mixing them).
//
// Reentrancy-safe fire (see the header comment): a handler may safely
// add/remove/destroy during a fire.
template <typename Signature>
class Signal;

template <typename Ret, typename... Args>
class Signal<Ret(Args...)>
{
public:
    using Handler = std::function<Ret(Args...)>;

    Signal() = default;
    ~Signal() = default;

    Signal(const Signal&)            = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&& other) noexcept { *this = std::move(other); }
    Signal& operator=(Signal&& other) noexcept
    {
        if (this != &other)
        {
            handlers_ = std::move(other.handlers_);
            nextToken_ = other.nextToken_;
            inFlight_  = other.inFlight_;
            other.nextToken_ = 1;
            other.inFlight_  = 0;
        }
        return *this;
    }

    // Adds `handler` (must be non-empty; a null handler is ignored) and returns
    // a monotonic token. The handler fires in registration order relative to
    // the other handlers.
    [[nodiscard]] SignalToken add(Handler handler)
    {
        if (!handler)
        {
            return 0;
        }
        const SignalToken token = nextToken_++;
        handlers_.emplace_back(token, std::move(handler));
        return token;
    }

    // Removes the handler identified by `token`. Returns true if `token` was
    // present (and is now removed), false for an unknown/expired token (no-op).
    // Reentrancy-safe during a fire (see header comment).
    [[nodiscard]] bool remove(SignalToken token)
    {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it)
        {
            if (it->first == token)
            {
                handlers_.erase(it);
                return true;
            }
        }
        return false;
    }

    // Fires every registered handler in registration order, passing `args`.
    // Reentrancy-safe (copy-before-invoke + in-flight guard). If any handler
    // returns a value, it is discarded (matches the fire-and-forget multi-cast
    // contract; only the routed-event engine's `handled` side-channel is
    // honored, via RoutedEventArgs mutation — not via a return value).
    void fire(Args... args)
    {
        if (handlers_.empty())
        {
            return;
        }
        // Copy-before-invoke (snapshot) so a handler that mutates the list
        // (add/remove) mid-fire cannot invalidate the iteration. The in-flight
        // guard also keeps the copied handlers alive if `this` is destroyed
        // during the fire (the destroyer's own in-flight observer may use them).
        ++inFlight_;
        std::vector<std::pair<SignalToken, Handler>> snapshot = handlers_;
        for (auto& entry : snapshot)
        {
            if (entry.second)
            {
                entry.second(args...);
            }
        }
        --inFlight_;
    }

    // Fires handlers in registration order until the `stop` predicate returns
    // true (checked AFTER each handler, with the args it may have mutated — the
    // routed event's `handled` side-channel). Stops early (remaining handlers
    // do NOT fire); the predicate can never re-enter this signal. Same
    // reentrancy-safe snapshot semantics as fire().
    template <typename Pred>
    void fire_until(Args... args, Pred stop)
    {
        if (handlers_.empty())
        {
            return;
        }
        ++inFlight_;
        std::vector<std::pair<SignalToken, Handler>> snapshot = handlers_;
        for (auto& entry : snapshot)
        {
            if (!entry.second)
            {
                continue;
            }
            entry.second(args...);
            if (stop(args...))
            {
                break;
            }
        }
        --inFlight_;
    }

    // True when no handler is registered.
    [[nodiscard]] bool empty() const noexcept { return handlers_.empty(); }

    // The number of registered handlers.
    [[nodiscard]] std::size_t count() const noexcept { return handlers_.size(); }

    // True if a fire is currently in flight (a handler is executing). Exposed
    // for the reentrancy tests + introspection.
    [[nodiscard]] bool in_flight() const noexcept { return inFlight_ > 0; }

private:
    std::vector<std::pair<SignalToken, Handler>> handlers_;
    SignalToken nextToken_ = 1;
    std::size_t inFlight_  = 0;
};

} // namespace vaml
