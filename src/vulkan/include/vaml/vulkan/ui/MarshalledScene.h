#pragma once
// S211 (dep:S210) — MARSHALLED-CHANGE CONSUMPTION PER FRAME, first wire
// (vaml::vulkan::ui): the UI thread MUTATES the core visual tree; a separate
// RENDER thread draws. The pure, lock-free scene-change POSTBOX that hands the
// CPU-encoded DrawItem snapshot from the producing UI thread to the consuming
// render thread — "consume the LATEST once per frame".
//
// WHY (the S210-recorded NO-GAP, resolved here): S210 delivered the single-thread
// SceneRenderer (set_scene encodes once, record/recycle every frame). Its NO-GAP
// explicitly deferred "UI-thread → render-thread marshalling" to S211 and named
// the Presenter DrawBody wiring as S211's. The core affinity model (S037/S041/
// S143) makes the value store the UI (affinity) thread's ALONE — a render thread
// cannot safely walk the live tree. So the UI thread (owns the tree + the S207/
// S208 encoder) CPU-encodes the DrawItem snapshot and POSTS it; the render thread
// (owns the GPU SceneRenderer) consumes the LATEST snapshot once per frame and
// re-encodes via set_scene. This "producer-on-UI-thread, consumer-on-render-
// thread" split is the only encoding race-free under the affinity contract, and
// it keeps the GPU encode (set_scene → UiScene::set_items) on the render thread
// exactly as S210 intends.
//
// ADR-018 — ZERO new locks (the standing single-lock governance): this is a
// LOCK-FREE SPSC handoff (single-producer = UI thread, single-consumer = render
// thread), built ONLY on two release/acquire atomics (version_ + lastConsumed_)
// + two alternating snapshot slots — the S146 relaxed-atomic / HeapStats
// precedent (atomics, never a mutex). No std::mutex, no spin, no wait.
//
// FRAME-ALIGNED SINGLE-PRODUCE/SINGLE-CONSUME CONTRACT (the lockdown that makes
// the lock-free double-buffer sound, recorded NO-GAP to the full S620/S626 model):
//   * The UI thread may mutate() as many times as it likes between frames; the
//     mutations COALESCE (latest snapshot wins) into a producer-private pending
//     list — no sync needed (producer-private write).
//   * publish() (the UI thread's frame boundary) flips the pending snapshot to
//     the render thread ONLY when the render thread has consumed the previous
//     version (version_ == lastConsumed_ — the destination slot is FREE). If the
//     render thread has not yet consumed, publish() backpressures: the pending
//     snapshot is RETAINED and delivered on the NEXT publish — a lagging render
//     thread simply receives the LATEST a frame later (correct UI semantics,
//     never a lost update, never a torn slot).
//   * The render thread consumes EXACTLY ONCE per frame: has_newer() gates the
//     re-encode; consume_latest() copies the LATEST slot OUT (the consumer never
//     holds a producer slot), so the producer's next flip target is always free.
//   * No tearing: the consumer reads slots_[frontIndex_] under the acquire of
//     version_; the producer release-stores version_ AFTER writing frontIndex_ +
//     the slot, so the acquire synchronizes the slot contents. Because publish
//     cannot run ahead of lastConsumed_ (backpressure), the producer never
//     reuses a slot the consumer is mid-read of — the lock-free handoff is sound
//     under the frame-aligned contract.
//
// This is a REAL, permanent Cat-1 public symbol (NOT MINIMAL REAL, NOT a STUB):
// kept-and-extended by S212 (vertical slice), S620 (forward-queue / per-event
// marshalling), S626 (multiple UI threads / copy-on-write). It is deliberately
// narrow — the full concurrent live-tree mutation model is S620 (recorded, not
// silent). ADR-011 maintained (lives in vaml-vulkan).

#include <vaml/vulkan/ui/UiScene.h>   // DrawItem (the snapshot element type)

#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// S211 — the lock-free scene-change postbox (see the file banner for the full
// contract). UI thread produces (mutate + publish); render thread consumes once
// per frame (has_newer + consume_latest). Pure atomics + vectors — fully
// headless-testable (no Vulkan runtime), Cat-1 real.
class VAML_VULKAN_EXPORT MarshalledScene
{
public:
    MarshalledScene() = default;

    // SceneRenderer is move-only (returned by value from create), so the postbox
    // must be movable. std::atomic is not itself movable (its deleted copy ctor
    // suppresses any implicit move), so the move is WRITTEN: load-from-source /
    // store-into-dest for the two atomics (relaxed — the source is a freshly
    // created, never-yet-shared postbox, so there is no concurrent access to
    // synchronize against), plus vector/flag steals, then reset the source so it
    // is safe to destroy. Not copyable (an SPSC postbox must not be aliased).
    MarshalledScene(MarshalledScene&& other) noexcept
    {
        steal_from(other);
    }
    MarshalledScene& operator=(MarshalledScene&& other) noexcept
    {
        if (this != &other)
        {
            // Overwriting destroys the current postbox state; then steal.
            MarshalledScene tmp(std::move(other));  // (reuses the move above)
            swap(std::move(tmp));
        }
        return *this;
    }
    MarshalledScene(const MarshalledScene&) = delete;
    MarshalledScene& operator=(const MarshalledScene&) = delete;

