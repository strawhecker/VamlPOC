// S169 — Device implementation (the namesake: the ONLY TU that owns a raw
// VkDevice / VkQueue in the whole repo).
//
// Implements the S169 pure decision seams (headless-testable) + the real
// Device::create() path. See Device.h for the full split + the LIFETIMES
// contract.

#include <vaml/vulkan/Device.h>

namespace vaml::vulkan {

VkDeviceQueueCreateInfo
describe_device_queue(std::uint32_t queueFamily, const float& queuePriority) noexcept
{
    VkDeviceQueueCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.queueFamilyIndex = queueFamily;
    info.queueCount       = 1;  // the S4 single-thread proof creates one graphics queue
    // NOTE: pQueuePriorities points INTO the caller's `queuePriority` storage (hence
    // the const-ref parameter, NOT a by-value copy — the copy would die on return and
    // dangle). The caller must hold the named priority lvalue across vkCreateDevice.
    info.pQueuePriorities = &queuePriority;
    return info;
}

std::vector<const char*>
device_extensions(bool useSwapchain) noexcept
{
    if (!useSwapchain)
        return {};
    // The swapchain extension is the ONE device extension the S164 swapchain
    // requires (enabled at device creation, else vkCreateSwapchainKHR fails).
    return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
}

VkDeviceCreateInfo
build_device_create_info(const VkDeviceQueueCreateInfo& queueInfo,
                         const std::vector<const char*>& extensions) noexcept
{
    VkDeviceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount    = 1;
    info.pQueueCreateInfos       = &queueInfo;
    info.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
    return info;
}

std::unique_ptr<Device>
Device::create(const VulkanLoader& loader,
               const PhysicalDeviceInfo& physical,
               std::uint32_t queueFamily,
               Config config)
{
    // The S163 device/queue entry points are required.
    if (!loader.has_device_loader())
        return nullptr;
    if (physical.device == VK_NULL_HANDLE)
        return nullptr;

    std::unique_ptr<Device> d(new Device(loader, physical, queueFamily));

    // S169 seams: the queue create info + the (swapchain) extension list.
    // `queuePriority` is a NAMED lvalue so describe_device_queue's stored
    // pQueuePriorities pointer stays valid through vkCreateDevice.
    const float queuePriority = config.queuePriority;
    const VkDeviceQueueCreateInfo queueInfo =
        describe_device_queue(queueFamily, queuePriority);
    const auto extList = device_extensions(config.enableSwapchain);

    const VkDeviceCreateInfo devInfo =
        build_device_create_info(queueInfo, extList);

    VkDevice device = VK_NULL_HANDLE;
    if (loader.vkCreateDevice(physical.device, &devInfo, nullptr, &device) != VK_SUCCESS)
        return nullptr;

    d->device_ = device;

    loader.vkGetDeviceQueue(device, queueFamily, 0, &d->graphicsQueue_);
    return d;
}

Device::Device(const VulkanLoader& loader, PhysicalDeviceInfo physical,
               std::uint32_t queueFamily)
    : loader_(&loader)
    , physical_(physical)
    , queueFamily_(queueFamily)
{
}

Device::Device(Device&&) noexcept = default;
Device& Device::operator=(Device&&) noexcept = default;

Device::~Device()
{
    if (!loader_)
        return;
    if (device_)
        loader_->vkDestroyDevice(device_, nullptr);
    device_        = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
