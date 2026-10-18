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
// S146 (refactor gate) — the pimpl is COLLAPSED: the item deques, the single
// queue mutex, and the affinity marker are now plain by-value members of
// ChangeQueue (see ChangeQueue.h). No Impl heap node, no impl_-> trampoline.
// The S037/S041 queue mutex stays IN PLACE (never moved); only the protected
// data is moved under lock (standard mutex-owner move practice).

#include "vaml/core/ChangeQueue.h"

#include "vaml/core/DependencyObject.h" // WrongThreadException (thrown by process/clear)
#include "vaml/core/Intern.h"           // S140: owner-thread re-intern of drained names

#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
ChangeQueue::ChangeQueue() = default;

VAML_CORE_EXPORT
ChangeQueue::~ChangeQueue() = default;

// Move ctor: the std::mutex is NOT moved (it stays where it is); only the
// protected deques + affinity are moved from `other` under `other`'s lock, and
// the moved-from queue is left with an empty (valid) state. This preserves the
// mutex-owner invariant the inlined member layout requires.
VAML_CORE_EXPORT
ChangeQueue::ChangeQueue(ChangeQueue&& other) noexcept
    : affinity(std::this_thread::get_id())
{
    std::lock_guard<std::mutex> lock(other.queue_mutex);
    pending       = std::move(other.pending);
    pendingWrites = std::move(other.pendingWrites);
}

// Move assign: same discipline — move the protected data under `other`'s lock.
VAML_CORE_EXPORT
ChangeQueue& ChangeQueue::operator=(ChangeQueue&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    std::lock_guard<std::mutex> lock(other.queue_mutex);
    pending       = std::move(other.pending);
    pendingWrites = std::move(other.pendingWrites);
    affinity      = std::this_thread::get_id();
    return *this;
}

VAML_CORE_EXPORT
void ChangeQueue::enqueue(PropertyId id, const Value& oldValue, const Value& newValue)
{
    // Own the name (S036/S037/S038 gotcha): copy the view into a stable item.
    std::lock_guard<std::mutex> lock(queue_mutex);
    pending.emplace_back(std::string(id.name()), oldValue, newValue);
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::pending_count() const noexcept
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return pending.size();
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::process(const NotifyFn& notify)
{
    // Drain is affinity-thread-only (mirrors S037 D3): a wrong-thread drain
    // would be a silent no-op hiding bugs — throw (NO-LAZY).
    if (affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::process: must be called from the queue's affinity thread");
    }

    // Swap the whole queue out under the lock so cross-thread enqueuers hitting
    // during the drain land in a FRESH queue (their notifications drain next
    // call), never into the one we are about to consume (S037/S038 swap-out).
    std::deque<Item> snapshot;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        snapshot.swap(pending);
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
    if (affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::clear: must be called from the queue's affinity thread");
    }
    std::lock_guard<std::mutex> lock(queue_mutex);
    pending.clear();
}

// === S143: S037 value-write family (same single mutex + affinity marker) ======

VAML_CORE_EXPORT
void ChangeQueue::enqueue_value(std::string_view name, const Value& coerced)
{
    // Own the name (S037 cross-thread marshal): a wrong-thread writer can never
    // hold/intern into the owner's pool. Same queue_mutex as the notification
    // family — the relocated per-object pending mutex (ADR-018, zero new locks).
    std::lock_guard<std::mutex> lock(queue_mutex);
    pendingWrites.emplace_back(std::string(name), coerced);
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::value_pending_count() const noexcept
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return pendingWrites.size();
}

VAML_CORE_EXPORT
std::size_t ChangeQueue::drain_values(const ValueDrainFn& fn)
{
    // Drain is affinity-thread-only (mirrors S037 D3): a wrong-thread drain
    // would mutate the affinity-owned store from the wrong thread — throw
    // (NO-LAZY). This is the S037 per-object drain guard, relocated with the
    // queue.
    if (affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("ChangeQueue::drain_values: must be called from the queue's affinity thread");
    }

    // Swap the whole value-write deque out under the lock so cross-thread
    // writers hitting during the drain land in a FRESH deque (their writes
    // drain next call), never into the one we are about to consume (S037 swap-out).
    std::deque<ValueWrite> snapshot;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        snapshot.swap(pendingWrites);
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
    return affinity;
}

} // namespace vaml
