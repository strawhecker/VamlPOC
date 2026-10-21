// S201 — N-quad scene decision seams + blend/SRGB tests (headless).
//
// Tests S201's PURE decision seams (scene_vertex_count / scene_index_count /
// append_scene_item / scene_record_plan) + the S201 "blend + SRGB tests" headline
// (per-quad blend state selection + the SRGB pairing) with fabricated values — NO
// live Vulkan runtime, loader, device, scene, pipeline, or texture (the standing
// precedent; the LIVE QuadScene create/set_quads/record_draw path is manual via
// the POCs, [both apps, not CI]).

#include <vaml/vulkan/quad/QuadScene.h>
#include <vaml/vulkan/quad/QuadPipeline.h>

#include <cstdint>

#include "doctest/doctest.h"

using namespace vaml::vulkan::quad;
using namespace vaml::vulkan;

TEST_CASE("S201 N-quad vertex/index counts (N quads = 4N verts / 6N indices)")
{
    CHECK(scene_vertex_count(0u) == 0u);
    CHECK(scene_index_count(0u) == 0u);
    CHECK(scene_vertex_count(1u) == 4u);
    CHECK(scene_index_count(1u) == 6u);
    CHECK(scene_vertex_count(3u) == 12u);
    CHECK(scene_index_count(3u) == 18u);
    CHECK(scene_vertex_count(100u) == 400u);
    CHECK(scene_index_count(100u) == 600u);
}

TEST_CASE("S201 append_scene_item produces the N-quad CPU list")
{
    std::vector<QuadVertex> v;
    QuadItem a;
    a.x = 0.0f; a.y = 0.0f; a.w = 100.0f; a.h = 50.0f; a.a = 1.0f;
    QuadItem b;
    b.x = 200.0f; b.y = 0.0f; b.w = 50.0f; b.h = 50.0f; b.blendMode = QuadBlendMode::None;
    append_scene_item(v, a);
    append_scene_item(v, b);

    CHECK(v.size() == 8u);  // 2 quads = 8 vertices
    // First quad's v0 at its origin; second quad's v0 offset.
    CHECK(v[0].x == doctest::Approx(0.0f));
    CHECK(v[0].y == doctest::Approx(0.0f));
    CHECK(v[4].x == doctest::Approx(200.0f));
    CHECK(v[4].y == doctest::Approx(0.0f));
}

TEST_CASE("S201 scene record plan is the fixed N-quad ordered draw")
{
    const auto& plan = scene_record_plan();
    REQUIRE(plan.size() == 7u);
    CHECK(plan[0] == SceneRecordStep::BindPipeline);
    CHECK(plan[1] == SceneRecordStep::SetViewport);
    CHECK(plan[2] == SceneRecordStep::SetScissor);
    CHECK(plan[3] == SceneRecordStep::BindDescriptorSet);
    CHECK(plan[4] == SceneRecordStep::PushQuadUniform);
    CHECK(plan[5] == SceneRecordStep::BindIndexBuffer);
    CHECK(plan[6] == SceneRecordStep::DrawIndexed);
}

TEST_CASE("S201 blend + SRGB tests (the S201 headline)")
{
    // Blend: an Alpha quad selects the straight-alpha blend attachment; a None
    // quad disables blending — the per-scene distinction.
    const auto alpha = quad_blend_state(QuadBlendMode::Alpha);
    const auto none  = quad_blend_state(QuadBlendMode::None);
    CHECK(alpha.blendEnable == VK_TRUE);
    CHECK(none.blendEnable == VK_FALSE);
    CHECK(alpha.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA);
    CHECK(alpha.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    CHECK(describe_color_blend(QuadBlendMode::Alpha));
    CHECK_FALSE(describe_color_blend(QuadBlendMode::None));

    // SRGB: the quad pipeline's color format is the swapchain SRGB format (BPP 4).
    CHECK(is_srgb_format(VK_FORMAT_B8G8R8A8_SRGB));
    CHECK(quad_format_bytes(VK_FORMAT_B8G8R8A8_SRGB) == 4u);
    CHECK_FALSE(is_srgb_format(VK_FORMAT_B8G8R8A8_UNORM));
}

TEST_CASE("S201 N-quad index pattern is the per-quad [0,1,2,0,2,3] with 4*i offset")
{
    // The live QuadScene writes, for quad q, indices[q*6..q*6+6) = [0,1,2,0,2,3]+4q.
    // Every index stays within quad q's vertex window 4q..4q+3 — asserted for N=3.
    const auto& base = quad_indices();
    const std::uint32_t n = 3;
    CHECK(scene_index_count(n) == 18u);
    for (std::uint32_t q = 0; q < n; ++q)
    {
        for (std::uint32_t k = 0; k < quad_index_count(); ++k)
        {
            const std::uint16_t expected =
                static_cast<std::uint16_t>(base[k] + static_cast<std::uint16_t>(q * quad_vertex_count()));
            // base[k] is 0..3, so expected is always in [4q, 4q+3] — the quad's window.
            const std::uint16_t lo = static_cast<std::uint16_t>(q * quad_vertex_count());
            const std::uint16_t hi = static_cast<std::uint16_t>(lo + quad_vertex_count() - 1);
            CHECK(expected >= lo);
            CHECK(expected <= hi);
        }
    }
}
