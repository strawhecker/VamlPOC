#pragma once
// S205 — the UI pipeline family: SDF text + solid color + border + descriptor
// sets on ONE SHARED layout (vaml::vulkan::ui), dep:S204.
//
// The S205 headline: the S200-local descriptor seam is GENERALIZED (kept-and-
// extended, NOT MINIMAL REAL) into ONE shared descriptor-set layout + ONE shared
// pipeline layout used by ALL THREE UI shader kinds (SDF text / solid / border),
// and the four real UI shaders (the embedded spiirv in ShaderModule.h/cpp) are
// delivered. One bound descriptor set (set 0, binding 0 = the combined image
// sampler over the S204 SDF / S202 atlas) + one 128-byte UiPushConstants serves
// all three pipelines, so a consumer can switch between the three shader kinds
// with a single pipeline-bind + push, and S206 can batch same-shader draws with
// minimal pipeline switches (the recorded S206 premise).
//
// The three fragment shaders SHARE the S200 unit-quad geometry + the S200
// straight-alpha blend + the S200 SRGB pairing:
//   * SDF TEXT  consumes the shared atlas (set 0 binding 0) + pc.params.x (aa),
//     applies the S204 `sdf_soft_alpha` linear-AA ramp MIRRORED EXACTLY in-shader
//     (RESOLVES the S204-recorded "text shader mirrors the CPU sdf_soft_alpha AA
//     contract" hand-off — this step is its resolution point, not a backfill).
//   * SOLID color is a flat straight-alpha fill = pc.color x vertex color.
//   * BORDER computes a rectangular-stroke coverage from vUv + pc.params +
//     pc; the in-shader SDF mirrors the `border_coverage` seam below exactly
//     (the same discipline that makes the SDF text shader mirror sdf_soft_alpha).
//
// Split (the S162/S200 precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime required) —
//       UiShaderKind { SdfText, Solid, Border }
//       UiPushConstants (the shared 128-byte block, 16-aligned) + ui_push_constant()
//       ui_shared_set_layout_binding()   (set 0, binding 0 = combined image sampler)
//       solid_style / border_style / sdf_style (the per-kind pc fillers)
//       sd_rounded_box + border_coverage (the border decision seam, mirrors the shader)
//       UiPipelineSpec (the negotiated spec, mirrors QuadPipelineSpec)
//     These are the S205 proof surface proven in ctest WITHOUT a live Vulkan runtime.
//   * `UiPipeline::create(...)` — assembles the real shared descriptor-set layout,
//     the shared pipeline layout (one 128-byte push-constant range), and the kind's
//     graphics pipeline (shared ui_vertex + the kind's fragment), and calls
//     vkCreateGraphicsPipelines (manual/POC-proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader, VkDevice, RenderPass AND the shader modules must
// OUTLIVE the UiPipeline (the pipeline references them). Destroy order: pipeline ->
// descriptor set layout -> pipeline layout. A consumer allocates ONE descriptor set
// from descriptor_set_layout(), binds the SDF/atlas sampler into it, and reuses that
// set across all three pipelines (the shared layout contract).
//
// NO-GAP recorded (not silent): S205_1 (monochrome icon tint — the Monochrome
// UiShaderKind + its single-channel alpha-mask fragment + the partial-alpha seam
// tint_alpha/tint_sample) is DELIVERED here and consumes the shared sampler path;
// S206 (scene encoder + same-shader batching) uses this shared layout;
// S208 (per-element Model) composes into UiPushConstants.mvp; S400 (Text control)
// samples the SDF-text shader. No MINIMAL REAL introduced/resolved. quad/font/icon
// untouched. ADR-011 maintained (ui lives in vaml-vulkan). ADR-018: ZERO new locks
// (all single-threaded creation).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad::ui_projection + quad::is_srgb_format + quad::Mat4

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// A 4x4 column-major matrix (reuses the S200 quad::Mat4 alias so the S205
// shared mvp slot is forward-compatible with quad::ui_projection + S208 Model).
using Mat4 = ::vaml::vulkan::quad::Mat4;

