#pragma once
// VAML WeakEventListenerBridge — the weak-subscription holder behind
// IWeakEventListener (11n-4, DAG anchor `IWeakEventListener`).
//
// The weak-LIFETIME contract: a binding / collection-view that implements
// IWeakEventListener is subscribed WEAKLY — the bridge holds a
// `std::weak_ptr<IWeakEventListener>` (not a strong std::function), so the
// SUBSCRIBER (listener) can be collected even while the event source lives.
// On fire, the bridge locks the weak handle; if the listener is gone, it
// silently drops the event (no dangling call, no leak).
//
// This is the honest weak-lifetime seam: the existing S243 strong live
// subscription (liveToken_) is the strong path; the IWeakEventListener bridge
// is the weak path a binding / CollectionView opts into for lifetime safety.

#include "vaml/core/IWeakEventListener.h"
#include "vaml/core/export.h"

#include <memory>

namespace vaml {

// A weak, RAII bridge to an IWeakEventListener. Copyable/movable; the
// subscription is weak in both cases (no strong reference is ever held).
class VAML_CORE_EXPORT WeakEventListenerBridge
{
public:
    // Subscribe to `listener` weakly.
    explicit WeakEventListenerBridge(std::shared_ptr<IWeakEventListener> listener)
        : listener_(std::move(listener))
    {}

    // Fires the weak event IF the listener is still alive. Returns true when
    // delivered (a live listener handled it); false when collected.
    [[nodiscard]] bool fire(const WeakEventManagerTag& managerTag,
                            const void* sender,
                            const Value& args) const
    {
        const auto lk = listener_.lock();
        if (!lk)
            return false;
        return lk->ReceiveWeakEvent(managerTag, sender, args);
    }

    // True while the listener is still alive (lockable).
    [[nodiscard]] bool alive() const noexcept { return !listener_.expired(); }

private:
    std::weak_ptr<IWeakEventListener> listener_;
};

} // namespace vaml
