// S041 — ChangeQueue out-of-line definitions.
// Per-thread pending notification queue (dep:S040, dep:S038, dep:S037).
//
// Design (recorded in ChangeQueue.h — NO LAZY LLM): a per-thread notification
// postbox that generalizes S037's per-object *value* queue onto the
// *notification* axis. The ONLY cross-thread datum is the mutex-protected
// pending deque; affinity-thread methods (process/clear) throw WrongThreadException
// from a non-affine thread (mirrors S037's drain contract, NO-LAZY). Keyed per
// item on an OWNED name (S036/S037/S038 gotcha honored) in FIFO order.
//
// Storage is pimpl'd so the public header exposes no std::deque / std::string /
// Value / std::mutex member.

#include "vaml/core/ChangeQueue.h"

#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace vaml {

struct ChangeQueue::Impl
{
    // One pending notification. `name` is an OWNED std::string copied from the
    // PropertyId so the item outlives the registering key (S036/S037 gotcha).
    struct Item
    {
        std::string name;
        Value       oldValue;
        Value       newValue;

        Item(std::string n, Value o, Value v)
            : name(std::move(n)), oldValue(std::move(o)), newValue(std::move(v)) {}
    };

    // FIFO pending notifications, protected by queue_mutex (the ONE cross-thread
    // datum, mirroring S037). process()/clear() run only on the affinity thread.
    std::deque<Item>     pending;
    std::mutex           queue_mutex;
    std::thread::id      affinity = std::this_thread::get_id();
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

VAML_CORE_EXPORT
std::thread::id ChangeQueue::thread_affinity_id() const noexcept
{
    return impl_->affinity;
}

} // namespace vaml