// A straight-alpha RGBA color (same shape as QuadVertex's color).
using Color4 = std::array<float, 4>;

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The UI shader kinds the UiPipeline family renders. One shared vertex
// shader + one of the kind's fragment shaders (see UiPipeline::create). All
// four ride the SAME shared 128-byte UiPushConstants + the SAME shared
// set-0-binding-0 atlas layout, so ONE shared pipeline layout + one bound
// descriptor set serves every kind (the S206 batching premise).
enum class UiShaderKind
{
    SdfText = 0,
    Solid = 1,
    Border = 2,
    // S205_1 — monochrome icon tint: the kind's fragment (monochrome_tint_
    // fragment_spirv) samples the shared atlas as a SINGLE-CHANNEL ALPHA-MASK
    // (.r = continuous coverage) and applies the Foreground color (pc.color)
    // ENTIRELY IN-SHADER (no bitmap color-effect recompositing), preserving
    // partial alpha (mask 0.5 -> 50% alpha). TintMode=Monochrome (S400_1's
    // control-DP surface) consumes this path; TintMode=Preserve samples the
    // multicolor texture via the existing SDF-text/solid/quad kinds — the
    // TintMode ENUM identity is S400_1's, recorded NO-GAP, not silent.
    Monochrome = 3,
};

// The SHARED 128-byte push-constant block (the S205 shared-layout contract):
//   offset 0  : mat4 mvp      (64) — the S200 quad::ui_projection (logical px →
//                clip), or an S208-composed per-element Model MVP.
//   offset 64 : vec4 uiAndPad (16) — x = uiScale (the S200 UI scale factor,
//                same offset as the S200 80-byte QuadUniform).
//   offset 80 : vec4 color    (16) — the straight-alpha fill/tint (solid/border/
//                text color), multiplied by the vertex color.
//   offset 96 : vec4 params   (16) — kind-specific:
//                SdfText: x = aa (the S204 sdf_soft_alpha half-width)
//                Border:  x = thickness01, y = radius01, z = feather, w = aa
//                Solid:   0
//   offset 112: vec4 size     (16) — the quad logical extent (border rounding;
//                reserved for future SDF-relative sizing).
//   total 128 bytes, 16-byte aligned, std140-clean.
struct UiPushConstants
{
    Mat4    mvp{};
    Color4  uiAndPad{};   // x = uiScale
    Color4  color{};
    Color4  params{};     // kind-specific (see header comment)
    Color4  size{};
};
static_assert(sizeof(UiPushConstants) == 128, "UiPushConstants must equal the 128-byte shared push range");
static_assert(offsetof(UiPushConstants, mvp) == 0, "mvp must be at offset 0 (S200-compatible)");
static_assert(offsetof(UiPushConstants, uiAndPad) == 64, "uiAndPad (uiScale) must be at offset 64 (S200-compatible)");

// The shared push-constant range: VERTEX|FRAGMENT, offset 0, size 128 (one
// UiPushConstants). Both the shared vertex shader (mvp) and the fragment shaders
// (color/params) read this block, so the range spans both stages. Pure + headless.
VAML_VULKAN_EXPORT VkPushConstantRange
ui_push_constant() noexcept;

// The SHARED descriptor-set binding: set 0, binding 0 = ONE combined image
// sampler (FRAGMENT stage) — the SDF/generic atlas every UI fragment samples.
// This GENERALIZES the S200-local quad_set_layout_binding() into the shared
// binding reused by all three UI pipelines (the S200-recorded "descriptor
// surface generalized by S205" hand-off, resolved in place — kept+extended, NOT
// a backfill).
VAML_VULKAN_EXPORT VkDescriptorSetLayoutBinding
ui_shared_set_layout_binding() noexcept;

// The S205 sampler constructor (reuses the S200 linear/clamp decision so SDF +
// solid/border sampling match the quad sampler).
VAML_VULKAN_EXPORT VkSamplerCreateInfo
ui_sampler() noexcept;

// The descriptor write for the shared binding: fills `info` (the caller-owned
// VkDescriptorImageInfo that MUST outlive the vkUpdateDescriptorSets call) with
// `imageView` + `sampler` and returns the VkWriteDescriptorSet for set-0-binding-0.
// Mirrors quad::quad_descriptor_write. Pure + headless-testable.
VAML_VULKAN_EXPORT VkWriteDescriptorSet
ui_descriptor_write(VkDescriptorSet set,
                    VkDescriptorImageInfo& info,
                    VkImageView imageView,
                    VkSampler sampler) noexcept;

