#pragma once
// S206 — the UI primitive draw batcher: rects + text glyphs from the SHARED
// descriptor layout, batching same-shader draws (vaml::vulkan::ui), dep:S205.
//
// S206 is the named consumer of S205's shared-layout premise. Given a material-
// agnostic list of draw items (solid rects, border rects, positioned SDF glyph
// quads), S206 BATCHES CONTIGUOUS same-shader draws into ONE vkCmdDrawIndexed
// (6N) each and records them through the S205 SHARED descriptor-set layout +
// the S205 SHARED 128-byte UiPushConstants + the S205 SHARED pipeline layout.
// The S205 premise it discharges: switching kinds = a pipeline rebind + a push
// update, NEVER a descriptor-set rebind (one bound set serves every batch/kind).
//
// The batcher is honest about what can share a single draw call: a batch groups
// CONTIGUOUS items that share the SAME push-constant state (kind + color +
// params) — only then does ONE vkCmdDrawIndexed(6N) with ONE push produce
// render-correct pixels. Text glyphs in a batch may differ in UV (the atlas sub-
// rect is PER-VERTEX data, not a push field), so contiguous glyphs of the same
// color+aa collapse into ONE draw. A per-item instanced/indirect buffer that
// lifts the same-state requirement is S209/S622 GPU-batching territory (recorded
// NO-GAP, not silent).
//
// Scope (owned elsewhere, NOT silent): the visual-tree walk (draw-list-by-
// material + z-order via tree order) is S207, which FEEDS this batcher's item
// list; text MEASURE/WRAP layout is S400 (S206 takes positioned glyph quads
// only); per-element Model composition into UiPushConstants.mvp is S208; the
// Monochrome tint baseline is deliberately unexercised here (S400_1/S528_1 own
// it) though the kind rides the identical shared layout.
//
// Split (the standing precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime) —
//       DrawItemKind { SolidRect, BorderRect, TextGlyph }
//       DrawItem (the pure value struct) + the rect/glyph constructors
//       ui_item_degenerate(item)         (zero/negative geometry -> skipped)
//       ui_batch_key(item)               (the batch-equality key: kind+color+params)
//       batch_plan(items) -> vector<UiBatch> (contiguous same-key batches)
//       ui_vertex_count(n)=4n / ui_index_count(n)=6n (the per-batch counts)
//       draw_kind_to_shader(kind)        (DrawItemKind -> UiShaderKind)
//       append_ui_item(out, item)        (one item -> 4 QuadVertex via S200)
//       ui_record_steps()                (the fixed ordered single-batch draw steps)
//     These are the S206 proof surface proven in ctest WITHOUT a live runtime.
//   * `UiScene::create(...)` + set_items/record_draw — the REAL owner (RAII,
//     move-only): owns the host-visible vertex + index buffers, the descriptor
//     pool + ONE descriptor set (allocated from the S205 SHARED set layout,
//     written to the atlas sampler), and records the batched draws.
//
// LIFETIMES: loader/device/render-pass/atlas-view/atlas-sampler + the three
// UiPipelines + the shared pipeline layout + shared set layout must OUTLIVE the
// UiScene (it borrows them). Destroy order: UiScene -> UiPipelines -> shader
// modules -> render pass -> device. ADR-018: zero new locks (single-threaded
// per-frame CPU list encode + record, the Sprint-5 model per S201).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/ui/UiPipeline.h>
#include <vaml/vulkan/ui/RetainedBufferPool.h>
#include <vaml/vulkan/quad/QuadVertex.h>

#include <vaml/core/Transform.h>   // vaml::Matrix2D (S208 per-element Model)

#include <cstdint>
#include <memory>
#include <span>
#include <utility>   // std::pair (S209 textureBindings_)
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The three S206 primitive draw kinds, each rendering the S200 unit quad via
// one of the S205 UiPipeline kinds: SolidRect -> Solid, BorderRect -> Border,
// TextGlyph -> SdfText (see draw_kind_to_shader).
enum class DrawItemKind
{
    SolidRect = 0,   // a flat straight-alpha rectangle (no sampling)
    BorderRect = 1,  // a rectangular stroke (thickness01/radius01/feather)
    TextGlyph = 2,   // a positioned SDF glyph quad sampling the shared atlas
};

