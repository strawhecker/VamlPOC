// S206 — UiScene implementation: the UI primitive draw batcher.
//
// Implements the S206 pure decision seams (headless-testable) + the real
// UiScene::create/set_items/record_draw path. See UiScene.h for the split +
// the shared-layout contract + LIFETIMES. The recorder reuses the S200
// append_quad encoder + the S205 shared push block, so S206 is pure glue over
// the already-delivered quad mesh + shared UI layout.

#include <vaml/vulkan/ui/UiScene.h>
#include <vaml/vulkan/Pipeline.h>  // viewport_scissor (the S165 dynamic viewport+scissor seam)
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad::ui_projection + is_srgb_format

#include <cstring>

namespace vaml::vulkan::ui {

namespace {

// Picks a host-visible|coherent memory type (mirror QuadScene/QuadTexture).
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

DrawItem solid_rect(float x, float y, float w, float h, const Color4& color) noexcept
{
    DrawItem it{};
    it.kind = DrawItemKind::SolidRect;
    it.x = x; it.y = y; it.w = w; it.h = h;
    it.r = color[0]; it.g = color[1]; it.b = color[2]; it.a = color[3];
    return it;
}

DrawItem border_rect(float x, float y, float w, float h, const Color4& color,
                     float thickness01, float radius01, float feather) noexcept
{
    DrawItem it{};
    it.kind = DrawItemKind::BorderRect;
    it.x = x; it.y = y; it.w = w; it.h = h;
    it.r = color[0]; it.g = color[1]; it.b = color[2]; it.a = color[3];
    it.thickness01 = thickness01;
    it.radius01    = radius01;
    it.feather     = feather;
    return it;
}

DrawItem text_glyph(float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1,
                    const Color4& color, float aa) noexcept
{
    DrawItem it{};
    it.kind = DrawItemKind::TextGlyph;
    it.x = x; it.y = y; it.w = w; it.h = h;
    it.r = color[0]; it.g = color[1]; it.b = color[2]; it.a = color[3];
    it.u0 = u0; it.v0 = v0; it.u1 = u1; it.v1 = v1;
    it.aa = aa;
    return it;
}

bool ui_item_degenerate(const DrawItem& item) noexcept
{
    // Zero/negative extent on either axis produces no renderable geometry.
    return !(item.w > 0.0f) || !(item.h > 0.0f);
}

UiBatchKey ui_batch_key(const DrawItem& item) noexcept
{
    UiBatchKey key{};
    key.kind = item.kind;
    key.color = { item.r, item.g, item.b, item.a };
    switch (item.kind)
    {
    case DrawItemKind::TextGlyph:
        key.params = { item.aa, 0.0f, 0.0f, 0.0f };  // sdf_style
        break;
    case DrawItemKind::BorderRect:
        key.params = { item.thickness01, item.radius01, item.feather, 0.0f };  // border_style
        break;
    case DrawItemKind::SolidRect:
    default:
        key.params = { 0.0f, 0.0f, 0.0f, 0.0f };  // solid_style
        break;
    }
    return key;
}

UiShaderKind draw_kind_to_shader(DrawItemKind kind) noexcept
{
    switch (kind)
    {
    case DrawItemKind::TextGlyph:  return UiShaderKind::SdfText;
    case DrawItemKind::BorderRect: return UiShaderKind::Border;
    case DrawItemKind::SolidRect:
    default:                       return UiShaderKind::Solid;
    }
}

std::uint32_t ui_vertex_count(std::uint32_t n) noexcept
{
    return n * quad::quad_vertex_count();  // 4N
}

std::uint32_t ui_index_count(std::uint32_t n) noexcept
{
    return n * quad::quad_index_count();  // 6N
}

void append_ui_item(std::vector<quad::QuadVertex>& out, const DrawItem& item) noexcept
{
    if (ui_item_degenerate(item))
        return;
    if (item.kind == DrawItemKind::TextGlyph)
    {
        quad::append_quad(out, item.x, item.y, item.w, item.h,
                          item.u0, item.v0, item.u1, item.v1,
                          item.r, item.g, item.b, item.a);
    }
    else
    {
        // Solid/border fill the whole atlas UV (they do not sample).
        quad::append_quad(out, item.x, item.y, item.w, item.h,
                          0.0f, 0.0f, 1.0f, 1.0f,
                          item.r, item.g, item.b, item.a);
    }
}

std::vector<UiBatch>
batch_plan(std::span<const DrawItem> items) noexcept
{
    std::vector<UiBatch> batches;
    if (items.empty())
        return batches;

    UiBatchKey cur{};
    bool haveCur = false;
    std::uint32_t compacted = 0;  // index into the compacted item sequence

    for (const DrawItem& item : items)
    {
        if (ui_item_degenerate(item))
            continue;  // skipped: never batched, never drawn, never reordered

        const UiBatchKey key = ui_batch_key(item);
        if (haveCur && key.kind == cur.kind && key.color == cur.color &&
            key.params == cur.params)
        {
            // Extend the current batch (contiguous same-key run).
            ++batches.back().count;
        }
        else
        {
            UiBatch b{};
            b.kind  = draw_kind_to_shader(item.kind);
            b.first = compacted;
            b.count = 1;
            batches.push_back(b);
            cur = key;
            haveCur = true;
        }
        ++compacted;
    }
    return batches;
}

const std::vector<UiRecordStep>& ui_record_steps() noexcept
{
    static const std::vector<UiRecordStep> plan = {
        UiRecordStep::BindPipeline,
        UiRecordStep::SetViewport,
        UiRecordStep::SetScissor,
        UiRecordStep::BindDescriptorSet,
        UiRecordStep::PushUiConstants,
        UiRecordStep::BindVertexBuffers,
        UiRecordStep::BindIndexBuffer,
        UiRecordStep::DrawIndexed,
    };
    return plan;
}

std::unique_ptr<UiScene>
UiScene::create(const VulkanLoader& loader, const Device& device, const Spec& spec)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_quad_loader() || !loader.has_texture_loader() || !vkDevice)
        return nullptr;
    if (!spec.renderPass || !spec.pipelineLayout || !spec.setLayout ||
        !spec.sdfPipeline || !spec.solidPipeline || !spec.borderPipeline ||
        !spec.atlasView || !spec.atlasSampler ||
        spec.colorFormat == VK_FORMAT_UNDEFINED || !quad::is_srgb_format(spec.colorFormat))
    {
        return nullptr;
    }

