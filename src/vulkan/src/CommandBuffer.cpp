// S166 — CommandBuffer implementation.
//
// Implements the S166 pure decision seams (headless-testable) + the real
// CommandPool::create() path. See CommandBuffer.h for the full split + the
// LIFETIMES contract.

#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/Framebuffer.h>

#include <array>

namespace vaml::vulkan {

VkCommandPoolCreateInfo
describe_command_pool(std::uint32_t queueFamily) noexcept
{
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = queueFamily;
    return info;
}

VkCommandBufferAllocateInfo
describe_command_buffer_alloc(VkCommandPool pool, std::uint32_t count) noexcept
{
    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool        = pool;
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = count;
    return info;
}

VkCommandBufferBeginInfo
describe_command_buffer_begin() noexcept
{
    VkCommandBufferBeginInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = nullptr;
    return info;
}

VkClearValue
describe_clear_value(const float color[4]) noexcept
{
    VkClearValue cv{};
    cv.color.float32[0] = color[0];
    cv.color.float32[1] = color[1];
    cv.color.float32[2] = color[2];
    cv.color.float32[3] = color[3];
    return cv;
}

VkRenderPassBeginInfo
build_render_pass_begin(VkRenderPass renderPass,
                        VkFramebuffer framebuffer,
                        VkExtent2D extent,
                        VkClearValue clearValue,
                        bool hasDepth,
                        bool hasResolve) noexcept
{
    // clearValues = [colorClear, (depthClear when hasDepth)]. Depth clears to 1.0/0
    // (the far plane — everything authored is closer). The color clear is the one
    // S166's caller selects (the S165 blue triangle is drawn over it). The S168
    // RESOLVE attachment (hasResolve) is DONT_CARE-loaded (the resolve overwrites
    // it), so it contributes NO clear value — clearValueCount stays 1+hasDepth
    // regardless of hasResolve (only the optional depth adds a clear).
    //
    // The clear array lives in STATIC (process-lifetime) storage because the returned
    // VkRenderPassBeginInfo exposes pClearValues into it: each call overwrites the
    // same buffer, and the returned struct is consumed synchronously by the draw loop
    // (vkCmdBeginRenderPass) before the next build_render_pass_begin() call. This is
    // documented (not hidden) — a single-threaded main-thread draw loop.
    static std::array<VkClearValue, 2> clears{};
    clears[0] = clearValue;
    clears[1] = VkClearValue{};
    clears[1].depthStencil = VkClearDepthStencilValue{ 1.0f, 0 };

    (void)hasResolve;  // the S168 resolve attachment is DONT_CARE-loaded (resolved over),
                       // so it adds NO clear value — clearValueCount stays 1+hasDepth.

    VkRenderPassBeginInfo info{};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = renderPass;
    info.framebuffer              = framebuffer;
    info.renderArea.offset        = VkOffset2D{ 0, 0 };
    info.renderArea.extent        = extent;
    info.clearValueCount          = 1 + (hasDepth ? 1u : 0u);
    info.pClearValues             = clears.data();
    return info;
}

const std::vector<RecordStep>&
draw_record_plan() noexcept
{
    // The FIXED ordered step sequence a single frame records (see RecordStep).
    static const std::vector<RecordStep> plan = {
        RecordStep::BindPipeline,
        RecordStep::SetViewport,
        RecordStep::SetScissor,
        RecordStep::PushMvp,
        RecordStep::BeginRenderPass,
        RecordStep::Draw,
        RecordStep::EndRenderPass,
    };
    return plan;
}

std::unique_ptr<CommandPool>
CommandPool::create(const VulkanLoader& loader,
                    const Device& device,
                    std::uint32_t queueFamily,
                    std::uint32_t imageCount)
{
    const VkDevice vkDevice = device.device();
    // The S166 command-loader entry points are required.
    if (!loader.has_command_loader() || !vkDevice || imageCount == 0)
        return nullptr;

    std::unique_ptr<CommandPool> cp(new CommandPool(loader, device));

    const VkCommandPoolCreateInfo poolInfo = describe_command_pool(queueFamily);
    if (loader.vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &cp->pool_) != VK_SUCCESS)
        return nullptr;

    const VkCommandBufferAllocateInfo allocInfo = describe_command_buffer_alloc(cp->pool_, imageCount);
    cp->commandBuffers_.resize(imageCount);
    if (loader.vkAllocateCommandBuffers(vkDevice, &allocInfo, cp->commandBuffers_.data()) != VK_SUCCESS)
        return nullptr;
    return cp;
}

CommandPool::CommandPool(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

CommandPool::CommandPool(CommandPool&&) noexcept = default;
CommandPool& CommandPool::operator=(CommandPool&&) noexcept = default;

CommandPool::~CommandPool()
{
    if (!loader_)
        return;
    if (pool_ && !commandBuffers_.empty())
        loader_->vkFreeCommandBuffers(device_, pool_, static_cast<std::uint32_t>(commandBuffers_.size()), commandBuffers_.data());
    if (pool_)
        loader_->vkDestroyCommandPool(device_, pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
    commandBuffers_.clear();
}

} // namespace vaml::vulkan
