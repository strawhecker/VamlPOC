// S202 — TextureAtlas implementation (the texture atlas + staging → device-local
// sub-region upload + sub-region invalidation).
//
// See TextureAtlas.h for the S202 contract + the recorded VMA-vs-raw-memory
// decision (raw S164 device memory, NOT VMA — consistent with the S200
// QuadTexture/QuadScene upload path; one image + one staging buffer = no
// sub-allocation pressure). This TU implements the pure atlas-grid seams +
// the RAII owner: image + view + raw device memory (S164 entries), the
// host-visible staging buffer (S200 buffer entries), the sampler, a persistent
// command pool + one reset-table command buffer (S166 command entries), and the
// per-tile dirty bookkeeper that drives the sub-region flush.
//
// Loader reuse (NO new S202 entry points — the S200 texture copy path +
// S166 command path): vkCmdCopyBufferToImage + vkCmdPipelineBarrier come from
// has_texture_loader(); the command-pool/buffer/begin/end/submit/wait entries
// come from has_command_loader(). Both must be bound for create() to succeed.

#include <vaml/vulkan/quad/TextureAtlas.h>
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad_sampler()

#include <algorithm>
#include <cstring>
#include <utility>

namespace vaml::vulkan::quad {

namespace {

// Picks the first memory type in `req` that has ALL the `required` properties
// (DEVICE_LOCAL for the image, HOST_VISIBLE | HOST_COHERENT for the staging
// buffer) — the same pattern QuadTexture.cpp uses.
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

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS
// ---------------------------------------------------------------------------

AtlasLayout atlas_layout(std::uint32_t atlasW, std::uint32_t atlasH,
                         std::uint32_t cols, std::uint32_t rows) noexcept
{
    AtlasLayout l;
    if (atlasW == 0u || atlasH == 0u || cols == 0u || rows == 0u)
        return l;
    if (atlasW % cols != 0u || atlasH % rows != 0u)
        return l;  // exact-grid contract violated
    l.atlasW = atlasW;
    l.atlasH = atlasH;
    l.cols   = cols;
    l.rows   = rows;
    l.tileW  = atlasW / cols;
    l.tileH  = atlasH / rows;
    return l;
}

bool atlas_layout_valid(const AtlasLayout& layout) noexcept
{
    return layout.atlasW != 0u && layout.atlasH != 0u &&
           layout.cols != 0u && layout.rows != 0u &&
           layout.tileW != 0u && layout.tileH != 0u &&
           layout.atlasW % layout.cols == 0u &&
           layout.atlasH % layout.rows == 0u &&
           layout.tileW * layout.cols == layout.atlasW &&
           layout.tileH * layout.rows == layout.atlasH;
}

std::uint32_t atlas_tile_count(const AtlasLayout& layout) noexcept
{
    if (!atlas_layout_valid(layout))
        return 0u;
    return layout.cols * layout.rows;
}

AtlasRegion atlas_tile_region(const AtlasLayout& layout, std::uint32_t i) noexcept
{
    AtlasRegion r;
    const std::uint32_t count = atlas_tile_count(layout);
    if (count == 0u || i >= count)
        return r;
    const std::uint32_t col = i % layout.cols;
    const std::uint32_t row = i / layout.cols;
    r.x = col * layout.tileW;
    r.y = row * layout.tileH;
    r.w = layout.tileW;
    r.h = layout.tileH;
    return r;
}

std::uint32_t atlas_tile_offset(const AtlasLayout& layout, std::uint32_t i) noexcept
{
    const AtlasRegion r = atlas_tile_region(layout, i);
    if (r.w == 0u || r.h == 0u)
        return 0u;
    return (r.y * layout.atlasW + r.x) * 4u;
}

std::uint32_t atlas_tile_bytes(const AtlasLayout& layout) noexcept
{
    if (!atlas_layout_valid(layout))
        return 0u;
    return layout.tileW * layout.tileH * 4u;
}

std::uint32_t atlas_image_bytes(const AtlasLayout& layout) noexcept
{
    if (!atlas_layout_valid(layout))
        return 0u;
    return layout.atlasW * layout.atlasH * 4u;
}

std::vector<AtlasRegion>
atlas_dirty_regions(const AtlasLayout& layout, const std::vector<std::uint32_t>& dirtyIndices) noexcept
{
    std::vector<AtlasRegion> out;
    const std::uint32_t count = atlas_tile_count(layout);
    if (count == 0u)
        return out;
    // Sub-region invalidation derivation: the dirty tiles' sub-rects in ASCENDING
    // tile-index order (regardless of input order), deduplicated by index; out-of-
    // range indices are ignored. Sorting first makes the ascension input-order-
    // independent and turns duplicates into adjacent runs, so a single dedupe pass
    // over the sorted list suffices.
    std::vector<std::uint32_t> sorted;
    sorted.reserve(dirtyIndices.size());
    for (const std::uint32_t idx : dirtyIndices)
    {
        if (idx < count)
            sorted.push_back(idx);  // drop out-of-range
    }
    std::sort(sorted.begin(), sorted.end());
    std::uint32_t prevIndex = 0u;
    bool havePrev = false;
    for (const std::uint32_t idx : sorted)
    {
        if (havePrev && idx == prevIndex)
            continue;  // dedupe by index
        prevIndex = idx;
        havePrev  = true;
        out.push_back(atlas_tile_region(layout, idx));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Real owner
// ---------------------------------------------------------------------------

namespace {

// Records ONE image memory barrier (whole-image subresource) into `buf`.
void record_image_barrier(const VulkanLoader& loader, VkCommandBuffer cmd,
                          VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                          VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.layerCount     = 1;
    loader.vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u,
                                0u, nullptr, 0u, nullptr, 1u, &barrier);
}

} // namespace

std::unique_ptr<TextureAtlas>
TextureAtlas::create(const VulkanLoader& loader, const Device& device,
                     std::uint32_t atlasW, std::uint32_t atlasH,
                     std::uint32_t cols, std::uint32_t rows)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_texture_loader() || !loader.has_command_loader() || !vkDevice)
        return nullptr;

