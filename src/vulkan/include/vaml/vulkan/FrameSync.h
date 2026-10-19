#pragma once
// S166 — Frame sync (semaphores + fences) for the main-thread draw loop.
//
// The S166 title's sync half, delivered as a Cat 1 (library-surface) RAII owner
// (FrameSync) + a set of PURE DECISION SEAMS (headless-testable, the S160/S162/S163/
// S164/S165 precedent — CI proves the decision layer; the LIVE vkCreateSemaphore /
// vkCreateFence / vkWaitForFences path is manual via the POCs).
//
// FrameSync owns, per in-flight frame `f`: one `imageAvailable` semaphore (signaled
// by the swapchain when image `i` is acquired), one `renderFinished` semaphore
// (signaled by the graphics queue when the record+draw is done), and one `inFlight`
// fence (guards the CPU against reusing a command buffer / submit while the GPU is
// still reading it). The CPU tick per frame: acquire (imageAvailable) -> reset fence
// -> record -> submit (waits imageAvailable, signals renderFinished, signals the
// fence) -> present (waits renderFinished). Full CPU-GPU pipelining within the
// single-thread proof.
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable) —
//       describe_semaphore()          (VkSemaphoreCreateInfo, default flags)
//       describe_fence(initiallySignaled) (FENCE_CREATE_SIGNALED_BIT vs 0)
//       choose_concurrency(imageCount)    (clamp to kMaxConcurrentFrames)
//     These are the S166 proof surface proven in ctest WITHOUT a live Vulkan runtime.
//   * `FrameSync::create(...)` — the real creation path: per-frame imageAvailable +
//     renderFinished semaphores + one inFlight fence (all initially signaled — the
//     first-frame handshake so frame 0's wait is a no-op), + vkCreateSemaphore /
//     vkCreateFence (manual/POC-proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader and the VkDevice must OUTLIVE the FrameSync (and the
// draw loop must be idle — a vkDeviceWaitIdle — before destruction, so no submit is
// in flight referencing the semaphores/fence). Destroy order: FrameSync (after the
// last submit + wait) -> CommandPool -> pipeline -> ... -> device. Documented (not
// hidden) — callers own the ordering.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// The maximum number of frames the CPU may have in flight (each in-flight frame owns
// one imageAvailable + one renderFinished + one inFlight). This is the "framed
// concurrency" cap of the S4 single-thread proof — enough to pipeline CPU record of
// frame N+1 against GPU draw of frame N without unbounded latency. A documented
// constant (not magic); choose_concurrency clamps to it.
inline constexpr std::uint32_t kMaxConcurrentFrames = 3;

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The semaphore create info: default flags (no timeline semaphore — a binary
// semaphore). Used for both imageAvailable + renderFinished.
VAML_VULKAN_EXPORT VkSemaphoreCreateInfo describe_semaphore() noexcept;

// The fence create info: initiallySignaled true -> VK_FENCE_CREATE_SIGNALED_BIT
// (the first-frame handshake — see the header comment), false -> 0 (an unsignaled
// fence, normally reset before first use).
VAML_VULKAN_EXPORT VkFenceCreateInfo describe_fence(bool initiallySignaled) noexcept;

// The in-flight frame concurrency for a swapchain with `imageCount` images: clamps
// imageCount to [1, kMaxConcurrentFrames]. Returns the number of frames the draw
// loop keeps in flight (each owning an imageAvailable + renderFinished + inFlight).
VAML_VULKAN_EXPORT std::uint32_t choose_concurrency(std::uint32_t imageCount) noexcept;

// ---------------------------------------------------------------------------
// Real frame-sync owner (RAII)
// ---------------------------------------------------------------------------

// The real S166 frame-sync owner: RAII + move-only. Constructed only via create();
// destroys fences then semaphores in reverse order on destruction. See the header
// comment for LIFETIMES.
class VAML_VULKAN_EXPORT FrameSync
{
public:
    // Creates `frameCount` frames' sync set on `device` via `loader`: per frame an
    // imageAvailable semaphore, a renderFinished semaphore, and an inFlight fence
    // (ALL initially signaled — the first-frame handshake so frame 0's wait is a
    // no-op). `frameCount` = choose_concurrency(imageCount), typically. Returns
    // nullptr (no throw) on failure. `loader` and `device` must outlive the returned
    // object; the draw loop must be idle before destruction.
    [[nodiscard]] static std::unique_ptr<FrameSync>
    create(const VulkanLoader& loader, VkDevice device, std::uint32_t frameCount);

    ~FrameSync();
    FrameSync(FrameSync&&) noexcept;
    FrameSync& operator=(FrameSync&&) noexcept;
    FrameSync(const FrameSync&)            = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    [[nodiscard]] std::uint32_t frame_count() const noexcept { return static_cast<std::uint32_t>(imageAvailable_.size()); }
    // The `f`-th frame's acquire-complete semaphore.
    [[nodiscard]] VkSemaphore image_available(std::uint32_t f) const noexcept { return (f < frame_count()) ? imageAvailable_[f] : VK_NULL_HANDLE; }
    // The `f`-th frame's render-complete semaphore.
    [[nodiscard]] VkSemaphore render_finished(std::uint32_t f) const noexcept { return (f < frame_count()) ? renderFinished_[f] : VK_NULL_HANDLE; }
    // The `f`-th frame's in-flight fence.
    [[nodiscard]] VkFence in_flight_fence(std::uint32_t f) const noexcept { return (f < frame_count()) ? inFlight_[f] : VK_NULL_HANDLE; }

private:
    FrameSync(const VulkanLoader& loader, VkDevice device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice           device_  = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence>     inFlight_;
};

} // namespace vaml::vulkan
