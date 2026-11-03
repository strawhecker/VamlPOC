// VAML Freezable — the immutable/animatable content base
// (WPF System.Windows.Freezable). 11n-2/11n-5 base.
//
// REAL, minimal-faithful kept base (NOT a STUB / NOT MINIMAL REAL in the
// scaffold sense). Derived from DependencyObject; consumed by InputBinding
// (11n-2) and Animatable (11n-5). ADR-018: ZERO new locks.

#include "vaml/core/Freezable.h"

#include <stdexcept>   // std::invalid_argument
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
void Freezable::Freeze()
{
    frozen_ = true;
    // WPF-faithful: once frozen, no more Changed fires and the Changed event
    // is detached. We clear the subscriber list so a frozen Freezable holds no
    // references (a frozen object cannot change, so no subscriber is ever
    // needed again). copy-before-clear is unneeded here — Freeze is not a
    // mutation callback cycle.
    changed_.clear();
    next_changed_token_ = 1;
}

VAML_CORE_EXPORT
Freezable::ChangedToken Freezable::add_changed(ChangedHandler handler)
{
    const ChangedToken t = next_changed_token_++;
    changed_.emplace_back(t, std::move(handler));
    return t;
}

VAML_CORE_EXPORT
void Freezable::remove_changed(ChangedToken token)
{
    for (auto it = changed_.begin(); it != changed_.end(); ++it)
    {
        if (it->first == token)
        {
            changed_.erase(it);
            return;
        }
    }
}

VAML_CORE_EXPORT
void Freezable::fire_changed()
{
    if (frozen_)
        return;
    // copy-before-invoke: a handler that (un)subscribes mid-fire is safe.
    const auto copy = changed_;
    for (const auto& [t, h] : copy)
    {
        if (h)
            h();
    }
}

VAML_CORE_EXPORT
void Freezable::throw_if_frozen(const char* what) const
{
    // CONDITIONAL: only throw when actually frozen. When not frozen this
    // returns normally so the write proceeds. WPF-faithful: "Cannot change a
    // Freezable after it has been frozen."
    if (!frozen_)
        return;
    throw std::invalid_argument(std::string("Cannot change a frozen Freezable: ") +
                                (what != nullptr ? what : ""));
}

} // namespace vaml