// --- The per-kind pc fillers (pure, headless-testable) ---

// Fills `pc` for the SOLID kind: pc.color = `color`, pc.params = 0 (no sampling,
// no params). The solid fragment outputs pc.color x vertex color.
VAML_VULKAN_EXPORT void solid_style(UiPushConstants& pc, const Color4& color) noexcept;

// Fills `pc` for the BORDER kind: pc.color = `color`; pc.params =
// { thickness01, radius01, feather, 0 }. The border fragment draws a rectangular
// stroke of thickness01-width (relative to the unit quad, [0,0.5]) with a
// radius01-rounded corner and a `feather`-wide shaded edge; alpha = the SDF
// decision seam border_coverage below.
VAML_VULKAN_EXPORT void border_style(UiPushConstants& pc,
                                     const Color4& color,
                                     float thickness01, float radius01, float feather) noexcept;

// Fills `pc` for the SDF-TEXT kind: pc.color = `color`; pc.params = { aa, 0, 0, 0 }.
// The sdf_text fragment samples the shared atlas + applies the S204
// sdf_soft_alpha ramp with half-width `aa` (a larger aa widens the soft edge).
VAML_VULKAN_EXPORT void sdf_style(UiPushConstants& pc,
                                  const Color4& color,
                                  float aa) noexcept;

// Fills `pc` for the MONOCHROME-TINT kind: pc.color = `foreground` (the tint);
// pc.params = 0 (no sampling params — the shader derives everything from the
// mask). The monochrome_tint fragment samples the shared atlas as a single-
// channel alpha-mask and applies `foreground` ENTIRELY IN-SHADER, preserving
// partial alpha. Does NOT clobber `uiScale` (offset 64), the same contract as
// solid_style/border_style/sdf_style.
VAML_VULKAN_EXPORT void monochrome_style(UiPushConstants& pc,
                                         const Color4& foreground) noexcept;

// --- The monochrome-tint decision seam (mirrors monochrome_tint.frag exactly) ---

// The PARTIAL-ALPHA authority: `mask01` (the recovered linear alpha-mask
// coverage in [0,1]) -> the output mask, clamped [0,1]. Identity for in-range
// masks (0.5 -> 0.5, 1 -> 1, 0 -> 0); out-of-range clamped. Mirrors the
// shader's clamp(pow(mask, 1/2.2), 0, 1) after the sRGB decode.
VAML_VULKAN_EXPORT float tint_alpha(float mask01) noexcept;

// The headless rewrite of monochrome_tint.frag's output: given the Foreground
// straight-alpha color and the recovered linear alpha-mask coverage `mask01`,
// returns the straight-alpha { fg.rgb, fg.a * tint_alpha(mask01) } — the
// CPU-side authority the shader mirrors verbatim (the same discipline that
// makes sdf_text mirror S204's sdf_soft_alpha and border mirror border_coverage).
// Partial alpha is preserved: a mask of 0.5 yields alpha 0.5 * fg.a.
VAML_VULKAN_EXPORT Color4 tint_sample(const Color4& foreground, float mask01) noexcept;

// --- The border decision seam (mirrors the border fragment shader exactly) ---

// The signed distance to an axis-aligned rounded box centered at the origin with
// half-extent `b` and corner radius `r` (negative inside, positive outside) — the
// in-shader sdRoundBox, mirrored in C++ headless (the same discipline that makes
// the SDF-text shader mirror S204's sdf_soft_alpha). Pure + headless-testable.
VAML_VULKAN_EXPORT float sd_rounded_box(float px, float py, float b, float r) noexcept;

// The BORDER decision seam: the alpha coverage of a rounded-rect border for a unit
// quad at UV (uvx, uvy), given thickness01 (border width / quad edge, clamped
// [0,0.5]), radius01 (corner rounding / quad edge, clamped [0,0.5]), a `feather`
// half-width, and `aa` (anti-alias half-width). Returns alpha in [0,1]: inside the
// border band -> 1, outside -> 0, the shaded edges -> the linear ramp. This is the
// CPU-side authority that border.frag mirrors verbatim. Pure + headless-testable.
VAML_VULKAN_EXPORT float border_coverage(float uvx, float uvy,
                                         float thickness01, float radius01,
                                         float feather, float aa) noexcept;

