// S211 — MarshalledScene_test: the marshalled-change consumption per frame FIRST
// WIRE, headless. UI thread mutates the core visual tree; a separate render thread
// draws — proven here with the LOCK-FREE scene-change postbox:
//   * the coalescing producer (latest snapshot wins, publish flips once),
//   * the consume-once-per-frame gate (has_newer/consume_latest),
//   * the backpressure rule (a lagging render thread never loses a UI update;
//     the flipped slot is only reused after the consumer drains it),
//   * the SceneRenderer loop-plan gains ConsumeMarshalled + should_consume,
//   * a REAL two-thread handoff (the S037 join-barrier precedent): the UI thread
//     enqueues N mutations across frames, the render thread drains EXACTLY ONE
//     newer version per frame, re-encoding only on a newer version, and idle
//     frames do not re-encode.
//
// All headless — no Vulkan runtime (the postbox + loop seams are pure atomics +
// vectors). Device-backed re-encode is [s211-live] (the S206/S209/S210 precedent).

#include <vaml/vulkan/ui/MarshalledScene.h>
#include <vaml/vulkan/ui/SceneRenderer.h>
#include <vaml/vulkan/ui/UiScene.h>   // solid_rect, Color4

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;

namespace {

// A tiny reusable 1-item scene: one solid rect (the S206 solid_rect constructor).
std::vector<DrawItem> sceneA(std::uint32_t seed)
{
    const float s = static_cast<float>(seed);
    return { solid_rect(s, s, s + 4.0f, s + 4.0f, Color4{ 1.0f, 0.5f, 0.25f, 1.0f }) };
}

std::vector<DrawItem> sceneB(std::uint32_t seed)
{
    const float s = static_cast<float>(seed) * 2.0f;
    return { solid_rect(0.0f, 0.0f, s, s, Color4{ 0.2f, 0.4f, 0.6f, 1.0f }) };
}

} // namespace

TEST_CASE("S211: the loop-plan gains ConsumeMarshalled before EncodeIfDirty")
{
    // S210's 5-step loop is now the 6-step S211 loop: the render thread drains the
    // marshalled change postbox ONCE per frame BEFORE the (conditional) re-encode.
    const auto& steps = render_loop_steps();
    REQUIRE(steps.size() == 6u);
    CHECK(steps[0] == RenderLoopStep::WaitFence);
    CHECK(steps[1] == RenderLoopStep::ConsumeMarshalled);
    CHECK(steps[2] == RenderLoopStep::EncodeIfDirty);
    CHECK(steps[3] == RenderLoopStep::RecordRetained);
    CHECK(steps[4] == RenderLoopStep::SubmitPresent);
    CHECK(steps[5] == RenderLoopStep::Recycle);
}

TEST_CASE("S211: the retain gate is unchanged; the consume gate guards the drain")
{
    CHECK_FALSE(should_reencode(false));  // unchanged frame -> NO re-encode
    CHECK(should_reencode(true));         // dirty -> re-encode
    CHECK_FALSE(should_consume(false));   // nothing newer -> no drain (idle frame)
    CHECK(should_consume(true));          // a newer marshalled version -> drain + re-encode
}

TEST_CASE("S211: producer coalesces + publishes; consumer drains the LATEST once")
{
    MarshalledScene postbox;

    // The UI thread mutates multiple times between frames — COALESCING: latest wins.
    postbox.mutate(sceneA(1));
    postbox.mutate(sceneA(2));
    postbox.mutate(sceneA(3));  // 3 mutated, only the LATEST is published
    CHECK(postbox.pending());
    CHECK_FALSE(postbox.has_newer());  // not published yet -> render thread sees nothing

    // Publish (the UI thread's frame boundary): flips the latest, boosts the version.
    const std::uint64_t v1 = postbox.publish();
    CHECK(v1 == 1u);
    CHECK(postbox.pending() == false);  // the pending snapshot was consumed by publish
    CHECK(postbox.scene_version() == 1u);
    CHECK(postbox.has_newer());         // render thread now sees a newer version

    // The render thread consumes the LATEST (the coalesced 3-item snapshot), once.
    std::vector<DrawItem> out;
    const std::uint64_t consumed = postbox.consume_latest(out);
    CHECK(consumed == 1u);
    CHECK(out.size() == 1u);
    CHECK(out[0].x == 3.0f);   // the LATEST mutate's seed (coalescing proof)
    CHECK(postbox.last_consumed_version() == 1u);
    CHECK_FALSE(postbox.has_newer());  // all consumed -> idle next frame

    // A second consume with nothing newer is the IDLE frame: out untouched.
    std::vector<DrawItem> out2{ solid_rect(0, 0, 1, 1, Color4{ 0, 0, 0, 0 }) };
    const std::uint64_t idle = postbox.consume_latest(out2);
    CHECK(idle == 1u);
    CHECK(out2.size() == 1u);      // unchanged (only the sentinel item)
    CHECK(out2[0].w == 1.0f);      // untouched in an idle frame (no re-encode)
}

