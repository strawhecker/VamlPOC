// S200 — QuadTexture implementation (the minimal device-local texture).
//
// See QuadTexture.h for the S200 textured-quad contract + the S202 NO-GAP
// hand-off (general staging → device-local upload + atlas). This TU creates the
// single device-local image (S164 image/memory/view entries) + the staging buffer
// + the S200 sampler, and copies CPU bytes → device-local with a
// TRANSFER_DST → SHADER_READ_ONLY barrier on the graphics queue (submitted +
// waited here, so the texture is shader-ready for S201's descriptor bind).

#include <vaml/vulkan/quad/QuadTexture.h>
#include <vaml/vulkan/quad/QuadPipeline.h>

#include <cstring>

namespace vaml::vulkan::quad {

namespace {

// Picks the first memory type in `req` that has ALL the `required` properties
// (a DEVICE_LOCAL image type, or a HOST_VISIBLE | HOST_COHERENT staging type).
// Returns UINT32_MAX if none matches — the same pattern the S164 Swapchain uses
// for its depth image memory (there, DEVICE_LOCAL only; here the staging buffer
// needs a host-visible type too).
std::uint32_t find_memory_type(const VulkanLoader& loader,
                               VkPhysicalDevice physicalDevice,
                               const VkMemoryRequirements& req,
                               VkMemoryPropertyFlags required) noexcept
{
    VkPhysicalDeviceMemoryProperties props{};
    if (!loader.vkGetPhysicalDeviceMemoryProperties)
        return UINT32_MAX;
    loader.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i)
    {
        if ((req.memoryTypeBits & (1u << i)) != 0 &&
            (props.memoryTypes[i].propertyFlags & required) == required)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace

std::vector<std::uint8_t>
create_procedural_checker(std::uint32_t width, std::uint32_t height, std::uint32_t cell) noexcept
{
    std::vector<std::uint8_t> out;
    if (width == 0u || height == 0u || cell == 0u)
        return out;
    out.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0u);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        const bool rowDark  = (y / cell) % 2u == 0u;
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const bool colDark = (x / cell) % 2u == 0u;
            // checker: XOR of row/col parity -> black vs white; a 1-cell border
            // accent alternates so the texture is visibly not a solid color.
            const bool accent = (x % cell == 0u) || (y % cell == 0u);
            std::uint8_t v = (rowDark == colDark) ? 255u : 0u;
            if (accent)
                v = (rowDark == colDark) ? 200u : 55u;
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 4u;
            out[i]     = v;   // R
            out[i + 1] = v;   // G
            out[i + 2] = v;   // B
            out[i + 3] = 255u; // A (opaque)
        }
    }
    return out;
}

std::uint32_t texture_format_bytes(std::uint32_t width, std::uint32_t height) noexcept
{
    return width * height * 4u;  // RGBA8 tightly packed
}

std::unique_ptr<QuadTexture>
QuadTexture::create_procedural(const VulkanLoader& loader, const Device& device,
                               std::uint32_t width, std::uint32_t height, std::uint32_t cell)
{
    auto bytes = create_procedural_checker(width, height, cell);
    if (bytes.empty())
        return nullptr;
    return create(loader, device, width, height, bytes);
}

