#pragma once
// S168 — the multisampled (color + optional depth) render target for MSAA.
//
// Delivered as a Cat 1 (library-surface) RAII owner. S168 renders the triangle
// THROUGH a multisampled target: the render pass draws at `sampleCount` samples
// into THIS target's multisampled color + depth, then RESOLVES down to the
// swapchain color image (a 1-sample attachment) at the end of the subpass — the
// presentable image is the resolved swapchain image, and the triangle's edges
// are antialiased. This owner is the multisampled half of the S168 framebuffer
// triple (msaa color / swapchain resolve / msaa depth); the swapchain's OWN
// sample-1 depth image remains the genuine non-MSAA path (unchanged, kept).
//
// Creation follows the S164 depth-image pattern (raw device memory, no VMA — VMA
// allocation is a recorded NO-GAP hand-off to S202): one multisampled color image
// (COLOR_ATTACHMENT usage, samples N), one multisampled depth image (optional,
// DEPTH_STENCIL_ATTACHMENT usage, samples N) when a depth format is supplied,
// each with its device-local memory + image view. Scope: this is generation-
// scoped (extent-bound) and is rebuilt on swapchain recreation, exactly like the
// framebuffers — the SwapchainStateMachine (S167_1) owns it alongside them.
//
// LIFETIMES: the VulkanLoader, the VkDevice, and the VkPhysicalDevice (for the
// device-memory property probe) must OUTLIVE the MsaaTarget. Destroy order:
// the framebuffers (which reference this target's views) -> this target ->
// the swapchain -> device. Documented (not hidden) — callers own the ordering.
//
// NO-GAP recorded (not silent): the physically-failing MSAA-image/depth build
// and DEVICE_LOST scenarios are scripted in the S176 mock-ICD lane via the
// replaceable `buildMsaa` seam (see SwapchainState.h) — named, not silent. The
// framework-lane full-renderer MSAA buy-in / deferred S4B composition remains
// the framework's concern (recorded, not silent).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <optional>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// The real S168 multisampled render-target owner: RAII + move-only, owning a
// multisampled color image + view + device memory and, when a depth format is
// supplied, a multisampled depth image + view + device memory. Constructed only
// via create(); destroys in reverse order (depth -> color, view -> memory ->
// image) on destruction. See the header comment for LIFETIMES + status.
class VAML_VULKAN_EXPORT MsaaTarget
{
public:
    // Creates the multisampled render target at `extent` with `sampleCount`
    // samples. `colorFormat` is the swapchain color format (the resolve target's
    // format = this target's format); `depthFormat` (optional) selects whether a
    // multisampled depth image is created. Returns nullptr (no throw) on any
    // failure, mirroring Swapchain::create. `loader`, `device`, and
    // `physicalDevice` must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<MsaaTarget>
    create(const VulkanLoader& loader,
           VkDevice device,
           VkPhysicalDevice physicalDevice,
           VkFormat colorFormat,
           std::optional<VkFormat> depthFormat,
           VkExtent2D extent,
           VkSampleCountFlagBits sampleCount);

    ~MsaaTarget();
    MsaaTarget(MsaaTarget&&) noexcept;
    MsaaTarget& operator=(MsaaTarget&&) noexcept;
    MsaaTarget(const MsaaTarget&)            = delete;
    MsaaTarget& operator=(const MsaaTarget&) = delete;

    [[nodiscard]] bool                valid()        const noexcept { return colorImage_ != VK_NULL_HANDLE; }
    [[nodiscard]] VkImage             color_image()  const noexcept { return colorImage_; }
    [[nodiscard]] VkImageView         color_view()   const noexcept { return colorImageView_; }
    [[nodiscard]] VkDeviceMemory      color_memory() const noexcept { return colorMemory_; }
    [[nodiscard]] VkImage             depth_image()  const noexcept { return depthImage_; }
    [[nodiscard]] VkImageView         depth_view()   const noexcept { return depthImageView_; }
    [[nodiscard]] VkDeviceMemory      depth_memory() const noexcept { return depthMemory_; }
    [[nodiscard]] std::optional<VkFormat> depth_format() const noexcept { return depthFormat_; }
    [[nodiscard]] VkSampleCountFlagBits sample_count() const noexcept { return sampleCount_; }
    [[nodiscard]] VkExtent2D          extent()       const noexcept { return extent_; }

private:
    MsaaTarget(const VulkanLoader& loader, VkDevice device);

    void destroy_resources() noexcept;

    const VulkanLoader* loader_ = nullptr;
    VkDevice           device_  = VK_NULL_HANDLE;

    VkImage              colorImage_      = VK_NULL_HANDLE;
    VkImageView          colorImageView_  = VK_NULL_HANDLE;
    VkDeviceMemory       colorMemory_     = VK_NULL_HANDLE;
    VkImage              depthImage_      = VK_NULL_HANDLE;
    VkImageView          depthImageView_  = VK_NULL_HANDLE;
    VkDeviceMemory       depthMemory_     = VK_NULL_HANDLE;
    std::optional<VkFormat> depthFormat_  = std::nullopt;
    VkSampleCountFlagBits sampleCount_    = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D           extent_{ 0, 0 };
};

} // namespace vaml::vulkan
