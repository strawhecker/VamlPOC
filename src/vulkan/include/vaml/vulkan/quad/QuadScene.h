#pragma once
// S201 — the N-quad scene encoder: CPU list → vertex/index buffers + indexed draw
// (vaml::vulkan::quad).
//
// S201's headline: draw N quads from a CPU list every frame. QuadScene owns the
// per-frame vertex + index buffers (a HOST-visible staging/mapped set, following
// the S200 minimal texture's host-visible staging posture) + the S200-local
// descriptor set + the record-plan decision. The app (or a scene) fills a
// std::vector<QuadVertex> (via append_quad) and calls QuadScene::set_quads()
// every frame; QuadScene uploads the buffers, records one vkCmdDrawIndexed(6N,
// 1, 0, 0, 0) after binding the quad pipeline + pushing QuadUniform + binding
// the texture descriptor, and owns the vertex/index buffer memory.
//
// The blend + SRGB tests (S201's "blend + SRGB tests" headline) are pure + live
// here: blend state selection per quad (QuadBlendMode) + the SRGB pairing (the
// scene's color format is asserted is_srgb_format) — proven headlessly by
// QuadScene_test.cpp.
//
// Split (the standing precedent):
//   * PURE DECISION SEAMS (headless-testable) —
//       scene_vertex_count(n) / scene_index_count(n)   (N quads = 4N verts / 6N indices)
//       scene_record_plan()                            (the fixed ordered N-quad draw steps)
//       QuadItem { x, y, w, h, u0, v0, u1, v1, color, blendMode }
//       append_scene_item(vector<QuadVertex>&, const QuadItem&)  (appends ONE N-quad item)
//     These are the S201 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime.
//   * `QuadScene::create(...)` — the real owner: creates the vertex + index
//     buffers (host-visible, mapped per frame) + the S200-local descriptor pool +
//     descriptor set (from the pipeline's set layout + the scene texture's view/
//     sampler), then set_quads / record_draw record the N-quad draw.
//
// NO-GAP recorded (not silent): S200 delivers the quad pipeline + the minimal
// texture + the mesh; S201 turns a CPU std::vector<QuadVertex> into a REAL
// per-frame N-quad draw + proves blend + SRGB. The GPU batching/retained-scene +
// per-element Model composition is S207/S208/S209 (S201 keeps the single-thread
// CPU-list-per-frame model, recorded). The descriptor surface stays the S200-local
// combined image sampler (generalized by S205).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/quad/QuadPipeline.h>
#include <vaml/vulkan/quad/QuadTexture.h>
#include <vaml/vulkan/quad/QuadVertex.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::quad {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The vertex count for N quads (4N). Pure + headless-testable.
VAML_VULKAN_EXPORT std::uint32_t scene_vertex_count(std::uint32_t n) noexcept;

// The index count for N quads (6N). Pure + headless-testable.
VAML_VULKAN_EXPORT std::uint32_t scene_index_count(std::uint32_t n) noexcept;

// One quad in a scene: the rectangle + uv sub-rect + straight-alpha color + its
// blend mode. S201's per-quad blend state selection (an Alpha quad vs a None
// quad) is driven by `blendMode` (see SceneBlend below).
struct QuadItem
{
    float x = 0.0f, y = 0.0f;        // top-left in logical px
    float w = 0.0f, h = 0.0f;        // size
    float u0 = 0.0f, v0 = 0.0f;      // uv sub-rect start
    float u1 = 1.0f, v1 = 1.0f;      // uv sub-rect end
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;  // straight-alpha color
    QuadBlendMode blendMode = QuadBlendMode::Alpha;  // the S201 per-quad blend
};

// Appends ONE scene item (4 vertices) to `out` via the S200 append_quad seam.
// Pure + headless-testable.
VAML_VULKAN_EXPORT void
append_scene_item(std::vector<QuadVertex>& out, const QuadItem& item) noexcept;

// The S201 record-plan (the standing S166 draw_record_plan precedent): the fixed
// ordered steps a QuadScene's per-frame record runs, for N quads. The step
// payload facts (push = QuadUniform via quad_push_constant; draw = vkCmdDrawIndexed
// 6N indices) are asserted by QuadScene_test independently.
enum class SceneRecordStep
{
    BindPipeline,        // bind the quad pipeline
    SetViewport,         // dynamic viewport (from viewport_scissor)
    SetScissor,          // dynamic scissor
    BindDescriptorSet,   // bind the scene texture descriptor set
    PushQuadUniform,     // push QuadUniform (mvp + uiScale), 80B VERTEX
    BindIndexBuffer,     // bind the index buffer
    DrawIndexed,         // vkCmdDrawIndexed(6N, 1, 0, 0, 0)
};

VAML_VULKAN_EXPORT const std::vector<SceneRecordStep>& scene_record_plan() noexcept;

// ---------------------------------------------------------------------------
// Real N-quad scene owner (RAII)
// ---------------------------------------------------------------------------