// One primitive draw item: the rectangle (top-left in LOGICAL px) + the fields
// the S205 shader kinds consume. A FLAT value struct (the S201 QuadItem
// precedent — no union/polymorphism, all fields carry through; the kind +
// style fillers decide which are meaningful):
//   * SolidRect / BorderRect: x/y/w/h + straight-alpha color; border also uses
//     thickness01/radius01/feather.
//   * TextGlyph: x/y/w/h + the atlas sub-rect (u0,v0)-(u1,v1) the glyph quad
//     samples + the straight-alpha color + aa (the S204 sdf_soft_alpha ramp
//     half-width). UV varies PER GLYPH (vertex data), so contiguous glyphs of
//     the same color+aa collapse into ONE batched draw.
//
// S208 (per-element Model + Clip): `model` is the element's Matrix2D render
// transform (identity defaults to today's rect geometry). append_ui_item CPU-
// FOLDS `model` into the four vertex positions BEFORE the S200 MVP projection
// (producing the per-element Model∘MVP composition) — an IDENTITY model is the
// no-op fast path, so existing S206/S207 geometry + batching is unchanged
// (backward compatible). `clip` = the element's clip rect in logical px (the
// S124 render slot / final_rect) mapped to frame px at record; `clipActive`
// marks a non-default scissor that must force a draw boundary (see the S208
// batch-split rule).
struct DrawItem
{
    DrawItemKind kind = DrawItemKind::SolidRect;

    float x = 0.0f, y = 0.0f;   // top-left in logical px
    float w = 0.0f, h = 0.0f;   // extent (logical px)

    // S208: the per-element affine Model. Default identity (no transform). The
    // encoder CPU-folds it into the vertex positions (Model∘MVP); it is PER-
    // VERTEX data, so a transform change does NOT break a same-state batch.
    vaml::Matrix2D model{};

    // S208 Clip: when `clipActive`, the element's clip rect in LOGICAL px
    // (== final_rect for a base element). record sets a dynamic scissor mapped
    // to frame px for this item's batch. `clipActive` false = no scissor change
    // (today's full-extent scissor) — identity-transform unclipped items keep
    // the existing batching.
    bool  clipActive = false;
    float clipX = 0.0f, clipY = 0.0f;   // top-left of the clip rect (logical px)
    float clipW = 0.0f, clipH = 0.0f;   // extent of the clip rect (logical px)

    // straight-alpha color (the S205 pc.color), shared by all three kinds.
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    // BorderRect only: { thickness01, radius01, feather } (the S205 border seam).
    float thickness01 = 0.0f;
    float radius01    = 0.0f;
    float feather     = 0.0f;

    // TextGlyph only: the atlas sub-rect (2nd atlas UV pair) + the SDF aa.
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 1.0f, v1 = 1.0f;
    float aa = 0.15f;   // the S204 sdf_soft_alpha half-width (default soft edge)

    // S209 — the TEXTURE IDENTITY this item samples. `VK_NULL_HANDLE` = the
    // scene's DEFAULT shared atlas set (set-0-binding-0, the S206 premise);
    // a non-null VkDescriptorSet = a per-texture descriptor set registered via
    // UiScene::set_texture_binding. Texture is PART OF THE BATCH KEY: items
    // sampling DIFFERENT descriptor sets can never share one vkCmdDrawIndexed
    // (each set must be bound before its draws). Because texture changes only
    // rebind a DESCRIPTOR SET (cheap) and NEVER a pipeline, the S209
    // "minimal pipeline switches" hold: a same-kind texture change stays in the
    // same pipeline, only scrolling the bound set.
    VkDescriptorSet texture = VK_NULL_HANDLE;
};

// Constructors for the three kinds (pure; the S201 append_scene_item precedent).

// A flat solid rectangle of `color` at (x,y) sized (w,h).
VAML_VULKAN_EXPORT DrawItem solid_rect(float x, float y, float w, float h,
                                       const Color4& color) noexcept;

