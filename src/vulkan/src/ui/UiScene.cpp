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

#include <algorithm>
#include <cstring>

namespace vaml::vulkan::ui {

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
    // S208 Clip: a distinct scissor (or clip-active-vs-inactive) is part of the
    // batch-equality key -> a draw boundary.
    key.clipActive = item.clipActive;
    key.clipX = item.clipX; key.clipY = item.clipY;
    key.clipW = item.clipW; key.clipH = item.clipH;
    // S209 Texture: the item's descriptor set is part of the batch-equality key —
    // items sampling DIFFERENT sets can never share one draw (each set must be
    // bound before its draws).
    key.texture = item.texture;
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

    // Choose the UV sub-rect (glyph atlas sub-rect for Text; full atlas for
    // Solid/Border, which do not sample).
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    if (item.kind == DrawItemKind::TextGlyph)
    {
        u0 = item.u0; v0 = item.v0; u1 = item.u1; v1 = item.v1;
    }

    // S208 per-element Model: CPU-fold `item.model` into the four quad vertex
    // positions BEFORE the S200 MVP projection (the per-element Model composes
    // into the MVP). An IDENTITY model is the no-op fast path (today's rect
    // geometry + batching are unchanged — backward compatible), so we only pay
    // the fold when the element actually carries a transform.
    if (is_transform_identity(item.model))
    {
        quad::append_quad(out, item.x, item.y, item.w, item.h,
                          u0, v0, u1, v1,
                          item.r, item.g, item.b, item.a);
        return;
    }

    // Affine path: emit 4 explicitly-transformed unit-quad corners.
    struct P { float u, v; };
    constexpr P kPos[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };
    std::array<quad::QuadVertex, 4> quad;
    for (std::size_t i = 0; i < 4; ++i)
    {
        const float px = item.x + item.w * kPos[i].u;
        const float py = item.y + item.h * kPos[i].v;
        float ox = 0.0f, oy = 0.0f;
        transform_point(item.model, px, py, ox, oy);
        quad::QuadVertex v;
        v.x = ox; v.y = oy;
        v.u = u0 + (u1 - u0) * kPos[i].u;
        v.v = v0 + (v1 - v0) * kPos[i].v;
        v.r = item.r; v.g = item.g; v.b = item.b; v.a = item.a;
        quad[i] = v;
    }
    out.insert(out.end(), quad.begin(), quad.end());
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
            key.params == cur.params &&
            key.clipActive == cur.clipActive && key.clipX == cur.clipX &&
            key.clipY == cur.clipY && key.clipW == cur.clipW && key.clipH == cur.clipH &&
            key.texture == cur.texture)
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
            // S208: carry the run's clip (scissor) state so record can set a
            // per-batch scissor for a clipped batch.
            b.clipActive = key.clipActive;
            b.clipX = key.clipX; b.clipY = key.clipY;
            b.clipW = key.clipW; b.clipH = key.clipH;
            // S209: carry the run's texture (descriptor set).
            b.texture = key.texture;
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

    // --- create the S209 RetainedBufferPool + the ACTIVE generation at a small cap ---
    // The pool owns the vertex/index buffer generations (the grow path RETIRES the
    // old generation into the pool and frees it once the in-flight fence signals —
    // the S209 fence-driven lifetime, no UAF). The scene's S206 accessors + record
    // bind alias the pool's ACTIVE generation.
    sc->pool_ = std::make_unique<detail::RetainedBufferPool>(loader, device, spec.inFlightCount);
    const std::uint32_t kInitialItems = 16u;
    if (!sc->pool_->ensure(kInitialItems))
        return nullptr;
    sc->vertexBuffer_ = sc->pool_->active().vertex;
    sc->indexBuffer_  = sc->pool_->active().index;
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

    // Grow the buffers on demand via the S209 RetainedBufferPool: a grow RETIRES
    // the previous generation into the pool (never frees it this frame — the
    // S166 in-flight ring may still be reading it); recycle_buffers() frees it
    // once the ring's fence signals. Refresh the record/accessor handle caches
    // to the pool's new ACTIVE generation.
    if (itemCount_ > pool_->active().capacityItems)
    {
        if (!pool_->ensure(itemCount_))  // exact-fit grow (amortized by the caller's reuse)
            return false;
        vertexBuffer_ = pool_->active().vertex;
        indexBuffer_  = pool_->active().index;
    }

    // Encode the vertex buffer (4N) via the S200 append_quad encoder.
    std::vector<quad::QuadVertex> vertices;
    vertices.reserve(ui_vertex_count(itemCount_));
    for (const DrawItem& it : items_)
        append_ui_item(vertices, it);

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(vertices.size() * sizeof(quad::QuadVertex));
    void* vMapped = nullptr;
    if (loader_->vkMapMemory(device_, pool_->active().vertexMemory, 0u, vertexBytes, 0u, &vMapped) != VK_SUCCESS)
        return false;
    std::memcpy(vMapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
    loader_->vkUnmapMemory(device_, pool_->active().vertexMemory);

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
    if (loader_->vkMapMemory(device_, pool_->active().indexMemory, 0u, indexBytes, 0u, &iMapped) != VK_SUCCESS)
        return false;
    std::memcpy(iMapped, indices.data(), static_cast<std::size_t>(indexBytes));
    loader_->vkUnmapMemory(device_, pool_->active().indexMemory);

    return true;
}