// The real S201 scene owner: RAII + move-only, owning the vertex + index buffers
// (host-visible, mapped per-frame), their memory, the descriptor pool + the scene
// descriptor set, and the negotiated projection (width/height/uiScale). The app
// parses a std::vector<QuadVertex> once per frame into set_quads() and calls
// record_draw() onto the presenter/loop's command buffer. See the header comment
// for the split + NO-GAP.
class VAML_VULKAN_EXPORT QuadScene
{
public:
    struct Spec
    {
        VkRenderPass              renderPass   = VK_NULL_HANDLE;
        VkFormat                  colorFormat  = VK_FORMAT_UNDEFINED;  // MUST be is_srgb_format
        VkSampleCountFlagBits     sampleCount  = VK_SAMPLE_COUNT_1_BIT;
        VkExtent2D                extent       = { 0, 0 };
        VkPipeline                quadPipeline = VK_NULL_HANDLE;
        VkPipelineLayout          quadLayout   = VK_NULL_HANDLE;
        VkDescriptorSetLayout     quadSetLayout = VK_NULL_HANDLE;
        VkImageView               textureView  = VK_NULL_HANDLE;
        VkSampler                 textureSampler = VK_NULL_HANDLE;
        bool                      hasDepth     = false;
        VkPipeline                blendPipeline = VK_NULL_HANDLE;  // optional: a no-blend pipeline
    };

    // Creates the scene buffers + descriptor set on `device` via `loader`.
    // `spec.colorFormat` MUST be SRGB (the S200 SRGB pairing). Returns nullptr
    // (no throw) on failure. `loader`, `device`, the render pass, the quad
    // pipeline + texture must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<QuadScene>
    create(const VulkanLoader& loader, const Device& device, const Spec& spec);

    ~QuadScene();
    QuadScene(QuadScene&&) noexcept;
    QuadScene& operator=(QuadScene&&) noexcept;
    QuadScene(const QuadScene&)            = delete;
    QuadScene& operator=(const QuadScene&) = delete;

    // (Re)uploads the per-frame CPU vertex list: N = vertices.size()/4 quads.
    // Maps the host-visible vertex + index buffers, copies the vertices + the
    // per-quad index pattern [0,1,2,0,2,3] offset by 4*i for quad i, unmaps.
    // Returns false on failure (no throw). Calling with an empty list clears the
    // scene (zero draws next record).
    bool set_quads(const std::vector<QuadVertex>& vertices, std::uint32_t n);

    // Records the N-quad indexed draw onto `cmd` after the render pass has begun
    // (the caller begins/ends it around this — the S175/loop owns pass boundaries).
    // Binds the quad pipeline, sets viewport/scissor, binds the scene descriptor
    // set, pushes the QuadUniform (from `mvp` or ui_projection(width,height,uiScale)),
    // binds the index buffer, and vkCmdDrawIndexed(6N,1,0,0,0). Returns false on
    // an empty scene (no draw) or a record failure.
    bool record_draw(VkCommandBuffer cmd, const Mat4& mvp, float uiScale);

    // Convenience: record with the projection computed from this scene's extent.
    bool record_draw(VkCommandBuffer cmd);

    [[nodiscard]] VkBuffer vertex_buffer() const noexcept { return vertexBuffer_; }
    [[nodiscard]] VkBuffer index_buffer()  const noexcept { return indexBuffer_; }
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept { return descriptorSet_; }
    [[nodiscard]] std::uint32_t quad_count() const noexcept { return quadCount_; }
    [[nodiscard]] std::uint32_t vertex_count() const noexcept { return quadCount_ * quad_vertex_count(); }
    [[nodiscard]] std::uint32_t index_count()  const noexcept { return quadCount_ * quad_index_count(); }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] VkFormat  color_format() const noexcept { return colorFormat_; }

private:
    QuadScene(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice            device_ = VK_NULL_HANDLE;
    VkPhysicalDevice    physical_ = VK_NULL_HANDLE;

    VkBuffer        vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory  vertexMemory_ = VK_NULL_HANDLE;
    VkBuffer        indexBuffer_  = VK_NULL_HANDLE;
    VkDeviceMemory  indexMemory_  = VK_NULL_HANDLE;

    VkDescriptorPool      descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet_  = VK_NULL_HANDLE;

    VkRenderPass          renderPass_     = VK_NULL_HANDLE;
    VkPipeline            quadPipeline_   = VK_NULL_HANDLE;
    VkPipelineLayout      quadLayout_     = VK_NULL_HANDLE;
    VkDescriptorSetLayout quadSetLayout_  = VK_NULL_HANDLE;
    VkPipeline            blendPipeline_  = VK_NULL_HANDLE;
    VkFormat              colorFormat_    = VK_FORMAT_UNDEFINED;
    VkExtent2D            extent_{ 0, 0 };
    std::uint32_t         quadCount_ = 0;
};

} // namespace vaml::vulkan::quad