// A border (rectangular stroke) of `color` at (x,y) sized (w,h), with the
// S205 border seam params (thickness01 = width/edge, radius01 = corner
// rounding, feather = shaded edge half-width).
VAML_VULKAN_EXPORT DrawItem border_rect(float x, float y, float w, float h,
                                        const Color4& color,
                                        float thickness01, float radius01,
                                        float feather) noexcept;

// A positioned SDF glyph quad at (x,y) sized (w,h) sampling the atlas sub-rect
// (u0,v0)-(u1,v1), of `color`, with the SDF AA half-width `aa`.
VAML_VULKAN_EXPORT DrawItem text_glyph(float x, float y, float w, float h,
                                       float u0, float v0, float u1, float v1,
                                       const Color4& color, float aa) noexcept;

// True iff `item` is DEGENERATE (zero/negative extent on either axis). Such an
// item produces no renderable geometry and is SKIPPED by batch_plan() + the
// UiScene encoder (never a negative/zero-size draw). Pure + headless-testable.
VAML_VULKAN_EXPORT bool ui_item_degenerate(const DrawItem& item) noexcept;

// The BATCH-EQUALITY KEY: { kind, color, params } — the push-constant state
// that a single vkCmdDrawIndexed(6N) + one push must share. `params` mirrors
// the S205 kind filler: Solid -> {0,0,0,0}; BorderRect -> { thickness01,
// radius01, feather, 0 }; TextGlyph -> { aa, 0, 0, 0 }. Two items with equal
// keys render identically under one push, so they can share one batched draw
// (their per-item geometry/UV differ through the VERTEX data). Pure + headless.
struct UiBatchKey
{
    DrawItemKind kind = DrawItemKind::SolidRect;
    Color4       color{};
    Color4       params{};
    // S208 Clip — part of the batch-equality key: a batch sets ONE dynamic
    // scissor, so items that disagree on the clip rect can NOT share a draw.
    // `clipActive` false (the identity/default) groups with anything else that
    // is also unclipped; a non-default clip forces a draw boundary (same-color
    // unclipped items never merge with a clipped one).
    bool  clipActive = false;
    float clipX = 0.0f, clipY = 0.0f;
    float clipW = 0.0f, clipH = 0.0f;
    // S209 — the TEXTURE identity (see DrawItem::texture): VK_NULL_HANDLE (the
    // default shared atlas set) groups with any other default item; two items
    // sampling DIFFERENT registered sets never share a batch. A texture change
    // is a descriptor-set rebind at record, NOT a pipeline rebind.
    VkDescriptorSet texture = VK_NULL_HANDLE;
};
VAML_VULKAN_EXPORT UiBatchKey ui_batch_key(const DrawItem& item) noexcept;

// Maps the S206 primitive kind to the S205 shader kind it renders (the
// pipeline rebind target): SolidRect -> Solid, BorderRect -> Border,
// TextGlyph -> SdfText. Pure + headless-testable.
VAML_VULKAN_EXPORT UiShaderKind draw_kind_to_shader(DrawItemKind kind) noexcept;

// The fixed vertex count of ONE draw item (4 — the S200 unit quad). A batch of
// N items = 4N vertices.
VAML_VULKAN_EXPORT std::uint32_t ui_vertex_count(std::uint32_t n) noexcept;

// The fixed index count of ONE draw item (6 — the S200 [0,1,2,0,2,3] pattern).
// A batch of N items = 6N indices + one vkCmdDrawIndexed(6N,1,0,0,0).
VAML_VULKAN_EXPORT std::uint32_t ui_index_count(std::uint32_t n) noexcept;

// Appends ONE draw item (4 vertices) to `out` via the S200 append_quad seam.
// Solid/Border use the full atlas UV (0,0)-(1,1) (they do not sample); Text
// uses the glyph's atlas sub-rect. Pure + headless-testable.
VAML_VULKAN_EXPORT void
append_ui_item(std::vector<quad::QuadVertex>& out, const DrawItem& item) noexcept;