    std::unique_ptr<UiScene> sc(new UiScene(loader, device));
    sc->physical_        = device.physical_device();
    sc->pipelineLayout_  = spec.pipelineLayout;
    sc->sdfPipeline_     = spec.sdfPipeline;
    sc->solidPipeline_   = spec.solidPipeline;
    sc->borderPipeline_  = spec.borderPipeline;
    sc->colorFormat_     = spec.colorFormat;
    sc->extent_          = spec.extent;

    // --- create the vertex + index buffers (host-visible) at a small initial cap ---
    const std::uint32_t kInitialItems = 16u;
    sc->capacityItems_ = kInitialItems;
    const VkDeviceSize kInitialVerts  = ui_vertex_count(kInitialItems) * sizeof(quad::QuadVertex);
    const VkDeviceSize kInitialIndices = ui_index_count(kInitialItems) * sizeof(std::uint16_t);

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

    // --- create the S205-SHARED descriptor pool + ONE descriptor set ---
    // The set is allocated from the SHARED set layout (the S206 premise: one
    // bound set serves every batch/kind — never a per-kind rebind).
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
    dsai.pSetLayouts        = &spec.setLayout;  // the SHARED set-0 binding-0 layout
    if (loader.vkAllocateDescriptorSets(vkDevice, &dsai, &sc->descriptorSet_) != VK_SUCCESS)
        return nullptr;

    // --- write the descriptor: the SHARED set-0-binding-0 atlas sampler ---
    VkDescriptorImageInfo info{};
    const VkWriteDescriptorSet write = ui_descriptor_write(sc->descriptorSet_, info,
                                                           spec.atlasView, spec.atlasSampler);
    loader.vkUpdateDescriptorSets(vkDevice, 1u, &write, 0u, nullptr);

    return sc;
}

