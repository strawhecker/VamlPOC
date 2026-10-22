// S209 — RetainedBufferPool implementation (vaml::vulkan::ui::detail).
//
// Implements the S209 fence-aware retired-buffer pool decision seam + the real
// generation owner. The buffer creation/memory helpers mirror UiScene's (the
// host-visible vertex/index pair), but a grow RETIRES the old generation into the
// pool instead of freeing it, and recycle() destroys it once the in-flight ring
// has wrapped far enough that the retiree's slot fence has SIGNALED. See
// RetainedBufferPool.h for the lifetime contract (the zero-UAF guarantee).

#include <vaml/vulkan/ui/RetainedBufferPool.h>
#include <vaml/vulkan/quad/QuadVertex.h>
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad_vertex_count / quad_index_count

#include <cstring>
#include <algorithm>

namespace vaml::vulkan::ui {
namespace detail {

bool should_free_generation(std::uint32_t frameLag, std::uint32_t inFlightCount) noexcept
{
    // A generation retired `frameLag` frames ago is safe to free once the ring has
    // wrapped back to (and past) its slot — i.e. once frameLag >= inFlightCount.
    // Before that the retiree's slot fence has not yet been re-waited, so the GPU
    // could still be reading the buffers (UAF). Pure + headless-testable.
    return frameLag >= inFlightCount;
}

namespace {

// Picks a host-visible|coherent memory type (mirrors QuadScene/UiScene).
std::uint32_t find_host_visible_type(const VulkanLoader& loader,
                                     VkPhysicalDevice physicalDevice,
                                     const VkMemoryRequirements& req) noexcept
{
    if (!loader.vkGetPhysicalDeviceMemoryProperties)
        return UINT32_MAX;
    VkPhysicalDeviceMemoryProperties props{};
    loader.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i)
    {
        if ((req.memoryTypeBits & (1u << i)) != 0 &&
            (props.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace

RetainedBufferPool::RetainedBufferPool(const VulkanLoader& loader, const Device& device,
                                       std::uint32_t inFlightCount)
    : loader_(&loader)
    , device_(device.device())
    , physical_(device.physical_device())
    , inFlight_(inFlightCount > 0 ? inFlightCount : 1)
{
}

RetainedBufferPool::~RetainedBufferPool()
{
    destroy_all();
}

RetainedBufferPool::RetainedBufferPool(RetainedBufferPool&&) noexcept = default;
RetainedBufferPool& RetainedBufferPool::operator=(RetainedBufferPool&&) noexcept = default;

bool RetainedBufferPool::ensure(std::uint32_t targetItems)
{
    if (!loader_ || !device_)
        return false;
    if (active_.vertex != VK_NULL_HANDLE && active_.capacityItems >= targetItems)
        return true;  // already big enough — no-op

    const VkDeviceSize vertBytes = static_cast<VkDeviceSize>(
        quad::quad_vertex_count() * targetItems * sizeof(quad::QuadVertex));
    const VkDeviceSize idxBytes = static_cast<VkDeviceSize>(
        quad::quad_index_count() * targetItems * sizeof(std::uint16_t));

    Generation g{};
    g.capacityItems = targetItems;
    g.retireFrame   = UINT32_MAX;  // the new active generation

    // --- vertex buffer ---
    VkBufferCreateInfo vbci{};
    vbci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbci.size        = vertBytes;
    vbci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader_->vkCreateBuffer(device_, &vbci, nullptr, &g.vertex) != VK_SUCCESS)
        return false;
    VkMemoryRequirements vreq{};
    loader_->vkGetBufferMemoryRequirements(device_, g.vertex, &vreq);
    const std::uint32_t vtype = find_host_visible_type(*loader_, physical_, vreq);
    if (vtype == UINT32_MAX) { loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }
    VkMemoryAllocateInfo vmai{};
    vmai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vmai.allocationSize  = vreq.size;
    vmai.memoryTypeIndex = vtype;
    if (loader_->vkAllocateMemory(device_, &vmai, nullptr, &g.vertexMemory) != VK_SUCCESS)
    { loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }
    if (loader_->vkBindBufferMemory(device_, g.vertex, g.vertexMemory, 0u) != VK_SUCCESS)
    { loader_->vkFreeMemory(device_, g.vertexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }

    // --- index buffer ---
    VkBufferCreateInfo ibci{};
    ibci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibci.size        = idxBytes;
    ibci.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ibci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader_->vkCreateBuffer(device_, &ibci, nullptr, &g.index) != VK_SUCCESS)
    { loader_->vkFreeMemory(device_, g.vertexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }
    VkMemoryRequirements ireq{};
    loader_->vkGetBufferMemoryRequirements(device_, g.index, &ireq);
    const std::uint32_t itype = find_host_visible_type(*loader_, physical_, ireq);
    if (itype == UINT32_MAX)
    { loader_->vkDestroyBuffer(device_, g.index, nullptr); loader_->vkFreeMemory(device_, g.vertexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }
    VkMemoryAllocateInfo imai{};
    imai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imai.allocationSize  = ireq.size;
    imai.memoryTypeIndex = itype;
    if (loader_->vkAllocateMemory(device_, &imai, nullptr, &g.indexMemory) != VK_SUCCESS)
    { loader_->vkDestroyBuffer(device_, g.index, nullptr); loader_->vkFreeMemory(device_, g.vertexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }
    if (loader_->vkBindBufferMemory(device_, g.index, g.indexMemory, 0u) != VK_SUCCESS)
    { loader_->vkFreeMemory(device_, g.indexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.index, nullptr); loader_->vkFreeMemory(device_, g.vertexMemory, nullptr); loader_->vkDestroyBuffer(device_, g.vertex, nullptr); return false; }

    // The new generation becomes ACTIVE; the previous active generation is RETIRED
    // (never freed this frame — the in-flight ring may still hold it).
    if (active_.vertex != VK_NULL_HANDLE)
    {
        active_.retireFrame = frame_;
        retired_.push_back(active_);
    }
    active_ = std::move(g);
    return true;
}

void RetainedBufferPool::recycle()
{
    if (!loader_ || !device_)
        return;
    // Advance the frame once per recycle (called once per frame after submit+present).
    ++frame_;

    // Free every retired generation whose slot fence has signaled (ring wrapped).
    retired_.erase(
        std::remove_if(retired_.begin(), retired_.end(),
                       [this](const Generation& g) {
                           // The retiree's last-use submit has completed once the ring
                           // has wrapped `inFlight_` frames past its retirement.
                           const std::uint32_t lag = frame_ - std::min(frame_, g.retireFrame);
                           if (!should_free_generation(lag, inFlight_))
                               return false;
                           if (g.indexMemory)   loader_->vkFreeMemory(device_, g.indexMemory, nullptr);
                           if (g.index)         loader_->vkDestroyBuffer(device_, g.index, nullptr);
                           if (g.vertexMemory)  loader_->vkFreeMemory(device_, g.vertexMemory, nullptr);
                           if (g.vertex)        loader_->vkDestroyBuffer(device_, g.vertex, nullptr);
                           return true;
                       }),
        retired_.end());
}

void RetainedBufferPool::destroy_all()
{
    if (!loader_)
        return;
    if (active_.indexMemory)
        loader_->vkFreeMemory(device_, active_.indexMemory, nullptr);
    if (active_.index)
        loader_->vkDestroyBuffer(device_, active_.index, nullptr);
    if (active_.vertexMemory)
        loader_->vkFreeMemory(device_, active_.vertexMemory, nullptr);
    if (active_.vertex)
        loader_->vkDestroyBuffer(device_, active_.vertex, nullptr);
    active_ = Generation{};

    for (const Generation& g : retired_)
    {
        if (g.indexMemory)
            loader_->vkFreeMemory(device_, g.indexMemory, nullptr);
        if (g.index)
            loader_->vkDestroyBuffer(device_, g.index, nullptr);
        if (g.vertexMemory)
            loader_->vkFreeMemory(device_, g.vertexMemory, nullptr);
        if (g.vertex)
            loader_->vkDestroyBuffer(device_, g.vertex, nullptr);
    }
    retired_.clear();
}

} // namespace detail
} // namespace vaml::vulkan::ui
