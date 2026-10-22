// S211 — MarshalledScene implementation (vaml::vulkan::ui).
//
// Implements the lock-free scene-change postbox. The logic is documented in
// MarshalledScene.h (the frame-aligned single-produce/single-consume contract +
// the release/acquire handoff). This TU carries only the mechanical moves;
// correctness reasoning lives in the header banner + the headless tests.

#include <vaml/vulkan/ui/MarshalledScene.h>

#include <utility>   // std::move

namespace vaml::vulkan::ui {

void MarshalledScene::mutate(std::span<const DrawItem> items)
{
    // Producer-private coalescing: the LATEST snapshot wins. Assigned (not
    // cleared+re-emitted) so the producer's vector owns the exact latest list.
    pending_.assign(items.begin(), items.end());
    dirty_ = true;
}

std::uint64_t MarshalledScene::publish()
{
    // Nothing to flip (no UI mutation since the last publish, or already flipped).
    if (!dirty_)
        return version_.load(std::memory_order_acquire);

    // BACKPRESSURE: the render thread must have DRAINED the previous version
    // (lastConsumed_ == version_) before the destination "other" slot is free
    // to be overwritten. If it has not yet consumed, RETAIN the pending snapshot
    // and deliver it on the next publish() — a lagging render thread receives the
    // LATEST a frame later (never lost, never torn). Reads both atomics as
    // acquire so the consumer's release-store of lastConsumed_ (which happens
    // AFTER it copied the slot out) is visible here.
    if (version_.load(std::memory_order_acquire) != lastConsumed_.load(std::memory_order_acquire))
        return version_.load(std::memory_order_acquire);

    // Flip allowed: write the producer's snapshot into the OTHER (free) slot,
    // advance the front index, bump + release-store the version. The consumer
    // acquire-loading version_ synchronizes with this release, so it sees the
    // just-written frontIndex_ AND the slot contents (the producer writes both
    // before the release-store).
    const int newIndex = 1 - frontIndex_;
    slots_[newIndex]   = std::move(pending_);
    frontIndex_        = newIndex;
    dirty_             = false;
    pending_.clear();
    const std::uint64_t v = version_.load(std::memory_order_relaxed) + 1u;
    version_.store(v, std::memory_order_release);
    return v;
}

bool MarshalledScene::has_newer() const noexcept
{
    // Acquire: this is the re-encode gate (a newer published version exists). The
    // acquire synchronizes the slot contents written before the producer release.
    return version_.load(std::memory_order_acquire) >
           lastConsumed_.load(std::memory_order_acquire);
}

std::uint64_t MarshalledScene::consume_latest(std::vector<DrawItem>& out)
{
    if (!has_newer())
        return lastConsumed_.load(std::memory_order_acquire);  // idle: out untouched

    const std::uint64_t v = version_.load(std::memory_order_acquire);
    // The producer wrote frontIndex_ before the release-store of version_, so the
    // acquire above makes this read see the LATEST published slot index (no tear).
    const int idx = frontIndex_;
    out           = slots_[idx];

    // Release-store the drained version AFTER the copy: the producer's acquire on
    // lastConsumed_ (the backpressure gate) then sees the copy is complete before
    // it flips the slot again — the lock-free SPSC handoff has no race.
    lastConsumed_.store(v, std::memory_order_release);
    return v;
}

std::uint64_t MarshalledScene::scene_version() const noexcept
{
    return version_.load(std::memory_order_acquire);
}

std::uint64_t MarshalledScene::last_consumed_version() const noexcept
{
    return lastConsumed_.load(std::memory_order_acquire);
}

bool MarshalledScene::pending() const noexcept
{
    return dirty_;
}

} // namespace vaml::vulkan::ui
