// S164 — Swapchain implementation.
//
// Implements the S164 pure decision seams (headless-testable) + the real
// Swapchain::create()/negotiate() path + the depth-image creation. See
// Swapchain.h for the full split + the LIFETIMES contract.

#include <vaml/vulkan/Swapchain.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace vaml::vulkan {

std::uint32_t format_bytes_per_pixel(VkFormat format) noexcept
{
    // Suspected: a swapchain supported-format set is small + closed (the Vulkan
    // surface formats + the D32/D24/D16 depth family). The BPP table is explicit
    // + exhaustive for exactly these (the formats vaml-vulkan actually renders):
    switch (format)
    {
        // 8-bit BGRA/RGBA — 4 BPP (the surface-format family).
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:  // actually 32-bit packed
            return 4;

        // Depth formats.
        case VK_FORMAT_D32_SFLOAT:         return 4;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return 5;
        case VK_FORMAT_D24_UNORM_S8_UINT:  return 4;
        case VK_FORMAT_D16_UNORM:          return 2;

        default: return 0;  // unknown BPP
    }
}

VkSurfaceFormatKHR
choose_surface_format(std::span<const VkSurfaceFormatKHR> formats)
{
    if (formats.empty())
        return { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    // Prefer the 8-bit BGRA SRGB (linear write + sRGB-encode on present).
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    // Fall back to the first concrete format, skipping a VK_FORMAT_UNDEFINED
    // "pick anything" sentinel entry (honored as "take the next available").
    for (const auto& f : formats)
    {
        if (f.format != VK_FORMAT_UNDEFINED)
            return f;
    }
    // Only UNDEFINED entries (or a single UNDEFINED) — take the first entry.
    return formats.front();
}

VkPresentModeKHR
choose_present_mode(std::span<const VkPresentModeKHR> presentModes)
{
    if (presentModes.empty())
        return VK_PRESENT_MODE_FIFO_KHR;  // the spec-guaranteed mode

    for (VkPresentModeKHR m : presentModes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    }
    for (VkPresentModeKHR m : presentModes)
    {
        if (m == VK_PRESENT_MODE_FIFO_KHR)
            return m;
    }
    return presentModes.front();
}

VkExtent2D
choose_extent(const VkSurfaceCapabilitiesKHR& caps,
              std::uint32_t framebufferWidth,
              std::uint32_t framebufferHeight)
{
    // When the platform FIXES the size (currentExtent is not the sentinel),
    // honor it verbatim.
    const std::uint32_t sentinel = std::numeric_limits<std::uint32_t>::max();
    if (caps.currentExtent.width != sentinel && caps.currentExtent.height != sentinel)
        return caps.currentExtent;

    std::uint32_t w = framebufferWidth;
    std::uint32_t h = framebufferHeight;
    if (caps.minImageExtent.width  != 0 && w < caps.minImageExtent.width)  w = caps.minImageExtent.width;
    if (caps.minImageExtent.height != 0 && h < caps.minImageExtent.height) h = caps.minImageExtent.height;
    if (caps.maxImageExtent.width  != 0 && w > caps.maxImageExtent.width)  w = caps.maxImageExtent.width;
    if (caps.maxImageExtent.height != 0 && h > caps.maxImageExtent.height) h = caps.maxImageExtent.height;
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    return VkExtent2D{ w, h };
}

std::optional<VkFormat>
choose_depth_format(std::span<const VkFormat> candidates)
{
    for (VkFormat wanted : depth_format_candidates())
    {
        for (VkFormat f : candidates)
        {
            if (f == wanted)
                return f;
        }
    }
    return std::nullopt;
}

std::uint32_t
negotiate_image_count(const VkSurfaceCapabilitiesKHR& caps, std::uint32_t desired)
{
    std::uint32_t n = desired;
    if (n < caps.minImageCount)
        n = caps.minImageCount;
    // maxImageCount == 0 means "driver has no upper bound".
    if (caps.maxImageCount != 0 && n > caps.maxImageCount)
        n = caps.maxImageCount;
    if (n == 0)
        n = caps.minImageCount != 0 ? caps.minImageCount : 1;
    return n;
}

const std::vector<VkFormat>& depth_format_candidates()
{
    static const std::vector<VkFormat> kCandidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    return kCandidates;
}

namespace {

// Probes the physical device's support for a depth format via
// vkGetPhysicalDeviceFormatProperties + the DEPTH_STENCIL_ATTACHMENT optimal
// tiling bit. Falls back to an always-true predicate when the loader lacks the
// probe (robust default — the candidates are universally-supported).
bool depth_format_supported(const VulkanLoader& loader,
                            VkPhysicalDevice physicalDevice,
                            VkFormat format)
{
    if (!loader.vkGetPhysicalDeviceFormatProperties)
        return true;
    VkFormatProperties props{};
    loader.vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

} // namespace

std::optional<SwapchainConfig>
Swapchain::negotiate(const VulkanLoader& loader,
                     VkPhysicalDevice physicalDevice,
                     const VulkanSurface& surface,
                     std::uint32_t framebufferWidth,
                     std::uint32_t framebufferHeight,
                     std::uint32_t desiredImageCount)
{
    const VkPhysicalDevice pd  = physicalDevice;
    const VkSurfaceKHR   surf = surface.surface();

    VkSurfaceCapabilitiesKHR caps{};
    if (!loader.vkGetPhysicalDeviceSurfaceCapabilitiesKHR ||
        loader.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &caps) != VK_SUCCESS)
    {
        return std::nullopt;
    }

    std::uint32_t formatCount = 0;
    if (!loader.vkGetPhysicalDeviceSurfaceFormatsKHR ||
        loader.vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &formatCount, nullptr) != VK_SUCCESS)
    {
        return std::nullopt;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0 &&
        loader.vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &formatCount, formats.data()) != VK_SUCCESS)
    {
        return std::nullopt;
    }

    std::uint32_t modeCount = 0;
    if (!loader.vkGetPhysicalDeviceSurfacePresentModesKHR ||
        loader.vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surf, &modeCount, nullptr) != VK_SUCCESS)
    {
        return std::nullopt;
    }
    std::vector<VkPresentModeKHR> modes(modeCount);
    if (modeCount > 0 &&
        loader.vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surf, &modeCount, modes.data()) != VK_SUCCESS)
    {
        return std::nullopt;
    }

    SwapchainConfig cfg;
    cfg.surfaceFormat = choose_surface_format(formats);
    cfg.presentMode   = choose_present_mode(modes);
    cfg.extent        = choose_extent(caps, framebufferWidth, framebufferHeight);
    cfg.imageCount    = negotiate_image_count(caps, desiredImageCount);

    // Depth: probe the candidates on this physical device, keep the first
    // supported (std::nullopt = skip the depth image when none is supported).
    std::vector<VkFormat> supported;
    supported.reserve(depth_format_candidates().size());
    for (VkFormat f : depth_format_candidates())
    {
        if (depth_format_supported(loader, pd, f))
            supported.push_back(f);
    }
    cfg.depthFormat = choose_depth_format(supported);

    return cfg;
}

