// S166 — command-buffer / sync / draw-record decision seams (headless).
//
// Tests S166's PURE decision seams with fabricated Vulkan values — NO live Vulkan
// runtime, loader, device, command pool, buffer, semaphore, or fence (the S160/S162/
// S163/S164/S165 precedent: CI proves the decision layer headless; the LIVE
// CommandPool::create / FrameSync::create / vkCmd* draw path are manual via the POCs,
// [both apps, not CI]).
//
// The HEADLINE proof: `draw_record_plan()` — the FIXED ordered step sequence a single
// frame records (bind pipeline -> viewport -> scissor -> push MVP -> begin pass ->
// draw 3 -> end pass) — is asserted verbatim WITHOUT a device, so the draw-loop
// contract is proven headless (the S165 tri_mvp precedent).

#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/FrameSync.h>
#include <vaml/vulkan/Framebuffer.h>

#include <cstdint>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S166 command-pool decision seams")
{
    const auto pool = describe_command_pool(1);
    CHECK(pool.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    CHECK(pool.queueFamilyIndex == 1u);
    // Per-frame re-record requires RESET; TRANSIENT signals short-lived buffers.
    CHECK((pool.flags & VK_COMMAND_POOL_CREATE_TRANSIENT_BIT) != 0);
    CHECK((pool.flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) != 0);
}

TEST_CASE("S166 command-buffer allocation/begin decision seams")
{
    const auto alloc = describe_command_buffer_alloc(reinterpret_cast<VkCommandPool>(0x1234), 4);
    CHECK(alloc.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CHECK(alloc.commandPool == reinterpret_cast<VkCommandPool>(0x1234));
    CHECK(alloc.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CHECK(alloc.commandBufferCount == 4u);

    const auto begin = describe_command_buffer_begin();
    CHECK(begin.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    CHECK((begin.flags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != 0);
    CHECK(begin.pInheritanceInfo == nullptr);
}

TEST_CASE("S166 clear value + render-pass begin decision seams")
{
    const float clear[4] = { 0.2f, 0.2f, 0.25f, 1.0f };
    const auto cv = describe_clear_value(clear);
    CHECK(cv.color.float32[0] == doctest::Approx(0.2f));
    CHECK(cv.color.float32[1] == doctest::Approx(0.2f));
    CHECK(cv.color.float32[2] == doctest::Approx(0.25f));
    CHECK(cv.color.float32[3] == doctest::Approx(1.0f));

    // Headerline color-only begin: color + no depth -> attachment count 1.
    const auto beginColorOnly = build_render_pass_begin(
        reinterpret_cast<VkRenderPass>(0x11), reinterpret_cast<VkFramebuffer>(0x22),
        VkExtent2D{ 800, 600 }, cv, /*hasDepth=*/false);
    CHECK(beginColorOnly.sType == VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    CHECK(beginColorOnly.renderPass == reinterpret_cast<VkRenderPass>(0x11));
    CHECK(beginColorOnly.framebuffer == reinterpret_cast<VkFramebuffer>(0x22));
    CHECK(beginColorOnly.renderArea.offset.x == 0);
    CHECK(beginColorOnly.renderArea.offset.y == 0);
    CHECK(beginColorOnly.renderArea.extent.width == 800u);
    CHECK(beginColorOnly.renderArea.extent.height == 600u);
    CHECK(beginColorOnly.clearValueCount == 1u);
    CHECK(beginColorOnly.pClearValues != nullptr);
    // The pClearValues buffer is the static one; its first entry is the color clear.
    CHECK(beginColorOnly.pClearValues[0].color.float32[0] == doctest::Approx(0.2f));

    // With depth: attachment count 2, and the depth clear is 1.0/0 (far plane).
    const auto beginDepth = build_render_pass_begin(
        reinterpret_cast<VkRenderPass>(0x33), reinterpret_cast<VkFramebuffer>(0x44),
        VkExtent2D{ 640, 480 }, cv, /*hasDepth=*/true);
    CHECK(beginDepth.clearValueCount == 2u);
    CHECK(beginDepth.pClearValues[0].color.float32[0] == doctest::Approx(0.2f));
    CHECK(beginDepth.pClearValues[1].depthStencil.depth == doctest::Approx(1.0f));
    CHECK(beginDepth.pClearValues[1].depthStencil.stencil == 0);
}

TEST_CASE("S166 draw-record plan is the exact fixed step sequence")
{
    const auto& plan = draw_record_plan();
    // The FIXED ordered sequence: bind -> viewport -> scissor -> push MVP -> begin
    // pass -> draw 3 -> end pass.
    CHECK(plan.size() == 7u);
    CHECK(plan[0] == RecordStep::BindPipeline);
    CHECK(plan[1] == RecordStep::SetViewport);
    CHECK(plan[2] == RecordStep::SetScissor);
    CHECK(plan[3] == RecordStep::PushMvp);
    CHECK(plan[4] == RecordStep::BeginRenderPass);
    CHECK(plan[5] == RecordStep::Draw);
    CHECK(plan[6] == RecordStep::EndRenderPass);
}

TEST_CASE("S166 semaphore + fence decision seams")
{
    const auto sem = describe_semaphore();
    CHECK(sem.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    CHECK(sem.flags == 0u);  // binary semaphore

    const auto signaled = describe_fence(true);
    CHECK(signaled.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    CHECK((signaled.flags & VK_FENCE_CREATE_SIGNALED_BIT) != 0);

    const auto unsignaled = describe_fence(false);
    CHECK((unsignaled.flags & VK_FENCE_CREATE_SIGNALED_BIT) == 0);
}

TEST_CASE("S166 concurrency = min(imageCount, cap), never zero")
{
    CHECK(choose_concurrency(0) == 1u);      // degenerate -> synchronous 1-frame
    CHECK(choose_concurrency(1) == 1u);
    CHECK(choose_concurrency(2) == 2u);
    CHECK(choose_concurrency(3) == 3u);
    CHECK(choose_concurrency(4) == kMaxConcurrentFrames);  // clamped (4 > 3)
    CHECK(choose_concurrency(100) == kMaxConcurrentFrames);
    CHECK(kMaxConcurrentFrames == 3u);
}
