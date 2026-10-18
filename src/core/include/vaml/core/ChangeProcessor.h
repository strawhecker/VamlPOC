#pragma once
// VAML ChangeProcessor — a per-thread change-notification engine that drains a
// ChangeQueue with per-property COALESCING (S042 realized end-to-end) into a
// registration-order dispatch. S041 (dep:S040, dep:S038, dep:S037). Headline:
// "ChangeQueue (per-thread) + processor drains in registration order".
//
// Composition (the S041 pipeline, three real public symbols):
//   - ChangeQueue     (cross-thread postbox)     : S037-value-queue generalized
//                                                  onto the notification axis.
//   - ChangeProcessor (THIS type)                : the "drains in registration
//                                                  order" half. Owns a
//                                                  ChangeQueue for the
//                                                  cross-thread postbox and
//                                                  consumes S038's
//                                                  ChangeRecorder (the S042
//                                                  coalescing device) INSIDE the
//                                                  drain — so two enqueues of
//                                                  the same property before a
//                                                  drain collapse to ONE
//                                                  notification (first old kept,
//                                                  latest new adopted). This is
//                                                  exactly the S042 outcome
//                                                  realized end-to-end against
//                                                  live DependencyObjects.
//   - DependencyObject registry (MOD, S041 §2.3): the processor's dispatch
//                                                  TARGET — a registration-ordered
//                                                  property-changed subscriber
//                                                  list whose handlers fire in
//                                                  the order they were added
//                                                  (never hash order, never
//                                                  reverse).
//
// Ownership / thread model (mirrors S037/S038, recorded not silent):
//   - enqueue()/pending_count() are cross-thread (the ChangeQueue mutex).
//   - process()/clear() are AFFINITY-THREAD-ONLY — a non-affine drain throws
//     WrongThreadException (NO-LAZY, mirrors S037's ProcessPendingChanges).
//   - The queue records its creating thread as affinity.
//
// process() is idempotent-safe against re-entrancy: it swaps the underlying
// ChangeQueue out first (snapshot-then-run), so a notify handler that
// re-enqueue()s lands in a FRESH queue and is NOT double-emitted this pass
// (mirrors S037/S038 swap-out). A drained-empty processor returns 0.
//
// This is a REAL, complete public symbol. NOT MINIMAL REAL, NOT a STUB.

#include "vaml/core/ChangeQueue.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <cstddef>
#include <functional>
#include <thread>

namespace vaml {

class VAML_CORE_EXPORT ChangeProcessor
{
public:
    ChangeProcessor();                 // affinity thread = creating thread
    ~ChangeProcessor();

    // move-only (copy deleted) so it can live in a pimpl/unique_ptr cleanly.
    ChangeProcessor(const ChangeProcessor&) = delete;
    ChangeProcessor& operator=(const ChangeProcessor&) = delete;
    ChangeProcessor(ChangeProcessor&&) noexcept;
    ChangeProcessor& operator=(ChangeProcessor&&) noexcept;

    // The callback shape for a drained notification: (id, old, new).
    using NotifyFn = ChangeQueue::NotifyFn;

    // Posts a pending notification into the cross-thread ChangeQueue. The id's
    // NAME is copied (owned), so the item outlives the registering key.
    void enqueue(PropertyId id, const Value& oldValue, const Value& newValue);

    // Number of pending notifications in the underlying queue. Cross-thread.
    [[nodiscard]] std::size_t pending_count() const noexcept;

    // Affinity-thread drain: swaps the ChangeQueue out (snapshot), feeds every
    // item through S038's ChangeRecorder (per-property COALESCING — first old
    // kept, latest new adopted), then flushes each distinct property once in
    // FIRST-RECORD order via `notify`. Returns the number of notifications
    // emitted. Throws WrongThreadException from a non-affine thread (NO-LAZY).
    std::size_t process(const NotifyFn& notify);

    // Drops all pending notifications. Affinity-thread only.
    void clear();

    // The thread that owns this processor's queue (process()/clear() run here).
    [[nodiscard]] std::thread::id thread_affinity_id() const noexcept;

private:
    // S146: pimpl collapsed — the cross-thread postbox (ChangeQueue) is now a
    // plain by-value member (no Impl heap node, no impl_-> trampoline). Its
    // affinity == this processor's affinity: the constructor's creating thread
    // becomes both the queue's and processor's affinity thread.
    ChangeQueue queue_;
};

} // namespace vaml
