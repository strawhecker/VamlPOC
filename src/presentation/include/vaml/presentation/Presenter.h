#pragma once
// S173 — Framework Core: the change-driven/retained presentation engine.
//
// Sprint 4B (S173-S175), the OPTIONAL presentation/composition framework ABOVE the
// VAML primitives. The library (vaml-core/vaml-vulkan) stays a NEUTRAL toolkit:
// atomic swapchain generation + `SwapchainState` (S167_1) + neutral tools (stable
// `RenderTarget`, viewport/scissor data) that an app MAY use. The COMPOSITION
// policy is NOT in the library — it lives ONLY in this optional framework, so a
// user can DIY their own on the library tools OR adopt the framework.
// "With-or-without-framework" is preserved: not linking vaml-presentation = the
// primitive-only (D.I.Y.) consumer.
//
// S173 delivers the PRESENTATION/ENGINE half: ONE presenter OWNER that owns the
// draw/present loop, the S166 sync + command resources, the resource lifetimes,
// and the S167_1 `SwapchainStateMachine` two-generation recovery BEHIND
// `OnPresentationLost` + the stale-ghost. App code DECLARES pipelines/scenes (a
// DrawBody) and resizes via request_resize(); it NEVER touches the loop or the
// swapchain. The framework wraps S167_1 + S166 (CommandPool/FrameSync) + S169
// (Device, borrowed) into ONE owner.
//
// Split (the S162..S167_1 pure-seam precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime) —
//       should_render(invalidated, tickDue, state)  (the change-driven gate)
//       iteration_action(state)                     (the loop verb for a state)
//     These reuse the S167_1 `SwapchainState`/`feedback_for_state` contract
//     (NOT re-authored) + prove the framework's loop decisions headlessly.
//   * `Presenter::create(...)` — the real creation path: builds the S167_1
//     SwapchainStateMachine (+ S166 CommandPool + FrameSync) behind one owner.
//   * `Presenter::tick(...)` — the change-driven iteration: reads state(),
//     obeys it, applies a pending resize TRANSACTION, drives acquire->record->
//     submit->present, and fires `OnPresentationLost` on Fatal WITHOUT
//     auto-device-rebirth (deferred-not-silent — the app decides recovery).
//
// LIFETIMES: the VulkanLoader, Device, VulkanSurface, and RenderPass must OUTLIVE
// the Presenter (the machine needs the render pass to build per-image framebuffers;
// the live loop borrows the device's queue + the loader's entry points). On
// destruction the Presenter destroys its owned machine + command pool + frame sync
// (in the documented order) before the app destroys the borrowed device/surface.

#include <vaml/vulkan/SwapchainState.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/FrameSync.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation {

// Forward-declared owner (defined below). Appears in the DrawBody signature.
class Presenter;

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The framework's loop verb for a `SwapchainState` (the S167_1 contract, not
// re-authored — this just maps it to the framework's loop action):
//   * Ready     -> Draw   (Gen N valid + presentable)
//   * Retryable -> Draw   (Gen N still live, under the stale-ghost frost)
//   * Suspended -> Park   (zero/minimized extent — nothing to present)
//   * Fatal     -> Stop   (engine stops + fires OnPresentationLost)
enum class IterationAction
{
    Draw,
    Park,
    Stop,
};

// The loop action for a `SwapchainState`. Ready + Retryable -> Draw (Gen N valid
// + presentable in both); Suspended -> Park; Fatal -> Stop. Pure +
// headless-testable.
VAML_PRESENTATION_EXPORT
IterationAction iteration_action(vaml::vulkan::SwapchainState state) noexcept;

// The CHANGE-DRIVEN gate (the WPF-model correction): re-render ONLY on an
// invalidation/dirty flag or a tick, AND only when the machine is in a drawable
// state (Ready or Retryable — Gen N live + presentable). A park (Suspended) or
// stop (Fatal) state never renders. Idle (no invalidated, no tickDue) -> no
// presents, regardless of state. Pure + headless-testable.
VAML_PRESENTATION_EXPORT
bool should_render(bool invalidated, bool tickDue, vaml::vulkan::SwapchainState state) noexcept;

