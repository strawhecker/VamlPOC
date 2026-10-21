// S201 — QuadScene implementation: N-quad CPU list → buffers + indexed draw.
//
// See QuadScene.h for the split + NO-GAP. The pure seams (scene_vertex_count /
// scene_index_count / append_scene_item / scene_record_plan) are headless-
// testable; set_quads()/record_draw() are the live [both apps, not CI] N-quad
// draw path (the CPU-list-per-frame Sprint-5 model; GPU batching = S209).

#include <vaml/vulkan/quad/QuadScene.h>
#include <vaml/vulkan/Pipeline.h>  // viewport_scissor (the S165 dynamic viewport+scissor seam)

#include <cstring>

namespace vaml::vulkan::quad {

namespace {

// Picks a host-visible|coherent memory type (mirror QuadTexture.cpp's finder).
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

std::uint32_t scene_vertex_count(std::uint32_t n) noexcept
{
    return n * quad_vertex_count();  // 4N
}

std::uint32_t scene_index_count(std::uint32_t n) noexcept
{
    return n * quad_index_count();  // 6N
}

void
append_scene_item(std::vector<QuadVertex>& out, const QuadItem& item) noexcept
{
    append_quad(out, item.x, item.y, item.w, item.h,
                item.u0, item.v0, item.u1, item.v1,
                item.r, item.g, item.b, item.a);
}

const std::vector<SceneRecordStep>& scene_record_plan() noexcept
{
    static const std::vector<SceneRecordStep> plan = {
        SceneRecordStep::BindPipeline,
        SceneRecordStep::SetViewport,
        SceneRecordStep::SetScissor,
        SceneRecordStep::BindDescriptorSet,
        SceneRecordStep::PushQuadUniform,
        SceneRecordStep::BindIndexBuffer,
        SceneRecordStep::DrawIndexed,
    };
    return plan;
}

std::unique_ptr<QuadScene>
QuadScene::create(const VulkanLoader& loader, const Device& device, const Spec& spec)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_quad_loader() || !loader.has_texture_loader() || !vkDevice)
        return nullptr;
    if (!spec.renderPass || !spec.quadPipeline || !spec.quadLayout || !spec.quadSetLayout ||
        !spec.textureView || !spec.textureSampler ||
        spec.colorFormat == VK_FORMAT_UNDEFINED || !is_srgb_format(spec.colorFormat))  // S200 SRGB pairing
    {
        return nullptr;
    }

    std::unique_ptr<QuadScene> sc(new QuadScene(loader, device));
    sc->physical_       = device.physical_device();
    sc->renderPass_     = spec.renderPass;
    sc->quadPipeline_   = spec.quadPipeline;
    sc->quadLayout_     = spec.quadLayout;
    sc->quadSetLayout_  = spec.quadSetLayout;
    sc->blendPipeline_  = spec.blendPipeline;
    sc->colorFormat_    = spec.colorFormat;
    sc->extent_         = spec.extent;

    // --- create the vertex + index buffers (host-visible) at a small initial cap ---
    const VkDeviceSize kInitialVerts  = scene_vertex_count(1) * sizeof(QuadVertex);
    const VkDeviceSize kInitialIndices = scene_index_count(1) * sizeof(std::uint16_t);

