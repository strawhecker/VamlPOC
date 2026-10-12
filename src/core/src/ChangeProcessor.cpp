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
// Storage is pimpl'd so the public header exposes no ChangeQueue / std::mutex
// member.

#include "vaml/core/ChangeProcessor.h"

#include "vaml/core/ChangeRecorder.h"

#include <memory>
#include <utility>

namespace vaml {

struct ChangeProcessor::Impl
{
    // The cross-thread postbox. Its affinity == this processor's affinity: the
    // constructor's creating thread becomes both the queue's and processor's
    // affinity thread.
    ChangeQueue queue;
};

VAML_CORE_EXPORT
ChangeProcessor::ChangeProcessor()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
ChangeProcessor::~ChangeProcessor() = default;

VAML_CORE_EXPORT
ChangeProcessor::ChangeProcessor(ChangeProcessor&&) noexcept = default;

VAML_CORE_EXPORT
ChangeProcessor& ChangeProcessor::operator=(ChangeProcessor&&) noexcept = default;

VAML_CORE_EXPORT
void ChangeProcessor::enqueue(PropertyId id, const Value& oldValue, const Value& newValue)
{
    impl_->queue.enqueue(id, oldValue, newValue);
}

VAML_CORE_EXPORT
std::size_t ChangeProcessor::pending_count() const noexcept
{
    return impl_->queue.pending_count();
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
    const std::size_t drained = impl_->queue.process(
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
    impl_->queue.clear();
}

VAML_CORE_EXPORT
std::thread::id ChangeProcessor::thread_affinity_id() const noexcept
{
    return impl_->queue.thread_affinity_id();
}

} // namespace vaml
