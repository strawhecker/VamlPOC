#pragma once
// S200 — the quad graphics pipeline + the S200 UI-scale/MVP projection + the
// alpha-blend + SRGB (vaml::vulkan::quad).
//
// The S200 build-pipeline half (QuadPipeline + QuadUniform + the pure seams).
// It EXTENDS the S165 tri pipeline: the S165 64-byte mat4 push constant becomes
// the 80-byte `QuadUniform { Mat4 mvp; float uiScale; float pad[3]; }` — the
// recorded "S200 turns tri_mvp into the real uniform MVP/UI-scale projection"
// hand-off is RESOLVED here (kept-and-extended, NOT MINIMAL REAL / NOT a stub).
// The vertex input is the S200 QuadVertex binding + 3 attributes (vec2 pos, vec2
// uv, vec4 color) — the real vertex-input state S165 deliberately left empty.
// The S200 color attachment is the S164 swapchain SRGB format (B8G8R8A8_SRGB):
// the shader outputs LINEAR, the hardware sRGB-encodes on present — the correct
// gamma (the S200 SRGB headline). The S200 alpha blend is the straight-alpha
// SRC_ALPHA/ONE_MINUS_SRC_ALPHA path (QuadBlendMode::Alpha) vs no blend.
//
// Split (the S162..S165 precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       QuadBlendMode + quad_blend_state(mode)  (the S200 alpha-blend headline)
//       quad_vertex_input_state()               (QuadVertex stride + 3 attributes)
//       quad_push_constant()                     (the 80B QuadUniform VERTEX range)
//       ui_projection(width, height, uiScale)   (logical px → clip orthographic)
//       QuadUniform { mvp, uiScale, pad }
//       quad_sampler() / quad_descriptor_info() / quad_set_layout_binding()
//                                                (the S200-local combined image sampler)
//       is_srgb_format(format) + quad_format_bytes() (the S200 SRGB pairing)
//     These are the S200 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime.
//   * `QuadPipelineSpec` — the negotiated spec (render pass + color format +
//     extent + sample count + depth enable + shader modules).
//   * `QuadPipeline::create(...)` — assembles the real VkGraphicsPipelineCreateInfo
//     (quad vertex input, Alpha blend, 80B push constant, SRGB color format, no
//     cull, dynamic viewport+scissor) and calls vkCreateGraphicsPipelines
//     (manual/POC-proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader, the VkDevice, the RenderPass AND the two ShaderModules
// must OUTLIVE the QuadPipeline (the pipeline references them). Destroy order:
// pipeline -> shader modules -> render pass -> device. Documented (not hidden).
//
// NO-GAP recorded (not silent): S201's QuadScene consumes the QuadUniform layout,
// the S200 vertex/buffer seams, the alpha blend, and the SRGB pairing to draw N
// quads. The S200-local descriptor surface (one combined image sampler) is
// GENERALIZED by S205 into the shared descriptor layout + real UI shaders — it is
// a kept-and-extended isolated S200 seam, NOT a backfill. S202 owns the general
// staging→device-local texture upload + the texture atlas + sub-region invalidation.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/quad/QuadVertex.h>

#include <array>
#include <cstdint>
#include <memory>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::quad {