// The documented presenter LIFETIMES precondition, proven headless: the loader,
// device, surface, and render-pass MUST OUTLIVE the presenter (the machine needs
// the render pass to build per-image framebuffers; the live loop borrows the
// device's queue + the loader's entry points). The structural "outlive" ORDERING
// is a caller contract (documented in the header); this gate proves the
// obviously-invalid NO-HANDLE case: the presenter cannot own the loop when the
// loader lacks the swapchain + command entry points or the render pass is absent.
// Pure + headless-testable — no live Device required.
VAML_PRESENTATION_EXPORT
bool presenter_lifetimes_ok(bool hasSwapchainLoader,
                            bool hasCommandLoader,
                            VkRenderPass renderPass) noexcept;

// ---------------------------------------------------------------------------
// The Presenter owner (RAII) — the S173 ONE-OWNER contract
// ---------------------------------------------------------------------------

// The app's recovery contract on Fatal (device/surface loss). Fires EXACTLY ONCE
// when the machine lands Fatal. The presenter does NOT auto-rebirth the device —
// the app decides recovery (deferred-not-silent). `userData` is
// PresenterOptions.userData; `state` is the Fatal state. The S167_1 stale-ghost
// (feedback_for_state) remains the skinnable presentation surface, reused.
using OnPresentationLostFn = void (*)(void* userData, vaml::vulkan::SwapchainState state);

// The S167_1 stale-ghost feedback remains the app's skinnable recoveully surface
// (feedback_for_state(); reused, not re-authored). The app may pass a callback to
// observe it each tick.
using OnFeedbackFn = void (*)(void* userData, vaml::vulkan::FeedbackKind feedback);

// The app's per-frame draw body — the "app DECLARES pipelines/scenes" seam. The
// presenter owns the loop + acquire + submit + present; the app RECORDS the
// command buffer (its pipeline/scene) onto `cmd` for the acquired `imageIndex`.
// The app reads the machine's live generation via `self.machine()` (framebuffer(
// imageIndex), extent(), depth_format(), ...) + the present extent via
// `self.extent()`. Returns true on a successful record (the presenter proceeds
// to submit + present); false to skip the present (the presenter still advances
// the frame slot). `userData` is PresenterOptions.userData.
using DrawBody = std::function<bool(void* userData,
                                    Presenter& self,
                                    VkCommandBuffer cmd,
                                    std::uint32_t imageIndex)>;

// The S173 Presenter options — the app's DECLARATION. The app declares the
// render-pass extent, the sample count, its draw body, its recovery/feedback
// callbacks; it NEVER declares the loop or the swapchain lifetime.
struct PresenterOptions
{
    // The initial framebuffer (render-pass) size for the swapchain + per-image
    // framebuffers (S167_1). The pass is size-independent; resizes come via
    // request_resize().
    std::uint32_t width            = 800;
    std::uint32_t height           = 600;
    // MSEAA sample count (1_BIT = the non-MSAA S165 path).
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    // The app's per-frame record seam (required; may not be empty).
    DrawBody draw;

    // The recovery contract: fires on Fatal. May be null (the app can poll
    // state() instead); the presenter NEVER auto-rebirths the device.
    OnPresentationLostFn onPresentationLost = nullptr;
    // Optional stale-ghost observer (feedback_for_state reuse).
    OnFeedbackFn onFeedback = nullptr;

    void* userData = nullptr;
};

