#pragma once
// S209 — the fence-aware retired-buffer pool (vaml::vulkan::ui::detail), dep:S208.
//
// The "retired-buffer lifetime via fence" half of S209. The S206 UiScene's buffer
// grow path previously FREED the old vertex/index buffer the instant it grew — a
// use-after-free risk under the S166 3-in-flight-frame window (the GPU may still
// be reading the old buffer from an in-flight submit). RetainedBufferPool owns the
// vertex/index buffer GENERATIONS instead:
//
//   * ensure(targetItems) — when the requested capacity exceeds the ACTIVE
//     generation's capacity, creates a NEW larger generation and RETIRES (never
//     frees) the old one, recording the frame at which it was retired.
//   * recycle() — advances the frame counter once per frame (the caller calls it
//     each frame, after submit+present). A retired generation is destroyed (+ its
//     device memory freed) once the in-flight ring has advanced `inFlightCount`
//     frames past its retirement — by then the retiree's last-use submit has
//     completed, because its slot fence has been waited (and thereby proven
//     SIGNALED) as the ring wrapped. This is the zero-UAF guarantee: a retired
//     generation is never freed while the GPU could still be reading it.
//
// This IS the "fence" lifetime: the fence is the S166 in-flight ring; the pool
// tracks it as a monotonically-advancing frame index that the ring's waited
// fences drive. The pure DECISION (the headless-testable contract) is
// should_free_generation(frameLag, inFlightCount) below — "a generation whose
// retirement is `inFlightCount` frames old is safe to free (its slot fence has
// signaled)". The real ensure/recycle (which create/destroy VkBuffers + memory via
// the S164/S200 loader entries) is device-backed — [s209-live] proved, the same
// device-backed split as S166 FrameSync / S206 UiScene.
//
// LIFETIMES: the VulkanLoader + VkDevice must OUTLIVE the pool (it borrows them).
// destroy_all() frees every generation (the UiScene dtor calls it after the draw
// loop is device-idle). ADR-018: zero locks (single-threaded per-frame callers).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {
namespace detail {

// ---------------------------------------------------------------------------
// PURE DECISION SEAM (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The S209 lifetime decision: is a retired generation safe to free given that its
// retirement is `frameLag` frames old and the in-flight ring is `inFlightCount`
// frames deep? TRUE (free) once frameLag >= inFlightCount — by then the retiree's
// last-use submit has completed (its slot fence has SIGNALED as the ring wrapped,
// which the CPU waits on at the start of each frame). FALSE (keep alive, no UAF)
// while frameLag < inFlightCount — the ring has NOT yet re-visited the retiree's
// slot, so the GPU might still hold it. Pure + headless-testable.
VAML_VULKAN_EXPORT bool
should_free_generation(std::uint32_t frameLag, std::uint32_t inFlightCount) noexcept;

// ---------------------------------------------------------------------------
// Real buffer-generation owner (RAII; device-backed)
// ---------------------------------------------------------------------------

// The real S209 retired-buffer pool: owns the ACTIVE generation (the buffers
// current record draws from) + a list of RETIRED generations awaiting the
// in-flight ring's wrap before their memory is freed. Only the UiScene (the
// retain/pool consumer) touches it — it is the named consumer of this pool.
class VAML_VULKAN_EXPORT RetainedBufferPool
{
public:
    // One vertex/index buffer generation with its capacity (in draw items) + the
    // frame index at which it was retired (UINT32_MAX for the still-active one).
    struct Generation
    {
        VkBuffer       vertex      = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer       index       = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory  = VK_NULL_HANDLE;
        std::uint32_t  capacityItems = 0;
        std::uint32_t  retireFrame   = UINT32_MAX;  // active gen: never retired
    };

    // The pool borrows `loader` + `device` (both must outlive it) and
    // `inFlightCount` is the S166 ring depth (choose_concurrency, typically 3).
    RetainedBufferPool(const VulkanLoader& loader, const Device& device,
                       std::uint32_t inFlightCount = 3);
    ~RetainedBufferPool();
    RetainedBufferPool(RetainedBufferPool&&) noexcept;
    RetainedBufferPool& operator=(RetainedBufferPool&&) noexcept;
    RetainedBufferPool(const RetainedBufferPool&)            = delete;
    RetainedBufferPool& operator=(const RetainedBufferPool&) = delete;

    // Ensures the ACTIVE generation can hold `targetItems`. If it already can, no-op.
    // Otherwise creates a NEW active generation sized to `targetItems` and RETIRES
    // (defers the free of) the previous active generation, recording its retire
    // frame. Returns false (no throw) on a creation failure. On failure the previous
    // active generation is left in place (still valid). The INITIAL call (blank pool)
    // just creates the active generation (nothing to retire).
    bool ensure(std::uint32_t targetItems);

    // Advances the frame counter by one (once per frame, after submit+present) and
    // destroys + frees every retired generation whose retirement is at least
    // `inFlightCount` frames old (see should_free_generation). Safe to call every
    // frame even with nothing to free.
    void recycle();

    // The current active generation (what record draws from). Always valid after a
    // successful ensure().
    [[nodiscard]] const Generation& active() const noexcept { return active_; }
    // The count of still-retired (not-yet-freed) generations.
    [[nodiscard]] std::size_t retired_count() const noexcept { return retired_.size(); }
    // The S166 ring depth this pool tracks for its lifetime guarantee.
    [[nodiscard]] std::uint32_t in_flight_count() const noexcept { return inFlight_; }
    // The current frame counter (advanced once per recycle()).
    [[nodiscard]] std::uint32_t frame_index() const noexcept { return frame_; }

    // Destroys + frees EVERY generation (the UiScene dtor — the loop must be
    // device-idle first so no in-flight submit references a generation).
    void destroy_all();

private:
    const VulkanLoader* loader_    = nullptr;
    VkDevice            device_    = VK_NULL_HANDLE;
    VkPhysicalDevice    physical_  = VK_NULL_HANDLE;
    std::uint32_t       inFlight_  = 3;
    std::uint32_t       frame_     = 0;

    Generation                      active_;
    std::vector<Generation>         retired_;
};

} // namespace detail
} // namespace vaml::vulkan::ui
