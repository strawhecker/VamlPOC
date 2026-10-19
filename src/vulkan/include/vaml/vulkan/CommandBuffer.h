#pragma once
// S166 — Vulkan command pool / command buffer + the draw-loop record decision.
//
// The S166 title's command-buffer half, delivered as a Cat 1 (library-surface) RAII
// owner (CommandPool) + a set of PURE DECISION SEAMS (headless-testable, the
// S160/S162/S163/S164/S165 precedent — CI proves the decision layer; the LIVE
// vkAllocateCommandBuffers / vkCmd* path is manual via the POCs). The command pool
// allocates one PRIMARY command buffer per swapchain image / in-flight frame, and the
// draw-loop record decision (bind pipeline -> viewport/scissor -> push MVP -> begin
// render pass with clear -> draw 3 -> end pass) is captured as a pure `DrawRecordPlan`
// so the exact step sequence is proven headless WITHOUT a device.
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       describe_command_pool(queueFamily)            (TRANSIENT | RESET_COMMAND_BUFFER)
//       describe_command_buffer_alloc(pool, count)    (PRIMARY, `count` buffers)
//       describe_command_buffer_begin()               (ONE_TIME_SUBMIT, no inheritance)
//       describe_clear_value(color)                   (the color clear for the pass)
//       build_render_pass_begin(renderPass, framebuffer, extent, clearValue, hasDepth)
//                                                     (the fully-populated render-path begin)
//       draw_record_plan()                            (the FIXED ordered draw step sequence)
//     These are the S166 proof surface proven in ctest WITHOUT a live Vulkan runtime.
//     The Live draw body consumes them.
//   * `CommandPool::create(...)` — the real creation path: vkCreateCommandPool +
//     vkAllocateCommandBuffers of `imageCount` PRIMARY buffers (retained in a fixed
//     vector, never reallocated per frame), owns them, vkFreeCommandBuffers +
//     vkDestroyCommandPool on destruction (manual/POC-proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader and the VkDevice must OUTLIVE the CommandPool (and the
// draw loop must be idle — a vkDeviceWaitIdle — before destroying the pool, so no
// command buffer is in flight). Destroy order: CommandPool (after the last submit +
// a fence/queue wait) -> pipeline -> ... -> device. Documented (not hidden) — callers
// own the ordering.
//
// NO-GAP recorded (not silent): S167's resize-safe recreation reuses this per-image
// command-pool/buffer set on swapchain recreation; S169's RAII consolidation treats
// this owner (with FrameSync) as its base; S210/S211's render-thread loop reuses the
// same command-buffer-per-in-flight-frame shape.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The command-pool create info: flags TRANSIENT | RESET_COMMAND_BUFFER. Per-frame
// re-record requires RESET; TRANSIENT tells the driver the buffers are short-lived.
// `queueFamily` is passed through (the graphics-family index from queue selection).
VAML_VULKAN_EXPORT VkCommandPoolCreateInfo
describe_command_pool(std::uint32_t queueFamily) noexcept;

// The command-buffer allocation info: LEVEL_PRIMARY, `count` buffers from `pool`.
// One primary buffer per swapchain image / in-flight frame.
VAML_VULKAN_EXPORT VkCommandBufferAllocateInfo
describe_command_buffer_alloc(VkCommandPool pool, std::uint32_t count) noexcept;

// The command-buffer begin info: ONE_TIME_SUBMIT (each frame is a self-contained
// record), no inheritance (no secondary buffers in S166).
VAML_VULKAN_EXPORT VkCommandBufferBeginInfo
describe_command_buffer_begin() noexcept;

// The clear value for the pass's color attachment. `color` is [r, g, b, a]; S166 uses
// a fixed light/gray clear so the blue triangle is visible against it.
VAML_VULKAN_EXPORT VkClearValue
describe_clear_value(const float color[4]) noexcept;

