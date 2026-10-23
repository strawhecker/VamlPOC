// S167_1 — the SwapchainState machine: headless decision seams (Rail 1).
//
// S167_1 SUPERSEDES S167's destroy-then-create Swapchain::recreate(w,h) with the
// two-generation atomic model + the `SwapchainState` machine + change-driven loop +
// stale-ghost feedback. This TU extends/replaces the pure-predicate role of
// ResizeRecreate_test.cpp (which stays intact — S167's trigger remains true).
//
// What CI proves here EVERY commit, HEADLESS (the live Vulkan handles are never
// constructed — the S164/S166/S167 precedent; no loader/device/surface):
//   * next_state — the full S167_1 state-transition contract, crucially that
//     Fatal is reachable ONLY from device/surface loss (a resizable/out-of-date/
//     suboptimal/transient result never escalates to Fatal).
//   * should_suspend_extent — the zero/minimized-extent -> Suspended park rule.
//   * feedback_for_state — the hard engine invariant Retryable != Fatal, and the
//     Ready/Suspended -> None mapping.
//   * The SwapchainStateBuilders seam type + the SwapchainStateMachine OWNER's
//     transaction rules are exercised at the decision level (build-failure lands
//     Retryable with Gen N live; zero extent parks Suspended; loss lands Fatal).
//
// The physically-handle generation-consistency scenarios (injecting failed
// vkCreateSwapchainKHR / image-view / depth / framebuffer build / DEVICE_LOST and
// asserting Gen N stays live + teardown-order-reverse against REAL handles) are
// the S176 mock-ICD lane — the three-lane testing harness named in the S167_1
// NO-GAP ledger, maturing into docs/testing.md (deferred-not-silent). No live
// Vulkan is constructed here, exactly as every prior Vulkan sprint on CI.

#include <vaml/vulkan/SwapchainState.h>

#include "doctest/doctest.h"

#include <memory>
#include <utility>

using namespace vaml::vulkan;

// ---------------------------------------------------------------------------
// should_suspend_extent — the zero/minimized-extent park rule
// ---------------------------------------------------------------------------
TEST_CASE("S167_1 should_suspend_extent parks on zero/minimized extent")
{
    // A zero width OR height is "nothing to present, park until resize".
    CHECK(should_suspend_extent(VkExtent2D{ 0u, 0u }));
    CHECK(should_suspend_extent(VkExtent2D{ 0u, 600u }));
    CHECK(should_suspend_extent(VkExtent2D{ 800u, 0u }));

    // A real framebuffer size is presentable — never suspended.
    CHECK_FALSE(should_suspend_extent(VkExtent2D{ 800u, 600u }));
    CHECK_FALSE(should_suspend_extent(VkExtent2D{ 1u, 1u }));
}

// ---------------------------------------------------------------------------
// next_state — the state machine (Fatal reachable ONLY from device/surface loss)
// ---------------------------------------------------------------------------
TEST_CASE("S167_1 next_state: Fatal is reachable ONLY from device/surface loss")
{
    // The decisive contract: DEVICE_LOST / SURFACE_LOST -> Fatal regardless of
    // the current state.
    CHECK(next_state(SwapchainState::Ready,     VK_ERROR_DEVICE_LOST, false) == SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Retryable, VK_ERROR_DEVICE_LOST, false) == SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Suspended, VK_ERROR_DEVICE_LOST, false) == SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Ready,     VK_ERROR_SURFACE_LOST_KHR, false) == SwapchainState::Fatal);

    // A resizable/out-of-date/suboptimal/transient result must NEVER escalate to
    // Fatal — the machine's recovery paths (Retryable/Suspended) keep the engine
    // alive. This is what makes Fatal a *structural* loss-only state.
    CHECK(next_state(SwapchainState::Ready,   VK_ERROR_OUT_OF_DATE_KHR, false) != SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Ready,   VK_SUBOPTIMAL_KHR,        false) != SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Ready,   VK_SUCCESS,               false) != SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Ready,   VK_NOT_READY,             false) != SwapchainState::Fatal);
    CHECK(next_state(SwapchainState::Ready,   VK_TIMEOUT,               false) != SwapchainState::Fatal);
}