    void steal_from(MarshalledScene& o) noexcept
    {
        slots_[0]     = std::move(o.slots_[0]);
        slots_[1]     = std::move(o.slots_[1]);
        frontIndex_   = o.frontIndex_;
        pending_      = std::move(o.pending_);
        dirty_        = o.dirty_;
        version_.store(o.version_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        lastConsumed_.store(o.lastConsumed_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        // Reset the source: it is a valid, empty, version-0 postbox afterward.
        o.slots_[0].clear(); o.slots_[1].clear();
        o.frontIndex_ = 0; o.pending_.clear(); o.dirty_ = false;
        o.version_.store(0, std::memory_order_relaxed);
        o.lastConsumed_.store(0, std::memory_order_relaxed);
    }
    void swap(MarshalledScene&& o) noexcept
    {
        // Transfer `o` (the tmp) into `this`. `this`'s prior state is discarded.
        slots_[0] = std::move(o.slots_[0]);
        slots_[1] = std::move(o.slots_[1]);
        frontIndex_ = o.frontIndex_;
        pending_ = std::move(o.pending_);
        dirty_ = o.dirty_;
        version_.store(o.version_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        lastConsumed_.store(o.lastConsumed_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // ===================== UI-thread (PRODUCER) side =====================
    // Records the LATEST CPU-encoded scene snapshot (the UI thread owns the tree
    // + the S207/S208 encoder, so this is race-free by affinity). COALESCING:
    // each call REPLACES the pending snapshot (latest wins) — the producer may
    // mutate many times between frames. Producer-private; no sync.
    void mutate(std::span<const DrawItem> items);

    // Publish the pending snapshot to the render thread (the UI thread's frame
    // boundary; call ONCE per frame). LOCK-FREE with backpressure: the flip
    // happens only when the render thread has CONSUMED the previous version
    // (version_ == lastConsumed_ — the destination slot is free); otherwise the
    // pending snapshot is retained and delivered on the next call (a lagging
    // render thread receives the LATEST a frame later — never lost, never torn).
    // Returns the version the render thread will observe (0 if nothing was
    // flipped this call — either nothing pending or backpressured).
    std::uint64_t publish();

    // ==================== render-thread (CONSUMER) side ====================
    // True iff a version newer than last_consumed has been published — the
    // re-encode GATE (idle frames: false → no re-encode). Acquire on version_.
    [[nodiscard]] bool has_newer() const noexcept;

    // Copies the LATEST published snapshot into `out` if one is newer than
    // last_consumed, and advances last_consumed; otherwise no-op (out untouched).
    // Returns the drained (now last-consumed) version. Render thread. Idle frame
    // with nothing newer → returns last_consumed, out untouched (the S211
    // "idle frames don't re-encode" proof feeds SceneRenderer::consume_changes).
    std::uint64_t consume_latest(std::vector<DrawItem>& out);

    // ---- accessors (all thread-safe to read) ----
    // The latest published version (never decreases). Acquire on version_.
    [[nodiscard]] std::uint64_t scene_version() const noexcept;
    // The version the render thread last drained.
    [[nodiscard]] std::uint64_t last_consumed_version() const noexcept;
    // True iff a UI mutation is pending but not yet flipped to the render thread.
    // Producer-side query (the coalescing pulse); harmless from any thread.
    [[nodiscard]] bool pending() const noexcept;

private:
    // Two alternating snapshot slots. `frontIndex_` = the index of the LATEST
    // published slot (producer writes it, then release-stores version_; the
    // consumer reads it under the acquire of version_ — synchronized, no tear).
    // The producer writes the OTHER slot (1 - frontIndex_), which is always free
    // under the backpressure contract (the consumer is idle at lastConsumed_ ==
    // version_ before any flip). The consumer copies OUT of slots_[frontIndex_].
    std::vector<DrawItem> slots_[2];
    int frontIndex_ = 0;

    // Producer-private coalescing snapshot (latest mutate() wins) + its dirty
    // pulse. Written only by the UI (producer) thread.
    std::vector<DrawItem> pending_;
    bool dirty_ = false;

    // The two atomics that make the handoff lock-free (release/acquire SPSC):
    //   * version_     — last PUBLISHED version (producer release-store, consumer
    //                    acquire-load). The single "there is a new snapshot" datum.
    //   * lastConsumed_ — last DRAINED version (consumer release-store, producer
    //                    acquire-load). The backpressure gate: publish() may only
    //                    flip when version_ == lastConsumed_.
    std::atomic<std::uint64_t> version_{0};
    std::atomic<std::uint64_t> lastConsumed_{0};
};

} // namespace vaml::vulkan::ui