// One BATCH of same-key (same kind + color + params) contiguous draw items. A
// batch renders with ONE vkCmdDrawIndexed(6N). `first`/`count` index into the
// COMPACTED (non-degenerate) item sequence — the same layout UiScene::set_items
// writes into its vertex/index buffers, so `first`*6 is the batch's firstIndex.
struct UiBatch
{
    UiShaderKind kind  = UiShaderKind::Solid;
    std::uint32_t first = 0;   // first compacted item index
    std::uint32_t count = 0;   // item count in this batch

    // S208 Clip: the batch's ONE dynamic scissor (mapped to frame px at record).
    // `clipActive` false = the full-extent scissor (the existing once-per-frame
    // behavior); true = record sets a per-batch scissor from these logical-px
    // bounds. Set by batch_plan from the run's representative UiBatchKey.
    bool  clipActive = false;
    float clipX = 0.0f, clipY = 0.0f;  // logical px
    float clipW = 0.0f, clipH = 0.0f;  // logical px

    // S209 — the batch's TEXTURE descriptor set (the run's representative
    // texture). Default VK_NULL_HANDLE = the scene's shared atlas set; a
    // non-null set = record binds THIS set before the batch's draws (a
    // descriptor rebind on a texture change, never a pipeline rebind).
    VkDescriptorSet texture = VK_NULL_HANDLE;

    [[nodiscard]] std::uint32_t vertex_count() const noexcept { return ui_vertex_count(count); }
    [[nodiscard]] std::uint32_t index_count()  const noexcept { return ui_index_count(count); }
};

// The HEADLINE S206 pure seam: `batch_plan(items)` groups CONTIGUOUS same-key
// draw items into batches (running the S205-kind pipelines as few times as
// possible). Non-contiguous same-key runs become SEPARATE batches (z/insertion
// order preserved — the batching never reorders). Degenerate (zero/negative
// size) items are SKIPPED (never batched, never drawn). Empty input -> zero
// batches. Pure + headless-testable.
VAML_VULKAN_EXPORT std::vector<UiBatch>
batch_plan(std::span<const DrawItem> items) noexcept;

// The S206 record-plan (the standing S166 draw_record_plan / S201
// scene_record_plan precedent): the fixed ordered steps a UiScene's per-batch
// record runs. The payload facts (push = UiPushConstants over the shared
// 128-byte VERTEX|FRAGMENT range; draw = vkCmdDrawIndexed(6N,1,0,0,0); the
// descriptor set is the SHARED set-0-binding-0 atlas sampler) are asserted by
// UiScene_test independently. Viewport/scissor + vertex/index buffer binds are
// done ONCE per frame (not per batch).
enum class UiRecordStep
{
    BindPipeline,        // bind the batch's UiPipeline
    SetViewport,         // dynamic viewport (once per frame)
    SetScissor,          // dynamic scissor (once per frame)
    BindDescriptorSet,   // bind the SHARED descriptor set (once per frame)
    PushUiConstants,     // push UiPushConstants, 128B VERTEX|FRAGMENT (per batch)
    BindVertexBuffers,   // bind the vertex buffer (once per frame)
    BindIndexBuffer,     // bind the index buffer (once per frame)
    DrawIndexed,         // vkCmdDrawIndexed(6N, 1, 0, first*6, 0)
};

VAML_VULKAN_EXPORT const std::vector<UiRecordStep>& ui_record_steps() noexcept;

// ---------------------------------------------------------------------------
// Real UI-scene owner (RAII; move-only)
// ---------------------------------------------------------------------------

