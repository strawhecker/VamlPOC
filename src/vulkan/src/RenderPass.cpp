// S165 — RenderPass implementation.
//
// Implements the S165 pure decision seams (headless-testable) + the real
// RenderPass::create() path. See RenderPass.h for the full split + the
// LIFETIMES contract.

#include <vaml/vulkan/RenderPass.h>

#include <vector>

namespace vaml::vulkan {

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
                   VkDevice device,
                   VkFormat colorFormat,
                   std::optional<VkFormat> depthFormat,
                   VkSampleCountFlagBits sampleCount)
{
    // The S165 render-pass entry points are required.
    if (!loader.has_render_loader() || !device)
        return nullptr;
    if (colorFormat == VK_FORMAT_UNDEFINED)
        return nullptr;

    std::unique_ptr<RenderPass> rp(new RenderPass(loader, device));
    rp->config_.colorFormat = colorFormat;
    rp->config_.depthFormat = depthFormat;
    rp->config_.sampleCount = sampleCount;

    // --- Attachment descriptions + references (S165 decision seams) ---
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference>   refs;  // [0] = color, [1] = optional depth
    VkAttachmentReference colorRef{};
    std::optional<VkAttachmentReference> depthRef;

    attachments.push_back(describe_color_attachment(colorFormat, sampleCount));
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    refs.push_back(colorRef);

    if (depthFormat)
    {
        attachments.push_back(describe_depth_attachment(*depthFormat, sampleCount));
        depthRef = VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
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
        attachments[1].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = ds.depthInitial;
        attachments[1].finalLayout   = ds.depthFinal;
        rp->config_.depthLoadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rp->config_.depthStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    // --- One subpass: color[0] + optional depth ---
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &refs[0];
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

    if (loader.vkCreateRenderPass(device, &rpci, nullptr, &rp->renderPass_) != VK_SUCCESS)
        return nullptr;

    return rp;
}

RenderPass::RenderPass(const VulkanLoader& loader, VkDevice device)
    : loader_(&loader)
    , device_(device)
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
