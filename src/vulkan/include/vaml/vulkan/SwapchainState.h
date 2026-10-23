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
// Generation abstraction (DECISION A  — the S176 mock-ICD injection point)
// ---------------------------------------------------------------------------

// One complete, atomic swapchain GENERATION: the live swapchain + image views +
// depth + (S168) multisampled target + per-image framebuffers, owned as a single
// unit. This is the S176 ENGINEERING-OUT (DECISION A): collapsing the machine's
// three concrete members (genN_/msaaN_/framebuffersN_) into ONE interface makes a
// generation a single injectable object — so the Rail-1 mock-ICD lane can script
// a `MockGeneration` that fails at ANY internal ICD stage (failed
// vkCreateSwapchainKHR / image-view / depth / framebuffer build / DEVICE_LOST)
// HEADLESS, and assert Gen N stays live + teardown-order-reverse against objects.
//
// A generation destroys its internals in REVERSE build order (framebuffers ->
// msaa target -> swapchain) in its own destructor. The real implementation
// (`RealGeneration`) wraps Swapchain + MsaaTarget + Framebuffer; a mock wraps a
// scripted fake. Machine public accessors delegate to the one `genN_`, so the
// Presenter + both POCs keep compiling unchanged.
struct VAML_VULKAN_EXPORT ISwapchainGeneration
{
    virtual ~ISwapchainGeneration() = default;

    [[nodiscard]] virtual const Swapchain*        swapchain()      const noexcept = 0;
    [[nodiscard]] virtual VkSwapchainKHR          handle()         const noexcept = 0;
    [[nodiscard]] virtual VkExtent2D              extent()         const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t           image_count()    const noexcept = 0;
    [[nodiscard]] virtual std::optional<VkFormat> depth_format()   const noexcept = 0;
    [[nodiscard]] virtual VkSampleCountFlagBits   sample_count()   const noexcept = 0;
    [[nodiscard]] virtual const std::vector<std::unique_ptr<Framebuffer>>& framebuffers() const noexcept = 0;
    [[nodiscard]] virtual const Framebuffer*      framebuffer(std::uint32_t i) const noexcept = 0;
    [[nodiscard]] virtual const MsaaTarget*       msaa_target()    const noexcept = 0;
};

// Builds one complete generation. `build` builds the whole generation
// (swapchain + image views + depth + S168 msaa target + per-image framebuffers)
// at `w` x `h`; returns nullptr on ANY failure (the stillborn case). The default
// (real) builder produces a `RealGeneration` via Swapchain::create /
// MsaaTarget::create / Framebuffer::create; tests REPLACE it with a scripted
// `MockGeneration` that mimics a Vulkan ICD (failed vkCreateSwapchainKHR /
// image-view / depth / framebuffer build / DEVICE_LOST) — the S176 Rail-1 seam.
//
// `oldSwapchain` is the CURRENT live swapchain handle when this build is a
// REPLACEMENT (the S167_1 two-generation path: Gen N+1 is built while Gen N is
// still live/presentable on the same surface). The real builder hands it to
// vkCreateSwapchainKHR as `oldSwapchain` so the new swapchain REPLACES the old
// atomically — required by the Vulkan spec + real drivers (NVIDIA faults with an
// access violation if a second swapchain is created for a live surface without
// it). It is VK_NULL_HANDLE for the initial Gen-0 build (the first swapchain for
// the surface).
//
// NOTE (supersedes the pre-S176 buildSwapchain/buildMsaa/buildFramebuffers
// granular hooks): the single `build` generation hook is the S176 seam. The mock
// coaxes a failure at the exact internal stage it wants via its scripted
// generation; the machine observes only "stillborn (nullptr) vs live generation",
// which is precisely the S176 contract (Gen N stays live, never submit N+1).
struct VAML_VULKAN_EXPORT SwapchainStateBuilders
{
    std::function<std::unique_ptr<ISwapchainGeneration>(std::uint32_t w, std::uint32_t h, VkSwapchainKHR oldSwapchain)> build;

    // The real builder: builds a `RealGeneration` (Swapchain::create + one
    // Framebuffer per image view vs the swapchain's depth view + S168
    // MsaaTarget::create), all threaded through the S169 `Device` owner (device +
    // physical device borrowed from it). Returns nullptr on any build failure.
    // `renderPass` must outlive the machine.
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
    // The S176 generation abstraction: every accessor delegates to the one owned
    // `ISwapchainGeneration` (injectable — the Rail-1 mock-ICD seam), so the
    // Presenter + both POCs keep compiling unchanged.
    [[nodiscard]] const Swapchain*        swapchain()  const noexcept { return genN_ ? genN_->swapchain() : nullptr; }
    [[nodiscard]] VkSwapchainKHR          handle()     const noexcept { return genN_ ? genN_->handle() : VK_NULL_HANDLE; }
    [[nodiscard]] VkExtent2D              extent()     const noexcept { return genN_ ? genN_->extent() : VkExtent2D{ 0, 0 }; }
    [[nodiscard]] std::uint32_t           image_count() const noexcept { return genN_ ? genN_->image_count() : 0u; }
    [[nodiscard]] std::optional<VkFormat> depth_format() const noexcept { return genN_ ? genN_->depth_format() : std::nullopt; }
    [[nodiscard]] const std::vector<std::unique_ptr<Framebuffer>>& framebuffers() const noexcept
    {
        return genN_ ? genN_->framebuffers() : emptyFramebuffers_;
    }
    // The `i`-th per-image framebuffer of the live generation (null when indices are
    // out of range).
    [[nodiscard]] const Framebuffer*      framebuffer(std::uint32_t i) const noexcept
    {
        return genN_ ? genN_->framebuffer(i) : nullptr;
    }
    // The S168 multisampled render target of the live generation (null for the
    // non-MSAA S165 path / 1_BIT). Owned alongside the framebuffers; generation-scoped.
    [[nodiscard]] const MsaaTarget*       msaa_target() const noexcept { return genN_ ? genN_->msaa_target() : nullptr; }
    [[nodiscard]] VkSampleCountFlagBits   sample_count() const noexcept { return sampleCount_; }

private:
    SwapchainStateMachine(SwapchainStateBuilders builders, VkSampleCountFlagBits sampleCount);

    // The S168 sample count of the live generation.
    VkSampleCountFlagBits sampleCount_ = VK_SAMPLE_COUNT_1_BIT;

    // The machine's ONLY generation member: the single injectable
    // `ISwapchainGeneration` (the S176 DECISION-A collapse). The generation is
    // atomic — its OWN destructor destroys its internals in reverse build order
    // (framebuffers -> msaa target -> swapchain), so the machine no longer needs
    // three separate members + a teardown-order mirror to hold that invariant.
    // C++ destroys this member last in the machine's own teardown (declared last).
    SwapchainStateBuilders builders_;
    SwapchainState         state_ = SwapchainState::Fatal;  // set by create()
    std::unique_ptr<ISwapchainGeneration> genN_;
    // A stable empty-vector fallback for `framebuffers()` when no generation is
    // live. NOT const (a const data member would make the move ctor/assign
    // deleted); it is never mutated.
    std::vector<std::unique_ptr<Framebuffer>> emptyFramebuffers_{};
};

} // namespace vaml::vulkan