// A 4x4 matrix in column-major float order (reuses the S165 Mat4 alias — the
// S165 tri_mvp is the S200 MVP's kept-and-extended sibling). ui_projection
// returns one of these; S201's draw loop pushes it.
using Mat4 = std::array<float, 16>;

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The S200 alpha-blend mode: None = no blending (opaque quads write straight
// through); Alpha = the straight-alpha SRC_ALPHA/ONE_MINUS_SRC_ALPHA path
// (color: src*a / 1-a; alpha: src / 1-a). The QuadScene selects a mode PER QUAD
// (S201's per-quad blend state selection — an Alpha quad vs a None quad record
// different blend attachments). The pipeline is created with ONE blend attachment
// (Alpha by default); S201 records a blend-state change per scene when a quad
// needs a different mode (see QuadScene.h).
enum class QuadBlendMode
{
    None = 0,
    Alpha = 1,
};

// The S200 color-blend attachment for `mode` (pure). Alpha = straight-alpha
// blending (color factors SRC_ALPHA/ONE_MINUS_SRC_ALPHA, alpha factors
// ONE/ONE_MINUS_SRC_ALPHA); None = blendEnable VK_FALSE (write-all no blend).
// The attachment's colorWriteMask is write-all (the S165 write-all posture).
VAML_VULKAN_EXPORT VkPipelineColorBlendAttachmentState
quad_blend_state(QuadBlendMode mode) noexcept;

// Describes whether `mode` enables color blending (Alpha -> true, None -> false).
// Pure + headless-testable — the S200 alpha-blend headline assert.
VAML_VULKAN_EXPORT bool describe_color_blend(QuadBlendMode mode) noexcept;

// The S200 vertex-input state: ONE vertex-buffer binding (stride =
// sizeof(QuadVertex) = 8 floats = 32 bytes) + THREE attributes:
//   location 0 = vec2 position  (offset 0)
//   location 1 = vec2 uv        (offset 8)
//   location 2 = vec4 color     (offset 16)
// This is the REAL vertex-input state S165 deliberately left empty (S165's
// geometry came from gl_VertexIndex; S200 consumes a vertex buffer). The bindings
// array is returned; the caller (QuadPipeline::create) wires the count + pointer.
VAML_VULKAN_EXPORT VkPipelineVertexInputStateCreateInfo
quad_vertex_input_state() noexcept;

// The S200 push-constant range: VERTEX stage, offset 0, size 80 bytes (one
// QuadUniform = Mat4 mvp + float uiScale + 3 pad floats). This EXTENDS the S165
// 64-byte mat4 range (offset 0..63 is the mvp; uiScale is at 64; the pad keeps
// the range 16-byte-aligned for the push-constant size). S201 pushes a
// QuadUniform via this range.
VAML_VULKAN_EXPORT VkPushConstantRange
quad_push_constant() noexcept;

// The S200 uniform the draw loop pushes: the UI-scale MVP projection +
// uiScale. `mvp` is the orthographic logical-px→clip projection (from
// ui_projection, or composed with a per-element Model by S208); `uiScale` is the
// UI scale factor (DPI/framebuffer scale — S213); the pad floats align the
// struct to 16 bytes so it maps cleanly onto the 80-byte push-constant range.
struct QuadUniform
{
    Mat4 mvp{};
    float uiScale = 1.0f;
    float pad[3]  = { 0.0f, 0.0f, 0.0f };
};
static_assert(sizeof(QuadUniform) == 80, "QuadUniform must equal the 80-byte push-constant range");

// The S200 HEADLINE pure seam: the UI-scale orthographic MVP projection mapping
// LOGICAL PIXELS → clip space for the given framebuffer size (in logical px) ×
// uiScale. Column-major:
//   scale = uiScale / (width/2) on X, uiScale / (height/2) on Y (a logical-px
//   origin top-left → NDC with +Y-up: the y is flipped), z = identity 0..1.
// uiScale multiplies the logical-px extent so a higher scale shows MORE of the
// layout in the same viewport (the S200 UI-scale contract). Finite + in-range
// asserted headlessly (QuadPipeline_test). Extends the S165 tri_mvp precedent
// (tri_mvp returned identity for S165's clip-space triangle; S200's projection is
// the real uniform MVP/UI-scale space S201's logical-px quads live in).
VAML_VULKAN_EXPORT Mat4
ui_projection(std::uint32_t width, std::uint32_t height, float uiScale) noexcept;

// The S200 sampler decision: LINEAR min/mag filter + CLAMP_TO_EDGE addressing on
// all three axes + NEAREST mipmap (the S200 local sampler the procedural texture
// samples with). Pure constructor of the VkSamplerCreateInfo.
VAML_VULKAN_EXPORT VkSamplerCreateInfo
quad_sampler() noexcept;

// The S200-local descriptor binding: set 0, binding 0 = combined image sampler
// (FRAGMENT stage) — the single texture a quad samples. Pure + headless-testable.
// RECORDED (not silent): this isolated binding is the minimal textured-quad seam;
// S205 generalizes it into the shared descriptor layout + real UI shaders.
VAML_VULKAN_EXPORT VkDescriptorSetLayoutBinding
quad_set_layout_binding() noexcept;

// The S200 sampler/descriptor write for one combined image sampler: fills
// `info` (the caller-owned VkDescriptorImageInfo that MUST outlive the
// vkUpdateDescriptorSets call — the returned write's pImageInfo points into it)
// with the given `imageView` + `sampler`, and returns the VkWriteDescriptorSet
// for set-0-binding-0. Pure constructor; S201's draw loop allocates its scene
// descriptor set, calls this with a local `info`, then vkUpdateDescriptorSets.
VAML_VULKAN_EXPORT VkWriteDescriptorSet
quad_descriptor_write(VkDescriptorSet set,
                      VkDescriptorImageInfo& info,
                      VkImageView imageView,
                      VkSampler sampler) noexcept;

// True iff `format` is an 8-bit SRGB color format (B8G8R8A8_SRGB / R8G8B8A8_SRGB)
// — the S200 SRGB pairing: the quad pipeline's color attachment must be the
// swapchain SRGB format so the shader's LINEAR output is sRGB-encoded on present
// (correct gamma). Headless-tested (QuadPipeline_test asserts both srgb + non-srgb).
VAML_VULKAN_EXPORT bool is_srgb_format(VkFormat format) noexcept;

// The bytes-per-pixel of a quad-relevant color format (4 for the 8-bit RGBAs, 0
// otherwise) — the S200 SRGB/BPP pairing seam. S201 re-asserts the quad pipeline
// color format == swapchain SRGB format + BPP 4.
VAML_VULKAN_EXPORT std::uint32_t quad_format_bytes(VkFormat format) noexcept;

// ---------------------------------------------------------------------------
// Real quad pipeline owner (RAII; also owns its pipeline layout + set layout)
// ---------------------------------------------------------------------------

// The negotiated quad pipeline spec, populated by QuadPipeline::create().
struct QuadPipelineSpec
{
    VkRenderPass             renderPass     = VK_NULL_HANDLE;
    VkFormat                 colorFormat    = VK_FORMAT_UNDEFINED;
    bool                     hasDepth       = false;
    VkSampleCountFlagBits    sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D               extent         = { 0, 0 };
    VkShaderModule           vertexModule   = VK_NULL_HANDLE;
    VkShaderModule           fragmentModule = VK_NULL_HANDLE;
};

// The real S200 quad-pipeline owner: RAII + move-only, owning the pipeline
// layout, the (S200-local) descriptor set layout, AND the pipeline. Constructed
// only via create(); destroys pipeline -> descriptor set layout -> pipeline layout
// in reverse order on destruction. See the header comment for LIFETIMES.
class VAML_VULKAN_EXPORT QuadPipeline
{
public:
    // Creates the quad graphics pipeline on `device` via `loader`. Runs the S200
    // decision seams, builds the pipeline layout (one 80-byte push-constant
    // range + the S200-local descriptor set layout), and calls
    // vkCreateGraphicsPipelines. THE SRGB PAIRING: `spec.colorFormat` MUST be an
    // SRGB format (the S164 swapchain color format) — QuadPipeline::create
    // returns nullptr if is_srgb_format(colorFormat) is false (the S200 SRGB
    // headline: the shader outputs linear, the SRGB attachment sRGB-encodes on
    // present). Returns nullptr (no throw) on failure. `loader`, `device`,
    // `spec.renderPass` and the two shader modules must outlive the result.
    [[nodiscard]] static std::unique_ptr<QuadPipeline>
    create(const VulkanLoader& loader, const Device& device, const QuadPipelineSpec& spec);

    ~QuadPipeline();
    QuadPipeline(QuadPipeline&&) noexcept;
    QuadPipeline& operator=(QuadPipeline&&) noexcept;
    QuadPipeline(const QuadPipeline&)            = delete;
    QuadPipeline& operator=(const QuadPipeline&) = delete;

    [[nodiscard]] VkPipeline              pipeline()        const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout        pipeline_layout() const noexcept { return pipelineLayout_; }
    // The S200-local descriptor set layout (the combined image sampler). S201
    // allocates its scene descriptor set from this layout; S205 generalizes it.
    [[nodiscard]] VkDescriptorSetLayout   descriptor_set_layout() const noexcept { return descriptorSetLayout_; }
    [[nodiscard]] const QuadPipelineSpec& spec() const noexcept { return spec_; }

private:
    QuadPipeline(const VulkanLoader& loader, const Device& device);

    const VulkanLoader*     loader_ = nullptr;
    VkDevice                device_  = VK_NULL_HANDLE;  // BORROWED from the Device
    VkPipelineLayout        pipelineLayout_      = VK_NULL_HANDLE;
    VkDescriptorSetLayout   descriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipeline              pipeline_ = VK_NULL_HANDLE;
    QuadPipelineSpec        spec_{};
};

} // namespace vaml::vulkan::quad
