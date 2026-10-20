// S165 — RenderPass implementation.
//
// Implements the S165 pure decision seams (headless-testable) + the real
// RenderPass::create() path. See RenderPass.h for the full split + the
// LIFETIMES contract.

#include <vaml/vulkan/RenderPass.h>

#include <vector>

namespace vaml::vulkan {

const std::vector<VkSampleCountFlagBits>&
sample_count_candidates() noexcept
{
    static const std::vector<VkSampleCountFlagBits> kCandidates = {
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_1_BIT,
    };
    return kCandidates;
}

VkSampleCountFlagBits
choose_sample_count(VkSampleCountFlags colorSupported, VkSampleCountFlags depthSupported) noexcept
{
    // Largest mutually-supported candidate wins (best antialiasing); 1_BIT (the
    // always-valid floor) is returned when nothing higher is supported in both.
    for (VkSampleCountFlagBits c : sample_count_candidates())
    {
        const VkSampleCountFlags f = static_cast<VkSampleCountFlags>(c);
        if ((colorSupported & f) != 0 && (depthSupported & f) != 0)
            return c;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

VkAttachmentDescription
describe_color_resolve_attachment(VkFormat format) noexcept
{
    VkAttachmentDescription d{};
    d.format         = format;
    d.samples        = VK_SAMPLE_COUNT_1_BIT;   // the resolve target is the swapchain image
    d.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // the resolve overwrites it
    d.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;     // the presented image
    d.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    d.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    d.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return d;
}

VkAttachmentDescription
describe_color_attachment(VkFormat format, VkSampleCountFlagBits sampleCount) noexcept
{
    VkAttachmentDescription d{};
    d.format         = format;
    d.samples        = sampleCount;
    d.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    d.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    d.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    d.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    d.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return d;
}

VkAttachmentDescription
describe_depth_attachment(VkFormat depthFormat, VkSampleCountFlagBits sampleCount) noexcept
{
    VkAttachmentDescription d{};
    d.format         = depthFormat;
    d.samples        = sampleCount;
    d.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    d.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // depth is transient during the pass
    d.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    d.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    d.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return d;
}

LoadStoreDecision
decide_load_store(bool hasDepth, bool initialLayoutIsUndefined) noexcept
{
    LoadStoreDecision d;
    // A fresh/first-use swapchain image (initialLayoutIsUndefined) is cleared on
    // load; a presented image (retained content) can be loaded but S165's pipeline
    // never relies on it.
    d.colorInitial = initialLayoutIsUndefined ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    d.colorFinal   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if (hasDepth)
    {
        d.depthInitial = VK_IMAGE_LAYOUT_UNDEFINED;
        d.depthFinal   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    return d;
}

std::unique_ptr<RenderPass>
RenderPass::create(const VulkanLoader& loader,
                   const Device& device,
                   VkFormat colorFormat,
                   std::optional<VkFormat> depthFormat,
                   VkSampleCountFlagBits sampleCount)
{
    const VkDevice vkDevice = device.device();
    // The S165 render-pass entry points are required.
    if (!loader.has_render_loader() || !vkDevice)
        return nullptr;
    if (colorFormat == VK_FORMAT_UNDEFINED)
        return nullptr;

    std::unique_ptr<RenderPass> rp(new RenderPass(loader, device));
    rp->config_.colorFormat = colorFormat;
    rp->config_.depthFormat = depthFormat;
    rp->config_.sampleCount = sampleCount;

    // --- Attachment descriptions + references (S165/S168 decision seams) ---
    const bool multisampled = sampleCount != VK_SAMPLE_COUNT_1_BIT;

    // Attachment layout (S168): when multisampled, the array is
    //   [0] msaa COLOR (sampleCount samples, resolve source)
    //   [1] RESOLVE = the swapchain color image (1 sample, presentable)   -- S168
    //   [2] msaa DEPTH (sampleCount samples)                               -- S165/S168
    // When 1-sample (S165), it is [0] color + [1] optional depth — byte-unchanged.
    // Attachment-indices bookkeeping (color/colorResolve/depthRef) is kept local
    // and is ORDERED so the non-MSAA path produces the exact same indices as S165.
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference>   refs;  // [0] = color, [1: msaa]=resolve, [last] = optional depth
    VkAttachmentReference colorRef{};

    attachments.push_back(describe_color_attachment(colorFormat, sampleCount));
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    refs.push_back(colorRef);

    // S168: the resolve attachment (the swapchain image) exists ONLY when the
    // pass is multisampled. It is ALWAYS at attachment index [1] (right after the
    // color); the depth attachment (when present) sits AFTER it at index [2] for
    // the MSAA path (or [1] for the 1-sample path). pResolveAttachments points at
    // refs slot 1 (the parallel reference array mirrors the attachment order).
    std::optional<VkAttachmentReference> resolveRef;
    if (multisampled)
    {
        // The resolve description is pushed immediately after the color, so it
        // occupies attachment index 1 (the subpass reference below mirrors this).
        attachments.push_back(describe_color_resolve_attachment(colorFormat));
        resolveRef = VkAttachmentReference{ /*attachment=*/1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        refs.push_back(*resolveRef);
    }

    std::optional<VkAttachmentReference> depthRef;
    if (depthFormat)
    {
        // Depth sits after color (+ resolve on the MSAA path): index 2 for MSAA,
        // index 1 for the 1-sample path.
        const std::uint32_t depthIndex = multisampled ? 2u : 1u;
        if (attachments.size() < depthIndex + 1)
            attachments.resize(depthIndex + 1, {});
        attachments[depthIndex] = describe_depth_attachment(*depthFormat, sampleCount);
        depthRef = VkAttachmentReference{ depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        refs.push_back(*depthRef);
        rp->config_.depthLoadOp    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rp->config_.depthStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    auto ds = decide_load_store(depthFormat.has_value(), /*initialLayoutIsUndefined=*/true);
    rp->config_.initialLayout = ds.colorInitial;
    rp->config_.finalLayout   = ds.colorFinal;

    // Apply the decided load/store layouts to the attachment descriptions.
    attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = ds.colorInitial;
    attachments[0].finalLayout   = ds.colorFinal;
    if (depthRef)
    {
        const std::uint32_t di = depthRef->attachment;
        attachments[di].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[di].storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[di].initialLayout = ds.depthInitial;
        attachments[di].finalLayout   = ds.depthFinal;
        rp->config_.depthLoadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rp->config_.depthStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    // --- One subpass: color[0] + optional depth (+ optional S168 resolve) ---
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &refs[0];
    subpass.pResolveAttachments  = resolveRef ? &refs[1] : nullptr;  // the resolve is always refs slot 1
    subpass.pDepthStencilAttachment = depthRef ? &*depthRef : nullptr;

    // --- One subpass dependency (external -> subpass 0) ---
    VkSubpassDependency dep{};
    dep.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass      = 0;
    dep.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask   = 0;
    dep.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    rpci.pAttachments    = attachments.data();
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    if (loader.vkCreateRenderPass(vkDevice, &rpci, nullptr, &rp->renderPass_) != VK_SUCCESS)
        return nullptr;

    return rp;
}

RenderPass::RenderPass(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

RenderPass::RenderPass(RenderPass&&) noexcept = default;
RenderPass& RenderPass::operator=(RenderPass&&) noexcept = default;

RenderPass::~RenderPass()
{
    if (!loader_)
        return;
    if (renderPass_)
        loader_->vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
