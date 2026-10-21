#pragma once
// S165 — Vulkan shader module + the embedded single-triangle SPIR-V.
//
// The S165 title's shader half, delivered as a Cat 1 (library-surface) RAII
// owner + the pre-baked single-triangle shader pair. Following the "hardcoded
// MVP" minimalism of S165, the vertex + fragment shaders are EMBEDDED as
// pre-baked SPIR-V byte arrays (no glslc/glslang build step — the repo has no
// SPIR-V toolchain, and a real shader-authoring pipeline is S205's job, recorded
// NO-GAP, not silent). The SPIR-V is deliberately authored by hand (SPIR-V 1.0,
// Vulkan 1.0-capable) and validated LIVE via vkCreateShaderModule on the POCs
// (the [both-apps, not CI] live path — no validator binary on either machine).
//
// Shader contract (what the S165 pipeline + S166 draw loop consume):
//   * VERTEX (tri_vertex_spirv): reads a single 4x4 MVP matrix from the VERTEX
//     stage push-constant range (offset 0), and three hardcoded clip-space
//     positions forming a single counter-clockwise triangle. The push constant
//     is written by S166's draw loop from the S165 `tri_mvp(width,height)`
//     hardcoded matrix. The rasterization order + the fixed clip-space positions
//     are asserted headlessly by Pipeline_test.cpp via `tri_vertex_positions()`.
//   * FRAGMENT (tri_fragment_spirv): outputs a hardcoded opaque color
//     (light blue) to the color attachment.
//
// The SPIR-V is produced from the canonical GLSL in the source comments and is
// structural-intent-tested (span lengths, magic, correct module) headlessly; the
// true "is it a valid device shader" check is vkCreateShaderModule on the POCs.
// If this pre-baked SPIR-V ever proves invalid on a real driver, the own-step
// fix is to substitute the tool-compiled binary here (S205 owns the in-repo
// glslc pipeline) — recorded, not silent.
//
// NO-GAP recorded (not silent): S205 owns the real shader authoring pipeline
// (glslc in CMake) + the SDF text / solid-color / border UI shaders + descriptor
// sets; S165 embeds only this hand-authored triangle pair to keep the step
// faithful to "hardcoded MVP". Multi-triangle / real geometry = S200+.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <span>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// The embedded single-triangle vertex shader as pre-baked SPIR-V words.
VAML_VULKAN_EXPORT std::span<const std::uint32_t> tri_vertex_spirv() noexcept;

// The embedded single-triangle fragment shader as pre-baked SPIR-V words
// (outputs a hardcoded light-blue color).
VAML_VULKAN_EXPORT std::span<const std::uint32_t> tri_fragment_spirv() noexcept;

// The three hardcoded clip-space positions of the single triangle, as (x, y, z, w)
// columns of VkClearValue-friendly floats. Exposed for the S165 headless contract:
// Pipeline_test asserts `tri_mvp(width,height)` maps each vertex into the
// viewport's xy range. The positions are a fixed counter-clockwise triangle
// spanning most of the clip space:
//   v0 = (-0.8, 0.8, 0.0, 1.0)   (top-left)
//   v1 = ( 0.8, 0.8, 0.0, 1.0)   (top-right)
//   v2 = ( 0.0,-0.8, 0.0, 1.0)   (bottom-center)
VAML_VULKAN_EXPORT std::span<const float> tri_vertex_positions() noexcept;

// The hardcoded per-vertex color the fragment shader outputs (r, g, b, a).
VAML_VULKAN_EXPORT std::span<const float> tri_fragment_color() noexcept;

// ---- S200 embedded QUAD shader pair (the textured-quad shaders) ----
// The S200 quad vertex + fragment shaders as pre-baked SPIR-V words (S205 owns
// the real in-repo glslc pipeline + the SDF/solid/border UI shaders; S200 embeds
// this one textured-quad pair so "textured quads" is real — the NO-GAP record at
// S200, not a backfill). The vertex shader reads the S200 80-byte QuadUniform
// push constant (mvp + uiScale) + the S200 vertex input (pos/uv/color); the
// fragment samples the S200-local combined-image-sampler descriptor (set 0,
// binding 0) and multiplies by the straight-alpha vertex color (the S200 alpha
// blend + SRGB path). See quad_vertex_spirv()/quad_fragment_spirv() below.
VAML_VULKAN_EXPORT std::span<const std::uint32_t> quad_vertex_spirv() noexcept;
VAML_VULKAN_EXPORT std::span<const std::uint32_t> quad_fragment_spirv() noexcept;