// The real S206 owner: RAII + move-only, owning the host-visible vertex + index
// buffers, the S205-shared descriptor pool + ONE descriptor set (allocated from
// the SHARED set layout, written to the atlas sampler), and the negotiated
// projection. The app fills the CPU item list once per frame via set_items()
// (which compacts + re-encodes the buffers AND recomputes batch_plan), then
// record_draw() records the batched draws onto the presenter/loop command
// buffer. See the header comment for the split + NO-GAP + LIFETIMES.
class VAML_VULKAN_EXPORT UiScene
{
public:
    struct Spec
    {
        VkRenderPass          renderPass   = VK_NULL_HANDLE;
        VkFormat              colorFormat  = VK_FORMAT_UNDEFINED;  // MUST be is_srgb_format
        VkExtent2D            extent       = { 0, 0 };
        // The S205 SHARED pipeline layout + SHARED descriptor-set layout (from
        // any one of the three UiPipelines — they all share them).
        VkPipelineLayout      pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout      = VK_NULL_HANDLE;
        // The three kind pipelines (SDF text / solid / border) over the shared layout.
        VkPipeline            sdfPipeline    = VK_NULL_HANDLE;
        VkPipeline            solidPipeline  = VK_NULL_HANDLE;
        VkPipeline            borderPipeline = VK_NULL_HANDLE;
        // The atlas every text glyph samples (set 0, binding 0) — the S204 SDF atlas.
        VkImageView           atlasView    = VK_NULL_HANDLE;
        VkSampler             atlasSampler = VK_NULL_HANDLE;
        // S209 — the S166 in-flight ring depth (choose_concurrency, typically 3)
        // the RetainedBufferPool uses to decide when a retired generation is safe
        // to free (its slot fence has signaled). Default 3 = the S166 max.
        std::uint32_t         inFlightCount = 3;
    };

    // Creates the scene buffers + the SHARED descriptor set on `device` via
    // `loader`. `spec.colorFormat` MUST be SRGB (the S200 pairing). Returns
    // nullptr (no throw) on failure. `loader`, `device`, the render pass, the
    // three UiPipelines + the atlas view/sampler must outlive the result.
    [[nodiscard]] static std::unique_ptr<UiScene>
    create(const VulkanLoader& loader, const Device& device, const Spec& spec);

    ~UiScene();
    UiScene(UiScene&&) noexcept;
    UiScene& operator=(UiScene&&) noexcept;
    UiScene(const UiScene&)            = delete;
    UiScene& operator=(const UiScene&) = delete;

    // (Re)encodes the per-frame CPU item list: COMPACTS degenerate (zero/negative
    // size) items, encodes the survivors into the vertex buffer (4N vertices) +
    // the index buffer ([0,1,2,0,2,3]+4*i per item, 6N indices), and recomputes
    // batch_plan. Grows the buffers on demand via the S209 RetainedBufferPool
    // (a grow RETIRES the old generation; the pool frees it once the in-flight
    // ring's fence signals — no UAF under the 3-in-flight window). Returns false
    // on failure (no throw). An empty (or all-degenerate) list clears the scene
    // (zero batches next record).
    bool set_items(std::span<const DrawItem> items);

    // S209 — registers a per-texture descriptor set for the set-0-binding-0 slot
    // under a caller-chosen `textureId`. Items set their DrawItem::texture to the
    // matching `set` (or use the scene's default shared set via VK_NULL_HANDLE).
    // record_draw binds the batch's set on a texture change (cheap descriptor
    // rebind, NEVER a pipeline rebind — the S209 minimal-pipeline-switch guard).
    // The `set` must outlive the scene (it is borrowed). Returns false if the
    // binding could not be registered (e.g. a null loader).
    bool set_texture_binding(std::uint32_t textureId, VkDescriptorSet set);

    // S209 — advances the RetainedBufferPool's frame counter once per frame (the
    // caller calls this after submit+present) so a retired generation whose slot
    // fence has SIGNALED (the ring wrapped) is freed — the fence-driven lifetime.
    void recycle_buffers();

    // S209 — the number of still-retired (not-yet-freed) buffer generations in
    // the RetainedBufferPool (read-only; for the POC/test observation of the
    // fence-driven retire/recycle lifecycle).
    [[nodiscard]] std::size_t retired_generation_count() const noexcept
    {
        return pool_ ? pool_->retired_count() : 0u;
    }
    // S209 — the pool's current frame index (advanced once per recycle_buffers).
    [[nodiscard]] std::uint32_t pool_frame_index() const noexcept
    {
        return pool_ ? pool_->frame_index() : 0u;
    }

