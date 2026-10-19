// S162 — QueueFamilySelection implementation.
//
// The pure family-selection seam (headless-testable) + the real physical-device
// enumeration surface for S163.

#include <vaml/vulkan/QueueFamilySelection.h>

#include <utility>

namespace vaml::vulkan {

std::optional<std::uint32_t>
select_queue_family(const VkQueueFamilyProperties* familyProperties,
                    std::uint32_t familyCount,
                    const PresentSupportFn& presentSupport)
{
    if (!familyProperties || familyCount == 0)
        return std::nullopt;

    std::optional<std::uint32_t> graphicsOnly;

    for (std::uint32_t i = 0; i < familyCount; ++i)
    {
        const bool graphics = (familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (!graphics)
            continue;
        if (!graphicsOnly)
            graphicsOnly = i;  // first graphics family (fallback)
        if (presentSupport && presentSupport(i))
            return i;          // graphics + present (unified) — the preferred
    }

    // No unified family: fall back to the first graphics-only family.
    return graphicsOnly;
}

std::optional<std::uint32_t>
find_present_family(std::uint32_t familyCount, const PresentSupportFn& presentSupport)
{
    if (!presentSupport || familyCount == 0)
        return std::nullopt;
    for (std::uint32_t i = 0; i < familyCount; ++i)
    {
        if (presentSupport(i))
            return i;
    }
    return std::nullopt;
}

std::vector<PhysicalDeviceInfo>
enumerate_physical_devices(const VulkanLoader& loader, const VulkanInstance& instance)
{
    std::vector<PhysicalDeviceInfo> out;
    if (!loader.vkEnumeratePhysicalDevices)
        return out;

    std::uint32_t count = 0;
    if (loader.vkEnumeratePhysicalDevices(instance.instance(), &count, nullptr) != VK_SUCCESS || count == 0)
        return out;

    std::vector<VkPhysicalDevice> devices(count);
    if (loader.vkEnumeratePhysicalDevices(instance.instance(), &count, devices.data()) != VK_SUCCESS)
        return out;

    out.reserve(count);
    for (VkPhysicalDevice dev : devices)
    {
        PhysicalDeviceInfo info;
        info.device = dev;
        loader.vkGetPhysicalDeviceProperties(dev, &info.properties);
        if (info.properties.deviceName[0] != '\0')
            info.name.assign(info.properties.deviceName);
        info.type = info.properties.deviceType;
        out.push_back(std::move(info));
    }
    return out;
}

} // namespace vaml::vulkan