// ---------------------------------------------------------------------------
// Real UI-pipeline owner (RAII; owns the shared layouts + one kind's pipeline)
// ---------------------------------------------------------------------------

// The negotiated UI-pipeline spec, populated by UiPipeline::create(). Mirrors
// QuadPipelineSpec; the SRGB colorFormat is REQUIRED (the S200 SRGB pairing).
struct UiPipelineSpec
{
    VkRenderPass             renderPass     = VK_NULL_HANDLE;
    VkFormat                 colorFormat    = VK_FORMAT_UNDEFINED;
    bool                     hasDepth       = false;
    VkSampleCountFlagBits    sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D               extent         = { 0, 0 };
    VkShaderModule           vertexModule   = VK_NULL_HANDLE;   // the shared ui_vertex
    VkShaderModule           fragmentModule = VK_NULL_HANDLE;   // the kind's fragment
};

// The real S205 UI-pipeline owner: RAII + move-only, owning the SHARED descriptor
// set layout, the SHARED pipeline layout, AND one kind's graphics pipeline.
// Constructed only via create(); destroys pipeline -> descriptor set layout ->
// pipeline layout in reverse order on destruction. A consumer allocates ONE
// descriptor set from descriptor_set_layout(), binds the atlas sampler, and shares
// it across all three created UiPipelines (the shared-layout contract = the S206
// batching premise). See the header comment for LIFETIMES.
class VAML_VULKAN_EXPORT UiPipeline
{
public:
    // Creates one UI graphics pipeline of `kind` on `device` via `loader`: builds
    // the SHARED set layout (once) + the SHARED pipeline layout (one 128-byte
    // VERTEX|FRAGMENT push-constant range + the shared set layout), then the kind's
    // pipeline (vertex stage = spec.vertexModule / the shared ui_vertex; fragment
    // stage = spec.fragmentModule / the kind's shader) over the S200 vertex-input
    // state + the S200 straight-alpha blend + the SRGB color format. `spec.colorFormat`
    // MUST be SRGB (create returns nullptr otherwise — the S200 SRGB pairing).
    // Returns nullptr (no throw) on failure. `loader`, `device`, `spec.renderPass`
    // and the two shader modules must outlive the result.
    [[nodiscard]] static std::unique_ptr<UiPipeline>
    create(const VulkanLoader& loader, const Device& device,
           const UiPipelineSpec& spec, UiShaderKind kind);

    ~UiPipeline();
    UiPipeline(UiPipeline&&) noexcept;
    UiPipeline& operator=(UiPipeline&&) noexcept;
    UiPipeline(const UiPipeline&)            = delete;
    UiPipeline& operator=(const UiPipeline&) = delete;

    [[nodiscard]] UiShaderKind kind() const noexcept { return kind_; }
    [[nodiscard]] VkPipeline pipeline()        const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept { return pipelineLayout_; }
    // The SHARED descriptor set layout (set 0, binding 0 = the combined image
    // sampler). One set allocated from this layout is reused across all three
    // UiPipelines to serve the SDF text / solid / border shaders (S206 batching).
    [[nodiscard]] VkDescriptorSetLayout descriptor_set_layout() const noexcept { return descriptorSetLayout_; }
    [[nodiscard]] const UiPipelineSpec& spec() const noexcept { return spec_; }

private:
    UiPipeline(const VulkanLoader& loader, const Device& device);

    const VulkanLoader*     loader_ = nullptr;
    VkDevice                device_  = VK_NULL_HANDLE;  // BORROWED from the Device
    UiShaderKind            kind_    = UiShaderKind::Solid;
    VkPipelineLayout        pipelineLayout_      = VK_NULL_HANDLE;
    VkDescriptorSetLayout   descriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipeline              pipeline_ = VK_NULL_HANDLE;
    UiPipelineSpec          spec_{};
};

} // namespace vaml::vulkan::ui
