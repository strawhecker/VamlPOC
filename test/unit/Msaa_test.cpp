// S168 — MSAA sample-count negotiation + resolve/multisample decision seams
// (headless).
//
// Tests S168's PURE decision seams with fabricated Vulkan values — NO live Vulkan
// runtime, loader, device, render pass, pipeline, framebuffer, or MsaaTarget (the
// S160/S162/S163/S164/S165/S166/S167_1 precedent: CI proves the decision layer
// headless; the LIVE RenderPass::create/Pipeline::create/Framebuffer::create_msaa/
// MsaaTarget::create paths are manual via the POCs, [both apps, not CI]).
//
// What CI proves here EVERY commit:
//   * choose_sample_count — the S168 sample-count NEGOTIATION: the largest
//     mutually-supported candidate wins; color-only/depth-only caps; disjoint
//     mask -> 1_BIT; empty mask -> 1_BIT.
//   * sample_count_candidates — the documented descending [8,4,2,1] preference.
//   * describe_color_resolve_attachment — the resolve attachment (1_sample,
//     DONT_CARE load / STORE store, present-final) that the swapchain image
//     carries in a multisampled pass.
//   * describe_multisample_state — the pipeline's rasterizationSamples honors the
//     pass's sample count (S168: no longer a hardcoded 1_BIT).
//   * framebuffer_attachment_count with a resolve — the S168 framebuffer layout
//     [color, resolve, depth?] (backward-compatible: hasResolve defaults false).
//   * build_render_pass_begin with a resolve — clearValueCount stays 1+hasDepth
//     (the S168 resolve is DONT_CARE-loaded, so it adds NO clear value).
//   * SwapchainStateBuilders — the S168 buildMsaa seam is a constructible,
//     replaceable value type (the S176 mock-ICD lane injects scripted fakes).

#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/SwapchainState.h>

#include <cstdint>
#include <memory>
#include <utility>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S168 sample_count_candidates descends [8,4,2,1]")
{
    const auto& c = sample_count_candidates();
    REQUIRE(c.size() == 4);
    CHECK(c[0] == VK_SAMPLE_COUNT_8_BIT);
    CHECK(c[1] == VK_SAMPLE_COUNT_4_BIT);
    CHECK(c[2] == VK_SAMPLE_COUNT_2_BIT);
    CHECK(c[3] == VK_SAMPLE_COUNT_1_BIT);
}

