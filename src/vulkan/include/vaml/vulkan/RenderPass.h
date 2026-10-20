#pragma once
// S165 — Vulkan render pass.
//
// The S165 title's render-pass half, delivered as a Cat 1 (library-surface) RAII
// owner + a set of PURE DECISION SEAMS (headless-testable, the S160/S162/S163/S164
// precedent — CI proves the decision layer; the LIVE vkCreateRenderPass is manual
// via the POCs). A single subpass renders to one color attachment (+ one optional
// depth attachment). The render pass is the pipeline's (and S166's draw loop's)
// attachment contract: S166's command buffer records its begin/end against this
// pass, and Framebuffer (S165) layers the swapchain image views onto it.
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       describe_color_attachment(format, sampleCount)   (the color attachment description)
//       describe_depth_attachment(depthFormat, sampleCount) (the depth attachment description,
//                                                             optional)
//       decide_load_store(hasDepth, initialLayoutIsUndefined) -> LoadStoreDecision
//     These are the S165 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime. The Live creation path consumes them.
//   * `RenderPassConfig` — the negotiated colorFormat/depthFormat/sampleCount/
//     loadOp/storeOp/initialLayout/finalLayout.
//   * `RenderPass::create(...)` — the real creation path that assembles the
//     VkAttachmentDescription[] + VkAttachmentReference[] + one subpass + one
//     subpass dependency and calls vkCreateRenderPass (manual/POC-proven,
//     [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader and the VkDevice must OUTLIVE the RenderPass.
// Destroy order: pipeline -> render pass -> device. The render pass must be
// destroyed before the device. This is documented (not hidden) — callers own
// the ordering.
//
// NO-GAP recorded (not silent): S166's command buffer records begin/end against
// this pass (the pass is retained for the draw loop); MSAA sample-count > 1 and
// the multisampled resolve layout = S168 (S165 uses sample count 1); the depth
// attachment is optional (surface-dependent depth is negotiated by the S164
// Swapchain; a pass with no depth attachment is a color-only pass).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The color attachment description for the swapchain's color image. `format` is
// the swapchain color format (S164's choose_surface_format result) and
// `sampleCount` the render-target sample count (S165 uses 1; > 1 is S168's
// MSAA). Returns a fully-populated VkAttachmentDescription: LOAD_OP_CLEAR /
// STORE_OP_STORE / COLOR_ATTACHMENT usage for the color attachment.
//
// This is a PURE constructor of the attachment description struct — it only
// assembles values, so it is headless-testable by construction (the S165 proof
// asserts the returned field values verbatim).
VAML_VULKAN_EXPORT VkAttachmentDescription
describe_color_attachment(VkFormat format, VkSampleCountFlagBits sampleCount) noexcept;

// The depth attachment description (referenced by the optional depth attachment
// when the swapchain has one). `depthFormat` is the S164 negotiated depth format.
// Returns the attachment description with DEPTH_STENCIL_ATTACHMENT usage and
// LOAD_OP_CLEAR / STORE_OP_DONT_CARE (depth is transient — it is only needed
// during the render pass, never stored back to the surface).
VAML_VULKAN_EXPORT VkAttachmentDescription
describe_depth_attachment(VkFormat depthFormat, VkSampleCountFlagBits sampleCount) noexcept;

// The load/store decision for the pass. `hasDepth` selects whether a depth
// attachment is present; `initialLayoutIsUndefined` records whether the pass
// starts from VK_IMAGE_LAYOUT_UNDEFINED (true when the images were just created /
// never presented — load clear is required) or from VK_IMAGE_LAYOUT_PRESENT_SRC
// (false when the swapchain image retains its presented contents — then the
// attachment can be loaded, though S165's pipeline does not rely on it). Returns
// the two layout references (initial/final) the pass uses for both attachments.
struct LoadStoreDecision
{
    VkImageLayout colorInitial = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout colorFinal   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkImageLayout depthInitial = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout depthFinal   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
};

// The S168 MSAA sample-count candidates, in DESCENDING preference order. S168
// picks the LARGEST count the physical device supports in BOTH the color and
// depth attachment sample-count sets (best antialiasing); 1_BIT is the always-
// valid floor returned when nothing higher is mutually supported. 8/4/2 are the
// commonly-supported multisampling counts; 1 means "no MSAA" (the S165 path).
VAML_VULKAN_EXPORT const std::vector<VkSampleCountFlagBits>& sample_count_candidates() noexcept;