std::unique_ptr<QuadTexture>
QuadTexture::create(const VulkanLoader& loader, const Device& device,
                    std::uint32_t width, std::uint32_t height,
                    const std::vector<std::uint8_t>& bytesRGBA)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_texture_loader() || !loader.has_quad_loader() || !vkDevice)
        return nullptr;
    if (width == 0u || height == 0u ||
        bytesRGBA.size() != texture_format_bytes(width, height))
    {
        return nullptr;
    }

    std::unique_ptr<QuadTexture> tex(new QuadTexture(loader, device));
    tex->width_  = width;
    tex->height_ = height;

    const VkPhysicalDevice physical = device.physical_device();
    const VkDeviceSize    imageBytes = static_cast<VkDeviceSize>(bytesRGBA.size());

    // --- 1) the device-local image (S164 image/memory entries) ---
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_SRGB;  // SRGB (matches the S200 SRGB pipeline + gamma)
    ici.extent        = { width, height, 1u };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (loader.vkCreateImage(vkDevice, &ici, nullptr, &tex->image_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements memReq{};
    loader.vkGetImageMemoryRequirements(vkDevice, tex->image_, &memReq);
    const std::uint32_t imageMemType = find_memory_type(loader, physical, memReq,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (imageMemType == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = imageMemType;
    if (loader.vkAllocateMemory(vkDevice, &mai, nullptr, &tex->imageMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindImageMemory(vkDevice, tex->image_, tex->imageMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    VkImageViewCreateInfo ivci{};
    ivci.sType       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image       = tex->image_;
    ivci.viewType    = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format      = ici.format;
    ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;
    if (loader.vkCreateImageView(vkDevice, &ivci, nullptr, &tex->imageView_) != VK_SUCCESS)
        return nullptr;

    // --- 2) the CPU staging buffer (S200 buffer entries; HOST_VISIBLE) ---
    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = imageBytes;
    bci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader.vkCreateBuffer(vkDevice, &bci, nullptr, &tex->stagingBuffer_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements bufMemReq{};
    loader.vkGetBufferMemoryRequirements(vkDevice, tex->stagingBuffer_, &bufMemReq);
    const std::uint32_t stagingMemType = find_memory_type(
        loader, physical, bufMemReq,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (stagingMemType == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo bmai{};
    bmai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bmai.allocationSize  = bufMemReq.size;
    bmai.memoryTypeIndex = stagingMemType;
    if (loader.vkAllocateMemory(vkDevice, &bmai, nullptr, &tex->stagingMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindBufferMemory(vkDevice, tex->stagingBuffer_, tex->stagingMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    // --- 3) copy the CPU bytes into the staging buffer (map + memcpy) ---
    void* mapped = nullptr;
    if (loader.vkMapMemory(vkDevice, tex->stagingMemory_, 0u, imageBytes, 0u, &mapped) != VK_SUCCESS)
        return nullptr;
    std::memcpy(mapped, bytesRGBA.data(), static_cast<std::size_t>(imageBytes));
    loader.vkUnmapMemory(vkDevice, tex->stagingMemory_);
    // HOST_COHERENT — no explicit flush needed (the staging path is S202's
    // generalization; S200 keeps this single minimal upload).

    // --- 4) copy staging → device-local image with a layout TRANSFER_DST → SHADER_READ_ONLY
    // (recorded NO-GAP: the general staging→device-local upload + atlas + sub-region
    // invalidation is S202; S200 performs exactly one one-shot copy + a submit + wait) ---
    VkCommandPool pool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        cpci.queueFamilyIndex = device.queue_family();
        if (loader.vkCreateCommandPool(vkDevice, &cpci, nullptr, &pool) != VK_SUCCESS)
            return nullptr;
    }
    {
        VkCommandBufferAllocateInfo cbai{};
        cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool        = pool;
        cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
        if (loader.vkAllocateCommandBuffers(vkDevice, &cbai, &cmdbuf) != VK_SUCCESS)
        {
            loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
            return nullptr;
        }
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (loader.vkBeginCommandBuffer(cmdbuf, &begin) != VK_SUCCESS)
        {
            loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
            return nullptr;
        }

        // UNDEFINED → TRANSFER_DST (the copy destination).
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask       = 0;
        toTransfer.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image               = tex->image_;
        toTransfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount     = 1;
        toTransfer.subresourceRange.layerCount     = 1;
        loader.vkCmdPipelineBarrier(cmdbuf,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0u, 0u, nullptr, 0u, nullptr, 1u, &toTransfer);

        // Copy buffer → image (tightly packed 2D region).
        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;  // tightly packed
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount     = 1;
        region.imageExtent                     = { width, height, 1u };
        loader.vkCmdCopyBufferToImage(cmdbuf, tex->stagingBuffer_, tex->image_,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

        // TRANSFER_DST → SHADER_READ_ONLY (the S201 descriptor samples it).
        VkImageMemoryBarrier toShaded{};
        toShaded.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaded.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaded.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        toShaded.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaded.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaded.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaded.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaded.image               = tex->image_;
        toShaded.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        toShaded.subresourceRange.levelCount     = 1;
        toShaded.subresourceRange.layerCount     = 1;
        loader.vkCmdPipelineBarrier(cmdbuf,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    0u, 0u, nullptr, 0u, nullptr, 1u, &toShaded);

        if (loader.vkEndCommandBuffer(cmdbuf) != VK_SUCCESS)
        {
            loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
            return nullptr;
        }

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmdbuf;
        if (loader.vkQueueSubmit(device.graphics_queue(), 1u, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
            return nullptr;
        }
        // Wait — the texture must be shader-ready before S201 binds it.
        if (loader.vkQueueWaitIdle && loader.vkQueueWaitIdle(device.graphics_queue()) != VK_SUCCESS)
        {
            loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
            return nullptr;
        }
        loader.vkDestroyCommandPool(vkDevice, pool, nullptr);
    }

    // --- 5) the S200 sampler (see QuadPipeline::quad_sampler) ---
    const VkSamplerCreateInfo sci = quad_sampler();
    if (loader.vkCreateSampler(vkDevice, &sci, nullptr, &tex->sampler_) != VK_SUCCESS)
        return nullptr;

    return tex;
}

QuadTexture::QuadTexture(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

QuadTexture::QuadTexture(QuadTexture&&) noexcept = default;
QuadTexture& QuadTexture::operator=(QuadTexture&&) noexcept = default;

QuadTexture::~QuadTexture()
{
    if (!loader_)
        return;
    if (sampler_)
        loader_->vkDestroySampler(device_, sampler_, nullptr);
    if (stagingMemory_)
        loader_->vkFreeMemory(device_, stagingMemory_, nullptr);
    if (stagingBuffer_)
        loader_->vkDestroyBuffer(device_, stagingBuffer_, nullptr);
    if (imageView_)
        loader_->vkDestroyImageView(device_, imageView_, nullptr);
    if (imageMemory_)
        loader_->vkFreeMemory(device_, imageMemory_, nullptr);
    if (image_)
        loader_->vkDestroyImage(device_, image_, nullptr);
    sampler_       = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    stagingBuffer_ = VK_NULL_HANDLE;
    imageView_     = VK_NULL_HANDLE;
    imageMemory_   = VK_NULL_HANDLE;
    image_         = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::quad
