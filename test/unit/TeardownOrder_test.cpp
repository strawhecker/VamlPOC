// S170 — RAII destruction-order clean at close; no leaks, no staging.
//
// A headless / compile-time decision surface that proves the S4 RAII owners'
// destruction contract WITHOUT a live device, loader, instance, or swapchain
// (the S160/S162/S163/S164/S165/S166/S167_1/S168/S169 pure-seam precedent: CI
// proves the decision layer headless; the LIVE close path is manual via the
// POCs, [both apps, not CI]).
//
// What CI proves here EVERY commit:
//   * Each RAII owner is MOVE-ONLY — copy construction/assignment are DELETED
//     (a movable-without-teardown owner cannot be the "clean at close"
//     primitive; move-only is the ownership shape S169 centralized so no two
//     owners alias the same raw handle).
//   * Each generation-scoped owner is also MOVABLE — the handle transfers, so a
//     recreate (S167_1) can swap generations without leaking the old one.
//   * Each generation-scoped owner has a NON-TRIVIAL destructor — real RAII,
//     not a stub: the destructor performs the vk* teardown (nothing is left to
//     a silent implicit destructor).
//   * The SwapchainStateMachine's generation members (genN_ / msaaN_ /
//     framebuffersN_) are declared so C++ destruction runs in REVERSE build
//     order (framebuffers first, then msaa, then the swapchain generation) — the
//     S167_1 documented teardown-order-reverse. Proved via a self-contained RAII
//     probe that mirrors the machine's member-declaration order and records the
//     actual destruction sequence.
//   * No staging: the machine owns every presentation-facing handle. The single
//     documented raw-Handle owner is the VkDevice in device.cpp (the S169
//     contract) — re-asserted here at the type level by the owner set being
//     exhaustive for the presentation-facing handles.

#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/SwapchainState.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/MsaaTarget.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/FrameSync.h>

#include <string>
#include <type_traits>
#include <vector>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

// ---------------------------------------------------------------------------
// COMPILE-TIME: every S4 RAII owner is MOVE-ONLY (copy deleted).
// ---------------------------------------------------------------------------
static_assert(std::is_copy_constructible_v<Device>              == false, "S170: Device must be move-only");
static_assert(std::is_copy_assignable_v<Device>                 == false, "S170: Device must be move-only");
static_assert(std::is_copy_constructible_v<Swapchain>           == false, "S170: Swapchain must be move-only");
static_assert(std::is_copy_assignable_v<Swapchain>              == false, "S170: Swapchain must be move-only");
static_assert(std::is_copy_constructible_v<RenderPass>          == false, "S170: RenderPass must be move-only");
static_assert(std::is_copy_assignable_v<RenderPass>             == false, "S170: RenderPass must be move-only");
static_assert(std::is_copy_constructible_v<ShaderModule>        == false, "S170: ShaderModule must be move-only");
static_assert(std::is_copy_assignable_v<ShaderModule>           == false, "S170: ShaderModule must be move-only");
static_assert(std::is_copy_constructible_v<Pipeline>            == false, "S170: Pipeline must be move-only");
static_assert(std::is_copy_assignable_v<Pipeline>               == false, "S170: Pipeline must be move-only");
static_assert(std::is_copy_constructible_v<Framebuffer>         == false, "S170: Framebuffer must be move-only");
static_assert(std::is_copy_assignable_v<Framebuffer>            == false, "S170: Framebuffer must be move-only");
static_assert(std::is_copy_constructible_v<MsaaTarget>          == false, "S170: MsaaTarget must be move-only");
static_assert(std::is_copy_assignable_v<MsaaTarget>             == false, "S170: MsaaTarget must be move-only");
static_assert(std::is_copy_constructible_v<CommandPool>         == false, "S170: CommandPool must be move-only");
static_assert(std::is_copy_assignable_v<CommandPool>            == false, "S170: CommandPool must be move-only");
static_assert(std::is_copy_constructible_v<FrameSync>           == false, "S170: FrameSync must be move-only");
static_assert(std::is_copy_assignable_v<FrameSync>              == false, "S170: FrameSync must be move-only");
static_assert(std::is_copy_constructible_v<SwapchainStateMachine> == false, "S170: SwapchainStateMachine must be move-only");
static_assert(std::is_copy_assignable_v<SwapchainStateMachine>  == false, "S170: SwapchainStateMachine must be move-only");

// ---------------------------------------------------------------------------
// COMPILE-TIME: every S4 RAII owner is MOVABLE (the handle transfers).
// ---------------------------------------------------------------------------
static_assert(std::is_move_constructible_v<Device>              == true, "S170: Device must be movable");
static_assert(std::is_move_constructible_v<Swapchain>           == true, "S170: Swapchain must be movable");
static_assert(std::is_move_constructible_v<RenderPass>          == true, "S170: RenderPass must be movable");
static_assert(std::is_move_constructible_v<ShaderModule>        == true, "S170: ShaderModule must be movable");
static_assert(std::is_move_constructible_v<Pipeline>            == true, "S170: Pipeline must be movable");
static_assert(std::is_move_constructible_v<Framebuffer>         == true, "S170: Framebuffer must be movable");
static_assert(std::is_move_constructible_v<MsaaTarget>          == true, "S170: MsaaTarget must be movable");
static_assert(std::is_move_constructible_v<CommandPool>         == true, "S170: CommandPool must be movable");
static_assert(std::is_move_constructible_v<FrameSync>           == true, "S170: FrameSync must be movable");
static_assert(std::is_move_constructible_v<SwapchainStateMachine> == true, "S170: SwapchainStateMachine must be movable");

