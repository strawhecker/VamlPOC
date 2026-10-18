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
// cross-thread datums are the mutex-protected pending deques — ONE for the S041
// notification family and ONE for the S037 value-write family (S143 relocation),
// guarded by the SAME mutex + SAME affinity marker. The queue records its
// creating (affinity) thread; enqueue()/pending_count()/enqueue_value()/
// value_pending_count() are safe from any thread, process()/clear()/drain_values()
// run on the affinity thread. It is a lightweight postbox — notifications carry
// no type semantics; a ValueTypeMismatch is a Set/Get concern (S030/S036/S037),
// never this type's.
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
//
// S143 (dep:S142) REWRITES S037/S041 IN PLACE: ChangeQueue becomes the SINGLE
// per-thread cross-thread postbox. It now owns BOTH item families under ONE
// mutex + ONE affinity marker:
//   - NOTIFICATIONS (S041, pre-existing): enqueue()/process()/clear().
//   - VALUE WRITES (S037, relocated): enqueue_value()/drain_values()/value_pending_count().
// The per-object pending mutex physically relocates OUT of DependencyObject
// INTO this queue (its raw pending_/queue_mutex_ are removed; DependencyObject
// now holds ONE ChangeQueue). This is the kept mutex relocation executed under
// ADR-018 (the standing pre-approved S037/S041 lock — moved, not created;
// ZERO new locks). The change-notification contract (QUEUED / coalescing /
// registration order) is BEHAVIOR-UNCHANGED.

// NOTE (S142): this header intentionally does NOT include DependencyObject.h.
// DependencyObject now holds a ChangeQueue BY VALUE, so including DependencyObject.h
// here would create a mutual include cycle (DependencyObject.h -> ChangeQueue.h ->
// DependencyObject.h) that leaves ChangeQueue incomplete where DependencyObject
// needs it. WrongThreadException (from DependencyObject.h) is used only in
// ChangeQueue.cpp, which includes DependencyObject.h itself — so the cycle is
// safely broken at this include.
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

    // The thread that owns this queue (its affinity thread; process()/clear()/drain_values()
    // run here). The creating thread by default.
    [[nodiscard]] std::thread::id thread_affinity_id() const noexcept;

    // === S143: S037 value-write family (relocated into the queue) ==========
    // The callback shape for a drained value-write: (ownedName, coerced value).
    using ValueDrainFn = std::function<void(std::string_view, const Value&)>;

    // Posts a pending S037 value-write from a NON-affine thread. The NAME is
    // copied (owned) — a wrong-thread writer can never hold/intern into the
    // owner's pool (S140 DECISION 1). Cross-thread safe (same single mutex as
    // the notification family). The coerced value was already coerce+validated
    // at queue time by the caller.
    void enqueue_value(std::string_view name, const Value& coerced);

    // Number of value-writes currently queued (queued-not-applied). Cross-thread safe.
    [[nodiscard]] std::size_t value_pending_count() const noexcept;

    // Owner-thread drain: swaps out the value-write deque (S037 snapshot-then-run)
    // and replays each {name, coerced} FIFO, returning the count. Must be called
    // from the queue's affinity thread — otherwise throws WrongThreadException. Each
    // drained `name` is RE-INTERNED on the owner thread to its canonical view before
    // the callback (S143 B2), so the callback always receives a content-equal
    // canonical interned view, never a transient/foreign one.
    std::size_t drain_values(const ValueDrainFn& fn);

private:
    struct Impl; // pimpl: hides std::deque + std::string + Value + std::mutex
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
