#pragma once
// S167_1 — the corrective to S167: two-generation atomic swapchain recreation +
// the `SwapchainState` machine + change-driven loop vision + stale-ghost feedback.
//
// S167 delivered destroy-then-create `Swapchain::recreate(w,h)` + the pure
// `should_recreate_swapchain` trigger. S167_1 SUPERSEDES that mechanism with a
// TRANSACTIONAL model: the presentation-facing owner builds `Gen N+1` (swapchain
// + image views + depth + per-image framebuffers) into FRESH temporaries and
// COMMITS (atomically swap active=N+1, then destroy Gen N) ONLY when every step
// has succeeded. ANY failure destroys only the stillborn N+1 temporaries and
// leaves Gen N fully live + presentable. There is NO code path in which a
// null/invalid swapchain is presentable — the "null-handle impossible" property
// is STRUCTURAL (the owner only ever presents its owned, valid generation), not
// asserted.
//
// The owner keeps command pool + frame sync FRAME-SLOT-indexed (independent of
// image count) — it does NOT touch them; pipeline + render pass are size-
// independent — it does NOT rebuild them. Callers never own a raw swapchain; the
// owner holds every presentation-facing handle (and the per-image framebuffers).
//
// CHANGE-DRIVEN / RETAINED LOOP (the WPF-model correction): this component is
// NOT a do-or-die fixed-fps loop. Every draw/present path ASKS state() and obeys
// it; the engine re-renders ONLY on an invalidation/dirty flag or a tick, and
// the pure `next_state` decision gates each acquire/present. Idle -> no presents.
//
// STALE-GHOST FEEDBACK (make bad-state visible, not hung): `feedback_for_state`
// maps Retryable -> a slow-breathing white FROST and Fatal -> a FLAT white overlay
// (hard engine invariant: Retryable != Fatal). Both are owner-reported +
// SKINNABLE by the app (the app owns localization — words-free by default, per the
// user guide). Feedback never steals focus / never intercepts input.
//
// SPLIT (the S162 pure-seam precedent): pure decision seams proven headless on both
// CI lanes (next_state / should_suspend_extent / feedback_for_state) + the real
// `SwapchainStateMachine` owner whose generation build goes through injectable
// `SwapchainStateBuilders` (defaulting to the real Swapchain::create /
// Framebuffer::create paths) — the Rail-1 mock-ICD seam (S176) scripts failures
// there (failed vkCreateSwapchainKHR / image-view / depth / framebuffer build /
// DEVICE_LOST) and asserts Gen N stays live + teardown order reverse.
//
// LIFETIMES: the VulkanLoader, VkDevice, VkPhysicalDevice, VulkanSurface, and
// VkRenderPass must OUTLIVE the machine (the render pass is needed to build the
// per-image framebuffers). On destruction the owner destroys its framebuffers
// then its swapchain (reverse of the build order).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/MsaaTarget.h>
#include <vaml/vulkan/Framebuffer.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// The `SwapchainState` machine (replaces the ambiguous bool-return of S167)
// ---------------------------------------------------------------------------

// The S167_1 presentation state. Every draw/present path ASKS state() and obeys
// it:
//   * Ready      — Gen N is valid + fully presentable -> draw.
//   * Suspended  — zero/minimized extent -> nothing to present, park until a
//                  resize re-drives.
//   * Retryable  — a Gen N+1 build failed, Gen N still live -> keep drawing N,
//                  retry.
//   * Fatal      — device/surface loss -> engine stops + OnPresentationLost.
// Fatal is reachable ONLY from device/surface loss (see next_state). Retryable /
// Suspended keep the engine alive.
enum class SwapchainState
{
    Ready,
    Suspended,
    Retryable,
    Fatal,
};

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// True precisely when `extent` is a zero/minimized extent (width == 0 || height
// == 0) — the "nothing to present, park until resize" condition that drives the
// machine into Suspended.
VAML_VULKAN_EXPORT bool should_suspend_extent(VkExtent2D extent) noexcept;

// The S167_1 state-transition decision given `current` and a `result` from an
// acquire/present pass. Rules (this is the proven machine contract):
//   * VK_ERROR_DEVICE_LOST / VK_ERROR_SURFACE_LOST_KHR -> Fatal. Fatal is
//     reachable ONLY from device/surface loss (the engine stops + raises
//     OnPresentationLost; the app decides recovery — auto-device-rebirth is
//     deferred-not-silent).
//   * extentZero -> Suspended (park until a resize re-drives).
//   * otherwise -> `current` unchanged (VK_SUCCESS and an out-of-date/suboptimal
//     result both keep the current generation live — the owner's prepare_recreate
//     drives Ready vs Retryable from the rebuild itself).
VAML_VULKAN_EXPORT SwapchainState
next_state(SwapchainState current, VkResult result, bool extentZero) noexcept;

