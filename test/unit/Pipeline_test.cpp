// S165 — render-pass/pipeline/framebuffer decision seams (headless).
//
// Tests S165's PURE decision seams with fabricated Vulkan values — NO live Vulkan
// runtime, loader, device, render pass, pipeline, or framebuffer (the S160/S162/
// S163/S164 precedent: CI proves the decision layer headless; the LIVE
// RenderPass/Pipeline/ShaderModule/Framebuffer create() paths are manual via the
// POCs, [both apps, not CI]).
//
// The HEADLINE proof: tri_mvp(width, height) — the hardcoded MVP — is finite,
// invertible (diagonal 1.0), and maps every tri_vertex_positions() clip-space
// vertex to an in-range NDC x/y when fed through it. This is the "hardcoded MVP"
// contract of S165 proven WITHOUT any device.

#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/ShaderModule.h>

#include <cmath>
#include <cstdint>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

namespace {

bool finite_mat(const Mat4& m)
{
    for (float v : m)
        if (!std::isfinite(v))
            return false;
    return true;
}

// Applies the column-major mat4 `m` to the column vector `v` (x,y,z,w) and
// returns the resulting x,y (after perspective divide by w).
void apply(const Mat4& m, float x, float y, float z, float w, float& ox, float& oy)
{
    const float rw = x * m[3] + y * m[7] + z * m[11] + w * m[15];
    const float rx = x * m[0] + y * m[4] + z * m[8]  + w * m[12];
    const float ry = x * m[1] + y * m[5] + z * m[9]  + w * m[13];
    ox = rx / rw;
    oy = ry / rw;
}

} // namespace

TEST_CASE("S165 tri_mvp is finite + invertible (hardcoded MVP contract)")
{
    const Mat4 m = tri_mvp(800, 600);
    CHECK(finite_mat(m));
    // Diagonal 1.0 identity (column-major), off-diagonal zero.
    CHECK(m[0] == 1.0f);
    CHECK(m[5] == 1.0f);
    CHECK(m[10] == 1.0f);
    CHECK(m[15] == 1.0f);
    CHECK(m[1] == 0.0f);
    CHECK(m[4] == 0.0f);
    CHECK(m[6] == 0.0f);
}

TEST_CASE("S165 tri_mvp maps every triangle vertex to in-range NDC")
{
    const Mat4 m = tri_mvp(800, 600);
    const auto pos = tri_vertex_positions();
    CHECK(pos.size() == 12);  // 3 vertices x 4 components

    for (std::size_t i = 0; i + 3 < pos.size(); i += 4)
    {
        float ox = 0.0f, oy = 0.0f;
        apply(m, pos[i], pos[i + 1], pos[i + 2], pos[i + 3], ox, oy);
        // Identity MVP -> the clip-space position maps verbatim into NDC [-1,1].
        CHECK(ox >= -1.0f);
        CHECK(ox <= 1.0f);
        CHECK(oy >= -1.0f);
        CHECK(oy <= 1.0f);
        CHECK(std::isfinite(ox));
        CHECK(std::isfinite(oy));
    }
}

TEST_CASE("S165 tri_mvp works for any framebuffer size")
{
    const Mat4 small = tri_mvp(1, 1);
    const Mat4 wide  = tri_mvp(1920, 1080);
    CHECK(finite_mat(small));
    CHECK(finite_mat(wide));
    // Identity for every size: all three vertices stay in NDC.
    const auto pos = tri_vertex_positions();
    for (std::size_t i = 0; i + 3 < pos.size(); i += 4)
    {
        float sx = 0, sy = 0, wx = 0, wy = 0;
        apply(small, pos[i], pos[i+1], pos[i+2], pos[i+3], sx, sy);
        apply(wide,  pos[i], pos[i+1], pos[i+2], pos[i+3], wx, wy);
        CHECK(sx >= -1.0f);
        CHECK(sx <= 1.0f);
        CHECK(wx >= -1.0f);
        CHECK(wx <= 1.0f);
    }
}