bool UiScene::set_items(std::span<const DrawItem> items)
{
    if (!loader_ || !device_)
        return false;

    // Compact (skip degenerate) + recompute the batch plan off the compacted list.
    items_.clear();
    items_.reserve(items.size());
    for (const DrawItem& it : items)
    {
        if (!ui_item_degenerate(it))
            items_.push_back(it);
    }
    batches_ = batch_plan(items_);
    itemCount_ = static_cast<std::uint32_t>(items_.size());

    if (itemCount_ == 0u)
        return true;  // empty scene -> zero batches next record

    // Grow the buffers on demand when the compacted list exceeds the cap.
    if (itemCount_ > capacityItems_)
    {
        const std::uint32_t newCap = itemCount_;  // exact-fit grow (amortized by the caller's reuse)
        const VkDeviceSize newVBase = static_cast<VkDeviceSize>(ui_vertex_count(newCap) * sizeof(quad::QuadVertex));
        const VkDeviceSize newIBase = static_cast<VkDeviceSize>(ui_index_count(newCap) * sizeof(std::uint16_t));

        VkBufferCreateInfo nvbci{};
        nvbci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        nvbci.size        = newVBase;
        nvbci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        nvbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer nvb = VK_NULL_HANDLE;
        VkDeviceMemory nvm = VK_NULL_HANDLE;
        if (loader_->vkCreateBuffer(device_, &nvbci, nullptr, &nvb) != VK_SUCCESS)
            return false;
        VkMemoryRequirements nvreq{};
        loader_->vkGetBufferMemoryRequirements(device_, nvb, &nvreq);
        const std::uint32_t nvtype = find_host_visible_type(*loader_, physical_, nvreq);
        if (nvtype == UINT32_MAX) { loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }
        VkMemoryAllocateInfo nvmai{};
        nvmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        nvmai.allocationSize = nvreq.size; nvmai.memoryTypeIndex = nvtype;
        if (loader_->vkAllocateMemory(device_, &nvmai, nullptr, &nvm) != VK_SUCCESS)
        { loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }
        if (loader_->vkBindBufferMemory(device_, nvb, nvm, 0u) != VK_SUCCESS)
        { loader_->vkFreeMemory(device_, nvm, nullptr); loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }

        VkBufferCreateInfo nibci{};
        nibci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        nibci.size = newIBase;
        nibci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        nibci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer nib = VK_NULL_HANDLE;
        VkDeviceMemory nim = VK_NULL_HANDLE;
        if (loader_->vkCreateBuffer(device_, &nibci, nullptr, &nib) != VK_SUCCESS)
        { loader_->vkFreeMemory(device_, nvm, nullptr); loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }
        VkMemoryRequirements nireq{};
        loader_->vkGetBufferMemoryRequirements(device_, nib, &nireq);
        const std::uint32_t nitype = find_host_visible_type(*loader_, physical_, nireq);
        if (nitype == UINT32_MAX) { loader_->vkDestroyBuffer(device_, nib, nullptr); loader_->vkFreeMemory(device_, nvm, nullptr); loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }
        VkMemoryAllocateInfo nimai{};
        nimai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        nimai.allocationSize = nireq.size; nimai.memoryTypeIndex = nitype;
        if (loader_->vkAllocateMemory(device_, &nimai, nullptr, &nim) != VK_SUCCESS)
        { loader_->vkDestroyBuffer(device_, nib, nullptr); loader_->vkFreeMemory(device_, nvm, nullptr); loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }
        if (loader_->vkBindBufferMemory(device_, nib, nim, 0u) != VK_SUCCESS)
        { loader_->vkFreeMemory(device_, nim, nullptr); loader_->vkDestroyBuffer(device_, nib, nullptr); loader_->vkFreeMemory(device_, nvm, nullptr); loader_->vkDestroyBuffer(device_, nvb, nullptr); return false; }

        // Swap in the new pair, free the old.
        loader_->vkFreeMemory(device_, indexMemory_, nullptr);
        loader_->vkDestroyBuffer(device_, indexBuffer_, nullptr);
        loader_->vkFreeMemory(device_, vertexMemory_, nullptr);
        loader_->vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        indexBuffer_ = nib; indexMemory_ = nim;
        vertexBuffer_ = nvb; vertexMemory_ = nvm;
        capacityItems_ = newCap;
    }

    // Encode the vertex buffer (4N) via the S200 append_quad encoder.
    std::vector<quad::QuadVertex> vertices;
    vertices.reserve(ui_vertex_count(itemCount_));
    for (const DrawItem& it : items_)
        append_ui_item(vertices, it);

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(vertices.size() * sizeof(quad::QuadVertex));
    void* vMapped = nullptr;
    if (loader_->vkMapMemory(device_, vertexMemory_, 0u, vertexBytes, 0u, &vMapped) != VK_SUCCESS)
        return false;
    std::memcpy(vMapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
    loader_->vkUnmapMemory(device_, vertexMemory_);

    // Build the index buffer [0,1,2,0,2,3] offset by 4*i for item i (the S200
    // quad_indices pattern) — N items = 6N indices.
    std::vector<std::uint16_t> indices(ui_index_count(itemCount_));
    const auto& base = quad::quad_indices();
    for (std::uint32_t q = 0; q < itemCount_; ++q)
    {
        const std::uint16_t off = static_cast<std::uint16_t>(q * quad::quad_vertex_count());
        for (std::uint32_t k = 0; k < quad::quad_index_count(); ++k)
            indices[q * quad::quad_index_count() + k] = static_cast<std::uint16_t>(base[k] + off);
    }
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint16_t));
    void* iMapped = nullptr;
    if (loader_->vkMapMemory(device_, indexMemory_, 0u, indexBytes, 0u, &iMapped) != VK_SUCCESS)
        return false;
    std::memcpy(iMapped, indices.data(), static_cast<std::size_t>(indexBytes));
    loader_->vkUnmapMemory(device_, indexMemory_);

    return true;
}

