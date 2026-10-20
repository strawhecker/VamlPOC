// S173 — the Presenter decision seams: headless (Rail 1).
//
// Sprint 4B (S173), the OPTIONAL presentation framework ABOVE the S167_1 primitive.
// This TU proves the framework's OWN loop/state/recovery DECISIONS headlessly,
// WITHOUT a live Vulkan runtime (the standing S160+ precedent — no loader/device/
// surface here). It reuses (NOT re-authors) the S167_1 `SwapchainState` +
// `feedback_for_state` contract and asserts the S173 seam mappings:
//   * iteration_action — Ready/Retryable -> Draw, Suspended -> Park, Fatal -> Stop.
//   * should_render — the change-driven gate: idle (no invalidated, no tickDue) -> no
//     presents; a park/stop state never renders; a drawable state renders on a tick.
//   * presenter_lifetimes_ok — the documented no-handle case is gated headless.
//
// The LIVE framed presenter loop (acquire/record/submit/present through the real
// device) is [both apps, not CI] manual via the POCs; the physically-handle
// Fatal/recovery against a scripted ICD is the S176 mock-ICD lane (deferred-not-silent).

#include <vaml/presentation/Presenter.h>
#include <vaml/vulkan/SwapchainState.h>

#include "doctest/doctest.h"

#include <cstdint>

using namespace vaml::presentation;
using vaml::vulkan::SwapchainState;

// ---------------------------------------------------------------------------
// iteration_action — the S167_1-loop mapping (not re-authored: reuses the state)
// ---------------------------------------------------------------------------
TEST_CASE("S173 iteration_action maps Ready/Retryable to Draw, Suspended to Park, Fatal to Stop")
{
    // Drawable states (Gen N live + presentable in both).
    CHECK(iteration_action(SwapchainState::Ready)     == IterationAction::Draw);
    CHECK(iteration_action(SwapchainState::Retryable) == IterationAction::Draw);
    // Park (zero/minimized extent — nothing to present).
    CHECK(iteration_action(SwapchainState::Suspended) == IterationAction::Park);
    // Stop (device/surface loss — engine stops + fires OnPresentationLost).
    CHECK(iteration_action(SwapchainState::Fatal)     == IterationAction::Stop);
}

// ---------------------------------------------------------------------------
// should_render — the CHANGE-DRIVEN gate (the WPF-model correction)
// ---------------------------------------------------------------------------
TEST_CASE("S173 should_render: idle (no invalidated, no tickDue) never renders")
{
    // The core change-driven property: NO invalidation + NO tick -> NO present,
    // regardless of the state. Idle = no churn, no presents.
    CHECK_FALSE(should_render(false, false, SwapchainState::Ready));
    CHECK_FALSE(should_render(false, false, SwapchainState::Retryable));
    CHECK_FALSE(should_render(false, false, SwapchainState::Fatal));
}

TEST_CASE("S173 should_render: a drawable state renders on a tick or invalidation")
{
    // Ready/Retryable render when there IS a change (a tick or an invalidation).
    CHECK(should_render(true,  false, SwapchainState::Ready));
    CHECK(should_render(false, true,  SwapchainState::Ready));
    CHECK(should_render(true,  false, SwapchainState::Retryable));
    CHECK(should_render(false, true,  SwapchainState::Retryable));
}

TEST_CASE("S173 should_render: a park/stop state never renders even on a tick")
{
    // Suspended (park) and Fatal (stop) never render — even a tick/invalidation.
    CHECK_FALSE(should_render(true, false, SwapchainState::Suspended));
    CHECK_FALSE(should_render(false, true,  SwapchainState::Suspended));
    CHECK_FALSE(should_render(true, false, SwapchainState::Fatal));
    CHECK_FALSE(should_render(false, true,  SwapchainState::Fatal));
}

// ---------------------------------------------------------------------------
// presenter_lifetimes_ok — the documented no-handle case, gated headless
// ---------------------------------------------------------------------------
TEST_CASE("S173 presenter_lifetimes_ok gates an absent loader/render-pass")
{
    // The presenter cannot own the loop without the swapchain + command loader entry
    // points (acquire/submit/present) or a render pass (the per-image framebuffers).
    CHECK_FALSE(presenter_lifetimes_ok(false, false, VK_NULL_HANDLE));
    CHECK_FALSE(presenter_lifetimes_ok(false, true,  VK_NULL_HANDLE));
    CHECK_FALSE(presenter_lifetimes_ok(true,  false, VK_NULL_HANDLE));
    CHECK_FALSE(presenter_lifetimes_ok(true,  true,  VK_NULL_HANDLE));

    // With both loader seams live + a non-null render pass -> the presenter can be
    // created (the ordering itself is the caller's documented contract). A crafted
    // non-null handle value (portable: reinterpret_cast from an integer) proves the
    // null-only gate, not a live handle.
    const VkRenderPass nonNullRp = reinterpret_cast<VkRenderPass>(static_cast<std::uintptr_t>(1));
    CHECK(presenter_lifetimes_ok(true, true, nonNullRp));
}
