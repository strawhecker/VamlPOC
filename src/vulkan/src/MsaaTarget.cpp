// S168 — MsaaTarget implementation.
//
// Implements the real MsaaTarget::create() path: the multisampled color image +
// view + device memory and (when a depth format is supplied) the multisampled
// depth image + view + device memory. Follows the S164 depth-image pattern
// (raw device memory, no VMA — VMA = S202, recorded NO-GAP). See MsaaTarget.h
// for LIFETIMES + status.

#include <vaml/vulkan/MsaaTarget.h>

namespace vaml::vulkan {

namespace {

// Allocates + binds DEVICE_LOCAL memory for `image` the S164 way: probe the
// physical device's memory types, pick the first the image accepts with
// DEVICE_LOCAL, allocate, bind. Returns the VkDeviceMemory or VK_NULL_HANDLE on
// failure (the caller releases the image itself on failure). Mirrors
// Swapchain::rebuild's depth-memory selection exactly.
VkDeviceMemory bind_device_local_memory(const VulkanLoader& loader,
                                        VkDevice device,
                                        VkPhysicalDevice physicalDevice,
                                        VkImage image,
                                        VkDeviceSize& outSize)
{
    if (!loader.vkGetImageMemoryRequirements || !loader.vkGetPhysicalDeviceMemoryProperties ||
        !loader.vkAllocateMemory || !loader.vkBindImageMemory)
    {
        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements req{};
    loader.vkGetImageMemoryRequirements(device, image, &req);
    outSize = req.size;

    VkPhysicalDeviceMemoryProperties memProps{};
    loader.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    std::uint32_t memType = UINT32_MAX;
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((req.memoryTypeBits & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
        {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX)
        return VK_NULL_HANDLE;

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (loader.vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    if (loader.vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS)
    {
        loader.vkFreeMemory(device, memory, nullptr);
        return VK_NULL_HANDLE;
    }
    return memory;
}

// Creates a single 2D image with the given format/usage/samples at `extent`,
// returned in `image`/`memory` (bound to device-local memory). Returns true on
// success. `memorySize` out-param records the allocation size.
bool build_sample_image(const VulkanLoader& loader,
                        VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkFormat format,
                        VkImageUsageFlags usage,
                        VkSampleCountFlagBits samples,
                        VkExtent2D extent,
                        VkImage& image,
                        VkDeviceMemory& memory,
                        VkDeviceSize& memorySize)
{
    image  = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    if (!loader.vkCreateImage || !loader.vkDestroyImage)
        return false;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = format;
    ici.extent        = VkExtent3D{ extent.width, extent.height, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = samples;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (loader.vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS)
        return false;

    memory = bind_device_local_memory(loader, device, physicalDevice, image, memorySize);
    if (memory == VK_NULL_HANDLE)
    {
        loader.vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

// Creates the image view (COLOR aspect for a color image, DEPTH for depth) for
// an image. Returns VK_NULL_HANDLE on failure (caller releases image+memory).
VkImageView build_image_view(const VulkanLoader& loader,
                             VkDevice device,
                             VkImage image,
                             VkFormat format,
                             VkImageAspectFlags aspect)
{
    if (!loader.vkCreateImageView || !loader.vkDestroyImageView)
        return VK_NULL_HANDLE;

    VkImageViewCreateInfo ivci{};
    ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image    = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format   = format;
    ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.subresourceRange.aspectMask     = aspect;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (loader.vkCreateImageView(device, &ivci, nullptr, &view) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return view;
}

} // namespace

std::unique_ptr<MsaaTarget>
MsaaTarget::create(const VulkanLoader& loader,
                   const Device& device,
                   VkFormat colorFormat,
                   std::optional<VkFormat> depthFormat,
                   VkExtent2D extent,
                   VkSampleCountFlagBits sampleCount)
{
    const VkDevice        vkDevice     = device.device();
    const VkPhysicalDevice physicalDevice = device.physical_device();
    if (!loader.has_swapchain_loader() || !vkDevice)
        return nullptr;
    if (colorFormat == VK_FORMAT_UNDEFINED || extent.width == 0 || extent.height == 0 ||
        sampleCount == VK_SAMPLE_COUNT_1_BIT)
    {
        return nullptr;
    }

    std::unique_ptr<MsaaTarget> t(new MsaaTarget(loader, device));
    t->physicalDevice_ = physicalDevice;
    t->depthFormat_  = depthFormat;
    t->sampleCount_  = sampleCount;
    t->extent_       = extent;

    // --- Multisampled color image + view + memory ---
    VkDeviceSize colorMemSize = 0;
    if (!build_sample_image(loader, vkDevice, physicalDevice, colorFormat,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, sampleCount,
                            extent, t->colorImage_, t->colorMemory_, colorMemSize))
    {
        return nullptr;  // dtor no-op (colorImage_ null)
    }
    t->colorImageView_ = build_image_view(loader, vkDevice, t->colorImage_, colorFormat,
                                          VK_IMAGE_ASPECT_COLOR_BIT);
    if (t->colorImageView_ == VK_NULL_HANDLE)
        return nullptr;  // dtor releases color image+memory

    // --- Multisampled depth image + view + memory (optional) ---
    if (depthFormat)
    {
        VkDeviceSize depthMemSize = 0;
        if (!build_sample_image(loader, vkDevice, physicalDevice, *depthFormat,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, sampleCount,
                                extent, t->depthImage_, t->depthMemory_, depthMemSize))
        {
            return nullptr;  // dtor releases color + frees depth resources if any
        }
        t->depthImageView_ = build_image_view(loader, vkDevice, t->depthImage_, *depthFormat,
                                              VK_IMAGE_ASPECT_DEPTH_BIT);
        if (t->depthImageView_ == VK_NULL_HANDLE)
            return nullptr;  // dtor releases all
    }

    return t;
}

MsaaTarget::MsaaTarget(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

void MsaaTarget::destroy_resources() noexcept
{
    if (!loader_)
        return;
    const VkDevice device = device_;
    // Reverse order: views -> memory -> image; depth before color.
    if (depthImageView_)
        loader_->vkDestroyImageView(device, depthImageView_, nullptr);
    if (depthMemory_)
        loader_->vkFreeMemory(device, depthMemory_, nullptr);
    if (depthImage_)
        loader_->vkDestroyImage(device, depthImage_, nullptr);
    if (colorImageView_)
        loader_->vkDestroyImageView(device, colorImageView_, nullptr);
    if (colorMemory_)
        loader_->vkFreeMemory(device, colorMemory_, nullptr);
    if (colorImage_)
        loader_->vkDestroyImage(device, colorImage_, nullptr);
    depthImageView_  = VK_NULL_HANDLE;
    depthMemory_     = VK_NULL_HANDLE;
    depthImage_      = VK_NULL_HANDLE;
    colorImageView_  = VK_NULL_HANDLE;
    colorMemory_     = VK_NULL_HANDLE;
    colorImage_      = VK_NULL_HANDLE;
}

MsaaTarget::MsaaTarget(MsaaTarget&&) noexcept = default;
MsaaTarget& MsaaTarget::operator=(MsaaTarget&&) noexcept = default;

MsaaTarget::~MsaaTarget()
{
    destroy_resources();
}

} // namespace vaml::vulkan