bool UiScene::set_texture_binding(std::uint32_t textureId, VkDescriptorSet set)
{
    if (!loader_ || !device_)
        return false;
    // Replace-or-append the texture binding under `textureId`.
    for (auto& entry : textureBindings_)
    {
        if (entry.first == textureId)
        {
            entry.second = set;
            return true;
        }
    }
    textureBindings_.emplace_back(textureId, set);
    return true;
}

VkDescriptorSet UiScene::texture_binding(std::uint32_t textureId) const noexcept
{
    for (const auto& entry : textureBindings_)
    {
        if (entry.first == textureId)
            return entry.second;
    }
    return VK_NULL_HANDLE;
}

void UiScene::recycle_buffers()
{
    if (pool_)
        pool_->recycle();
}

bool UiScene::record_draw(VkCommandBuffer cmd, const Mat4& mvp, float uiScale)
{
    if (!cmd || !loader_ || !device_ || batches_.empty())
        return false;

    // Once-per-frame: viewport + buffers. The scissor + descriptor set are set
    // per batch below (full-extent default scissor; the S209 texture set binds
    // on a texture change — a cheap descriptor rebind, never a pipeline rebind).
    const auto vs = viewport_scissor(extent_);
    loader_->vkCmdSetViewport(cmd, 0, 1, &vs.viewport);
    // pOffsets MUST be a non-null array (>= bindingCount) per the Vulkan spec —
    // passing nullptr is UB and NVIDIA's driver AVs on it. This is the S209
    // vertex-buffer bind path's first-ever LIVE exercise (S177 only clears, so a
    // headless/offscreen test never touched it).
    const VkDeviceSize vboOffset = 0u;
    loader_->vkCmdBindVertexBuffers(cmd, 0u, 1u, &vertexBuffer_, &vboOffset);
    loader_->vkCmdBindIndexBuffer(cmd, indexBuffer_, 0u, VK_INDEX_TYPE_UINT16);

    // The CURRENT scissor for the batch being recorded. Starts at the full
    // extent; a clipActive batch sets its mapped scissor and restores the full
    // extent after (so the next unclipped batch draws to the whole frame).
    VkRect2D currentScissor = vs.scissor;
    loader_->vkCmdSetScissor(cmd, 0, 1, &currentScissor);

    // The per-batch record (the stored batch_plan from the last set_items):
    // bind the batch's kind pipeline (rebind ONLY on a kind change), bind the
    // batch's descriptor set (the SHARED default set; a per-texture set bound
    // ONLY on a texture change — a cheap descriptor rebind, NEVER a pipeline
    // rebind: the S209 minimal-pipeline-switch guard), push one UiPushConstants
    // (mvp + uiScale + the batch's style), draw 6N indices.
    UiShaderKind lastKind = UiShaderKind::Solid;
    VkDescriptorSet lastSet = VK_NULL_HANDLE;  // sentinel: nothing bound yet
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

        // S209 Texture — the batch's descriptor set. A texture CHANGES -> bind the
        // batch's set (a cheap descriptor rebind; NO pipeline change — the minimal
        // pipeline-switch guard holds across the texture axis). The SHARED default
        // set (descriptorSet_) is used for VK_NULL_HANDLE batches (the S206 premise).
        const VkDescriptorSet batchSet = (b.texture != VK_NULL_HANDLE) ? b.texture : descriptorSet_;
        if (firstBatch || batchSet != lastSet)
        {
            loader_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                                             0u, 1u, &batchSet, 0u, nullptr);
            lastSet = batchSet;
        }

        // S208 Clip: a clipped batch sets its OWN dynamic scissor (the element's
        // clip rect in logical px mapped to frame px via uiScale); restore the
        // full-extent scissor for the next unclipped batch.
        if (b.clipActive)
        {
            VkRect2D cs{};
            // Logical-px -> frame-px: the logical viewport spans
            // [0, extent/uiScale] -> [0, extent] px, so framePx = logical*uiScale.
            cs.offset = { static_cast<std::int32_t>(b.clipX * uiScale),
                          static_cast<std::int32_t>(b.clipY * uiScale) };
            const std::int32_t cw = static_cast<std::int32_t>(b.clipW * uiScale);
            const std::int32_t ch = static_cast<std::int32_t>(b.clipH * uiScale);
            // Clamp to the framebuffer extent (never exceed it, never negative).
            cs.extent = {
                static_cast<std::uint32_t>(std::max(0, std::min(cw, static_cast<std::int32_t>(extent_.width)))),
                static_cast<std::uint32_t>(std::max(0, std::min(ch, static_cast<std::int32_t>(extent_.height)))),
            };
            loader_->vkCmdSetScissor(cmd, 0, 1, &cs);
            currentScissor = cs;
        }
        else if (currentScissor.offset.x != vs.scissor.offset.x ||
                 currentScissor.offset.y != vs.scissor.offset.y ||
                 currentScissor.extent.width != vs.scissor.extent.width ||
                 currentScissor.extent.height != vs.scissor.extent.height)
        {
            // Restore the full-extent scissor for an unclipped batch after a
            // clipped one (a clip set the scissor to a sub-rect).
            currentScissor = vs.scissor;
            loader_->vkCmdSetScissor(cmd, 0, 1, &currentScissor);
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
    descriptorPool_ = VK_NULL_HANDLE;
    // The vertex/index buffers are owned by the S209 RetainedBufferPool (now
    // destroyed after the descriptor pool — it frees every generation; the caller
    // must have device-idled the loop first so nothing references a generation).
    pool_.reset();
    vertexBuffer_ = VK_NULL_HANDLE;
    indexBuffer_  = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::ui
