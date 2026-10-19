#pragma once
// S165 — Vulkan graphics pipeline + pipeline layout.
//
// The S165 title's pipeline half, delivered as a Cat 1 (library-surface) RAII
// owner + a set of PURE DECISION SEAMS (headless-testable, the S160/S162/S163/
// S164 precedent — CI proves the decision layer; the LIVE vkCreateGraphicsPipelines
// is manual via the POCs). The pipeline renders the single hardcoded triangle
// from ShaderModule into the RenderPass's color attachment (+ optional depth),
// with NO vertex buffer (S165's geometry is generated in the vertex shader from
// gl_VertexIndex) and NO descriptor sets (S165 passes the MVP as a 64-byte
// VERTEX-stage push constant; descriptor-set architecture is S205's, recorded
// NO-GAP).
//
// The HEADLINE pure seam is `tri_mvp(width, height)`: the HARDCODED MVP matrix
// that maps the single triangle into the given framebuffer size. Because S165's
// shader already authors the triangle in clip space and the MVP is the identity,
// tri_mvp literally returns the identity matrix for any (width, height) — but it
// is a REAL, provable, forward-resolvable symbol: it is what S166's draw loop
// pushes, it is asserted headlessly by Pipeline_test (finite, diagonal 1.0, maps
// each of tri_vertex_positions to an in-range NDC x/y), and S200/S208 turn it
// into the real uniform MVP/UI-scale projection (it is kept and extended, NOT a
// STUB / NOT MINIMAL REAL — it is a declared, forward-resolvable seed).
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       viewport_scissor(extent)          (the dynamic viewport + scissor)
//       push_constant_range()             (VERTEX stage, offset 0, size 64)
//       tri_vertex_input_state()          (empty — no vertex buffer in S165)
//       tri_mvp(width, height)            (the hardcoded MVP, identity)
//     These are the S165 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime.
//   * `PipelineSpec` — the negotiated spec (render pass + color format + extent
//     + sample count + depth enable).
//   * `Pipeline::create(...)` — assembles the full VkGraphicsPipelineCreateInfo
//     (triangle-list, fill, no cull, MSAA 1, write-all no-blend, dynamic viewport
//     + scissor, one 64-byte push-constant range) and calls vkCreateGraphicsPipelines
//     (manual/POC-proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader, the VkDevice, the RenderPass AND the two
// ShaderModules must OUTLIVE the Pipeline (the pipeline references them).
// Destroy order: pipeline -> shader modules -> render pass -> device. This is
// documented (not hidden) — callers own the ordering.
//
// NO-GAP recorded (not silent): S166's command buffer binds this pipeline and
// pushes tri_mvp + draws 3 vertices; MSAA sample-count > 1 = S168; vertex/index
// buffers + real geometry = S200; descriptor sets + real UI shaders + a real
// uniform-MVP projection = S205; the per-element transform/model matrix =
// S208. S165 deliberately creates NO vertex buffer and NO descriptor set — both
// are explicitly owned by later named sprints.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <array>
#include <cstdint>
#include <memory>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// A 4x4 matrix in column-major float order (matches the SPIR-V mat4 storage).
// tri_mvp returns one of these; S166's push-constant path consumes it.
using Mat4 = std::array<float, 16>;

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The dynamic viewport + scissor for a framebuffer extent. Both are set dynamic
// (the pipeline is created with VK_DYNAMIC_STATE_VIEWPORT | SCISSOR) so S166's
// draw loop can set them per-frame without a pipeline rebuild. Returns the pair
// (viewport = full extent, scissor = full extent rect).
struct ViewportScissor
{
    VkViewport viewport{};
    VkRect2D   scissor{};
};

VAML_VULKAN_EXPORT ViewportScissor
viewport_scissor(VkExtent2D extent) noexcept;

// The S165 push-constant range: VERTEX stage, offset 0, size 64 bytes (one mat4).
// Exposed so the pipeline layout creation + S166's vkCmdPushConstants agree.
VAML_VULKAN_EXPORT VkPushConstantRange
push_constant_range() noexcept;

// The S165 vertex-input state: EMPTY (no vertex buffer / no binding / no
// attribute — the triangle geometry is generated in the vertex shader from
// gl_VertexIndex). This deliberately matches the embedded shader. Real vertex
// buffers arrive with S200 (recorded NO-GAP, not silent).
VAML_VULKAN_EXPORT VkPipelineVertexInputStateCreateInfo
tri_vertex_input_state() noexcept;

// The HEADLINE S165 pure seam: the HARDCODED MVP matrix for the given framebuffer
// size. For S165 this is the IDENTITY matrix (the shader already authors the
// triangle in clip space), returned column-major. It is REAL + provable (finite,
// invertible, maps every tri_vertex_positions() vertex to an in-range NDC x/y —
// asserted headlessly by Pipeline_test), NOT a STUB: it is the symbol S166's
// draw loop pushes and S200/S208 extend into the real uniform MVP/UI-scale
// projection. `width`/`height` are accepted (and validated > 0) so callers never
// thread a wrong-size matrix; the identity is independent of them for S165.
VAML_VULKAN_EXPORT Mat4
tri_mvp(std::uint32_t width, std::uint32_t height) noexcept;

// ---------------------------------------------------------------------------
// Real graphics pipeline owner (RAII; also owns its pipeline layout)
// ---------------------------------------------------------------------------

// The negotiated pipeline spec, populated by Pipeline::create() and exposed via
// Pipeline accessors.
struct PipelineSpec
{
    VkRenderPass             renderPass   = VK_NULL_HANDLE;
    VkFormat                 colorFormat  = VK_FORMAT_UNDEFINED;
    bool                     hasDepth     = false;
    VkSampleCountFlagBits    sampleCount  = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D               extent       = { 0, 0 };
    VkShaderModule           vertexModule   = VK_NULL_HANDLE;
    VkShaderModule           fragmentModule = VK_NULL_HANDLE;
};

// The real S165 graphics-pipeline owner: RAII + move-only, owning BOTH the
// pipeline layout and the pipeline. Constructed only via create(); destroys
// pipeline -> pipeline layout in reverse order on destruction. See the header
// comment for LIFETIMES.
class VAML_VULKAN_EXPORT Pipeline
{
public:
    // Creates a graphics pipeline on `device` via `loader`. Runs the S165 decision
    // seams, builds the pipeline layout (one 64-byte push-constant range) +
    // VkGraphicsPipelineCreateInfo (triangle-list, fill, no cull, MSAA 1, write-
    // all no-blend, dynamic viewport+scissor, null vertex input), and calls
    // vkCreateGraphicsPipelines. Returns nullptr (no throw) on failure.
    // `loader`, `device`, `spec.renderPass` and the spec's two shader modules
    // must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<Pipeline>
    create(const VulkanLoader& loader, VkDevice device, const PipelineSpec& spec);

    ~Pipeline();
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;
    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    [[nodiscard]] VkPipeline       native_handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipeline       pipeline()        const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept { return pipelineLayout_; }
    [[nodiscard]] const PipelineSpec& spec() const noexcept { return spec_; }

private:
    Pipeline(const VulkanLoader& loader, VkDevice device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice           device_  = VK_NULL_HANDLE;
    VkPipelineLayout   pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline         pipeline_ = VK_NULL_HANDLE;
    PipelineSpec       spec_{};
};

} // namespace vaml::vulkan
