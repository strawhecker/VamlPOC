// S640a — vaml::presentation::RenderTarget implementation.
//
// Implements the S640a pure decision seams (headless-testable) + the real
// RenderTarget::create/render_to/resize owner. See RenderTarget.h for the split +
// the zero-drift pairing + the RESIZE-BUFFER contract + LIFETIMES.

#include <vaml/presentation/RenderTarget.h>
#include <vaml/presentation/VamlRenderer.h> // VisualOf + default_visual_of (the S212 seam)

#include <vaml/vulkan/CommandBuffer.h>      // describe_command_buffer_begin, build_render_pass_begin
#include <vaml/vulkan/RenderPass.h>         // RenderPass (offscreen, owned)
#include <vaml/vulkan/Framebuffer.h>        // Framebuffer (offscreen, owned)
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad::is_srgb_format, quad::ui_projection
#include <vaml/presentation/dpi.h>          // S213: dpi::dpi_render_plan (mvp + uiScale)

#include <algorithm>
#include <cstring>
#include <utility>

namespace vaml::presentation {

// ---------------------------------------------------------------------------
// Pure decision seams + the CPU core (headless-testable)
// ---------------------------------------------------------------------------

namespace Impl {

std::size_t image_buffer_bytes(std::uint32_t width, std::uint32_t height) noexcept
{
    // BGRA = 4 bytes/pixel. Guard the multiply against overflow.
    if (width == 0u || height == 0u)
        return 0u;
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > (SIZE_MAX / 4u) || h > (SIZE_MAX / 4u) / w)
        return SIZE_MAX;
    return w * h * 4u;
}

VkFormat offscreen_format_for(VkFormat windowFormat) noexcept
{
    // ZERO DRIFT: the offscreen target MUST use the same SRGB format as the window's
    // swapchain (so the sRGB-encoded bytes copy_image_to_buffer reads back equal what
    // the window presents). A non-SRGB format -> UNDEFINED (the create guard rejects it).
    return vaml::vulkan::quad::is_srgb_format(windowFormat) ? windowFormat : VK_FORMAT_UNDEFINED;
}

VkExtent2D offscreen_extent_at(std::uint32_t logicalW, std::uint32_t logicalH,
                               std::uint32_t capW, std::uint32_t capH) noexcept
{
    // The record viewport: the logical size CLAMPED to the oversized buffer capacity
    // (never exceeds the buffer). Non-positive/zero -> guarded to 1x1 (no renderable area).
    VkExtent2D e{};
    e.width  = std::clamp(logicalW, 1u, std::max(capW, 1u));
    e.height = std::clamp(logicalH, 1u, std::max(capH, 1u));
    return e;
}

bool resize_buffer_decision(std::uint32_t capW, std::uint32_t capH,
                            std::uint32_t newW, std::uint32_t newH) noexcept
{
    // GROW only when the new size EXCEEDS the single buffer's capacity. A non-positive
    // new size never forces a grow (reuse). FALSE = reuse within capacity (zero churn —
    // the driver's "avoid swapchain churn" mandate).
    if (newW == 0u || newH == 0u)
        return false;
    return newW > capW || newH > capH;
}

Impl::ImageBuffer&
image_buffer_ensure_capacity(Impl::ImageBuffer& out, std::uint32_t w, std::uint32_t h) noexcept
{
    // The external-app RESIZE-BUFFER contract: REUSE the oversized std::vector when it
    // already holds enough bytes (no re-allocation, NEVER shrink); grow ONLY when it
    // MUST (w*h*4 > current size).
    const std::size_t need = image_buffer_bytes(w, h);
    if (out.bgra.size() < need)
        out.bgra.resize(need);
    out.width  = w;
    out.height = h;
    return out;
}

VkBufferImageCopy offscreen_copy_region(std::uint32_t w, std::uint32_t h,
                                        std::uint32_t rowPitch, VkDeviceSize bufferOffset) noexcept
{
    // The copy region maps the offscreen rows onto the staging buffer HONORING the
    // per-row stride (rowPitch from vkGetImageSubresourceLayout).
    VkBufferImageCopy region{};
    region.bufferOffset      = bufferOffset;
    region.bufferRowLength   = rowPitch;  // source bytes per row (the image's true stride)
    region.bufferImageHeight = h;          // the full copy height (no 3D repack)
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1u };
    return region;
}