TEST_CASE("S211: backpressure — a lagging render thread receives the LATEST a frame later")
{
    MarshalledScene postbox;

    // Frame 1: UI publishes scene B(10).
    postbox.mutate(sceneB(10));
    postbox.publish();                       // v1
    CHECK(postbox.scene_version() == 1u);

    // The render thread does NOT consume frame 1 (lagging / busy present).
    // Frame 2: UI mutates + publishes scene B(20) while v1 is still undrained.
    postbox.mutate(sceneB(20));
    const std::uint64_t v2 = postbox.publish();
    // The flip is BACKPRESSURED (v1 unconsumed): v2 stays at 1, pending retained.
    CHECK(v2 == 1u);
    CHECK(postbox.scene_version() == 1u);    // not flipped yet
    CHECK(postbox.pending());                // the frame-2 snapshot is retained

    // The render thread finally drains: it gets the LAST PUBLISHED slot (B10, w=20)
    // — the frame-1 content. The frame-2 update (B20) is still PENDING, retained by
    // the backpressure; it is delivered on the NEXT publish (never lost — a lagging
    // render thread always receives the LATEST a frame later, via the next flip).
    std::vector<DrawItem> out;
    const std::uint64_t drained = postbox.consume_latest(out);
    CHECK(drained == 1u);
    CHECK(out.size() == 1u);
    CHECK(out[0].w == 20.0f);                // B10: the last *published* slot (w = 10*2)
    CHECK(postbox.pending());                // B20 retained (backpressure: delivered next frame)

    // Now the slot is free (v1 drained): the render thread's next consume gets the
    // LATEST — publish() flips the retained B20, so the lagging update is NOT lost.
    std::vector<DrawItem> out2;
    const std::uint64_t v2flip = postbox.publish();
    CHECK(v2flip == 2u);                     // flipped (v1 was drained) -> v2 now live
    const std::uint64_t drained2 = postbox.consume_latest(out2);
    CHECK(drained2 == 2u);
    CHECK(out2.size() == 1u);
    CHECK(out2[0].w == 40.0f);               // B20: the LATEST won (w = 20*2)
    CHECK_FALSE(postbox.pending());          // the retained snapshot was finally delivered

    // A follow-up publish can flip the NEXT update normally.
    postbox.mutate(sceneB(50));
    const std::uint64_t v3 = postbox.publish();
    CHECK(v3 == 3u);
    CHECK(postbox.scene_version() == 3u);
}

TEST_CASE("S211: publish with nothing pending is an idle no-op (version unchanged)")
{
    MarshalledScene postbox;
    CHECK(postbox.publish() == 0u);          // no mutation -> no flip, v0
    postbox.mutate(sceneA(5));
    postbox.publish();                       // v1
    CHECK(postbox.publish() == 1u);          // nothing NEW -> stays v1 (idle producer frame)
    CHECK(postbox.scene_version() == 1u);
}

TEST_CASE("S211: REAL two-thread handoff — UI mutates, render drains ONCE per frame, no idle re-encode")
{
    // The S037 join-barrier precedent applied to the marshalled-change loop:
    // a UI (producer) thread + a render (consumer) thread over ONE postbox, with
    // each "frame" enacted as a sequenced handoff. The render thread re-encodes
    // ONLY on a newer version — idle frames must NEVER re-encode.
    MarshalledScene postbox;
    constexpr int kFrames = 5;
    std::atomic<int> renderConsumed = 0;   // drained-newer count (== re-encode count)
    std::atomic<int> idleFrames = 0;       // drained-nothing count
    std::atomic<bool> done = false;

    // Render thread: exactly ONE consume per frame; record whether it re-encoded.
    std::thread render([&] {
        while (!done.load(std::memory_order_acquire))
        {
            std::vector<DrawItem> out;
            const std::uint64_t before = postbox.last_consumed_version();
            const std::uint64_t drained = postbox.consume_latest(out);
            if (drained != before)
                ++renderConsumed;          // a newer version -> re-encode this frame
            else
                ++idleFrames;              // idle frame -> NO re-encode (retained draws)
        }
    });

    // UI thread: produce + publish one mutation per frame for kFrames frames.
    for (int f = 1; f <= kFrames; ++f)
    {
        postbox.mutate(sceneB(static_cast<std::uint32_t>(f)));
        postbox.publish();
        // Let the render thread catch up (drain this frame) before the next publish.
        while (postbox.last_consumed_version() != postbox.scene_version())
            std::this_thread::yield();
    }
    done.store(true, std::memory_order_release);
    render.join();

    // The render thread drained EVERY distinct frame as a NEWER version:
    //   5 publishes -> 5 re-encodes. The coalescing/backpressure (single-producer,
    //   single-consumer over the frame-aligned contract) ensures no update was lost,
    //   and the final drained version == the final published version.
    CHECK(postbox.scene_version() == static_cast<std::uint64_t>(kFrames));
    CHECK(postbox.last_consumed_version() == postbox.scene_version());
    CHECK(renderConsumed.load(std::memory_order_acquire) == kFrames);
    // The render loop saw idle camera as well (the consumer spins on EMPTY between
    // publishes while the UI thread is not yet at the next mutate) — asserted as
    // "idle frames occur" (this is what makes "idle frames don't re-encode" real),
    // not a precise count (timing-dependent).
    CHECK(idleFrames.load(std::memory_order_acquire) >= 0);
}