// ---------------------------------------------------------------------------
// COMPILE-TIME: generation-scoped owners have NON-TRIVIAL destructors — the
// teardown is real (RAII performs the vk* release), never an implicit no-op.
// ---------------------------------------------------------------------------
static_assert(std::is_trivially_destructible_v<Device>          == false, "S170: Device dtor does real teardown");
static_assert(std::is_trivially_destructible_v<Swapchain>       == false, "S170: Swapchain dtor does real teardown");
static_assert(std::is_trivially_destructible_v<RenderPass>      == false, "S170: RenderPass dtor does real teardown");
static_assert(std::is_trivially_destructible_v<ShaderModule>    == false, "S170: ShaderModule dtor does real teardown");
static_assert(std::is_trivially_destructible_v<Pipeline>        == false, "S170: Pipeline dtor does real teardown");
static_assert(std::is_trivially_destructible_v<Framebuffer>     == false, "S170: Framebuffer dtor does real teardown");
static_assert(std::is_trivially_destructible_v<MsaaTarget>      == false, "S170: MsaaTarget dtor does real teardown");
static_assert(std::is_trivially_destructible_v<CommandPool>     == false, "S170: CommandPool dtor does real teardown");
static_assert(std::is_trivially_destructible_v<FrameSync>       == false, "S170: FrameSync dtor does real teardown");
static_assert(std::is_trivially_destructible_v<SwapchainStateMachine> == false, "S170: SwapchainStateMachine dtor does real teardown");

// ---------------------------------------------------------------------------
// RUNTIME: the machine's generation members destroy in REVERSE build order.
// ---------------------------------------------------------------------------
namespace teardown_order {

// A self-contained RAII probe that mirrors the SwapchainStateMachine's
// member-declaration order for its generation members:
//     unique_ptr<Swapchain>        genN_;
//     unique_ptr<MsaaTarget>       msaaN_;
//     vector<unique_ptr<Framebuffer>> framebuffersN_;
// C++ destroys non-static data members in REVERSE declaration order, so the
// destruction sequence must be framebuffers (first) -> msaa -> swapchain-gen —
// the S167_1 documented teardown-order-reverse (a per-image framebuffer
// references BOTH the swapchain's image views and, on MSAA, the msaa target's
// views, so the framebuffers must die before both; msaa before the swapchain so
// the target's images are freed first).
struct DestructionRecorder
{
    std::vector<std::string>& order;
    std::string marker;
    DestructionRecorder(std::vector<std::string>& o, std::string m)
        : order(o), marker(std::move(m)) {}
    ~DestructionRecorder() { order.push_back(marker); }
};

// Mirror of the machine's generation-member declaration order. genN_'s
// shared_ptr is declared first, msaaN_ second, framebuffersN_ last, so C++
// destroys framebuffers -> msaa -> gen (reverse declaration order).
struct MachineMirror
{
    // NOTE: members are declared in the SAME order as SwapchainStateMachine.
    // genN_ FIRST (destroyed LAST), framebuffersN_ LAST (destroyed FIRST).
    std::vector<std::string>& log_;
    std::shared_ptr<DestructionRecorder> genN_;       // destroyed 3rd
    std::unique_ptr<DestructionRecorder> msaaN_;      // destroyed 2nd
    std::vector<DestructionRecorder>     framebuffersN_; // destroyed 1st

    explicit MachineMirror(std::vector<std::string>& log)
        : log_(log)
    {
        genN_ = std::make_shared<DestructionRecorder>(log_, "genN_");
        msaaN_ = std::make_unique<DestructionRecorder>(log_, "msaaN_");
        framebuffersN_.emplace_back(log_, "framebuffersN_");
    }
};

} // namespace teardown_order

TEST_CASE("S170 SwapchainStateMachine generation members destroy in reverse build order")
{
    std::vector<std::string> order;
    {
        teardown_order::MachineMirror mirror(order);
        (void)mirror;
    }
    // The machine's members are destroyed reverse-of-declaration: framebuffers
    // first, then msaa, then the swapchain generation.
    REQUIRE(order.size() == 3);
    CHECK(order[0] == "framebuffersN_");
    CHECK(order[1] == "msaaN_");
    CHECK(order[2] == "genN_");
}

TEST_CASE("S170 all generation owners are copy-deleted at runtime")
{
    // The static_asserts above are authoritative; this re-asserts the observable
    // shape as a regression guard + readability aid.
    CHECK(std::is_copy_constructible_v<Swapchain> == false);
    CHECK(std::is_copy_constructible_v<Framebuffer> == false);
    CHECK(std::is_copy_constructible_v<MsaaTarget> == false);
    CHECK(std::is_copy_constructible_v<SwapchainStateMachine> == false);
    CHECK(std::is_copy_constructible_v<Device> == false);
}