const std::vector<RenderTargetLoopStep>& render_target_loop_steps() noexcept
{
    // The fixed per-render sequence (S210 loop-plan precedent): every render_to runs
    // EncodeIfDirty (conditional — only when the scene changed) -> RecordRetained ->
    // CopyToBuffer (always).
    static const std::vector<RenderTargetLoopStep> plan = {
        RenderTargetLoopStep::EncodeIfDirty,
        RenderTargetLoopStep::RecordRetained,
        RenderTargetLoopStep::CopyToBuffer,
    };
    return plan;
}

} // namespace Impl

// ---------------------------------------------------------------------------
// The real offscreen owner (RAII; move-only)
// ---------------------------------------------------------------------------

namespace {

// Fetches the physical device's memory properties via the loader (the Device's
// physical handle is queried through the loader's bound entry). The payload is the
// borrowed physical device; the loader's vkGetPhysicalDeviceMemoryProperties must
// be bound (S163 — the S640a/offscreen path only ever runs on a real device).
VkPhysicalDeviceMemoryProperties
memory_properties(const vaml::vulkan::VulkanLoader& vk, VkPhysicalDevice physical) noexcept
{
    VkPhysicalDeviceMemoryProperties mp{};
    if (vk.vkGetPhysicalDeviceMemoryProperties && physical)
        vk.vkGetPhysicalDeviceMemoryProperties(physical, &mp);
    return mp;
}

// Finds a memory type index in `bits` that satisfies ALL of the required `props`
// (a subset bitmap). Returns UINT32_MAX if none.
std::uint32_t find_memory_type(const VkPhysicalDeviceMemoryProperties& mp,
                               std::uint32_t bits, VkMemoryPropertyFlags props) noexcept
{
    for (std::uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        if ((bits & (1u << i)) != 0 &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

// The offscreen RAII resources OWNED by a RenderTarget (destroyed in reverse build
// order on host destruction). Built + filled by RenderTarget::create. The borrowed
// Device is captured so the exceed-capacity grow can re-probe memory + rebuild the
// render pass.
struct OffscreenResources
{
    // --- the single oversized offscreen color image ---
    VkImage        image       = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView    imageView   = VK_NULL_HANDLE;

    // --- the compatible color-only render pass + the framebuffer over the image view ---
    std::unique_ptr<vaml::vulkan::RenderPass>  renderPass;
    std::unique_ptr<vaml::vulkan::Framebuffer> framebuffer;

    // --- the command pool + ONE command buffer (the S166 shape) ---
    VkCommandPool    commandPool   = VK_NULL_HANDLE;
    VkCommandBuffer  commandBuffer = VK_NULL_HANDLE;

    // --- the host-visible staging buffer the image is copied into ---
    VkBuffer         stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory   stagingMemory = VK_NULL_HANDLE;
    std::size_t      stagingBytes  = 0;

    // --- capsule metadata ---
    VkFormat     colorFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D   capacity{ 0, 0 };
    VkQueue      queue       = VK_NULL_HANDLE;

    const vaml::vulkan::VulkanLoader* loader_ = nullptr;  // borrowed
    const vaml::vulkan::Device*    device_ = nullptr;  // borrowed
    VkDevice                       native_ = VK_NULL_HANDLE;
    VkPhysicalDevice               physical_ = VK_NULL_HANDLE;

    ~OffscreenResources();
};

OffscreenResources::~OffscreenResources()
{
    if (!loader_ || !native_)
        return;
    // Reverse build order: -> staging -> command buffer/pool -> framebuffer ->
    // render pass -> image view -> image/memory.
    if (stagingBuffer)
        loader_->vkDestroyBuffer(native_, stagingBuffer, nullptr);
    stagingBuffer = VK_NULL_HANDLE;
    if (stagingMemory)
        loader_->vkFreeMemory(native_, stagingMemory, nullptr);
    stagingMemory = VK_NULL_HANDLE;
    if (commandPool)
    {
        if (commandBuffer)
            loader_->vkFreeCommandBuffers(native_, commandPool, 1u, &commandBuffer);
        commandBuffer = VK_NULL_HANDLE;
        loader_->vkDestroyCommandPool(native_, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
    framebuffer.reset();
    renderPass.reset();
    if (imageView)
        loader_->vkDestroyImageView(native_, imageView, nullptr);
    imageView = VK_NULL_HANDLE;
    if (image)
        loader_->vkDestroyImage(native_, image, nullptr);
    image = VK_NULL_HANDLE;
    if (imageMemory)
        loader_->vkFreeMemory(native_, imageMemory, nullptr);
    imageMemory = VK_NULL_HANDLE;
}

// (Re)creates the offscreen color image + memory + view + the compatible render pass +
// framebuffer at `cap`. Shared by create() and the exceed-capacity resize grow. On
// success returns the new render-pass native handle; on failure rolls back (leaves the
// image/framebuffer/pass cleared) and returns VK_NULL_HANDLE. The command pool + staging
// buffer are created SEPARATELY (once, retained across grows).
VkRenderPass
recreate_image_target(OffscreenResources& d, VkExtent2D cap)
{
    if (!d.loader_ || !d.device_ || !d.native_)
        return VK_NULL_HANDLE;
    const vaml::vulkan::VulkanLoader& vk = *d.loader_;
    const VkDevice vkDevice = d.native_;
    if (cap.width == 0u || cap.height == 0u || d.colorFormat == VK_FORMAT_UNDEFINED)
        return VK_NULL_HANDLE;

    // Tear down any prior image/framebuffer/pass before recreating.
    d.framebuffer.reset();
    d.renderPass.reset();
    if (d.imageView)
        vk.vkDestroyImageView(vkDevice, d.imageView, nullptr);
    d.imageView = VK_NULL_HANDLE;
    if (d.image)
        vk.vkDestroyImage(vkDevice, d.image, nullptr);
    d.image = VK_NULL_HANDLE;
    if (d.imageMemory)
        vk.vkFreeMemory(vkDevice, d.imageMemory, nullptr);
    d.imageMemory = VK_NULL_HANDLE;
    d.capacity = cap;

    // --- 1) the offscreen color image (COLOR_ATTACHMENT | TRANSFER_SRC) ---
    {
        VkImageCreateInfo ici{};
        ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType     = VK_IMAGE_TYPE_2D;
        ici.format        = d.colorFormat;
        ici.extent        = { cap.width, cap.height, 1u };
        ici.mipLevels     = 1u;
        ici.arrayLayers   = 1u;
        ici.samples       = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ici.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vk.vkCreateImage(vkDevice, &ici, nullptr, &d.image) != VK_SUCCESS)
            return VK_NULL_HANDLE;
    }
    // --- 2) device-local memory for the image ---
    {
        VkMemoryRequirements mr{};
        vk.vkGetImageMemoryRequirements(vkDevice, d.image, &mr);
        const auto mp = memory_properties(vk, d.physical_);
        const std::uint32_t memIndex = find_memory_type(mp, mr.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memIndex == UINT32_MAX)
            return VK_NULL_HANDLE;
        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = mr.size;
        mai.memoryTypeIndex = memIndex;
        if (vk.vkAllocateMemory(vkDevice, &mai, nullptr, &d.imageMemory) != VK_SUCCESS)
            return VK_NULL_HANDLE;
        if (vk.vkBindImageMemory(vkDevice, d.image, d.imageMemory, 0u) != VK_SUCCESS)
            return VK_NULL_HANDLE;
    }
    // --- 3) the image view ---
    {
        VkImageViewCreateInfo ivc{};
        ivc.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivc.image            = d.image;
        ivc.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        ivc.format           = d.colorFormat;
        ivc.components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                 VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
        if (vk.vkCreateImageView(vkDevice, &ivc, nullptr, &d.imageView) != VK_SUCCESS)
            return VK_NULL_HANDLE;
    }

    // --- 4) the compatible color-only render pass (SRGB, MSAA 1, color-only) ---
    d.renderPass = vaml::vulkan::RenderPass::create(*d.loader_, *d.device_, d.colorFormat,
                                                    /*depth=*/std::nullopt,
                                                    /*sampleCount=*/VK_SAMPLE_COUNT_1_BIT);
    if (!d.renderPass)
        return VK_NULL_HANDLE;

    // --- 5) the framebuffer over the image view at CAPACITY (the oversized target) ---
    d.framebuffer = vaml::vulkan::Framebuffer::create(
        *d.loader_, *d.device_, d.renderPass->render_pass(), cap,
        { d.imageView }, /*depth=*/std::nullopt);
    if (!d.framebuffer)
        return VK_NULL_HANDLE;

    return d.renderPass->render_pass();
}

} // namespace

struct RenderTarget::DeviceOwner : public OffscreenResources
{
    using OffscreenResources::OffscreenResources;
};

std::unique_ptr<RenderTarget>
RenderTarget::create(const RenderTargetOptions& opts)
{
    if (!opts.loader || !opts.device || !opts.device->device())
        return nullptr;
    // Guard: the scene color format MUST be SRGB (the zero-drift pairing).
    const VkFormat offFormat = Impl::offscreen_format_for(opts.sceneSpec.colorFormat);
    if (offFormat == VK_FORMAT_UNDEFINED)
        return nullptr;
    // The offscreen copy + the pipeline/framebuffer/command entries must be live.
    if (!opts.loader->has_offscreen_loader() || !opts.loader->has_render_loader() ||
        !opts.loader->has_command_loader() || !opts.loader->has_quad_loader())
        return nullptr;
    if (!opts.logicalW || !opts.logicalH)
        return nullptr;

    // Capacity defaults to the initial logical size (an exact-fit single buffer).
    const std::uint32_t capW = (opts.bufferCapW > 0u) ? opts.bufferCapW : opts.logicalW;
    const std::uint32_t capH = (opts.bufferCapH > 0u) ? opts.bufferCapH : opts.logicalH;

    std::unique_ptr<RenderTarget> host(new RenderTarget());
    host->logicalW_   = opts.logicalW;
    host->logicalH_   = opts.logicalH;
    host->bufferCapW_ = capW;
    host->bufferCapH_ = capH;
    host->visualOf_   = opts.sceneSpec.colorFormat ? vaml::presentation::default_visual_of()
                                                   : vaml::presentation::VisualOf();
    host->onResize_   = opts.on_resize;

    auto dev = std::make_unique<DeviceOwner>();
    dev->loader_     = opts.loader;
    dev->device_     = opts.device;
    dev->native_     = opts.device->device();
    dev->queue       = opts.device->graphics_queue();
    dev->physical_   = opts.device->physical_device();
    dev->colorFormat = offFormat;

    // --- the offscreen image target at the oversized CAPACITY ---
    const VkRenderPass rp = recreate_image_target(*dev, { capW, capH });
    if (rp == VK_NULL_HANDLE)
        return nullptr;
    host->renderPass_ = rp;

    // --- the command pool + ONE command buffer (the S166 shape) ---
    {
        const auto poolInfo = vaml::vulkan::describe_command_pool(opts.device->queue_family());
        if (opts.loader->vkCreateCommandPool(dev->native_, &poolInfo, nullptr, &dev->commandPool) != VK_SUCCESS)
            return nullptr;
        const auto allocInfo = vaml::vulkan::describe_command_buffer_alloc(dev->commandPool, 1u);
        if (opts.loader->vkAllocateCommandBuffers(dev->native_, &allocInfo, &dev->commandBuffer) != VK_SUCCESS)
            return nullptr;
    }

    // --- the host-visible staging buffer (the copy_image_to_buffer destination) ---
    {
        const VkDeviceSize imageBytes = static_cast<VkDeviceSize>(capW) * capH * 4u;
        VkBufferCreateInfo bci{};
        bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size        = imageBytes;
        bci.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (opts.loader->vkCreateBuffer(dev->native_, &bci, nullptr, &dev->stagingBuffer) != VK_SUCCESS)
            return nullptr;

        VkMemoryRequirements mr{};
        opts.loader->vkGetBufferMemoryRequirements(dev->native_, dev->stagingBuffer, &mr);
        const auto mp = memory_properties(*opts.loader, dev->physical_);
        std::uint32_t memIndex = find_memory_type(
            mp, mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memIndex == UINT32_MAX)
            memIndex = find_memory_type(mp, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (memIndex == UINT32_MAX)
            return nullptr;
        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = mr.size;
        mai.memoryTypeIndex = memIndex;
        if (opts.loader->vkAllocateMemory(dev->native_, &mai, nullptr, &dev->stagingMemory) != VK_SUCCESS)
            return nullptr;
        if (opts.loader->vkBindBufferMemory(dev->native_, dev->stagingBuffer, dev->stagingMemory, 0u) != VK_SUCCESS)
            return nullptr;
        dev->stagingBytes = static_cast<std::size_t>(imageBytes);
    }

    // --- the RETAINED SceneRenderer over the SAME scene spec (zero drift) ---
    host->sceneRenderer_ = vaml::vulkan::ui::SceneRenderer::create(*opts.loader, *opts.device, opts.sceneSpec);
    if (!host->sceneRenderer_)
        return nullptr;

    host->dev_ = std::move(dev);
    return host;
}

RenderTarget::RenderTarget() = default;
RenderTarget::RenderTarget(RenderTarget&&) noexcept = default;
RenderTarget& RenderTarget::operator=(RenderTarget&&) noexcept = default;
RenderTarget::~RenderTarget() = default;

bool RenderTarget::set_root(vaml::UIElement* root) noexcept
{
    root_ = root;
    dirty_ = true;  // a new root is never arranged yet -> a re-encode is pending
    return root != nullptr;
}

void RenderTarget::set_frame_size(std::uint32_t width, std::uint32_t height) noexcept
{
    logicalW_ = width;
    logicalH_ = height;
    dirty_ = true;
}

void RenderTarget::set_visual_of(
    std::function<vaml::vulkan::ui::ElementVisual(const vaml::UIElement&)> visualOf)
{
    visualOf_ = std::move(visualOf);
    dirty_ = true;
}

bool RenderTarget::invalidate()
{
    if (!root_ || !sceneRenderer_ || !logicalW_ || !logicalH_)
        return false;

    // Measure/Arrange the attached core tree at the logical frame size (the
    // S123/S124 final_rects the encoder reads).
    root_->Measure(vaml::Size{ static_cast<float>(logicalW_), vaml::Size::Infinite().height });
    root_->Arrange(vaml::Rect{ 0.0f, 0.0f, static_cast<float>(logicalW_), static_cast<float>(logicalH_) });

    // CPU-encode the arranged tree into the S206 draw list (the S212 producer path).
    std::vector<vaml::vulkan::ui::DrawItem> items;
    const auto seam = visualOf_ ? visualOf_ : default_visual_of();
    vaml::vulkan::ui::encode_visual_tree(*root_, seam, items);

    // set_scene into the RETAINED SceneRenderer (the EncodeIfDirty pulse). This is a
    // NO-OP for unchanged content (the retained guarantee).
    sceneRenderer_->set_scene(items);
    dirty_ = true;  // a re-encode is pending for the next render_to
    return true;
}

void RenderTarget::resize(std::uint32_t width, std::uint32_t height)
{
    logicalW_ = width;
    logicalH_ = height;
    dirty_ = true;

    // The RESIZE-BUFFER policy: within capacity -> reuse the single oversized buffer
    // (recompute extent + copy region, NO image/framebuffer churn). Exceed capacity ->
    // grow ONCE + fire the external-app on_resize callback (the driver's model).
    const bool mustGrow = Impl::resize_buffer_decision(bufferCapW_, bufferCapH_, width, height);
    if (mustGrow)
    {
        const std::uint32_t newCapW = std::max(bufferCapW_, width);
        const std::uint32_t newCapH = std::max(bufferCapH_, height);
        if (dev_)
        {
            // Recreate the single oversized offscreen image at the new capacity (the
            // ONLY churn — this exceed-capacity grow). The command pool + staging buffer
            // are retained (recreate_image_target rebuilds the image/pass/framebuffer only).
            const VkRenderPass rp = recreate_image_target(*dev_, { newCapW, newCapH });
            if (rp != VK_NULL_HANDLE)
            {
                renderPass_ = rp;
                // Grow the staging buffer to the new capacity if needed.
                const std::size_t need = static_cast<std::size_t>(newCapW) * newCapH * 4u;
                if (need > dev_->stagingBytes)
                {
                    const vaml::vulkan::VulkanLoader& vk = *dev_->loader_;
                    if (dev_->stagingBuffer)
                    {
                        vk.vkDestroyBuffer(dev_->native_, dev_->stagingBuffer, nullptr);
                        dev_->stagingBuffer = VK_NULL_HANDLE;
                    }
                    if (dev_->stagingMemory)
                    {
                        vk.vkFreeMemory(dev_->native_, dev_->stagingMemory, nullptr);
                        dev_->stagingMemory = VK_NULL_HANDLE;
                    }
                    VkBufferCreateInfo bci{};
                    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bci.size        = static_cast<VkDeviceSize>(need);
                    bci.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    if (vk.vkCreateBuffer(dev_->native_, &bci, nullptr, &dev_->stagingBuffer) != VK_SUCCESS)
                        return;
                    VkMemoryRequirements mr{};
                    vk.vkGetBufferMemoryRequirements(dev_->native_, dev_->stagingBuffer, &mr);
                    const auto mp = memory_properties(vk, dev_->physical_);
                    std::uint32_t memIndex = find_memory_type(
                        mp, mr.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    if (memIndex == UINT32_MAX)
                        memIndex = find_memory_type(mp, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                    if (memIndex == UINT32_MAX)
                        return;
                    VkMemoryAllocateInfo mai{};
                    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    mai.allocationSize  = mr.size;
                    mai.memoryTypeIndex = memIndex;
                    if (vk.vkAllocateMemory(dev_->native_, &mai, nullptr, &dev_->stagingMemory) != VK_SUCCESS)
                        return;
                    if (vk.vkBindBufferMemory(dev_->native_, dev_->stagingBuffer, dev_->stagingMemory, 0u) != VK_SUCCESS)
                        return;
                    dev_->stagingBytes = need;
                }
                bufferCapW_ = newCapW;
                bufferCapH_ = newCapH;
            }
        }
        else
        {
            bufferCapW_ = newCapW;
            bufferCapH_ = newCapH;
        }
        // The external-app on_resize callback (the driver's model — the app re-sizes its
        // oversized CPU ImageBuffer if it wants).
        if (onResize_)
            onResize_(width, height);
    }
    // (within-capacity resize: no device churn — the next render_to recomputes the
    // extent + copy region only)
}

bool RenderTarget::render_to(Impl::ImageBuffer& out)
{
    if (!dev_ || !sceneRenderer_ || !root_)
        return false;
    const vaml::vulkan::VulkanLoader& vk = *dev_->loader_;
    const VkDevice vkDevice = dev_->native_;
    if (!dev_->commandBuffer || !dev_->framebuffer || !dev_->renderPass || !dev_->image ||
        !dev_->queue)
        return false;

    // The record viewport: the logical size clamped to the oversized buffer capacity.
    const VkExtent2D extent = Impl::offscreen_extent_at(logicalW_, logicalH_,
                                                        bufferCapW_, bufferCapH_);
    if (extent.width == 0u || extent.height == 0u)
        return false;
    const std::size_t outBytes = Impl::image_buffer_bytes(extent.width, extent.height);
    if (outBytes > dev_->stagingBytes || outBytes == 0u)
        return false;

    // --- RecordRetained: reset + begin, begin the offscreen render pass, record the
    // retained scene, end the pass. ---
    if (vk.vkResetCommandBuffer(dev_->commandBuffer, /*flags=*/0) != VK_SUCCESS)
        return false;
    const auto begin = vaml::vulkan::describe_command_buffer_begin();
    if (vk.vkBeginCommandBuffer(dev_->commandBuffer, &begin) != VK_SUCCESS)
        return false;

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const auto clear = vaml::vulkan::describe_clear_value(clearColor);
    const auto rpBegin = vaml::vulkan::build_render_pass_begin(
        dev_->renderPass->render_pass(), dev_->framebuffer->framebuffer(), extent, clear,
        /*hasDepth=*/false, /*hasResolve=*/false);
    vk.vkCmdBeginRenderPass(dev_->commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Record the RETAINED scene via the S213 render plan. The offscreen target IS the
    // physical framebuffer at the record extent; logical == physical here, so the plan's
    // derived uiScale = 1 and the mvp = the canvas projection at the record extent.
    const auto plan = dpi::dpi_render_plan(extent.width, extent.height,
                                           static_cast<float>(extent.width),
                                           static_cast<float>(extent.height));
    const bool recorded = sceneRenderer_->record(dev_->commandBuffer, plan.mvp, plan.uiScale);
    lastScale_ = plan.uiScale;

    vk.vkCmdEndRenderPass(dev_->commandBuffer);
    if (!recorded)
        return false;

    // --- CopyToBuffer: transition the image to TRANSFER_SRC, then copy_image_to_buffer
    // into the host-visible staging buffer, honoring the rowPitch. ---
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = dev_->image;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
        barrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vk.vkCmdPipelineBarrier(dev_->commandBuffer,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                                0u, nullptr, 0u, nullptr, 1u, &barrier);
    }
    // The per-row stride of the sRGB image (the copy region pays it forward).
    VkSubresourceLayout subres{};
    {
        VkImageSubresource sr{};
        sr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        sr.mipLevel   = 0;
        sr.arrayLayer = 0;
        vk.vkGetImageSubresourceLayout(vkDevice, dev_->image, &sr, &subres);
    }
    const std::uint32_t rowPitch = static_cast<std::uint32_t>(std::max(subres.rowPitch, (VkDeviceSize)0));
    const auto region = Impl::offscreen_copy_region(extent.width, extent.height, rowPitch, 0u);
    vk.vkCmdCopyImageToBuffer(dev_->commandBuffer, dev_->image,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              dev_->stagingBuffer, 1u, &region);
    if (vk.vkEndCommandBuffer(dev_->commandBuffer) != VK_SUCCESS)
        return false;

    // Submit + wait (the S166 one-shot shape; the S209 fence is the retained sync — here
    // the single offscreen command buffer is submitted + waited before the CPU read).
    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1u;
    si.pCommandBuffers    = &dev_->commandBuffer;
    if (vk.vkQueueSubmit(dev_->queue, 1u, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        return false;
    vk.vkQueueWaitIdle(dev_->queue);

    // --- the ImageBuffer: the GPU-emitted BGRA bytes, row-by-row (removing padding). ---
    Impl::image_buffer_ensure_capacity(out, extent.width, extent.height);
    void* mapped = nullptr;
    if (vk.vkMapMemory(vkDevice, dev_->stagingMemory, 0u, dev_->stagingBytes, 0u, &mapped) != VK_SUCCESS)
        return false;
    const std::uint8_t* src = static_cast<const std::uint8_t*>(mapped);
    std::uint8_t* dst = out.bgra.data();
    const std::size_t tightRow = static_cast<std::size_t>(extent.width) * 4u;
    for (std::uint32_t y = 0; y < extent.height; ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * tightRow,
                    src + static_cast<std::size_t>(y) * rowPitch, tightRow);
    }
    vk.vkUnmapMemory(vkDevice, dev_->stagingMemory);

    dirty_ = false;
    return true;
}

std::optional<Impl::ImageBuffer> RenderTarget::render()
{
    Impl::ImageBuffer buf;
    buf.bgra.resize(Impl::image_buffer_bytes(logicalW_, logicalH_));
    if (!render_to(buf))
        return std::nullopt;
    return buf;
}

} // namespace vaml::presentation