TEST_CASE("S165 triangle positions are the documented hardcoded clip vertices")
{
    const auto pos = tri_vertex_positions();
    CHECK(pos[0] == doctest::Approx(-0.8f));
    CHECK(pos[1] == doctest::Approx(0.8f));
    CHECK(pos[4] == doctest::Approx(0.8f));
    CHECK(pos[5] == doctest::Approx(0.8f));
    CHECK(pos[8] == doctest::Approx(0.0f));
    CHECK(pos[9] == doctest::Approx(-0.8f));
    // w == 1 for all three.
    CHECK(pos[3] == doctest::Approx(1.0f));
    CHECK(pos[7] == doctest::Approx(1.0f));
    CHECK(pos[11] == doctest::Approx(1.0f));
}

TEST_CASE("S165 embedded SPIR-V modules are structurally sound")
{
    const auto vert = tri_vertex_spirv();
    const auto frag = tri_fragment_spirv();
    CHECK(!vert.empty());
    CHECK(!frag.empty());
    // SPIR-V magic header word + a plausible length.
    CHECK(vert[0] == 0x07230203u);
    CHECK(frag[0] == 0x07230203u);
    // Exploded word count > just the header.
    CHECK(vert.size() > 5u);
    CHECK(frag.size() > 5u);
}

TEST_CASE("S165 shader color is the documented light blue")
{
    const auto c = tri_fragment_color();
    CHECK(c.size() == 4);
    CHECK(c[0] == doctest::Approx(0.5f));
    CHECK(c[1] == doctest::Approx(0.8f));
    CHECK(c[2] == doctest::Approx(1.0f));
    CHECK(c[3] == doctest::Approx(1.0f));
}

TEST_CASE("S165 render pass decision seams")
{
    // Color attachment: CLEAR/STORE, color usage, present-source final layout.
    const VkAttachmentDescription color = describe_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, VK_SAMPLE_COUNT_1_BIT);
    CHECK(color.format == VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(color.samples == VK_SAMPLE_COUNT_1_BIT);
    CHECK(color.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
    CHECK(color.storeOp == VK_ATTACHMENT_STORE_OP_STORE);
    CHECK(color.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED);
    CHECK(color.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Depth attachment: CLEAR/DONT_CARE, depth-stencil optimal final.
    const VkAttachmentDescription depth = describe_depth_attachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
    CHECK(depth.format == VK_FORMAT_D32_SFLOAT);
    CHECK(depth.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
    CHECK(depth.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
    CHECK(depth.finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Load/store decision: fresh image clears (undefined initial); with depth,
    // the depth stays in the depth-stencil layout.
    auto dsColorOnly = decide_load_store(false, true);
    CHECK(dsColorOnly.colorInitial == VK_IMAGE_LAYOUT_UNDEFINED);
    CHECK(dsColorOnly.colorFinal == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    auto dsDepth = decide_load_store(true, false);
    CHECK(dsDepth.depthInitial == VK_IMAGE_LAYOUT_UNDEFINED);
    CHECK(dsDepth.depthFinal == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST_CASE("S165 pipeline decision seams")
{
    const auto vs = viewport_scissor(VkExtent2D{ 800, 600 });
    CHECK(vs.viewport.width == doctest::Approx(800.0f));
    CHECK(vs.viewport.height == doctest::Approx(600.0f));
    CHECK(vs.viewport.minDepth == doctest::Approx(0.0f));
    CHECK(vs.viewport.maxDepth == doctest::Approx(1.0f));
    CHECK(vs.scissor.extent.width == 800u);
    CHECK(vs.scissor.extent.height == 600u);

    // Push-constant range: VERTEX stage, offset 0, size 64 (one mat4).
    const auto range = push_constant_range();
    CHECK(range.stageFlags == VK_SHADER_STAGE_VERTEX_BIT);
    CHECK(range.offset == 0u);
    CHECK(range.size == 64u);

    // Vertex input is EMPTY (no vertex buffer in S165).
    const auto vi = tri_vertex_input_state();
    CHECK(vi.vertexBindingDescriptionCount == 0u);
    CHECK(vi.vertexAttributeDescriptionCount == 0u);
}

TEST_CASE("S165 framebuffer attachment layout contract")
{
    CHECK(framebuffer_attachment_count(0, false) == 0u);
    CHECK(framebuffer_attachment_count(3, false) == 3u);
    CHECK(framebuffer_attachment_count(3, true) == 4u);
    CHECK(framebuffer_attachment_count(1, true) == 2u);
}