// The stale-ghost feedback kind for a state. Hard engine invariant: Retryable
// must map to a DIFFERENT feedback than Fatal (Retryable == slow-breathing white
// FROST, Fatal == FLAT white overlay) so the two are visually distinct. Ready /
// Suspended -> None.
enum class FeedbackKind
{
    None,
    RetryableFrost,  // slow-breathing white frost over Gen N's last-good frame
    FatalWhite,      // flat white overlay + OnPresentationLost
};

VAML_VULKAN_EXPORT FeedbackKind feedback_for_state(SwapchainState state) noexcept;

// ---------------------------------------------------------------------------
// Generation build seam (the Rail-1 mock-ICD injection point)
// ---------------------------------------------------------------------------

// Builds one complete generation. `buildSwapchain` builds the swapchain + image
// views + depth at `w` x `h` (returns nullptr on failure = the stillborn case);
// `buildFramebuffers` builds the per-image framebuffers against a swapchain's
// views/extent/depth (returns empty on failure). The default (real) builders use
// Swapchain::create / Framebuffer::create; tests REPLACE them with scripted fakes
// that mimic a Vulkan ICD (failed vkCreateSwapchainKHR / image-view / depth /
// framebuffer build / DEVICE_LOST) — the S176 Rail-1 seam.
struct VAML_VULKAN_EXPORT SwapchainStateBuilders
{
    std::function<std::unique_ptr<Swapchain>(std::uint32_t w, std::uint32_t h)> buildSwapchain;
    // S168: builds the multisampled render target (msaa color + optional msaa
    // depth at the given size/sampleCount). Returns nullptr on failure (the
    // stillborn generation teardown path) — a failed build is a stillborn
    // generation. This is the S176 mock-ICD seam for the physically-failing
    // MSAA-image/depth build + DEVICE_LOST scenarios (recorded, not silent) —
    // tests REPLACE it with scripted fakes.
    std::function<std::unique_ptr<MsaaTarget>(const Swapchain&, VkSampleCountFlagBits)> buildMsaa;
    // Builds the per-image framebuffers for a swapchain. `msaaTarget` is the
    // S168 multisampled target of the same generation (null for the non-MSAA
    // path). Returns empty on any build failure (the stillborn teardown path).
    std::function<std::vector<std::unique_ptr<Framebuffer>>(const Swapchain&, const MsaaTarget*)> buildFramebuffers;