    // Records the batched draws onto `cmd` AFTER the render pass has begun (the
    // caller owns pass boundaries — the S201 QuadScene precedent). Runs the
    // stored batch_plan (from the last set_items): per batch binds the batch's
    // UiPipeline (rebind ONLY on a kind change), sets viewport/scissor once,
    // binds the SHARED vertex/index buffers ONCE, binds the batch's descriptor
    // set (the shared set by default; a per-texture set on a texture change) —
    // the descriptor rebind is CHEAP and never a pipeline rebind — pushes one
    // UiPushConstants (mvp + uiScale + the batch's style), and issues ONE
    // vkCmdDrawIndexed(6N). Returns false on an empty scene (no draw) or a
    // record failure.
    bool record_draw(VkCommandBuffer cmd, const Mat4& mvp, float uiScale);

    // Convenience: record with the projection computed from this scene's extent.
    bool record_draw(VkCommandBuffer cmd);

    [[nodiscard]] VkBuffer        vertex_buffer() const noexcept { return vertexBuffer_; }
    [[nodiscard]] VkBuffer        index_buffer()  const noexcept { return indexBuffer_; }
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept { return descriptorSet_; }
    // S209 — the descriptor set registered for `textureId`, or VK_NULL_HANDLE if
    // not registered.
    [[nodiscard]] VkDescriptorSet texture_binding(std::uint32_t textureId) const noexcept;
    [[nodiscard]] std::uint32_t   texture_binding_count() const noexcept { return static_cast<std::uint32_t>(textureBindings_.size()); }
    [[nodiscard]] std::uint32_t   item_count()  const noexcept { return itemCount_; }
    [[nodiscard]] std::uint32_t   batch_count() const noexcept { return static_cast<std::uint32_t>(batches_.size()); }
    [[nodiscard]] const std::vector<UiBatch>& batches() const noexcept { return batches_; }
    [[nodiscard]] VkExtent2D extent()      const noexcept { return extent_; }
    [[nodiscard]] VkFormat  color_format() const noexcept { return colorFormat_; }

private:
    UiScene(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice            device_ = VK_NULL_HANDLE;
    VkPhysicalDevice    physical_ = VK_NULL_HANDLE;

    // S209 — the real vertex/index buffer owner is the RetainedBufferPool (the
    // device-backed generations). These handle caches alias its ACTIVE generation
    // so record_draw + the accessors bind the live buffers without re-querying the
    // pool every call. The S206-era raw buffer/memory members are REPLACED by the
    // pool (the grow path now RETIRES instead of freeing — the S209 lifetime).
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;  // == pool_->active().vertex
    VkBuffer indexBuffer_  = VK_NULL_HANDLE;  // == pool_->active().index

    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_  = VK_NULL_HANDLE;

    VkPipelineLayout      pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline            sdfPipeline_    = VK_NULL_HANDLE;
    VkPipeline            solidPipeline_  = VK_NULL_HANDLE;
    VkPipeline            borderPipeline_ = VK_NULL_HANDLE;
    VkFormat              colorFormat_    = VK_FORMAT_UNDEFINED;
    VkExtent2D            extent_{ 0, 0 };

    // The per-frame state driven by set_items + consumed by record_draw.
    std::vector<DrawItem> items_;     // COMPACTED (non-degenerate) items
    std::vector<UiBatch>  batches_;   // from batch_plan(items_)
    std::uint32_t         itemCount_ = 0;

    // S209 — the fence-aware retired-buffer pool (owns the vertex/index buffer
    // generations; a grow retires, recycle() frees once the in-flight fence
    // signals). It owns the REAL buffers; the scene's vertexBuffer_/indexBuffer_
    // handle caches (above) alias its ACTIVE generation.

    std::unique_ptr<detail::RetainedBufferPool> pool_;

    // S209 — per-texture descriptor sets (by textureId) bound at record on a
    // texture change (cheap descriptor rebind, never a pipeline rebind).
    std::vector<std::pair<std::uint32_t, VkDescriptorSet>> textureBindings_;
};

} // namespace vaml::vulkan::ui
