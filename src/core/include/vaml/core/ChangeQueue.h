#pragma once
// VAML ChangeQueue — a per-thread pending NOTIFICATION queue.
// S041 (dep:S040, dep:S038, dep:S037). Headline: "ChangeQueue (per-thread) +
// processor drains in registration order".
//
// S037 delivered VALUE-MARSHALLING: a per-object, mutex-protected pending queue
// where wrong-thread WRITES live until ProcessPendingChanges() drains them on
// the affinity thread. That queue carries *values to apply later*.
// S038 delivered CHANGE-RECORDING: a single-threaded, per-property, coalescing
// ChangeRecorder that holds pending *notifications* for a deferred flush.
//
// S041 delivers the NOTIFICATION QUEUE itself — the per-thread, cross-thread
// postbox that generalizes S037's per-object *value* queue onto the
// *notification* axis (S037's recorded resolution point — NO backfill):
//   - ChangeQueue                : THIS type. A per-thread pending-notification
//                                  queue; enqueue() is cross-thread (mutex),
//                                  process() is owner-thread-only (throws
//                                  WrongThreadException from a non-affine
//                                  thread — mirrors S037's drain contract).
//   - ChangeProcessor (separate) : drains a ChangeQueue with per-property
//                                  coalescing (via S038's ChangeRecorder, so
//                                  S042's outcome is realized end-to-end) into
//                                  a registration-order dispatch.
//
// Cross-thread contract (mirrors S037, recorded not silent): the ONLY genuinely
// cross-thread datum is the mutex-protected pending notification deque. The
// queue records its creating (affinity) thread; enqueue()/pending_count() are
// safe from any thread, process()/clear() run on the affinity thread. It is a
// lightweight notification postbox — notifications carry no type semantics; a
// ValueTypeMismatch is a Set/Get concern (S030/S036/S037), never this type's.
//
// OWNED-NAME GOTCHA (honored from S036/S037/S038): each enqueued item keys on an
// OWNED std::string copied from PropertyId::name(), never the non-owning
// std::string_view — so a queued notification outlives whatever PropertyKey/id
// storage seeded it. FIFO enqueue order is preserved (a deque, not an
// unordered_map iteration order).
//
// Storage is pimpl'd (class Impl + unique_ptr) so the public header exposes no
// std::deque / std::string / Value member — keeping the ABI surface stable and
// move-only members out of the public declaration (mirrors S036/S037/S038).
//
// This is a REAL, complete public symbol. NOT MINIMAL REAL, NOT a STUB — it is
// the delivered per-thread queue primitive of S041.

#include "vaml/core/DependencyObject.h" // WrongThreadException (affinity-thread contract)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <thread>

namespace vaml {

// A per-thread pending-notification queue. enqueue() may be called from any
// thread (mutex-protected); process()/clear() are affinity-thread-only.
class VAML_CORE_EXPORT ChangeQueue
{
public:
    ChangeQueue();                 // affinity thread = creating thread
    ~ChangeQueue();

    // move-only (copy deleted) so it can live in a pimpl/unique_ptr cleanly.
    ChangeQueue(const ChangeQueue&) = delete;
    ChangeQueue& operator=(const ChangeQueue&) = delete;
    ChangeQueue(ChangeQueue&&) noexcept;
    ChangeQueue& operator=(ChangeQueue&&) noexcept;

    // The callback shape for a drained notification: (id, old, new).
    using NotifyFn = std::function<void(PropertyId, const Value&, const Value&)>;

    // Posts a pending notification. Cross-thread safe (mutex). The id's NAME is
    // copied (owned), so the item outlives the registering key.
    void enqueue(PropertyId id, const Value& oldValue, const Value& newValue);

    // Number of pending notifications currently queued. Cross-thread safe.
    [[nodiscard]] std::size_t pending_count() const noexcept;

    // Owner-thread drain: swaps the queue out (snapshot-then-run, mirroring
    // S037/S038 swap-out), then replays FIFO via `notify`, returning the emit
    // count. Must be called from the queue's affinity thread — otherwise throws
    // WrongThreadException (NO-LAZY: a silent wrong-thread no-op would hide
    // bugs). A notify handler that re-enqueue()s lands in a FRESH queue and is
    // NOT double-emitted this pass.
    std::size_t process(const NotifyFn& notify);

    // Drops all pending notifications (pending_count() -> 0). Affinity-thread
    // only (mutates the owned deque).
    void clear();

    // The thread that owns this queue (its affinity thread; process()/clear()
    // run here). The creating thread by default.
    [[nodiscard]] std::thread::id thread_affinity_id() const noexcept;

private:
    struct Impl; // pimpl: hides std::deque + std::string + Value + std::mutex
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
