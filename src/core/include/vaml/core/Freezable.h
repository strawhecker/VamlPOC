#pragma once
// VAML Freezable — the immutable/animatable content base
// (WPF System.Windows.Freezable). 11n-3-adjacent base; consumed by
// InputBinding (11n-2: InputBinding : Freezable) and by Animatable
// (11n-5: Model3D/Geometry3D : Animatable : Freezable).
//
// WPF-faithful shape: Freezable owns a Freeze state (IsFrozen) + the
// changed/Changed notifications; a frozen Freezable is IMMUTABLE (reads are
// safe; writes throw InvalidOperationException — WPF's
// "Cannot change a Freezable after it has been frozen").
//
// VAML keeps it MINIMAL-faithful (REAL surface, not a STUB / not MINIMAL
// REAL in the scaffold sense — this is a kept base, forward-extended by the
// 11n-2 InputBinding + 11n-5 Animatable steps). It derives from
// DependencyObject (the effective-value/value-store host) so a Freezable can
// carry DPs (e.g. InputBinding's Command/CommandParameter/CommandTarget,
// Geometries' properties).
//
// ADR-018: ZERO new locks (single-threaded affinity; the Freeze state is a
// plain member, not atomic — reads/writes on the affinity thread only).

#include "vaml/core/DependencyObject.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace vaml {

class VAML_CORE_EXPORT Freezable : public DependencyObject
{
public:
    bool is_frozen() const noexcept { return frozen_; }

    // Freezes this Freezable (irreversible — WPF-faithful). After Freeze(),
    // is_frozen() is true and any value write throws InvalidOperationException
    // (the WPF "cannot change a frozen Freezable" guard). The Changed
    // notification is suppressed once frozen (a frozen Freezable cannot
    // change — WPF suppresses Changed on frozen objects).
    void Freeze();

    // --- Changed notification (weak-lifetime contract; the 11n-4
    //     IWeakEventListener subscription-lifetime seam). -------------------
    // Fires when the Freezable's content CHANGES (only while not frozen;
    // frozen objects cannot change, so no Changed fires). REGISTRATION ORDER,
    // copy-before-invoke — the codebase-established token pattern. A subscriber
    // that needs the weak-lifetime contract subscribes via IWeakEventListener
    // (11n-4) rather than holding this owner directly.
    using ChangedHandler = std::function<void()>;
    using ChangedToken   = std::size_t;

    [[nodiscard]] ChangedToken add_changed(ChangedHandler handler);
    void remove_changed(ChangedToken token);

protected:
    // Fires the Changed notification (subclasses call this after mutating
    // content while not frozen). No-op when frozen.
    void fire_changed();

    // Subclasses must reject writes after Freeze() — this helper centralizes the
    // WPF "cannot change a frozen Freezable" throw so every value DP on a
    // Freezable-derived type enforces the freeze invariant uniformly.
    // NOT [[noreturn]]: it only throws when already frozen (a conditional
    // thrower that returns normally while unfrozen).
    void throw_if_frozen(const char* what) const;

private:
    std::vector<std::pair<ChangedToken, ChangedHandler>> changed_;
    ChangedToken next_changed_token_ = 1;
    bool frozen_ = false;
};

} // namespace vaml
