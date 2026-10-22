// S210 — SceneRenderer_test: headless proof of the S210 render-thread loop
// contract (single thread first): the retained scene is drawn each frame WITHOUT
// re-encoding, and the fence-driven recycle frees retired generations only after
// the in-flight ring's fence signals. No Vulkan runtime needed — the loop DECISION
// seams (render_loop_steps, should_reencode) + the retain/move-only contract are
// proven headlessly.
//
// NO-GAP (recorded, not silent): the LIVE SceneRenderer::create/set_scene/record/
// recycle (which builds the S209 fence-aware UiScene + records retained draws on a
// real driver) is device-backed — proven by the [s210-live] POC block, not CI (the
// S206/S209 precedent).

#include <vaml/vulkan/ui/SceneRenderer.h>
#include <vaml/vulkan/ui/RetainedBufferPool.h>

#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;

TEST_CASE("S210: the loop-plan is the fixed ordered per-frame sequence")
{
    // The S166/S201/S206 record-plan precedent: the loop's fixed per-frame steps.
    const auto& steps = render_loop_steps();
    REQUIRE(steps.size() == 5u);
    CHECK(steps[0] == RenderLoopStep::WaitFence);
    CHECK(steps[1] == RenderLoopStep::EncodeIfDirty);
    CHECK(steps[2] == RenderLoopStep::RecordRetained);
    CHECK(steps[3] == RenderLoopStep::SubmitPresent);
    CHECK(steps[4] == RenderLoopStep::Recycle);
}

TEST_CASE("S210: the retain gate — only a dirty flag re-encodes")
{
    // The "retained scene drawn each frame" headless proof: an unchanged
    // (not-dirty) frame does NOT re-encode; it only records the retained buffers.
    CHECK_FALSE(should_reencode(false));  // unchanged -> NO re-encode
    CHECK(should_reencode(true));         // dirty (scene changed) -> re-encode
}

TEST_CASE("S210: SceneRenderer is move-only (RAII structural contract)")
{
    // The renderer owns a UiScene (the S209 fence-aware batcher): copy-deleted,
    // move-constructible — the same contract as UiScene/Presenter.
    CHECK_FALSE(std::is_copy_constructible_v<SceneRenderer>);
    CHECK_FALSE(std::is_copy_assignable_v<SceneRenderer>);
    CHECK(std::is_move_constructible_v<SceneRenderer>);
}

// The RetainedBufferPool should_free_generation feeds the S210 recycle: a retired
// generation is free only after the ring wraps (its slot fence SIGNALED). Prove
// the cross-lane contract the loop's Recycle step relies on.
TEST_CASE("S210: the fence-driven recycle frees only after the ring wraps (cross-lane)")
{
    constexpr std::uint32_t kInFlight = 3u;  // the S166 max concurrency
    // Not-yet-signaled (frameLag < inFlight): the retiree is held — no UAF.
    CHECK_FALSE(vaml::vulkan::ui::detail::should_free_generation(0u, kInFlight));
    CHECK_FALSE(vaml::vulkan::ui::detail::should_free_generation(2u, kInFlight));
    // Signaled (ring wrapped): the retiree is safe to free.
    CHECK(vaml::vulkan::ui::detail::should_free_generation(3u, kInFlight));
}
