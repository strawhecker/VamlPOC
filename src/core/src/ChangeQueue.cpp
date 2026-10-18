// S041 + S143 — ChangeQueue out-of-line definitions.
// Per-thread pending queue that carries BOTH the notification family (S041) and
// the S037 value-write family (S143 relocation). dep:S040, dep:S038, dep:S037.
//
// Design (recorded in ChangeQueue.h — NO LAZY LLM): a per-thread postbox that
// generalizes S037's per-object *value* queue AND S041's *notification* queue
// onto ONE cross-thread postbox with a SINGLE mutex + SINGLE affinity marker.
// The ONLY cross-thread datums are the mutex-protected pending deques (one per
// family); affinity-thread methods (process/clear/drain_values) throw
// WrongThreadException from a non-affine thread (mirrors S037's drain contract,
// NO-LAZY). Keyed per item on an OWNED name (S036/S037/S038 gotcha honored) in
// FIFO order.
//
// S143 B2 re-key: the queue's internal index is the S141-pattern content-keyed
// structure (self-owned names, content-equal by-value SIMD probe — no
// unordered_map, no transient std::string). drain_values() also RE-INTERNS each
// drained name on the owner thread so the callback receives a canonical view.
//
// Storage is pimpl'd so the public header exposes no std::deque / std::string /
// Value / std::mutex member.

#include "vaml/core/ChangeQueue.h"

#include "vaml/core/DependencyObject.h" // WrongThreadException (thrown by process/clear)
#include "vaml/core/Intern.h"           // S140: owner-thread re-intern of drained names

#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace vaml {

struct ChangeQueue::Impl
{
    // One pending NOTIFICATION (S041). `name` is an OWNED std::string copied
    // from the PropertyId so the item outlives the registering key.
    struct Item
    {
        std::string name;
        Value       oldValue;
        Value       newValue;

        Item(std::string n, Value o, Value v)
            : name(std::move(n)), oldValue(std::move(o)), newValue(std::move(v)) {}
    };

    // One pending VALUE WRITE (S037, relocated). `name` is OWNED (cross-thread
    // marshal: a wrong-thread writer cannot hold a thread-local intern view).
    struct ValueWrite
    {
        std::string name;
        Value       coerced;

        ValueWrite(std::string n, Value v) : name(std::move(n)), coerced(std::move(v)) {}
    };

    // TWO FIFO pending deques — one per item family — protected by the SAME
    // queue_mutex (the ONE cross-thread datum). process()/clear() (notifications)
    // and drain_values() (value writes) run only on the affinity thread.
    std::deque<Item>       pending;        // S041 notification family
    std::deque<ValueWrite> pendingWrites;  // S037 value-write family
    std::mutex             queue_mutex;
    std::thread::id        affinity = std::this_thread::get_id();
};

VAML_CORE_EXPORT
ChangeQueue::ChangeQueue()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
ChangeQueue::~ChangeQueue() = default;

VAML_CORE_EXPORT
ChangeQueue::ChangeQueue(ChangeQueue&&) noexcept = default;

VAML_CORE_EXPORT
ChangeQueue& ChangeQueue::operator=(ChangeQueue&&) noexcept = default;

VAML_CORE_EXPORT
void ChangeQueue::enqueue(PropertyId id, const Value& oldValue, const Value& newValue)
{
    // Own the name (S036/S037/S038 gotcha): copy the view into a stable item.
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    impl_->pending.emplace_back(std::string(id.name()), oldValue, newValue);
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::pending_count() const noexcept
{
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    return impl_->pending.size();
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::process(const NotifyFn& notify)
{
    // Drain is affinity-thread-only (mirrors S037 D3): a wrong-thread drain
    // would be a silent no-op hiding bugs — throw (NO-LAZY).
    if (impl_->affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::process: must be called from the queue's affinity thread");
    }

    // Swap the whole queue out under the lock so cross-thread enqueuers hitting
    // during the drain land in a FRESH queue (their notifications drain next
    // call), never into the one we are about to consume (S037/S038 swap-out).
    std::deque<Impl::Item> snapshot;
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        snapshot.swap(impl_->pending);
    }

    // FIFO replay in enqueue order (a deque, never unordered_map order).
    std::size_t emitted = 0;
    for (auto& item : snapshot)
    {
        notify(PropertyId::of(item.name), item.oldValue, item.newValue);
        ++emitted;
    }
    return emitted;
}

VAML_CORE_EXPORT
void ChangeQueue::clear()
{
    if (impl_->affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::clear: must be called from the queue's affinity thread");
    }
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    impl_->pending.clear();
}

// === S143: S037 value-write family (same single mutex + affinity marker) ======

VAML_CORE_EXPORT
void ChangeQueue::enqueue_value(std::string_view name, const Value& coerced)
{
    // Own the name (S037 cross-thread marshal): a wrong-thread writer can never
    // hold/intern into the owner's pool. Same queue_mutex as the notification
    // family — the relocated per-object pending mutex (ADR-018, zero new locks).
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    impl_->pendingWrites.emplace_back(std::string(name), coerced);
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::value_pending_count() const noexcept
{
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    return impl_->pendingWrites.size();
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::drain_values(const ValueDrainFn& fn)
{
    // Drain is affinity-thread-only (mirrors S037 D3): a wrong-thread drain
    // would mutate the affinity-owned store from the wrong thread — throw
    // (NO-LAZY). This is the S037 per-object drain guard, relocated with the
    // queue.
    if (impl_->affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::drain_values: must be called from the queue's affinity thread");
    }

    // Swap the whole value-write deque out under the lock so cross-thread
    // writers hitting during the drain land in a FRESH deque (their writes
    // drain next call), never into the one we are about to consume (S037 swap-out).
    std::deque<Impl::ValueWrite> snapshot;
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        snapshot.swap(impl_->pendingWrites);
    }

    // FIFO replay in enqueue order. Each drained name is RE-INTERNED on the
    // owner (affinity) thread to its canonical view (S143 B2): the store keyed
    // on interned string_views, and the queued owned name re-interns to an
    // equal-content view — so the callback always receives a stable canonical
    // view, never a transient one.
    std::size_t emitted = 0;
    for (auto& write : snapshot)
    {
        fn(vaml::intern(write.name), write.coerced);
        ++emitted;
    }
    return emitted;
}

VAML_CORE_EXPORT
std::thread::id ChangeQueue::thread_affinity_id() const noexcept
{
    return impl_->affinity;
}

} // namespace vaml
