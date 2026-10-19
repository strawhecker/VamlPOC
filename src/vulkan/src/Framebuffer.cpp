// S165 — Framebuffer implementation.
//
// Implements the S165 Framebuffer owner + the attachment-count helper. See
// Framebuffer.h for the LIFETIMES contract + the attachment layout.

#include <vaml/vulkan/Framebuffer.h>

#include <vector>

namespace vaml::vulkan {

std::uint32_t
framebuffer_attachment_count(std::uint32_t colorViewCount, bool hasDepth) noexcept
{
    return colorViewCount + (hasDepth ? 1u : 0u);
}

std::unique_ptr<Framebuffer>
Framebuffer::create(const VulkanLoader& loader,
                    VkDevice device,
                    VkRenderPass renderPass,
                    VkExtent2D extent,
                    const std::vector<VkImageView>& colorViews,
                    std::optional<VkImageView> depthView)
{
    if (!loader.vkCreateFramebuffer || !device || !renderPass)
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

    if (loader.vkCreateFramebuffer(device, &fci, nullptr, &fb->framebuffer_) != VK_SUCCESS)
        return nullptr;
    return fb;
}

Framebuffer::Framebuffer(const VulkanLoader& loader, VkDevice device)
    : loader_(&loader)
    , device_(device)
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
