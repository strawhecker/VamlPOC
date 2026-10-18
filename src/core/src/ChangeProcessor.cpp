// S041 — ChangeProcessor out-of-line definitions.
// Per-thread change-notification engine: ChangeQueue drain + S038 ChangeRecorder
// coalescing (S042 realized end-to-end) into a registration-order dispatch.
// (dep:S040, dep:S038, dep:S037).
//
// Design (recorded in ChangeProcessor.h — NO LAZY LLM): composition of three
// real symbols — ChangeQueue (cross-thread postbox), ChangeRecorder (the S042
// coalescing device, consumed INSIDE the drain), and the DependencyObject
// registration-ordered subscriber registry (the dispatch target). The drain
// swaps the ChangeQueue out (snapshot-then-run) and coalesces, so two enqueues
// of the same property before a drain collapse to ONE notification.
//
// S146 (refactor gate) — the pimpl is COLLAPSED: the cross-thread postbox is
// now a plain by-value member (queue_) of ChangeProcessor (see
// ChangeProcessor.h). No Impl heap node, no impl_-> trampoline. The S037/S041
// queue mutex is unchanged — it lives inside ChangeQueue (the sole kept,
// approved lock; ZERO new locks, ADR-018).

#include "vaml/core/ChangeProcessor.h"

#include "vaml/core/ChangeRecorder.h"

#include <utility>

namespace vaml {

VAML_CORE_EXPORT
ChangeProcessor::ChangeProcessor() = default;

VAML_CORE_EXPORT
ChangeProcessor::~ChangeProcessor() = default;

VAML_CORE_EXPORT
ChangeProcessor::ChangeProcessor(ChangeProcessor&&) noexcept = default;

VAML_CORE_EXPORT
ChangeProcessor& ChangeProcessor::operator=(ChangeProcessor&&) noexcept = default;

VAML_CORE_EXPORT
void ChangeProcessor::enqueue(PropertyId id, const Value& oldValue, const Value& newValue)
{
    queue_.enqueue(id, oldValue, newValue);
}

VAML_CORE_EXPORT
std::size_t ChangeProcessor::pending_count() const noexcept
{
    return queue_.pending_count();
}

VAML_CORE_EXPORT
std::size_t ChangeProcessor::process(const NotifyFn& notify)
{
    // Drain the underlying ChangeQueue (affinity-thread-only; throws
    // WrongThreadException from a non-affine thread, NO-LAZY). Snapshot-then-run
    // is inside ChangeQueue::process (swap-out). We then COALESCE the drained
    // items per property via S038's ChangeRecorder — S042's outcome realized
    // end-to-end: two enqueues of the same property before a drain collapse to
    // ONE notification (first old kept, latest new adopted).
    ChangeRecorder recorder;
    const std::size_t drained = queue_.process(
        [&recorder](PropertyId id, const Value& o, const Value& n) {
            recorder.record(id, o, n);
        });

    // Flush each distinct property exactly once, in FIRST-RECORD order.
    std::size_t emitted = 0;
    if (drained != 0)
    {
        emitted = recorder.flush(notify);
    }
    return emitted;
}

VAML_CORE_EXPORT
void ChangeProcessor::clear()
{
    queue_.clear();
}

VAML_CORE_EXPORT
std::thread::id ChangeProcessor::thread_affinity_id() const noexcept
{
    return queue_.thread_affinity_id();
}

} // namespace vaml