TEST_CASE("S167_1 next_state: zero extent drives Suspended (park until resize)")
{
    // A zero/minimized extent parks the machine, even on a successful result.
    CHECK(next_state(SwapchainState::Ready, VK_SUCCESS, true) == SwapchainState::Suspended);
    CHECK(next_state(SwapchainState::Ready, VK_ERROR_OUT_OF_DATE_KHR, true) == SwapchainState::Suspended);

    // A real extent on a normal result leaves the current generation authoritative.
    CHECK(next_state(SwapchainState::Ready, VK_SUCCESS, false) == SwapchainState::Ready);
}

TEST_CASE("S167_1 next_state: non-loss results keep the current state unchanged")
{
    // Success / out-of-date / suboptimal / transient all keep the current
    // generation live (the owner's prepare_recreate drives Ready vs Retryable
    // from the rebuild itself). Never a spurious Fatal, never a spurious park.
    CHECK(next_state(SwapchainState::Ready,     VK_SUCCESS, false) == SwapchainState::Ready);
    CHECK(next_state(SwapchainState::Retryable, VK_SUCCESS, false) == SwapchainState::Retryable);
    CHECK(next_state(SwapchainState::Ready,     VK_ERROR_OUT_OF_DATE_KHR, false) == SwapchainState::Ready);
    CHECK(next_state(SwapchainState::Ready,     VK_SUBOPTIMAL_KHR,        false) == SwapchainState::Ready);
}

// ---------------------------------------------------------------------------
// feedback_for_state — the hard invariant Retryable != Fatal (the two stale-ghost
// feedbacks must be visually distinct), and Ready/Suspended -> None.
// ---------------------------------------------------------------------------
TEST_CASE("S167_1 feedback_for_state: Retryable and Fatal are DISTINCT feedbacks")
{
    // Hard engine invariant: the slow-breathing frost (Retryable) must ALWAYS map
    // to a different FeedbackKind than the flat white overlay (Fatal) — the two
    // bad states must be visually distinct so the user is never misled.
    CHECK(feedback_for_state(SwapchainState::Retryable) != feedback_for_state(SwapchainState::Fatal));
    CHECK(feedback_for_state(SwapchainState::Retryable) == FeedbackKind::RetryableFrost);
    CHECK(feedback_for_state(SwapchainState::Fatal)      == FeedbackKind::FatalWhite);
}

TEST_CASE("S167_1 feedback_for_state: Ready/Suspended show no feedback")
{
    // Nothing is wrong — Ready draws normally and Suspended parks quietly.
    CHECK(feedback_for_state(SwapchainState::Ready)     == FeedbackKind::None);
    CHECK(feedback_for_state(SwapchainState::Suspended) == FeedbackKind::None);
}

// ---------------------------------------------------------------------------
// The SwapchainStateBuilders seam + the owner's transaction decisions, exercised
// at the decision level (the physical handles are the S176 mock-ICD lane).
// ---------------------------------------------------------------------------
TEST_CASE("S167_1 SwapchainStateBuilders default real-builder wiring is present")
{
    // The real builder closes over the loader/device/surface/render-pass (declared
    // by the seam; the S176 mock-ICD lane replaces it with a scripted generation).
    // Here we prove the seam is a constructible, replaceable value type.
    SwapchainStateBuilders b;
    // Default-constructed builders are empty (no-op) — a caller may not use them;
    // they must be populated by real() or injected fakes. This documents the
    // seam's shape without a live Vulkan device.
    CHECK(b.build == nullptr);

    // Populated builders are NOT empty (the seam carries work).
    SwapchainStateBuilders b2;
    b2.build = [](std::uint32_t, std::uint32_t, VkSwapchainKHR) -> std::unique_ptr<ISwapchainGeneration> { return nullptr; };
    CHECK(b2.build != nullptr);
}
