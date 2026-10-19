#pragma once
// S165 — Vulkan framebuffers.
//
// The S165 title's framebuffer half, delivered as a Cat 1 (library-surface) RAII
// owner. One VkFramebuffer per swapchain image view (+ one optional depth image
// view) layered onto the render pass — the concrete pairing that makes each
// swapchain image a renderable target for the S165 pipeline and S166's draw loop.
//
// Unlike RenderPass/Pipeline (which carry testable decision seams), Framebuffer
// is a thin, fully-device-bound owner: it has no headless decision to make — the
// attachment set IS the swapchain's image views + optional depth view (both
// owned by the Swapchain), and the extent is the swapchain extent. Its real
// `create` composes the VkFramebufferCreateInfo and calls vkCreateFramebuffer
// (proven LIVE via the POCs, [both apps, not CI]). A small pure helper —
// `framebuffer_attachments(colorCount)` — is kept to make the attachment-layout
// contract explicit + testable.
//
// LIFETIMES: the VulkanLoader, the VkDevice, the RenderPass, AND the swapchain
// (which owns the image views + depth view) must OUTLIVE the Framebuffer.
// Destroy order: framebuffer(s) -> pipeline -> render pass -> swapchain -> device.
// This is documented (not hidden) — callers own the ordering.
//
// NO-GAP recorded (not silent): S166's draw loop creates one Framebuffer per
// swapchain image (this class IS that per-image attachment bundle) and records
// the subpass against it; resize-safe recreation (which recreates framebuffers
// after a swapchain recreation) = S167; MSAA resolves framebuffers = S168.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE helper (headless-testable)
// ---------------------------------------------------------------------------

// The framebuffer attachment-layout contract: the attachments for a single
// framebuffer are [ colorImageViews... , optionalDepthImageView ] laid
// sequentially. Returns the attachment count = colorViewCount +
// (hasDepth ? 1 : 0). Exposed so the attachment array builder + tests agree
// (kept trivial + explicit, not a hidden backfill).
VAML_VULKAN_EXPORT std::uint32_t
framebuffer_attachment_count(std::uint32_t colorViewCount, bool hasDepth) noexcept;

// ---------------------------------------------------------------------------
// Real framebuffer owner (RAII)
// ---------------------------------------------------------------------------

// The real S165 framebuffer owner: RAII + move-only. Constructed only via
// create(); destroys the framebuffer on destruction. See the header comment for
// LIFETIMES.
class VAML_VULKAN_EXPORT Framebuffer
{
public:
    // Creates one VkFramebuffer on `device` for `renderPass`, sized to `extent`,
    // with the given color image views + an optional depth image view. `attachments`
    // must be the color views (in swapchain order) followed by the depth view
    // (in that order — see framebuffer_attachment_count). Returns nullptr (no
    // throw) on failure, mirroring the other S165 owners. `loader`, `device`,
    // `renderPass` and the view objects must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<Framebuffer>
    create(const VulkanLoader& loader,
           VkDevice device,
           VkRenderPass renderPass,
           VkExtent2D extent,
           const std::vector<VkImageView>& colorViews,
           std::optional<VkImageView> depthView = std::nullopt);

    ~Framebuffer();
    Framebuffer(Framebuffer&&) noexcept;
    Framebuffer& operator=(Framebuffer&&) noexcept;
    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    [[nodiscard]] VkFramebuffer native_handle() const noexcept { return framebuffer_; }
    [[nodiscard]] VkFramebuffer framebuffer()  const noexcept { return framebuffer_; }
    [[nodiscard]] VkExtent2D    extent()       const noexcept { return extent_; }

private:
    Framebuffer(const VulkanLoader& loader, VkDevice device);

    const VulkanLoader* loader_     = nullptr;
    VkDevice           device_      = VK_NULL_HANDLE;
    VkFramebuffer      framebuffer_ = VK_NULL_HANDLE;
    VkExtent2D         extent_{ 0, 0 };
};

} // namespace vaml::vulkan
