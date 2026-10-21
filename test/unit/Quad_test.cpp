// S200 — quad renderer decision seams (headless).
//
// Tests S200's PURE decision seams with fabricated Vulkan values — NO live Vulkan
// runtime, loader, device, render pass, pipeline, or texture (the S160..S169
// precedent: CI proves the decision layer headless; the LIVE QuadPipeline/
// QuadTexture/QuadScene create paths are manual via the POCs, [both apps, not CI]).
//
// The HEADLINE proofs: the alpha-blend state (QuadBlendMode::Alpha = the S200
// straight-alpha path), the vertex-input state (QuadVertex stride + 3 attributes),
// the 80-byte QuadUniform push constant (S200's kept-and-extended resolution of
// S165's 64-byte tri_mvp), ui_projection (finite, maps logical px into clip,
// scales with uiScale), and the SRGB pairing (is_srgb_format + quad_format_bytes).

#include <vaml/vulkan/quad/QuadPipeline.h>
#include <vaml/vulkan/quad/QuadVertex.h>
#include <vaml/vulkan/quad/QuadTexture.h>
#include <vaml/vulkan/ShaderModule.h>

#include <cmath>
#include <cstdint>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan::quad;
using namespace vaml::vulkan;

namespace {

bool finite_mat(const Mat4& m)
{
    for (float v : m)
        if (!std::isfinite(v))
            return false;
    return true;
}

// Applies the column-major mat4 `m` to the column vector (x,y,0,1) and returns
// the resulting clip x,y (after the perspective divide by w).
void apply(const Mat4& m, float x, float y, float& ox, float& oy)
{
    const float rw = x * m[3] + y * m[7] + 0.0f * m[11] + 1.0f * m[15];
    const float rx = x * m[0] + y * m[4] + 0.0f * m[8]  + 1.0f * m[12];
    const float ry = x * m[1] + y * m[5] + 0.0f * m[9]  + 1.0f * m[13];
    ox = rx / rw;
    oy = ry / rw;
}

} // namespace

TEST_CASE("S200 unit-quad mesh seams (vertex/index counts + indices + positions)")
{
    CHECK(quad_vertex_count() == 4u);
    CHECK(quad_index_count() == 6u);

    const auto& idx = quad_indices();
    CHECK(idx.size() == 6u);
    CHECK(idx[0] == 0u);
    CHECK(idx[1] == 1u);
    CHECK(idx[2] == 2u);
    CHECK(idx[3] == 0u);
    CHECK(idx[4] == 2u);
    CHECK(idx[5] == 3u);

    const auto pos = unit_quad_positions();
    CHECK(pos.size() == 8u);
    CHECK(pos[0] == doctest::Approx(0.0f));
    CHECK(pos[1] == doctest::Approx(0.0f));
    CHECK(pos[2] == doctest::Approx(1.0f));
    CHECK(pos[3] == doctest::Approx(0.0f));
    CHECK(pos[4] == doctest::Approx(1.0f));
    CHECK(pos[5] == doctest::Approx(1.0f));
    CHECK(pos[6] == doctest::Approx(0.0f));
    CHECK(pos[7] == doctest::Approx(1.0f));
}

TEST_CASE("S200 append_quad writes 4 vertices with correct position/uv/color")
{
    std::vector<QuadVertex> v;
    append_quad(v, 10.0f, 20.0f, 100.0f, 50.0f,
                0.0f, 0.0f, 1.0f, 1.0f,
                1.0f, 0.0f, 0.0f, 0.5f);
    CHECK(v.size() == 4u);

    // v0 = (10,20) uv (0,0)
    CHECK(v[0].x == doctest::Approx(10.0f));
    CHECK(v[0].y == doctest::Approx(20.0f));
    CHECK(v[0].u == doctest::Approx(0.0f));
    CHECK(v[0].v == doctest::Approx(0.0f));
    // v2 = (10+100, 20+50) = (110,70) uv (1,1)
    CHECK(v[2].x == doctest::Approx(110.0f));
    CHECK(v[2].y == doctest::Approx(70.0f));
    CHECK(v[2].u == doctest::Approx(1.0f));
    CHECK(v[2].v == doctest::Approx(1.0f));
    // color carried straight
    for (const auto& q : v)
    {
        CHECK(q.r == doctest::Approx(1.0f));
        CHECK(q.g == doctest::Approx(0.0f));
        CHECK(q.b == doctest::Approx(0.0f));
        CHECK(q.a == doctest::Approx(0.5f));
    }
}