std::unique_ptr<Swapchain>
Swapchain::create(const VulkanLoader& loader,
                  VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  const VulkanSurface& surface,
                  std::uint32_t framebufferWidth,
                  std::uint32_t framebufferHeight,
                  std::optional<SwapchainConfig> configOpt)
{
    // The S164 swapchain entry points are required.
    if (!loader.has_swapchain_loader() || !device)
        return nullptr;

    // Query the surface + negotiate unless an explicit config was supplied.
    SwapchainConfig cfg;
    if (configOpt)
    {
        cfg = *configOpt;
    }
    else
    {
        auto negotiated = negotiate(loader, physicalDevice, surface,
                                    framebufferWidth, framebufferHeight);
        if (!negotiated)
            return nullptr;
        cfg = std::move(*negotiated);
    }

    if (cfg.surfaceFormat.format == VK_FORMAT_UNDEFINED || cfg.imageCount == 0)
        return nullptr;

    std::unique_ptr<Swapchain> sw(new Swapchain(loader, device));
    sw->config_ = cfg;

    // --- Create the swapchain ---
    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = surface.surface();
    sci.minImageCount    = cfg.imageCount;
    sci.imageFormat      = cfg.surfaceFormat.format;
    sci.imageColorSpace  = cfg.surfaceFormat.colorSpace;
    sci.imageExtent      = cfg.extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = cfg.presentMode;
    sci.clipped          = VK_TRUE;

    if (loader.vkCreateSwapchainKHR(device, &sci, nullptr, &sw->swapchain_) != VK_SUCCESS)
        return nullptr;

    // --- Fetch the swapchain images ---
    std::uint32_t imageCount = 0;
    if (loader.vkGetSwapchainImagesKHR(device, sw->swapchain_, &imageCount, nullptr) != VK_SUCCESS ||
        imageCount == 0)
    {
        return nullptr;
    }
    sw->images_.resize(imageCount);
    if (loader.vkGetSwapchainImagesKHR(device, sw->swapchain_, &imageCount, sw->images_.data()) != VK_SUCCESS)
        return nullptr;

    // --- Create an image view per swapchain image ---
    sw->imageViews_.reserve(sw->images_.size());
    for (VkImage image : sw->images_)
    {
        VkImageViewCreateInfo ivci{};
        ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image    = image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format   = cfg.surfaceFormat.format;
        ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        VkImageView view = VK_NULL_HANDLE;
        if (loader.vkCreateImageView(device, &ivci, nullptr, &view) != VK_SUCCESS)
            return nullptr;
        sw->imageViews_.push_back(view);
    }

    // --- Create the depth image + image view + device memory (raw, no VMA:
    // a single depth image needs 4 allocator entry points; VMA = S202) ---
    if (cfg.depthFormat && loader.vkCreateImage && loader.vkAllocateMemory &&
        loader.vkGetImageMemoryRequirements && loader.vkBindImageMemory)
    {
        VkFormat depthFormat = *cfg.depthFormat;

        VkImageCreateInfo ici{};
        ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType     = VK_IMAGE_TYPE_2D;
        ici.format        = depthFormat;
        ici.extent        = VkExtent3D{ cfg.extent.width, cfg.extent.height, 1 };
        ici.mipLevels     = 1;
        ici.arrayLayers   = 1;
        ici.samples       = VK_SAMPLE_COUNT_1_BIT;  // MSAA depth = S168
        ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ici.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (loader.vkCreateImage(device, &ici, nullptr, &sw->depthImage_) == VK_SUCCESS)
        {
            VkMemoryRequirements req{};
            loader.vkGetImageMemoryRequirements(device, sw->depthImage_, &req);

            // Pick the first memory type the image accepts with DEVICE_LOCAL.
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

            if (memType != UINT32_MAX)
            {
                VkMemoryAllocateInfo mai{};
                mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                mai.allocationSize  = req.size;
                mai.memoryTypeIndex = memType;
                if (loader.vkAllocateMemory(device, &mai, nullptr, &sw->depthMemory_) == VK_SUCCESS &&
                    loader.vkBindImageMemory(device, sw->depthImage_, sw->depthMemory_, 0) == VK_SUCCESS)
                {
                    VkImageViewCreateInfo divci{};
                    divci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    divci.image    = sw->depthImage_;
                    divci.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    divci.format   = depthFormat;
                    divci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
                    divci.subresourceRange.baseMipLevel   = 0;
                    divci.subresourceRange.levelCount     = 1;
                    divci.subresourceRange.baseArrayLayer = 0;
                    divci.subresourceRange.layerCount     = 1;
                    if (loader.vkCreateImageView(device, &divci, nullptr, &sw->depthImageView_) != VK_SUCCESS)
                    {
                        // Depth image view failed — tear the depth image down.
                        loader.vkFreeMemory(device, sw->depthMemory_, nullptr);
                        loader.vkDestroyImage(device, sw->depthImage_, nullptr);
                        sw->depthMemory_ = VK_NULL_HANDLE;
                        sw->depthImage_  = VK_NULL_HANDLE;
                    }
                }
                else
                {
                    // Allocation/bind failed — release the image.
                    if (sw->depthMemory_)
                        loader.vkFreeMemory(device, sw->depthMemory_, nullptr);
                    loader.vkDestroyImage(device, sw->depthImage_, nullptr);
                    sw->depthMemory_ = VK_NULL_HANDLE;
                    sw->depthImage_  = VK_NULL_HANDLE;
                }
            }
            else
            {
                loader.vkDestroyImage(device, sw->depthImage_, nullptr);
                sw->depthImage_ = VK_NULL_HANDLE;
            }
        }
    }

    return sw;
}

Swapchain::Swapchain(const VulkanLoader& loader, VkDevice device)
    : loader_(&loader)
    , device_(device)
{
}

Swapchain::Swapchain(Swapchain&&) noexcept = default;
Swapchain& Swapchain::operator=(Swapchain&&) noexcept = default;

Swapchain::~Swapchain()
{
    if (!loader_)
        return;

    // Destroy in reverse order: depth -> image views -> swapchain. The swapchain,
    // device, surface, instance, and loader must all still be alive here (callers
    // own the ordering — see the header LIFETIMES comment).
    for (VkImageView v : imageViews_)
    {
        if (v)
            loader_->vkDestroyImageView(device_, v, nullptr);
    }
    imageViews_.clear();

    if (depthImageView_)
        loader_->vkDestroyImageView(device_, depthImageView_, nullptr);
    if (depthMemory_)
        loader_->vkFreeMemory(device_, depthMemory_, nullptr);
    if (depthImage_)
        loader_->vkDestroyImage(device_, depthImage_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthMemory_    = VK_NULL_HANDLE;
    depthImage_     = VK_NULL_HANDLE;

    if (swapchain_)
        loader_->vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
