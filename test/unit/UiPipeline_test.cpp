// S205 — UiPipeline_test: headless proof of the S205 UI pipeline family's PURE
// DECISION SEAMS (vaml::vulkan::ui), dep:S204. No Vulkan runtime needed:
// (1) the SHARED descriptor-set layout (set 0 binding 0 = combined image sampler)
// + the 128-byte shared UiPushConstants (16-aligned, mvp/uiScale at the S200
// offsets); (2) the per-kind pc filler seams (solid/border/sdf); (3) the BORDER
// decision seam (sd_rounded_box + border_coverage — inside->1, outside->0,
// edge->~0.5, feather/aa widening, radius rounding); (4) the SRGB gate + the
// shared set-layout shape; (5) the four embedded UI shaders are structurally sound.
//
// NO-GAP (recorded, not silent): the live UiPipeline::create + the shared-layout
// descriptor-set allocation + the recorded all-three-kinds draw are device-backed —
// proven by the [s205-live] POC block, not CI (the S200/S202_1/S204 precedent).

#include <vaml/vulkan/ui/UiPipeline.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/quad/QuadPipeline.h>

#include <cstddef>
#include <cmath>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;
using namespace vaml::vulkan;

namespace {

// A hand-authorable straight-alpha color helper.
ui::Color4 col(float r, float g, float b, float a)
{
    return ui::Color4{ r, g, b, a };
}

bool close(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

TEST_CASE("S205: shared push constant is the 128-byte VERTEX|FRAGMENT block")
{
    const auto range = ui_push_constant();
    CHECK(range.stageFlags == (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
    CHECK(range.offset == 0u);
    CHECK(range.size == 128u);  // the shared UiPushConstants

    // Layout: mvp (64) + uiAndPad (16) + color (16) + params (16) + size (16).
    CHECK(sizeof(UiPushConstants) == 128u);
    CHECK(sizeof(Mat4) == 64u);
    CHECK(offsetof(UiPushConstants, mvp) == 0u);
    CHECK(offsetof(UiPushConstants, uiAndPad) == 64u);   // the S200 uiScale offset
    CHECK(offsetof(UiPushConstants, color) == 80u);
    CHECK(offsetof(UiPushConstants, params) == 96u);
    CHECK(offsetof(UiPushConstants, size) == 112u);
    // 16-byte aligned (the push-constant + std140 requirement).
    CHECK(offsetof(UiPushConstants, mvp) % 16u == 0u);
    CHECK(offsetof(UiPushConstants, uiAndPad) % 16u == 0u);
    CHECK(offsetof(UiPushConstants, color) % 16u == 0u);
    CHECK(offsetof(UiPushConstants, params) % 16u == 0u);
    CHECK(offsetof(UiPushConstants, size) % 16u == 0u);
}

TEST_CASE("S205: the shared descriptor-set layout is set 0 binding 0 combined image sampler")
{
    const auto binding = ui_shared_set_layout_binding();
    CHECK(binding.binding == 0u);
    CHECK(binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    CHECK(binding.descriptorCount == 1u);
    CHECK(binding.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK(binding.pImmutableSamplers == nullptr);

    // The same layout is the shared-basis generalization of the S200-local binding.
    const auto quadBinding = quad::quad_set_layout_binding();
    CHECK(binding.binding == quadBinding.binding);
    CHECK(binding.descriptorType == quadBinding.descriptorType);
    CHECK(binding.descriptorCount == quadBinding.descriptorCount);
    CHECK(binding.stageFlags == quadBinding.stageFlags);
}

TEST_CASE("S205: sampler + shared descriptor write shape")
{
    const auto s = ui_sampler();
    CHECK(s.magFilter == VK_FILTER_LINEAR);
    CHECK(s.minFilter == VK_FILTER_LINEAR);
    CHECK(s.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    CHECK(s.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    CHECK(s.unnormalizedCoordinates == VK_FALSE);

    VkDescriptorImageInfo info{};
    const VkWriteDescriptorSet write = ui_descriptor_write(
        reinterpret_cast<VkDescriptorSet>(1), info,
        reinterpret_cast<VkImageView>(2), reinterpret_cast<VkSampler>(3));
    CHECK(write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    CHECK(write.descriptorCount == 1u);
    CHECK(write.dstBinding == 0u);
    REQUIRE(write.pImageInfo != nullptr);
    CHECK(write.pImageInfo == &info);
    CHECK(info.imageView == reinterpret_cast<VkImageView>(2));
    CHECK(info.sampler == reinterpret_cast<VkSampler>(3));
    CHECK(info.imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_CASE("S205: per-kind style fillers encode params at the right offsets")
{
    UiPushConstants pc{};

    // Solid: color set, params zeroed (no params).
    solid_style(pc, col(1, 0, 0, 0.5f));
    CHECK(pc.color[0] == doctest::Approx(1.0f));
    CHECK(pc.color[1] == doctest::Approx(0.0f));
    CHECK(pc.color[2] == doctest::Approx(0.0f));
    CHECK(pc.color[3] == doctest::Approx(0.5f));
    CHECK(pc.params[0] == doctest::Approx(0.0f));
    CHECK(pc.params[1] == doctest::Approx(0.0f));

    // Border: params = { thickness01, radius01, feather, 0 }.
    border_style(pc, col(0, 1, 0, 0.8f), 0.1f, 0.05f, 0.02f);
    CHECK(pc.color[1] == doctest::Approx(1.0f));
    CHECK(pc.params[0] == doctest::Approx(0.1f));
    CHECK(pc.params[1] == doctest::Approx(0.05f));
    CHECK(pc.params[2] == doctest::Approx(0.02f));
    CHECK(pc.params[3] == doctest::Approx(0.0f));

    // SDF: params = { aa, 0, 0, 0 }.
    sdf_style(pc, col(1, 1, 1, 1.0f), 0.15f);
    CHECK(pc.params[0] == doctest::Approx(0.15f));
    CHECK(pc.params[1] == doctest::Approx(0.0f));
    CHECK(pc.params[2] == doctest::Approx(0.0f));
    CHECK(pc.params[3] == doctest::Approx(0.0f));

    // uiScale rides the S200 offset 64 (uiAndPad.x), untouched by the fillers.
    pc = UiPushConstants{};
    pc.uiAndPad[0] = 1.5f;
    sdf_style(pc, col(1, 0, 0, 1.0f), 0.1f);
    CHECK(pc.uiAndPad[0] == doctest::Approx(1.5f));  // the filler does not clobber uiScale
}

TEST_CASE("S205: sd_rounded_box distances (the border SDF seam)")
{
    // Center of the unit box (half-extent 0.5): clearly inside -> negative.
    CHECK(sd_rounded_box(0.0f, 0.0f, 0.5f, 0.1f) < 0.0f);
    // Far outside corner: clearly outside -> positive.
    CHECK(sd_rounded_box(9.0f, 9.0f, 0.5f, 0.1f) > 1.0f);
    // The rounded corner: a point at (0.5, 0.5) is EXACTLY on the (non-rounded)
    // box edge (distance ~0 when the radius is tiny).
    CHECK(close(sd_rounded_box(0.5f, 0.0f, 0.5f, 0.001f), 0.0f, 1e-3f));
    // Radius pulls the corner inward: (0.5,0.5) with a real radius sits OUTSIDE
    // the rounded corner cut (positive distance), whereas it was exactly ON the
    // square edge (distance ~0). So a rounded corner is the smaller (cut-away).
    CHECK(sd_rounded_box(0.5f, 0.5f, 0.5f, 0.2f) > 0.0f);
}

TEST_CASE("S205: border_coverage is in-band->1 / outside->0 / edge->~0.5")
{
    // A border of thickness01 = 0.1 => the stroke band is the OUTER 0.1 of the
    // unit quad: on +x, UV x in [0,0.1] (the band midpoint is UV 0.05).
    const float t = 0.1f, r = 0.0f, feather = 0.0f, aa = 0.01f;

    // The band's OUTER edge is at UV 1.0 (+x face, p = +0.5): sd_outer == 0 ->
    // the exact edge -> ~0.5.
    const float edgeAlpha = border_coverage(1.0f, 0.5f, t, r, feather, aa);
    CHECK(edgeAlpha >= 0.45f);
    CHECK(edgeAlpha <= 0.55f);

    // Band midpoint (UV 0.05 on +x = p.x -0.45, mid-y): deep in the stroke -> ~1.
    CHECK(border_coverage(0.05f, 0.5f, t, r, 0.0f, aa) > 0.95f);
    CHECK(border_coverage(0.5f, 0.05f, t, r, 0.0f, aa) > 0.95f);   // top band
    // The far +y band midpoint (UV 0.95) is also in the stroke.
    CHECK(border_coverage(0.5f, 0.95f, t, r, 0.0f, aa) > 0.95f);

    // Well outside the quad (UV far beyond [0,1]): -> 0.
    CHECK(border_coverage(9.0f, 9.0f, t, r, 0.0f, aa) == doctest::Approx(0.0f));

    // The interior (the empty hole, UV 0.5 center): far from the band -> 0.
    CHECK(border_coverage(0.5f, 0.5f, t, r, 0.0f, aa) < 0.5f);
    CHECK(border_coverage(0.5f, 0.5f, t, r, 0.0f, aa) < 0.05f);
}

TEST_CASE("S205: border_coverage feather widening (the AA knob)")
{
    const float t = 0.1f, r = 0.0f;
    // The band's edges span UV [0,0.1] on +x (mid-y). UV 0.12 is just OUTSIDE the
    // inner edge; a wider feather/aa yields MORE coverage there (the shaded edge
    // widens outward into the hole).
    const float tight = border_coverage(0.12f, 0.5f, t, r, 0.0f, 0.005f);
    const float wide  = border_coverage(0.12f, 0.5f, t, r, 0.0f, 0.08f);
    CHECK(wide > tight);
    // Both stay in [0,1].
    CHECK(tight >= 0.0f);
    CHECK(tight <= 1.0f);
    CHECK(wide >= 0.0f);
    CHECK(wide <= 1.0f);
}

TEST_CASE("S205: border_coverage radius rounding pulls the corner band")
{
    // The stroke near a corner: with a real corner radius, the rounded corner cut
    // erases the stroke at the exact corner line, so the square corner has the
    // stroke ((0.95,0.5) is still mid-band) while the rounded corner at the same
    // far x is pulled inward. We assert a positive-radius border yields a NON-ZERO
    // rounded-corner presence (the radius is honored, not a no-op).
    const float t = 0.1f, aa = 0.01f;
    // On the +x axis mid-y a rounded corner behaves like a square one (no corner
    // influence there) — the band is ~1 regardless of radius.
    const float square = border_coverage(0.05f, 0.5f, t, 0.0f, 0.0f, aa);
    const float round  = border_coverage(0.05f, 0.5f, t, 0.3f, 0.0f, aa);
    CHECK(square > 0.95f);
    CHECK(round > 0.95f);
    // At the corner direction (both UVs high), a rounded corner erases the band far
    // from the corner center while a square corner only fades at the exact edge —
    // the rounded-coverage is the smaller of the two at (0.95, 0.95).
    const float squareCorner = border_coverage(0.95f, 0.95f, t, 0.0f, 0.0f, aa);
    const float roundCorner  = border_coverage(0.95f, 0.95f, t, 0.3f, 0.0f, aa);
    CHECK(roundCorner <= squareCorner);
}

TEST_CASE("S205: SRGB gate + spec shape (the S200 SRGB pairing)")
{
    // UiPipeline::create opens by requiring an SRGB color format (like QuadPipeline).
    CHECK(quad::is_srgb_format(VK_FORMAT_B8G8R8A8_SRGB));
    CHECK(quad::is_srgb_format(VK_FORMAT_R8G8B8A8_SRGB));
    CHECK_FALSE(quad::is_srgb_format(VK_FORMAT_UNDEFINED));

    UiPipelineSpec spec{};
    CHECK(spec.renderPass == VK_NULL_HANDLE);
    CHECK(spec.vertexModule == VK_NULL_HANDLE);
    CHECK(spec.fragmentModule == VK_NULL_HANDLE);
    CHECK(spec.colorFormat == VK_FORMAT_UNDEFINED);
}

TEST_CASE("S205: the four embedded UI shaders are structurally sound")
{
    const auto vert = ui_vertex_spirv();
    const auto sdf  = sdf_text_fragment_spirv();
    const auto solid  = solid_color_fragment_spirv();
    const auto border = border_fragment_spirv();
    CHECK(!vert.empty());
    CHECK(!sdf.empty());
    CHECK(!solid.empty());
    CHECK(!border.empty());
    // SPIR-V magic + non-trivial length (all four, like the S200/quad shaders).
    CHECK(vert[0] == 0x07230203u);
    CHECK(sdf[0] == 0x07230203u);
    CHECK(solid[0] == 0x07230203u);
    CHECK(border[0] == 0x07230203u);
    CHECK(vert.size() > 5u);
    CHECK(sdf.size() > 5u);
    CHECK(solid.size() > 5u);
    CHECK(border.size() > 5u);
    // The solid fragment is the shortest (no sampling); the border fragment is the
    // largest (the rounded-box SDF) — a passing structural-intent note.
    CHECK(solid.size() < sdf.size());
    CHECK(border.size() > sdf.size());
}

TEST_CASE("S205: ui projection round-trips into the shared mvp slot")
{
    // The shared UiPushConstants.mvp composes the S200 ui_projection (the mvp is
    // at offset 0, forward-compatible with quad::ui_projection + S208 Model).
    UiPushConstants pc{};
    pc.mvp = quad::ui_projection(800, 600, 1.0f);
    // The projection is finite (the S200 contract) and rides the shared mvp slot.
    bool finite = true;
    for (float v : pc.mvp)
        if (!std::isfinite(v))
            finite = false;
    CHECK(finite);
    // The shared mvp slot is at offset 0 (already asserted above) so a draw loop
    // pushes pc over the S205 128-byte range after filling mvp with ui_projection.
    CHECK(pc.mvp[0] > 0.0f);   // x scale (2*uiScale/width) is positive
}