TEST_CASE("S200 alpha-blend state (the S200 alpha-blend headline)")
{
    const auto alpha = quad_blend_state(QuadBlendMode::Alpha);
    CHECK(alpha.blendEnable == VK_TRUE);
    CHECK(alpha.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA);
    CHECK(alpha.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    CHECK(alpha.colorBlendOp == VK_BLEND_OP_ADD);
    CHECK(alpha.srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE);
    CHECK(alpha.dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    CHECK(alpha.alphaBlendOp == VK_BLEND_OP_ADD);
    CHECK(alpha.colorWriteMask == (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT));
    CHECK(describe_color_blend(QuadBlendMode::Alpha));

    const auto none = quad_blend_state(QuadBlendMode::None);
    CHECK(none.blendEnable == VK_FALSE);
    CHECK_FALSE(describe_color_blend(QuadBlendMode::None));
}

TEST_CASE("S200 vertex-input state (QuadVertex stride + 3 attributes)")
{
    const auto vi = quad_vertex_input_state();
    CHECK(vi.vertexBindingDescriptionCount == 1u);
    CHECK(vi.vertexAttributeDescriptionCount == 3u);
    REQUIRE(vi.pVertexBindingDescriptions != nullptr);
    REQUIRE(vi.pVertexAttributeDescriptions != nullptr);

    // Binding 0: stride == sizeof(QuadVertex) == 32, per-vertex.
    CHECK(vi.pVertexBindingDescriptions[0].binding == 0u);
    CHECK(vi.pVertexBindingDescriptions[0].stride == sizeof(QuadVertex));
    CHECK(vi.pVertexBindingDescriptions[0].inputRate == VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute 0 = vec2 position @ 0; 1 = vec2 uv @ 8; 2 = vec4 color @ 16.
    CHECK(vi.pVertexAttributeDescriptions[0].location == 0u);
    CHECK(vi.pVertexAttributeDescriptions[0].format == VK_FORMAT_R32G32_SFLOAT);
    CHECK(vi.pVertexAttributeDescriptions[0].offset == 0u);

    CHECK(vi.pVertexAttributeDescriptions[1].location == 1u);
    CHECK(vi.pVertexAttributeDescriptions[1].format == VK_FORMAT_R32G32_SFLOAT);
    CHECK(vi.pVertexAttributeDescriptions[1].offset == 8u);

    CHECK(vi.pVertexAttributeDescriptions[2].location == 2u);
    CHECK(vi.pVertexAttributeDescriptions[2].format == VK_FORMAT_R32G32B32A32_SFLOAT);
    CHECK(vi.pVertexAttributeDescriptions[2].offset == 16u);
}

TEST_CASE("S200 push constant is the 80-byte QuadUniform (kept-and-extended of S165)")
{
    const auto range = quad_push_constant();
    CHECK(range.stageFlags == VK_SHADER_STAGE_VERTEX_BIT);
    CHECK(range.offset == 0u);
    CHECK(range.size == 80u);  // the S165 64-byte mat4 is EXTENDED to Mat4+uiScale+pad

    // QuadUniform layout: Mat4 (64) + float uiScale (4) + pad (12) = 80.
    CHECK(sizeof(Mat4) == 64u);
    CHECK(sizeof(QuadUniform) == 80u);
}

TEST_CASE("S200 ui_projection is finite + maps logical px into clip")
{
    const Mat4 m = ui_projection(800, 600, 1.0f);
    CHECK(finite_mat(m));

    // Logical-px origin (0,0) maps to the top-left NDC (-1, +1) [y flipped].
    float ox = 0, oy = 0;
    apply(m, 0.0f, 0.0f, ox, oy);
    CHECK(ox == doctest::Approx(-1.0f));
    CHECK(oy == doctest::Approx(1.0f));

    // Center (400,300) maps to NDC center (0,0).
    apply(m, 400.0f, 300.0f, ox, oy);
    CHECK(ox == doctest::Approx(0.0f));
    CHECK(oy == doctest::Approx(0.0f));

    // Bottom-right (800,600) maps to (1,-1).
    apply(m, 800.0f, 600.0f, ox, oy);
    CHECK(ox == doctest::Approx(1.0f));
    CHECK(oy == doctest::Approx(-1.0f));

    // A mid-screen point stays in NDC [-1,1].
    apply(m, 200.0f, 150.0f, ox, oy);
    CHECK(ox >= -1.0f);
    CHECK(ox <= 1.0f);
    CHECK(oy >= -1.0f);
    CHECK(oy <= 1.0f);
}

TEST_CASE("S200 ui_projection scales with uiScale (zoom semantics)")
{
    // uiScale is a UI-zoom factor on the logical-px space: scaling the logical
    // px by uiScale means proportionally MORE layout fits in the same clip, so a
    // logical point lands at HALF the clip offset when uiScale doubles. The
    // clean invariant: the logical px that lands at NDC center (0,0) is W/2 under
    // scale 1 but W/4 under scale 2 (half the width visible = 2x zoom).
    const Mat4 m1 = ui_projection(800, 600, 1.0f);
    const Mat4 m2 = ui_projection(800, 600, 2.0f);

    float ox = 0, oy = 0;
    apply(m1, 400.0f, 300.0f, ox, oy);   // scale 1: (W/2, H/2) at center
    CHECK(ox == doctest::Approx(0.0f));
    CHECK(oy == doctest::Approx(0.0f));

    apply(m2, 200.0f, 150.0f, ox, oy);   // scale 2: (W/4, H/4) at center
    CHECK(ox == doctest::Approx(0.0f));
    CHECK(oy == doctest::Approx(0.0f));

    // A fixed logical point near the edge pushes proportionally further out
    // (toward clip) as uiScale grows — never collapses to the screen edge.
    float x1 = 0, y1 = 0, x2v = 0, y2v = 0;
    apply(m1, 300.0f, 225.0f, x1, y1);
    apply(m2, 300.0f, 225.0f, x2v, y2v);
    CHECK(x1 == doctest::Approx(-0.25f));  // 2*300/800-1 = -0.25
    CHECK(y1 == doctest::Approx(0.25f));
    CHECK(x2v == doctest::Approx(0.5f));   // 4*300/800-1 = 0.5
    CHECK(y2v == doctest::Approx(-0.5f));
    CHECK(std::abs(x2v) > std::abs(x1));   // higher scale -> larger offset
}

TEST_CASE("S200 SRGB pairing (is_srgb_format + quad_format_bytes)")
{
    CHECK(is_srgb_format(VK_FORMAT_B8G8R8A8_SRGB));
    CHECK(is_srgb_format(VK_FORMAT_R8G8B8A8_SRGB));
    CHECK_FALSE(is_srgb_format(VK_FORMAT_B8G8R8A8_UNORM));
    CHECK_FALSE(is_srgb_format(VK_FORMAT_UNDEFINED));
    CHECK_FALSE(is_srgb_format(VK_FORMAT_D32_SFLOAT));

    CHECK(quad_format_bytes(VK_FORMAT_B8G8R8A8_SRGB) == 4u);
    CHECK(quad_format_bytes(VK_FORMAT_R8G8B8A8_SRGB) == 4u);
    CHECK(quad_format_bytes(VK_FORMAT_UNDEFINED) == 0u);
}

TEST_CASE("S200 sampler + descriptor seam shape")
{
    const auto s = quad_sampler();
    CHECK(s.magFilter == VK_FILTER_LINEAR);
    CHECK(s.minFilter == VK_FILTER_LINEAR);
    CHECK(s.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    CHECK(s.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    CHECK(s.unnormalizedCoordinates == VK_FALSE);

    const auto binding = quad_set_layout_binding();
    CHECK(binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    CHECK(binding.descriptorCount == 1u);
    CHECK(binding.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK(binding.binding == 0u);

    // The descriptor write's pImageInfo points into the caller-owned info.
    VkDescriptorImageInfo info{};
    const VkWriteDescriptorSet write = quad_descriptor_write(
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

TEST_CASE("S200 procedural checker texture bytes")
{
    const std::uint32_t w = 16, h = 8;
    CHECK(texture_format_bytes(w, h) == w * h * 4u);
    const auto bytes = create_procedural_checker(w, h, 2);
    CHECK(bytes.size() == texture_format_bytes(w, h));
    if (bytes.size() >= 4u)
    {
        // Two opposite corners of a 2-cell checker differ (not a solid fill).
        const std::size_t corner0 = 0u;
        const std::size_t corner1 = (static_cast<std::size_t>(h - 1) * w + (w - 1)) * 4u;
        const bool same = (bytes[corner0] == bytes[corner1]) &&
                          (bytes[corner0 + 1] == bytes[corner1 + 1]) &&
                          (bytes[corner0 + 2] == bytes[corner1 + 2]);
        CHECK_FALSE(same);
    }
}

TEST_CASE("S200 embedded quad shaders are structurally sound")
{
    const auto vert = quad_vertex_spirv();
    const auto frag = quad_fragment_spirv();
    CHECK(!vert.empty());
    CHECK(!frag.empty());
    CHECK(vert[0] == 0x07230203u);  // SPIR-V magic
    CHECK(frag[0] == 0x07230203u);
    CHECK(vert.size() > 5u);
    CHECK(frag.size() > 5u);
}