    // The real builders: Swapchain::create + one Framebuffer per image view
    // (vs the swapchain's depth view) + MsaaTarget::create, all threaded through
    // the S169 `Device` owner (device + physical device borrowed from it).
    // Returns empty/null on any build failure. `renderPass` must outlive the
    // machine.
    static SwapchainStateBuilders
    real(const VulkanLoader& loader,
         const Device& device,
         const VulkanSurface& surface,
         VkRenderPass renderPass,
         VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
};

// ---------------------------------------------------------------------------
// The presentation-facing owner (RAII)
// ---------------------------------------------------------------------------

// The S167_1 two-generation atomic owner. Holds Gen N (live swapchain + image
// views + depth + per-image framebuffers); a recreate is a TRANSACTION via
// prepare_recreate(w,h) that builds Gen N+1 into fresh temporaries and COMMITS
// only on full success. On any failure only the stillborn N+1 temporaries are
// destroyed and Gen N stays live + presentable (state lands Retryable). No path
// presents a null/invalid swapchain — the owner only ever presents its owned,
// valid generation. RAII + move-only.
class VAML_VULKAN_EXPORT SwapchainStateMachine
{
public:
    // Creates a machine holding Gen 0 (the initial swapchain + framebuffers) at
    // `framebufferWidth` x `framebufferHeight`, built via `builders`. Returns
    // nullptr (no throw) if even the INITIAL generation cannot be built (that is
    // the one legitimate "no presentable generation" state — there is no Gen N
    // to fall back to; the engine should treat it as Fatal-equivalent at startup
    // rather than present a null handle). `loader`/the device/`surface`/`renderPass`
    // must outlive the returned object.
    // `sampleCount` is the S168 MSAA sample count for the multisampled target
    // (1_BIT = the non-MSAA S165 path, no resolve/msaa images are built).
    [[nodiscard]] static std::unique_ptr<SwapchainStateMachine>
    create(SwapchainStateBuilders builders,
           std::uint32_t framebufferWidth,
           std::uint32_t framebufferHeight,
           VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

    ~SwapchainStateMachine();
    SwapchainStateMachine(SwapchainStateMachine&&) noexcept;
    SwapchainStateMachine& operator=(SwapchainStateMachine&&) noexcept;
    SwapchainStateMachine(const SwapchainStateMachine&)            = delete;
    SwapchainStateMachine& operator=(const SwapchainStateMachine&) = delete;

    // The current presentation state (Ready/Suspended/Retryable/Fatal). Every
    // draw/present path MUST ask this and obey it.
    [[nodiscard]] SwapchainState state() const noexcept { return state_; }

    // The two-generation atomic recreate TRANSACTION at `w` x `h`. Builds Gen
    // N+1 completely into fresh temporaries; on ANY failure destroys only the
    // stillborn N+1 temporaries, leaves Gen N fully live + presentable, lands
    // `Retryable` (keep drawing N, retry) and returns false. On success COMMITS
    // (swap active=N+1, then destroy Gen N), lands `Ready` and returns true.
    //
    // The caller (the draw/present loop) MUST have quiesced the device
    // (vkDeviceWaitIdle) + drained in-flight frames before calling — no submitted
    // work may reference the old swapchain/views (the S167 "no validation errors
    // on resize" contract, unchanged). The owner does NOT touch command pool +
    // frame sync (FRAME-SLOT-indexed) nor pipeline + render pass (size-independent).
    bool prepare_recreate(std::uint32_t w, std::uint32_t h);

    // Parks the machine into Suspended (zero/minimized extent -> nothing to
    // present). A later prepare_recreate at a non-zero extent re-drives it to
    // Ready (or Retryable on rebuild failure).
    void suspend() noexcept { state_ = SwapchainState::Suspended; }

    // Lands the machine into Fatal (device/surface loss -> engine stops + raises
    // OnPresentationLost; the app decides recovery). Fatal must NOT be entered
    // from a recoverable path.
    void mark_fatal() noexcept { state_ = SwapchainState::Fatal; }

    // --- The live generation (owned by the machine; callers only read) ---
    [[nodiscard]] const Swapchain*        swapchain()  const noexcept { return genN_.get(); }
    [[nodiscard]] VkSwapchainKHR          handle()     const noexcept { return genN_ ? genN_->swapchain() : VK_NULL_HANDLE; }
    [[nodiscard]] VkExtent2D              extent()     const noexcept { return genN_ ? genN_->extent() : VkExtent2D{ 0, 0 }; }
    [[nodiscard]] std::uint32_t           image_count() const noexcept { return genN_ ? genN_->image_count() : 0u; }
    [[nodiscard]] std::optional<VkFormat> depth_format() const noexcept { return genN_ ? genN_->depth_format() : std::nullopt; }
    [[nodiscard]] const std::vector<std::unique_ptr<Framebuffer>>& framebuffers() const noexcept { return framebuffersN_; }
    // The `i`-th per-image framebuffer of the live generation (null when indices are
    // out of range).
    [[nodiscard]] const Framebuffer*      framebuffer(std::uint32_t i) const noexcept
    {
        return (i < framebuffersN_.size()) ? framebuffersN_[i].get() : nullptr;
    }
    // The S168 multisampled render target of the live generation (null for the
    // non-MSAA S165 path / 1_BIT). Owned alongside the framebuffers; generation-scoped.
    [[nodiscard]] const MsaaTarget*       msaa_target() const noexcept { return msaaN_.get(); }
    [[nodiscard]] VkSampleCountFlagBits   sample_count() const noexcept { return sampleCount_; }

private:
    SwapchainStateMachine(SwapchainStateBuilders builders, VkSampleCountFlagBits sampleCount);

    // S168: the sample count + the multisampled target of the live generation.
    VkSampleCountFlagBits sampleCount_ = VK_SAMPLE_COUNT_1_BIT;

    // Members are declared SO THAT destruction runs in REVERSE build order
    // (teardown-order-reverse). C++ destroys in REVERSE declaration order, so the
    // ORDER below is: [trivial state] -> genN_ -> msaaN_ -> framebuffersN_, giving
    // a DESTRUCTION order of framebuffersN_ (first) -> msaaN_ -> genN_ -> state.
    // A per-image framebuffer references BOTH the swapchain's image views and (on
    // the S168 MSAA path) the msaa target's views, so the framebuffers must be
    // destroyed before both; msaaN_ before genN_ so the target's images are freed
    // before the swapchain. See the LIFETIMES comment in the header.
    SwapchainStateBuilders builders_;
    SwapchainState         state_ = SwapchainState::Fatal;  // set by create()
    std::unique_ptr<Swapchain>                                  genN_;
    std::unique_ptr<MsaaTarget>                                 msaaN_;
    std::vector<std::unique_ptr<Framebuffer>>                   framebuffersN_;
};

} // namespace vaml::vulkan