    std::unique_ptr<TextureAtlas> atlas(new TextureAtlas(loader, device));
    atlas->graphicsQueue_ = device.graphics_queue();

    const AtlasLayout layout = atlas_layout(atlasW, atlasH, cols, rows);
    if (!atlas_layout_valid(layout))
        return nullptr;

    const VkPhysicalDevice physical = device.physical_device();
    const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(atlas_image_bytes(layout));

    atlas->layout_ = layout;

    // --- 1) the device-local atlas image (S164 image/memory/view entries) ---
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_SRGB;  // SRGB (matches the S200 SRGB pipeline)
    ici.extent        = { layout.atlasW, layout.atlasH, 1u };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (loader.vkCreateImage(vkDevice, &ici, nullptr, &atlas->image_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements memReq{};
    loader.vkGetImageMemoryRequirements(vkDevice, atlas->image_, &memReq);
    const std::uint32_t imageMemType = find_memory_type(loader, physical, memReq,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (imageMemType == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = imageMemType;
    if (loader.vkAllocateMemory(vkDevice, &mai, nullptr, &atlas->imageMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindImageMemory(vkDevice, atlas->image_, atlas->imageMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    VkImageViewCreateInfo ivci{};
    ivci.sType       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image       = atlas->image_;
    ivci.viewType    = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format      = ici.format;
    ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel   = 0;
    ivci.subresourceRange.levelCount     = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount     = 1;
    if (loader.vkCreateImageView(vkDevice, &ivci, nullptr, &atlas->imageView_) != VK_SUCCESS)
        return nullptr;

    // --- 2) the full-atlas CPU staging buffer (S200 buffer entries; HOST_VISIBLE) ---
    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = imageBytes;
    bci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader.vkCreateBuffer(vkDevice, &bci, nullptr, &atlas->stagingBuffer_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements bufMemReq{};
    loader.vkGetBufferMemoryRequirements(vkDevice, atlas->stagingBuffer_, &bufMemReq);
    const std::uint32_t stagingMemType = find_memory_type(
        loader, physical, bufMemReq,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (stagingMemType == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo bmai{};
    bmai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bmai.allocationSize  = bufMemReq.size;
    bmai.memoryTypeIndex = stagingMemType;
    if (loader.vkAllocateMemory(vkDevice, &bmai, nullptr, &atlas->stagingMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindBufferMemory(vkDevice, atlas->stagingBuffer_, atlas->stagingMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    // --- 3) the sampler (shared quad_sampler — same S200 sampler as QuadTexture) ---
    const VkSamplerCreateInfo sci = quad_sampler();
    if (loader.vkCreateSampler(vkDevice, &sci, nullptr, &atlas->sampler_) != VK_SUCCESS)
        return nullptr;

    // --- 4) the persistent command pool + one reset-table command buffer (S166) ---
    {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = device.queue_family();
        if (loader.vkCreateCommandPool(vkDevice, &cpci, nullptr, &atlas->commandPool_) != VK_SUCCESS)
            return nullptr;
        VkCommandBufferAllocateInfo cbai{};
        cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool        = atlas->commandPool_;
        cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        if (loader.vkAllocateCommandBuffers(vkDevice, &cbai, &atlas->commandBuffer_) != VK_SUCCESS)
            return nullptr;
    }

    // --- 5) the CPU bookkeeper: full-atlas buffer + per-tile dirty flags (all clean) ---
    atlas->cpuBuffer_.assign(static_cast<std::size_t>(imageBytes), 0u);
    atlas->dirtyFlags_.assign(atlas_tile_count(layout), 0u);
    atlas->dirtyCount_ = 0;

    return atlas;
}

TextureAtlas::TextureAtlas(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

TextureAtlas::TextureAtlas(TextureAtlas&&) noexcept = default;
TextureAtlas& TextureAtlas::operator=(TextureAtlas&&) noexcept = default;

bool TextureAtlas::upload_tile(std::uint32_t tileIndex,
                               const std::vector<std::uint8_t>& bytesRGBA) noexcept
{
    if (tileIndex >= dirtyFlags_.size())
        return false;
    const std::uint32_t tileBytes = atlas_tile_bytes(layout_);
    if (tileBytes == 0u || bytesRGBA.size() != tileBytes)
        return false;
    const std::size_t offset = atlas_tile_offset(layout_, tileIndex);
    if (offset + tileBytes > cpuBuffer_.size())
        return false;
    std::memcpy(cpuBuffer_.data() + offset, bytesRGBA.data(), tileBytes);
    if (dirtyFlags_[tileIndex] == 0u)
    {
        dirtyFlags_[tileIndex] = 1u;
        ++dirtyCount_;
    }
    return true;
}

bool TextureAtlas::is_dirty(std::uint32_t tileIndex) const noexcept
{
    if (tileIndex >= dirtyFlags_.size())
        return false;
    return dirtyFlags_[tileIndex] != 0u;
}

std::uint32_t TextureAtlas::flush()
{
    if (!loader_ || dirtyCount_ == 0u)
        return 0u;
    const VkDevice vkDevice = device_;
    const std::uint32_t tileCount = static_cast<std::uint32_t>(dirtyFlags_.size());

    const std::uint32_t tileBytes = atlas_tile_bytes(layout_);
    if (tileBytes == 0u || cpuBuffer_.empty())
    {
        // defensive: no valid buffer — keep dirty for a later retry
        return 0u;
    }

    // --- 1) map the staging buffer + copy ONLY the dirty tiles' bytes at their offsets
    void* mapped = nullptr;
    if (loader_->vkMapMemory(vkDevice, stagingMemory_, 0u,
                             static_cast<VkDeviceSize>(cpuBuffer_.size()), 0u, &mapped) != VK_SUCCESS)
        return 0u;
    std::memcpy(mapped, cpuBuffer_.data(), cpuBuffer_.size());
    loader_->vkUnmapMemory(vkDevice, stagingMemory_);
    // HOST_COHERENT — no explicit flush needed.

    // --- 2) record ONE command buffer: whole-image barrier → TRANSFER_DST, one
    // vkCmdCopyBufferToImage per dirty sub-region, barrier → SHADER_READ_ONLY.
    VkCommandBuffer cmd = commandBuffer_;
    if (loader_->vkResetCommandBuffer(cmd, 0u) != VK_SUCCESS)
        return 0u;
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (loader_->vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        return 0u;

    record_image_barrier(*loader_, cmd, image_, currentLayout_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          /*srcAccess*/ 0u, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Sub-region copies: one VkBufferImageCopy per dirty tile.
    std::vector<VkBufferImageCopy> regions;
    std::vector<AtlasRegion> tileRects;
    regions.reserve(dirtyCount_);
    tileRects.reserve(dirtyCount_);
    for (std::uint32_t i = 0; i < tileCount; ++i)
    {
        if (dirtyFlags_[i] == 0u)
            continue;
        const AtlasRegion r = atlas_tile_region(layout_, i);
        tileRects.push_back(r);
        VkBufferImageCopy region{};
        region.bufferOffset      = atlas_tile_offset(layout_, i);
        region.bufferRowLength   = 0;  // tightly packed (the staging buffer holds the full atlas)
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset.x = static_cast<std::int32_t>(r.x);
        region.imageOffset.y = static_cast<std::int32_t>(r.y);
        region.imageOffset.z = 0;
        region.imageExtent   = { r.w, r.h, 1u };
        regions.push_back(region);
    }
    loader_->vkCmdCopyBufferToImage(cmd, stagingBuffer_, image_,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    static_cast<std::uint32_t>(regions.size()), regions.data());

    record_image_barrier(*loader_, cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    if (loader_->vkEndCommandBuffer(cmd) != VK_SUCCESS)
        return 0u;

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    if (loader_->vkQueueSubmit(graphicsQueue_, 1u, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        return 0u;
    if (loader_->vkQueueWaitIdle && loader_->vkQueueWaitIdle(graphicsQueue_) != VK_SUCCESS)
        return 0u;

    // --- 3) success: the image is shader-ready; clear the dirty set ---
    currentLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const std::uint32_t uploaded = dirtyCount_;
    std::fill(dirtyFlags_.begin(), dirtyFlags_.end(), 0u);
    dirtyCount_ = 0;
    return uploaded;
}

TextureAtlas::~TextureAtlas()
{
    if (!loader_)
        return;
    if (commandPool_)
    {
        if (commandBuffer_)
            loader_->vkFreeCommandBuffers(device_, commandPool_, 1u, &commandBuffer_);
        loader_->vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
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
    commandBuffer_  = VK_NULL_HANDLE;
    commandPool_    = VK_NULL_HANDLE;
    sampler_        = VK_NULL_HANDLE;
    stagingMemory_  = VK_NULL_HANDLE;
    stagingBuffer_  = VK_NULL_HANDLE;
    imageView_      = VK_NULL_HANDLE;
    imageMemory_    = VK_NULL_HANDLE;
    image_          = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::quad