// The S173 presenter owner: RAII + move-only. Constructed only via create();
// owns the S167_1 SwapchainStateMachine + the S166 CommandPool + FrameSync
// behind ONE presenter. The app never touches the loop or a raw swapchain; it
// ticks the presenter and records via its draw body.
class VAML_PRESENTATION_EXPORT Presenter
{
public:
    // Creates the presenter. Builds the S167_1 machine at `width` x `height` via
    // SwapchainStateBuilders::real (+ the S166 CommandPool + FrameSync, frame-slot
    // indexed). Returns nullptr (no throw) if even the INITIAL generation cannot
    // be built (no Gen N to fall back to — the app treats it as startup failure,
    // never a null presentable). `loader`, `device`, `surface`, `renderPass` must
    // OUTLIVE the returned object.
    [[nodiscard]] static std::unique_ptr<Presenter>
    create(const vaml::vulkan::VulkanLoader& loader,
           const vaml::vulkan::Device& device,
           const vaml::vulkan::VulkanSurface& surface,
           VkRenderPass renderPass,
           const PresenterOptions& options);

    ~Presenter();
    Presenter(Presenter&&) noexcept;
    Presenter& operator=(Presenter&&) noexcept;
    Presenter(const Presenter&)            = delete;
    Presenter& operator=(const Presenter&) = delete;

    // The S167_1 machine owner (the one owner's presentation-facing state). The
    // app reads state()/extent()/framebuffer(i) through it; it never writes a raw
    // swapchain.
    [[nodiscard]] const vaml::vulkan::SwapchainStateMachine& machine() const noexcept { return *machine_; }
    [[nodiscard]] vaml::vulkan::SwapchainStateMachine& machine() noexcept { return *machine_; }
    [[nodiscard]] vaml::vulkan::SwapchainState state() const noexcept { return machine_->state(); }
    [[nodiscard]] VkExtent2D extent() const noexcept { return machine_->extent(); }

    // The borrowed device + loader the app's draw body + the loop share.
    [[nodiscard]] const vaml::vulkan::Device& device() const noexcept { return *device_; }
    [[nodiscard]] const vaml::vulkan::VulkanLoader& loader() const noexcept { return *loader_; }
    // The owned S166 command pool + frame sync (the app's draw body may borrow a
    // command buffer; the frame slot advance is the presenter's).
    [[nodiscard]] const vaml::vulkan::CommandPool& command_pool() const noexcept { return *commandPool_; }
    [[nodiscard]] const vaml::vulkan::FrameSync& frame_sync() const noexcept { return *frameSync_; }

    // Queues a resize of the render-pass extent (the S167_1 two-generation atomic
    // TRANSACTION). The presenter applies it on the next tick() (quiescing the
    // device first). A zero/minimized size parks the machine (Suspended).
    void request_resize(std::uint32_t width, std::uint32_t height);

    // The CHANGE-DRIVEN iteration. Returns the frame slot drained (0..n-1) when a
    // frame was recorded + presented, or std::nullopt when nothing was drawn this
    // iteration (idle, park, stop/fatal, or a retry that skipped the frame). Fires
    // OnPresentationLost exactly once when the machine lands Fatal.
    [[nodiscard]] std::optional<std::uint32_t> tick(bool invalidated, bool tickDue);

private:
    Presenter(const vaml::vulkan::VulkanLoader& loader,
              const vaml::vulkan::Device& device);

    const vaml::vulkan::VulkanLoader* loader_  = nullptr;
    const vaml::vulkan::Device*       device_  = nullptr;
    VkRenderPass                      renderPass_ = VK_NULL_HANDLE;  // borrowed; must outlive

    std::unique_ptr<vaml::vulkan::SwapchainStateMachine> machine_;
    std::unique_ptr<vaml::vulkan::CommandPool> commandPool_;
    std::unique_ptr<vaml::vulkan::FrameSync>   frameSync_;

    PresenterOptions options_;
    std::uint32_t    frameIndex_ = 0;
    bool             resizePending_ = false;
    std::uint32_t    resizeW_ = 0;
    std::uint32_t    resizeH_ = 0;
    bool             fatalReported_ = false;
};

} // namespace vaml::presentation
