// S166 — FrameSync implementation.
//
// Implements the S166 pure decision seams (headless-testable) + the real
// FrameSync::create() path. See FrameSync.h for the full split + the LIFETIMES
// contract.

#include <vaml/vulkan/FrameSync.h>

namespace vaml::vulkan {

VkSemaphoreCreateInfo describe_semaphore() noexcept
{
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = 0;  // binary semaphore (no timeline)
    return info;
}

VkFenceCreateInfo describe_fence(bool initiallySignaled) noexcept
{
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = initiallySignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;
    return info;
}

std::uint32_t choose_concurrency(std::uint32_t imageCount) noexcept
{
    if (imageCount == 0)
        return 1;  // a degenerate swapchain is still a synchronous 1-frame loop
    if (imageCount > kMaxConcurrentFrames)
        return kMaxConcurrentFrames;
    return imageCount;
}

std::unique_ptr<FrameSync>
FrameSync::create(const VulkanLoader& loader, const Device& device, std::uint32_t frameCount)
{
    const VkDevice vkDevice = device.device();
    // The S166 sync-loader entry points are required.
    if (!loader.has_command_loader() || !vkDevice || frameCount == 0)
        return nullptr;

    std::unique_ptr<FrameSync> fs(new FrameSync(loader, device));
    fs->imageAvailable_.resize(frameCount, VK_NULL_HANDLE);
    fs->renderFinished_.resize(frameCount, VK_NULL_HANDLE);
    fs->inFlight_.resize(frameCount, VK_NULL_HANDLE);

    const VkSemaphoreCreateInfo semInfo = describe_semaphore();
    // All fences initially signaled: the first-frame handshake so frame 0's wait is a
    // no-op (see the header comment).
    const VkFenceCreateInfo fenceInfo = describe_fence(/*initiallySignaled=*/true);

    for (std::uint32_t i = 0; i < frameCount; ++i)
    {
        if (loader.vkCreateSemaphore(vkDevice, &semInfo, nullptr, &fs->imageAvailable_[i]) != VK_SUCCESS)
            return nullptr;
        if (loader.vkCreateSemaphore(vkDevice, &semInfo, nullptr, &fs->renderFinished_[i]) != VK_SUCCESS)
            return nullptr;
        if (loader.vkCreateFence(vkDevice, &fenceInfo, nullptr, &fs->inFlight_[i]) != VK_SUCCESS)
            return nullptr;
    }
    return fs;
}

FrameSync::FrameSync(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

FrameSync::FrameSync(FrameSync&&) noexcept = default;
FrameSync& FrameSync::operator=(FrameSync&&) noexcept = default;

FrameSync::~FrameSync()
{
    if (!loader_)
        return;
    for (VkFence fence : inFlight_)
        if (fence)
            loader_->vkDestroyFence(device_, fence, nullptr);
    for (VkSemaphore sem : renderFinished_)
        if (sem)
            loader_->vkDestroySemaphore(device_, sem, nullptr);
    for (VkSemaphore sem : imageAvailable_)
        if (sem)
            loader_->vkDestroySemaphore(device_, sem, nullptr);
    inFlight_.clear();
    renderFinished_.clear();
    imageAvailable_.clear();
}

} // namespace vaml::vulkan