// ---- S205 embedded UI shader set (the four "UI shaders") ----
// S205's shader half: ONE shared UI vertex shader + THREE UI fragment shaders
// (SDF text / solid color / border), all rendering the S200 unit quad. Each is
// pre-baked, validated SPIR-V (the S165/S200/S203 precedent — compiled with
// glslangValidator on the S205 shader tools, spirv-val, spirv-opt --strip-debug,
// then the EXACT word array embedded). The canonical GLSL is in the source
// comments of ShaderModule.cpp as the auditable source of truth; the validators
// ran on the Ubuntu VM (the repo's declared shader-authoring toolchain), NOT
// CMake glslc — a recorded divergence of the S165/S200 "glslc in CMake" wording.
//   * ui_vertex_spirv()         — shared UI vertex: reads UiPushConstants.mvp +
//     the S200 QuadVertex input (pos/uv/vcolor) -> clip + the uv/color varyings.
//   * sdf_text_fragment_spirv() — SDF text: samples the shared set-0-binding-0
//     atlas, recovers the packed SDF distance, applies the S204 `sdf_soft_alpha`
//     linear-AA ramp mirrored exactly in-shader (the S204-recorded "text shader
//     mirrors the CPU sdf_soft_alpha AA contract" hand-off, RESOLVED here),
//     multiplied by the straight-alpha text/vertex color.
//   * solid_color_fragment_spirv() — flat straight-alpha fill from
//     UiPushConstants.color x the vertex color; no sampling.
//   * border_fragment_spirv()  — rectangular-stroke coverage from vUv +
//     pc.params (thickness01/radius01/feather/aa) + pc; outputs the border color
//     in the band, transparent outside; mirrors UiPipeline's border_coverage().
// All three fragments share the SAME 128-byte UiPushConstants + the SAME shared
// set-0-binding-0 atlas layout as the vertex shader, so one shared pipeline
// layout + one bound descriptor set serves all three (the S206 batching premise).
VAML_VULKAN_EXPORT std::span<const std::uint32_t> ui_vertex_spirv() noexcept;
VAML_VULKAN_EXPORT std::span<const std::uint32_t> sdf_text_fragment_spirv() noexcept;
VAML_VULKAN_EXPORT std::span<const std::uint32_t> solid_color_fragment_spirv() noexcept;
VAML_VULKAN_EXPORT std::span<const std::uint32_t> border_fragment_spirv() noexcept;

// The REAL S165 shader-module owner: RAII + move-only wrapping a single
// vkCreateShaderModule result. Constructed only via create(); destroys the
// shader module on destruction. `loader` and `device` must outlive it.
class VAML_VULKAN_EXPORT ShaderModule
{
public:
    // Creates a shader module from a pre-baked SPIR-V word span (e.g. the
    // tri_vertex_spirv()/tri_fragment_spirv() pair) on `device` (the S169
    // Device owner). Returns nullptr (no throw) on failure — including when the
    // SPIR-V is rejected by the driver (the S165 live validation path). Mirrors
    // PlatformWindow/VulkanInstance/Swapchain/RenderPass::create.
    [[nodiscard]] static std::unique_ptr<ShaderModule>
    create(const VulkanLoader& loader, const Device& device, std::span<const std::uint32_t> spirv);

    ~ShaderModule();
    ShaderModule(ShaderModule&&) noexcept;
    ShaderModule& operator=(ShaderModule&&) noexcept;
    ShaderModule(const ShaderModule&)            = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    [[nodiscard]] VkShaderModule native_handle() const noexcept { return module_; }
    [[nodiscard]] VkShaderModule shader_module() const noexcept { return module_; }

private:
    ShaderModule(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice           device_  = VK_NULL_HANDLE;  // BORROWED from the Device
    VkShaderModule     module_  = VK_NULL_HANDLE;
};

} // namespace vaml::vulkan