// The S168 sample-count NEGOTIATION (pure): given the physical device's
// supported COLOR and DEPTH sample-count bitmasks (VkPhysicalDeviceProperties
// .limits.framebufferColorSampleCounts / .framebufferDepthSampleCounts), returns
// the largest candidate present in BOTH, else VK_SAMPLE_COUNT_1_BIT (always
// valid — the S165 non-MSAA path). A 0 supported-flag (no samples reported)
// yields 1_BIT. Pure + headless-testable (the S168 proof surface, RESOLVED for
// real — NOT MINIMAL REAL).
VAML_VULKAN_EXPORT VkSampleCountFlagBits
choose_sample_count(VkSampleCountFlags colorSupported, VkSampleCountFlags depthSupported) noexcept;

// The description of the S168 color RESOLVE attachment — the swapchain color
// image (1 sample) that the multisampled color attachment resolves into at the
// end of the subpass. `format` is the swapchain color format. Returns a
// fully-populated VkAttachmentDescription: 1_BIT samples, LOAD_OP_DONT_CARE
// (the resolve overwrites it), STORE_OP_STORE (the presented image), final
// layout PRESENT_SRC_KHR. Used when the pass's sampleCount > 1; the non-MSAA
// path (1_BIT) has no resolve attachment.
VAML_VULKAN_EXPORT VkAttachmentDescription
describe_color_resolve_attachment(VkFormat format) noexcept;

// Derives the pass's load/store layouts: a fresh/first-use swapchain image
// (initialLayoutIsUndefined) clears; the color attachment always finishes in the
// present-source layout (so S166's present consumes it) and the depth attachment
// stays in the depth-stencil-optimal layout.
VAML_VULKAN_EXPORT LoadStoreDecision
decide_load_store(bool hasDepth, bool initialLayoutIsUndefined) noexcept;

// ---------------------------------------------------------------------------
// Real render-pass owner (RAII)
// ---------------------------------------------------------------------------

// The negotiated render-pass parameters, populated by RenderPass::create() and
// exposed via RenderPass accessors.
struct RenderPassConfig
{
    VkFormat                 colorFormat    = VK_FORMAT_UNDEFINED;
    std::optional<VkFormat>  depthFormat    = std::nullopt;  // std::nullopt => color-only pass
    VkSampleCountFlagBits    sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout            initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout            finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentLoadOp       colorLoadOp    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp      colorStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp       depthLoadOp    = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp      depthStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // True when the pass is MULTISAMPLED (sampleCount > 1): the render pass
    // resolves the multisampled color attachment into a 1-sample swapchain
    // attachment at the end of the subpass (see RenderPass::create + the S168
    // resolve attachment). False (the S165 default) = a single 1-sample color
    // attachment with no resolve. Mirrors sampleCount != 1_BIT; kept explicit
    // so framebuffer/command-buffer consumers branch on one clear signal.
    [[nodiscard]] bool uses_msaa_resolve() const noexcept
    {
        return sampleCount != VK_SAMPLE_COUNT_1_BIT;
    }
};

// The real S165 render-pass owner: RAII + move-only. Constructed only via
// create(); destroys the render pass on destruction. See the header comment for
// LIFETIMES.
class VAML_VULKAN_EXPORT RenderPass
{
public:
    // Creates a color render pass (+ optional depth) on `device` (the S169
    // Device owner) via `loader`. Runs the S165 decision seams for the
    // attachment descriptions and the load/store layouts, assembles one subpass
    // (color[0], optional depth) + one subpass dependency, and calls
    // vkCreateRenderPass. Returns nullptr (no throw) on failure, mirroring
    // PlatformWindow/VulkanInstance/Swapchain::create. `loader` and `device`
    // must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<RenderPass>
    create(const VulkanLoader& loader,
           const Device& device,
           VkFormat colorFormat,
           std::optional<VkFormat> depthFormat,
           VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

    ~RenderPass();
    RenderPass(RenderPass&&) noexcept;
    RenderPass& operator=(RenderPass&&) noexcept;
    RenderPass(const RenderPass&)            = delete;
    RenderPass& operator=(RenderPass&)       = delete;

    [[nodiscard]] VkRenderPass native_handle() const noexcept { return renderPass_; }
    [[nodiscard]] VkRenderPass render_pass() const noexcept { return renderPass_; }
    [[nodiscard]] const RenderPassConfig& config() const noexcept { return config_; }

private:
    RenderPass(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_  = nullptr;
    VkDevice           device_   = VK_NULL_HANDLE;  // BORROWED from the Device
    VkRenderPass       renderPass_ = VK_NULL_HANDLE;
    RenderPassConfig   config_{};
};

} // namespace vaml::vulkan