TEST_CASE("S168 choose_sample_count picks the largest mutually-supported count")
{
    // Highest candidate in both -> that candidate wins.
    const VkSampleCountFlags both4 =
        VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
    CHECK(choose_sample_count(both4, both4) == VK_SAMPLE_COUNT_4_BIT);

    // 8 supported in color but only 2 in depth -> 2 (largest COMMON).
    const VkSampleCountFlags color8 = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_8_BIT;
    const VkSampleCountFlags depth2 = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT;
    CHECK(choose_sample_count(color8, depth2) == VK_SAMPLE_COUNT_2_BIT);

    // 8 only in color, color also has 1 but depth only 1 -> 1.
    CHECK(choose_sample_count(VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_8_BIT,
                              VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
}

TEST_CASE("S168 choose_sample_count degrades to 1_BIT on cap / disjoint / empty")
{
    // color-only caps (depth has NOTHING): no common multisampled candidate -> 1_BIT.
    CHECK(choose_sample_count(VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT,
                              0) == VK_SAMPLE_COUNT_1_BIT);
    // depth-only caps (color has nothing) -> 1_BIT.
    CHECK(choose_sample_count(0, VK_SAMPLE_COUNT_8_BIT) == VK_SAMPLE_COUNT_1_BIT);
    // Disjoint multisampled masks (no common count at all) -> 1_BIT.
    CHECK(choose_sample_count(VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_2_BIT,
                              VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
    // Both empty -> 1_BIT (always valid — the S165 non-MSAA path).
    CHECK(choose_sample_count(0, 0) == VK_SAMPLE_COUNT_1_BIT);
}

TEST_CASE("S168 describe_color_resolve_attachment is the swapchain image target")
{
    const VkAttachmentDescription r = describe_color_resolve_attachment(VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(r.format == VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(r.samples == VK_SAMPLE_COUNT_1_BIT);        // the resolve target is 1-sample
    CHECK(r.loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE);  // the resolve overwrites it
    CHECK(r.storeOp == VK_ATTACHMENT_STORE_OP_STORE);    // the presented image
    CHECK(r.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

TEST_CASE("S168 describe_multisample_state honors the pass sample count")
{
    // S168: rasterizationSamples follows the pass (was a hardcoded 1_BIT in S165).
    const auto ms4 = describe_multisample_state(VK_SAMPLE_COUNT_4_BIT);
    CHECK(ms4.rasterizationSamples == VK_SAMPLE_COUNT_4_BIT);
    CHECK(ms4.sampleShadingEnable == VK_FALSE);  // no per-sample shading (edge AA only)

    // The S165 1-sample path is unchanged (byte-compatible).
    const auto ms1 = describe_multisample_state(VK_SAMPLE_COUNT_1_BIT);
    CHECK(ms1.rasterizationSamples == VK_SAMPLE_COUNT_1_BIT);
}

TEST_CASE("S168 framebuffer_attachment_count includes the resolve (backward-compatible)")
{
    // S165 behavior retained: no resolve -> colorCount + hasDepth.
    CHECK(framebuffer_attachment_count(0, false) == 0u);
    CHECK(framebuffer_attachment_count(3, false) == 3u);
    CHECK(framebuffer_attachment_count(3, true) == 4u);
    CHECK(framebuffer_attachment_count(1, true) == 2u);

    // S168: hasResolve adds one (the swapchain resolve view sits between color+depth).
    CHECK(framebuffer_attachment_count(1, false, true) == 2u);  // [color, resolve]
    CHECK(framebuffer_attachment_count(1, true,  true) == 3u);  // [color, resolve, depth]
}

TEST_CASE("S168 build_render_pass_begin resolve adds NO clear value")
{
    // clearValueCount stays 1+hasDepth — the S168 resolve attachment is DONT_CARE
    // loaded (the resolve overwrites it), so it contributes no clear.
    const float kClear[4] = { 0.2f, 0.2f, 0.25f, 1.0f };
    const VkClearValue clear = describe_clear_value(kClear);
    // Color-only + resolve: 1 clear (the color), not 2.
    const auto rpColor = build_render_pass_begin(VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                 VkExtent2D{ 800, 600 }, clear,
                                                 /*hasDepth=*/false, /*hasResolve=*/true);
    CHECK(rpColor.clearValueCount == 1u);
    // Color + depth + resolve: 2 clears (color + depth), not 3.
    const auto rpDepth = build_render_pass_begin(VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                 VkExtent2D{ 800, 600 }, clear,
                                                 /*hasDepth=*/true, /*hasResolve=*/true);
    CHECK(rpDepth.clearValueCount == 2u);

    // Backward-compat: hasResolve defaults false and never changes the S166 count.
    const auto rp166 = build_render_pass_begin(VK_NULL_HANDLE, VK_NULL_HANDLE,
                                               VkExtent2D{ 800, 600 }, clear, /*hasDepth=*/false);
    CHECK(rp166.clearValueCount == 1u);
}

TEST_CASE("S168 SwapchainStateBuilders build seam is replaceable")
{
    // The S176 generation-build seam (which consumes the S168 MSAA build inside
    // the single atomic generation) is a constructible, replaceable std::function —
    // default empty, populate to carry work. Proved at the decision level without
    // a live Vulkan device.
    SwapchainStateBuilders b;
    CHECK(b.build == nullptr);

    SwapchainStateBuilders b2;
    b2.build = [](std::uint32_t, std::uint32_t) -> std::unique_ptr<ISwapchainGeneration> {
        return nullptr;
    };
    CHECK(b2.build != nullptr);
}
