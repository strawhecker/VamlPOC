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
#include <vaml/vulkan/Device.h>
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

class MsaaTarget;  // S168 — the multisampled target; see MsaaTarget.h

// ---------------------------------------------------------------------------
// PURE helper (headless-testable)
// ---------------------------------------------------------------------------

// The framebuffer attachment-layout contract: the attachments for a single
// framebuffer are [ colorImageViews... , resolveImageView? , optionalDepthImageView ]
// laid sequentially. Returns the attachment count = colorViewCount +
// (hasResolve ? 1 : 0) + (hasDepth ? 1 : 0). `hasResolve` (S168) is true when
// the pass is multisampled and the framebuffer carries the 1-sample resolve
// (swapchain) image view alongside the multisampled color. With the default
// hasResolve=false the behavior is EXACTLY the S165 contract (colorViewCount +
// hasDepth), so S165/Framebuffer tests are byte-unchanged.
VAML_VULKAN_EXPORT std::uint32_t
framebuffer_attachment_count(std::uint32_t colorViewCount, bool hasDepth, bool hasResolve = false) noexcept;

// ---------------------------------------------------------------------------
// Real framebuffer owner (RAII)
// ---------------------------------------------------------------------------

// The real S165 framebuffer owner: RAII + move-only. Constructed only via
// create(); destroys the framebuffer on destruction. See the header comment for
// LIFETIMES.
class VAML_VULKAN_EXPORT Framebuffer
{
public:
    // Creates one VkFramebuffer on `device` (the S169 Device owner) for
    // `renderPass`, sized to `extent`, with the given color image views + an
    // optional depth image view. `attachments` must be the color views (in
    // swapchain order) followed by the depth view (in that order — see
    // framebuffer_attachment_count). Returns nullptr (no throw) on failure,
    // mirroring the other S165 owners. `loader`, `device`, `renderPass` and the
    // view objects must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<Framebuffer>
    create(const VulkanLoader& loader,
           const Device& device,
           VkRenderPass renderPass,
           VkExtent2D extent,
           const std::vector<VkImageView>& colorViews,
           std::optional<VkImageView> depthView = std::nullopt);

    // S168 — creates a MULTISAMPLED framebuffer for a resolved (MSAA) render
    // pass: attachments = [ msaa->color_view(), resolveView, msaa->depth_view? ].
    // `msaa` is the S168 multisampled target (yields the msaa color + optional
    // msaa depth views); `resolveView` is the 1-sample swapchain image view the
    // pass resolves into. The attachment count follows
    // framebuffer_attachment_count(1, hasDepth, hasResolve=true). Returns
    // nullptr (no throw) on failure. `loader`, `device`, `renderPass`, `msaa`
    // and `resolveView`'s owner must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<Framebuffer>
    create_msaa(const VulkanLoader& loader,
                const Device& device,
                VkRenderPass renderPass,
                VkExtent2D extent,
                const MsaaTarget& msaa,
                VkImageView resolveView);

    ~Framebuffer();
    Framebuffer(Framebuffer&&) noexcept;
    Framebuffer& operator=(Framebuffer&&) noexcept;
    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    [[nodiscard]] VkFramebuffer native_handle() const noexcept { return framebuffer_; }
    [[nodiscard]] VkFramebuffer framebuffer()  const noexcept { return framebuffer_; }
    [[nodiscard]] VkExtent2D    extent()       const noexcept { return extent_; }

private:
    Framebuffer(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_     = nullptr;
    VkDevice           device_      = VK_NULL_HANDLE;  // BORROWED from the Device
    VkFramebuffer      framebuffer_ = VK_NULL_HANDLE;
    VkExtent2D         extent_{ 0, 0 };
};

} // namespace vaml::vulkan