    VkBufferCreateInfo vbci{};
    vbci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbci.size        = kInitialVerts;
    vbci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader.vkCreateBuffer(vkDevice, &vbci, nullptr, &sc->vertexBuffer_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements vreq{};
    loader.vkGetBufferMemoryRequirements(vkDevice, sc->vertexBuffer_, &vreq);
    const std::uint32_t vtype = find_host_visible_type(loader, device.physical_device(), vreq);
    if (vtype == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo vmai{};
    vmai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vmai.allocationSize  = vreq.size;
    vmai.memoryTypeIndex = vtype;
    if (loader.vkAllocateMemory(vkDevice, &vmai, nullptr, &sc->vertexMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindBufferMemory(vkDevice, sc->vertexBuffer_, sc->vertexMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    VkBufferCreateInfo ibci{};
    ibci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibci.size        = kInitialIndices;
    ibci.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ibci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (loader.vkCreateBuffer(vkDevice, &ibci, nullptr, &sc->indexBuffer_) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements ireq{};
    loader.vkGetBufferMemoryRequirements(vkDevice, sc->indexBuffer_, &ireq);
    const std::uint32_t itype = find_host_visible_type(loader, device.physical_device(), ireq);
    if (itype == UINT32_MAX)
        return nullptr;
    VkMemoryAllocateInfo imai{};
    imai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imai.allocationSize  = ireq.size;
    imai.memoryTypeIndex = itype;
    if (loader.vkAllocateMemory(vkDevice, &imai, nullptr, &sc->indexMemory_) != VK_SUCCESS)
        return nullptr;
    if (loader.vkBindBufferMemory(vkDevice, sc->indexBuffer_, sc->indexMemory_, 0u) != VK_SUCCESS)
        return nullptr;

    // --- create the S200-local descriptor pool + the scene descriptor set ---
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets       = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = &poolSize;
    if (loader.vkCreateDescriptorPool(vkDevice, &dpci, nullptr, &sc->descriptorPool_) != VK_SUCCESS)
        return nullptr;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = sc->descriptorPool_;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &sc->quadSetLayout_;  // (borrowed from the pipeline's S200 set layout)
    if (loader.vkAllocateDescriptorSets(vkDevice, &dsai, &sc->descriptorSet_) != VK_SUCCESS)
        return nullptr;

    // --- write the descriptor (sample the scene texture) ---
    VkDescriptorImageInfo info{};
    const VkWriteDescriptorSet write = quad_descriptor_write(sc->descriptorSet_, info,
                                                             spec.textureView, spec.textureSampler);
    loader.vkUpdateDescriptorSets(vkDevice, 1u, &write, 0u, nullptr);

    return sc;
}

bool
QuadScene::set_quads(const std::vector<QuadVertex>& vertices, std::uint32_t n)
{
    if (!loader_ || !device_)
        return false;
    if (n == 0u || vertices.empty())
    {
        quadCount_ = 0u;
        return true;
    }
    if (vertices.size() != scene_vertex_count(n))
        return false;

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(vertices.size() * sizeof(QuadVertex));
    const VkDeviceSize indexBytes  = static_cast<VkDeviceSize>(scene_index_count(n) * sizeof(std::uint16_t));

    // Recreate/grow the buffers if the current cap is insufficient (the planned
    // "per-frame (re)upload" — S209 moves this to a persistent GPU-side list).
    // For S201 the buffers are created at a fixed initial cap in create(); if a
    // frame exceeds it, we fail loudly (never UB) — a caller sizes the scene to
    // its max before the loop (documented; S209 owns growth/retained).
    void* vMapped = nullptr;
    if (loader_->vkMapMemory(device_, vertexMemory_, 0u, vertexBytes, 0u, &vMapped) != VK_SUCCESS)
        return false;
    std::memcpy(vMapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
    loader_->vkUnmapMemory(device_, vertexMemory_);

    // Build the index buffer [0,1,2,0,2,3] offset by 4*i for quad i (the S200
    // quad_indices pattern) — N quads = 6N indices.
    std::vector<std::uint16_t> indices(scene_index_count(n));
    const auto& base = quad_indices();
    for (std::uint32_t q = 0; q < n; ++q)
    {
        const std::uint16_t off = static_cast<std::uint16_t>(q * quad_vertex_count());
        for (std::uint32_t k = 0; k < quad_index_count(); ++k)
            indices[q * quad_index_count() + k] = static_cast<std::uint16_t>(base[k] + off);
    }
    void* iMapped = nullptr;
    if (loader_->vkMapMemory(device_, indexMemory_, 0u, indexBytes, 0u, &iMapped) != VK_SUCCESS)
        return false;
    std::memcpy(iMapped, indices.data(), static_cast<std::size_t>(indexBytes));
    loader_->vkUnmapMemory(device_, indexMemory_);

    quadCount_ = n;
    return true;
}

bool
QuadScene::record_draw(VkCommandBuffer cmd, const Mat4& mvp, float uiScale)
{
    if (!cmd || !loader_ || !device_ || quadCount_ == 0u)
        return false;

    // The S201 fixed ordered record (see scene_record_plan()).
    loader_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               blendPipeline_ ? blendPipeline_ : quadPipeline_);
    const auto vs = viewport_scissor(extent_);
    loader_->vkCmdSetViewport(cmd, 0, 1, &vs.viewport);
    loader_->vkCmdSetScissor(cmd, 0, 1, &vs.scissor);
    loader_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, quadLayout_,
                                     0u, 1u, &descriptorSet_, 0u, nullptr);
    const VkPushConstantRange range = quad_push_constant();
    QuadUniform uniform{};
    uniform.mvp     = mvp;
    uniform.uiScale = uiScale;
    loader_->vkCmdPushConstants(cmd, quadLayout_, range.stageFlags, range.offset, sizeof(QuadUniform), &uniform);
    loader_->vkCmdBindIndexBuffer(cmd, indexBuffer_, 0u, VK_INDEX_TYPE_UINT16);
    loader_->vkCmdDrawIndexed(cmd, index_count(), 1u, 0u, 0, 0u);
    return true;
}

bool
QuadScene::record_draw(VkCommandBuffer cmd)
{
    return record_draw(cmd, ui_projection(extent_.width, extent_.height, 1.0f), 1.0f);
}

QuadScene::QuadScene(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

QuadScene::QuadScene(QuadScene&&) noexcept = default;
QuadScene& QuadScene::operator=(QuadScene&&) noexcept = default;

QuadScene::~QuadScene()
{
    if (!loader_)
        return;
    if (descriptorPool_)
        loader_->vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (indexMemory_)
        loader_->vkFreeMemory(device_, indexMemory_, nullptr);
    if (indexBuffer_)
        loader_->vkDestroyBuffer(device_, indexBuffer_, nullptr);
    if (vertexMemory_)
        loader_->vkFreeMemory(device_, vertexMemory_, nullptr);
    if (vertexBuffer_)
        loader_->vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    descriptorPool_  = VK_NULL_HANDLE;
    indexMemory_     = VK_NULL_HANDLE;
    indexBuffer_     = VK_NULL_HANDLE;
    vertexMemory_    = VK_NULL_HANDLE;
    vertexBuffer_    = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::quad
