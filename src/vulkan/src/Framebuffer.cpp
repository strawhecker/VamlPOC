// S165 — Framebuffer implementation.
//
// Implements the S165 Framebuffer owner + the attachment-count helper. See
// Framebuffer.h for the LIFETIMES contract + the attachment layout.

#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/MsaaTarget.h>

#include <vector>

namespace vaml::vulkan {

std::uint32_t
framebuffer_attachment_count(std::uint32_t colorViewCount, bool hasDepth, bool hasResolve) noexcept
{
    return colorViewCount + (hasResolve ? 1u : 0u) + (hasDepth ? 1u : 0u);
}

std::unique_ptr<Framebuffer>
Framebuffer::create(const VulkanLoader& loader,
                    const Device& device,
                    VkRenderPass renderPass,
                    VkExtent2D extent,
                    const std::vector<VkImageView>& colorViews,
                    std::optional<VkImageView> depthView)
{
    const VkDevice vkDevice = device.device();
    if (!loader.vkCreateFramebuffer || !vkDevice || !renderPass)
        return nullptr;
    if (colorViews.empty() || extent.width == 0 || extent.height == 0)
        return nullptr;

    // Attachments: [colorViews..., depthView?] — see framebuffer_attachment_count.
    std::vector<VkImageView> attachments;
    attachments.reserve(framebuffer_attachment_count(
        static_cast<std::uint32_t>(colorViews.size()), depthView.has_value()));
    attachments.insert(attachments.end(), colorViews.begin(), colorViews.end());
    if (depthView)
        attachments.push_back(*depthView);

    std::unique_ptr<Framebuffer> fb(new Framebuffer(loader, device));
    fb->extent_ = extent;

    VkFramebufferCreateInfo fci{};
    fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass      = renderPass;
    fci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    fci.pAttachments    = attachments.data();
    fci.width           = extent.width;
    fci.height          = extent.height;
    fci.layers          = 1;

    if (loader.vkCreateFramebuffer(vkDevice, &fci, nullptr, &fb->framebuffer_) != VK_SUCCESS)
        return nullptr;
    return fb;
}

std::unique_ptr<Framebuffer>
Framebuffer::create_msaa(const VulkanLoader& loader,
                         const Device& device,
                         VkRenderPass renderPass,
                         VkExtent2D extent,
                         const MsaaTarget& msaa,
                         VkImageView resolveView)
{
    const VkDevice vkDevice = device.device();
    if (!loader.vkCreateFramebuffer || !vkDevice || !renderPass)
        return nullptr;
    if (!msaa.valid() || resolveView == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0)
        return nullptr;

    const bool hasDepth = msaa.depth_view() != VK_NULL_HANDLE;

    // S168 attachment order for a multisampled framebuffer (matches RenderPass):
    //   [0] msaa color view
    //   [1] resolve (swapchain) view
    //   [2] msaa depth view (when present)
    std::vector<VkImageView> attachments;
    attachments.reserve(framebuffer_attachment_count(1, hasDepth, /*hasResolve=*/true));
    attachments.push_back(msaa.color_view());
    attachments.push_back(resolveView);
    if (hasDepth)
        attachments.push_back(msaa.depth_view());

    std::unique_ptr<Framebuffer> fb(new Framebuffer(loader, device));
    fb->extent_ = extent;

    VkFramebufferCreateInfo fci{};
    fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass      = renderPass;
    fci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    fci.pAttachments    = attachments.data();
    fci.width           = extent.width;
    fci.height          = extent.height;
    fci.layers          = 1;

    if (loader.vkCreateFramebuffer(vkDevice, &fci, nullptr, &fb->framebuffer_) != VK_SUCCESS)
        return nullptr;
    return fb;
}

Framebuffer::Framebuffer(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

Framebuffer::Framebuffer(Framebuffer&&) noexcept = default;
Framebuffer& Framebuffer::operator=(Framebuffer&&) noexcept = default;

Framebuffer::~Framebuffer()
{
    if (!loader_)
        return;
    if (framebuffer_)
        loader_->vkDestroyFramebuffer(device_, framebuffer_, nullptr);
    framebuffer_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