bool UiScene::record_draw(VkCommandBuffer cmd, const Mat4& mvp, float uiScale)
{
    if (!cmd || !loader_ || !device_ || batches_.empty())
        return false;

    // Once-per-frame: viewport/scissor + the SHARED descriptor set + buffers.
    const auto vs = viewport_scissor(extent_);
    loader_->vkCmdSetViewport(cmd, 0, 1, &vs.viewport);
    loader_->vkCmdSetScissor(cmd, 0, 1, &vs.scissor);
    loader_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                     0u, 1u, &descriptorSet_, 0u, nullptr);
    loader_->vkCmdBindVertexBuffers(cmd, 0u, 1u, &vertexBuffer_, nullptr);
    loader_->vkCmdBindIndexBuffer(cmd, indexBuffer_, 0u, VK_INDEX_TYPE_UINT16);

    // The per-batch record (the stored batch_plan from the last set_items):
    // bind the batch's kind pipeline (rebind ONLY on a kind change), push one
    // UiPushConstants (mvp + uiScale + the batch's style), draw 6N indices.
    UiShaderKind lastKind = UiShaderKind::Solid;
    bool firstBatch = true;
    for (const UiBatch& b : batches_)
    {
        VkPipeline pipeline = solidPipeline_;
        if (b.kind == UiShaderKind::SdfText)      pipeline = sdfPipeline_;
        else if (b.kind == UiShaderKind::Border)  pipeline = borderPipeline_;

        if (firstBatch || b.kind != lastKind)
        {
            loader_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            lastKind = b.kind;
            firstBatch = false;
        }

        // The S205 shared 128-byte UiPushConstants (mvp -> the 0 offset; uiAndPad.x
        // = uiScale at 64; then the batch's style flash straight onto color/params).
        UiPushConstants pc{};
        pc.mvp = mvp;
        pc.uiAndPad[0] = uiScale;
        const DrawItem& rep = items_[b.first];  // the batch's representative item
        if (b.kind == UiShaderKind::SdfText)
            sdf_style(pc, { rep.r, rep.g, rep.b, rep.a }, rep.aa);
        else if (b.kind == UiShaderKind::Border)
            border_style(pc, { rep.r, rep.g, rep.b, rep.a }, rep.thickness01, rep.radius01, rep.feather);
        else
            solid_style(pc, { rep.r, rep.g, rep.b, rep.a });

        const VkPushConstantRange range = ui_push_constant();
        loader_->vkCmdPushConstants(cmd, pipelineLayout_, range.stageFlags, range.offset,
                                    sizeof(UiPushConstants), &pc);
        loader_->vkCmdDrawIndexed(cmd, b.index_count(), 1u, b.first * 6u, 0, 0u);
    }
    return true;
}

bool UiScene::record_draw(VkCommandBuffer cmd)
{
    return record_draw(cmd, quad::ui_projection(extent_.width, extent_.height, 1.0f), 1.0f);
}

UiScene::UiScene(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

UiScene::UiScene(UiScene&&) noexcept = default;
UiScene& UiScene::operator=(UiScene&&) noexcept = default;

UiScene::~UiScene()
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
    descriptorPool_ = VK_NULL_HANDLE;
    indexMemory_    = VK_NULL_HANDLE;
    indexBuffer_    = VK_NULL_HANDLE;
    vertexMemory_   = VK_NULL_HANDLE;
    vertexBuffer_   = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::ui