// The fully-populated VkRenderPassBeginInfo: the render pass + one framebuffer +
// the render area = the framebuffer extent at (0,0) + the clear values. Attachment
// count = framebuffer_attachment_count(1, hasDepth); clearValues = [colorClear,
// (depth 1.0/0 clears when hasDepth)]. `clearValue` is the color clear (the pass's
// LOAD_OP_CLEAR uses it verbatim).
VAML_VULKAN_EXPORT VkRenderPassBeginInfo
build_render_pass_begin(VkRenderPass renderPass,
                        VkFramebuffer framebuffer,
                        VkExtent2D extent,
                        VkClearValue clearValue,
                        bool hasDepth) noexcept;

// ---------------------------------------------------------------------------
// The draw-loop record DECISION (pure; headless-provable)
// ---------------------------------------------------------------------------

// The ordered steps a single frame's command buffer records, in EXACT order. S166's
// step sequence binds the pipeline, sets the dynamic viewport + scissor (from
// viewport_scissor(extent)), pushes the MVP (tri_mvp(width,height), 64 bytes,
// VERTEX stage), begins the render pass (with the cleared framebuffer), draws 3
// vertices (the S165 triangle), and ends the pass. The enum + draw_record_plan are
// the S166 headless contract: CI asserts the exact step sequence + payload without a
// GPU, mirroring S165's tri_mvp-kept-and-extended approach.
enum class RecordStep
{
    BindPipeline,
    SetViewport,
    SetScissor,
    PushMvp,       // push_constant_range(): VERTEX, offset 0, size 64
    BeginRenderPass,  // build_render_pass_begin + describe_clear_value
    Draw,          // vkCmdDraw(3, 1, 0, 0)
    EndRenderPass,
};

// The S166 record plan: the FIXED ordered step list (see RecordStep). Returns the
// steps S166's live draw body runs verbatim. The payload facts (MVP pushed is
// tri_mvp(width,height); the draw is 3 vertices/1 instance/0/0; the clear is
// describe_clear_value) are asserted by CommandSync_test independently.
VAML_VULKAN_EXPORT const std::vector<RecordStep>& draw_record_plan() noexcept;

// ---------------------------------------------------------------------------
// Real command-pool owner (RAII)
// ---------------------------------------------------------------------------

// The real S166 command-pool owner: RAII + move-only, owning BOTH the command pool
// and its `imageCount` PRIMARY command buffers (one per swapchain image / in-flight
// frame). Constructed only via create(); destroys pool -> buffers in reverse order on
// destruction. See the header comment for LIFETIMES.
class VAML_VULKAN_EXPORT CommandPool
{
public:
    // Creates a command pool on `device` via `loader` for `queueFamily`, allocating
    // `imageCount` PRIMARY command buffers (one per swapchain image / in-flight frame).
    // Runs the S166 seams (describe_command_pool + describe_command_buffer_alloc),
    // calls vkCreateCommandPool + vkAllocateCommandBuffers. Returns nullptr (no throw)
    // on failure. `loader` and `device` must outlive the returned object; the draw
    // loop must be idle before destruction.
    [[nodiscard]] static std::unique_ptr<CommandPool>
    create(const VulkanLoader& loader, VkDevice device, std::uint32_t queueFamily, std::uint32_t imageCount);

    ~CommandPool();
    CommandPool(CommandPool&&) noexcept;
    CommandPool& operator=(CommandPool&&) noexcept;
    CommandPool(const CommandPool&)            = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    [[nodiscard]] VkCommandPool native_handle() const noexcept { return pool_; }
    [[nodiscard]] VkCommandPool pool()          const noexcept { return pool_; }
    [[nodiscard]] std::uint32_t count()         const noexcept { return static_cast<std::uint32_t>(commandBuffers_.size()); }
    // The `i`-th primary command buffer (one per swapchain image / in-flight frame).
    [[nodiscard]] VkCommandBuffer command_buffer(std::uint32_t i) const noexcept { return (i < count()) ? commandBuffers_[i] : VK_NULL_HANDLE; }
    [[nodiscard]] const std::vector<VkCommandBuffer>& command_buffers() const noexcept { return commandBuffers_; }

private:
    CommandPool(const VulkanLoader& loader, VkDevice device);

    const VulkanLoader*        loader_ = nullptr;
    VkDevice                   device_ = VK_NULL_HANDLE;
    VkCommandPool              pool_   = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
};

} // namespace vaml::vulkan
