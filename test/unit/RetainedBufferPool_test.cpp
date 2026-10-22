// S209 — RetainedBufferPool_test: headless proof of the S209 fence-aware retired-
// buffer pool lifetime decision (vaml::vulkan::ui::detail), dep:S208. No Vulkan
// runtime needed:
// (1) the should_free_generation HEADLINE — a retired generation is safe to free
// once the in-flight ring has wrapped `inFlightCount` frames past its retirement
// (frameLag >= inFlightCount -> its slot fence has SIGNALED), and NOT before (the
// GPU might still hold it — the zero-UAF guard);
// (2) the RAII/structural contract: the pool is move-only, borrows its loader/
// device, tracks retired generations by retire-frame for the fence-driven free.
//
// NO-GAP (recorded, not silent): the LIVE ensure/recycle (which create/destroy
// VkBuffers + device memory over a real driver) is device-backed — proven by the
// [s209-live] POC block, not CI (the S166 FrameSync / S206 UiScene precedent).

#include <vaml/vulkan/ui/RetainedBufferPool.h>

#include <cstdint>
#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui::detail;

TEST_CASE("S209: should_free_generation — free only after the in-flight ring wraps")
{
    // inFlightCount = 3 (the S166 max concurrency). A generation is safe to free
    // once its slot fence has SIGNALED — the ring has wrapped back to (past) the
    // retiree's slot, so the GPU is no longer reading it.
    constexpr std::uint32_t kInFlight = 3u;
    // Immediately after retirement (lag 0, 1, 2 < 3): NOT safe — the retiree's
    // slot fence has not yet been re-waited -> the GPU may still hold the buffer
    // (UAF if freed).
    CHECK_FALSE(should_free_generation(0u, kInFlight));
    CHECK_FALSE(should_free_generation(1u, kInFlight));
    CHECK_FALSE(should_free_generation(2u, kInFlight));
    // lag 3 (== inFlightCount): the ring has wrapped past the retiree -> safe.
    CHECK(should_free_generation(3u, kInFlight));
    CHECK(should_free_generation(4u, kInFlight));
    CHECK(should_free_generation(50u, kInFlight));
}

TEST_CASE("S209: should_free_generation — 1-deep ring frees immediately, deep ring holds")
{
    // A 1-frame ring (no pipelining): the retiree is free as soon as the one frame
    // after it completes (lag >= 1).
    CHECK_FALSE(should_free_generation(0u, 1u));
    CHECK(should_free_generation(1u, 1u));

    // A deep ring holds far longer: a 3-frame-deep ring holds the retiree for 3 frames.
    CHECK_FALSE(should_free_generation(2u, 3u));
    CHECK(should_free_generation(3u, 3u));
}

TEST_CASE("S209: RetainedBufferPool is move-only (RAII structural contract)")
{
    // The pool borrows its loader/device and owns the buffer generations: it must
    // not be copyable (const/move-only) — the same contract as UiScene/Presenter.
    CHECK_FALSE(std::is_copy_constructible_v<RetainedBufferPool>);
    CHECK_FALSE(std::is_copy_assignable_v<RetainedBufferPool>);
    CHECK(std::is_move_constructible_v<RetainedBufferPool>);
}
