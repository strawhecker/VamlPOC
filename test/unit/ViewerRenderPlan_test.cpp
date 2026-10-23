// S222 (dep:S221) — the "soup-to-nuts" render-plan seam (headless Rail 1).
//
// Proves the WIRING CONTRACT of the S222 viewer render host WITHOUT a live
// Vulkan runtime (the standing S160+ precedent), through the shared ViewerApp
// host's PURE DECISION SEAMS (vaml_viewer/app.{h,cpp}, compiled into the test
// target = the ADR-021 consumer path, single source of truth across both apps
// + the test):
//   (1) `viewer_stack_steps()` — the serialized S222 build ORDER proving the
//       LIFETIMES contract (each borrowed handle outlives the next owner; the
//       VamlRenderer is the LAST step = destroyed FIRST).
//   (2) `viewer_setup_ready(...)` — the create_device PRECONDITION gate that
//       recomposes the S173 `presenter_lifetimes_ok` + the S200
//       `quad::is_srgb_format` + the S206 non-null atlas view/sampler into ONE
//       front-door check.
//   (3) The ViewerApp is move-only RAII (the S170 discipline).
//
// The LIVE create()/run_loop() (the window + the Vulkan stack + create_device
// + the presented frame) is [both apps, not CI] — the DISPLAY proof is never a
// CI lane (the standing S160+/S212 precedent), verified live on this box.

#include "app.h"   // the S222 shared viewer render host

#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace vaml::viewer;

TEST_CASE("S222: viewer_stack_steps serializes the LIFETIMES build order (VamlRenderer last)")
{
    const auto& steps = viewer_stack_steps();
    REQUIRE(steps.size() >= 12u);

    // The S222 stack build order, each step's borrowed handles outliving the
    // next owner. The VamlRenderer ("Host") is the LAST step — the LAST built,
    // and therefore the FIRST destroyed (reverse of build = the LIFETIMES contract).
    CHECK(steps[0]  == "Loader");
    CHECK(steps[1]  == "Instance");
    CHECK(steps[2]  == "Surface");
    CHECK(steps[3]  == "Gpu");
    CHECK(steps[4]  == "Device");
    CHECK(steps[5]  == "RenderPass");
    CHECK(steps[6]  == "Pipelines");
    CHECK(steps[7]  == "Atlas");
    CHECK(steps[8]  == "Font");
    CHECK(steps[9]  == "Sdf");
    CHECK(steps[10] == "UiSceneSpec");
    CHECK(steps[11] == "Host");       // VamlRenderer::create_device — last-built, first-destroyed
}

TEST_CASE("S222: viewer_step_name is a readable bounded accessor")
{
    CHECK(std::string(viewer_step_name(0)) == "Loader");
    CHECK(std::string(viewer_step_name(11)) == "Host");
    CHECK(std::string(viewer_step_name(999)) == "(out of range)");
}

TEST_CASE("S222: viewer_setup_ready recomposes the S173/S200/S206 precondition gate")
{
    // A non-null render pass + a non-null atlas view/sampler sentinel for the
    // "ready" positive cases (raw handle values — no live runtime).
    const VkRenderPass rp   = reinterpret_cast<VkRenderPass>(static_cast<std::uintptr_t>(1));
    const VkImageView  view = reinterpret_cast<VkImageView>(static_cast<std::uintptr_t>(2));
    const VkSampler    smp  = reinterpret_cast<VkSampler>(static_cast<std::uintptr_t>(3));

    // (1) The S173 presenter-lifetime gate: swapchain + command loaders + a real
    // render pass. No swapchain/command loader -> NOT ready.
    CHECK_FALSE(viewer_setup_ready(false, true, rp, VK_FORMAT_B8G8R8A8_SRGB, view, smp));
    CHECK_FALSE(viewer_setup_ready(true, false, rp, VK_FORMAT_B8G8R8A8_SRGB, view, smp));
    // Null render pass -> NOT ready.
    CHECK_FALSE(viewer_setup_ready(true, true, VK_NULL_HANDLE, VK_FORMAT_B8G8R8A8_SRGB, view, smp));

    // (2) The S200 SRGB pairing: the swapchain color format MUST be SRGB.
    CHECK_FALSE(viewer_setup_ready(true, true, rp, VK_FORMAT_B8G8R8A8_UNORM, view, smp));

    // (3) The S206 atlas contract: the SDF atlas view + sampler must be non-null.
    CHECK_FALSE(viewer_setup_ready(true, true, rp, VK_FORMAT_B8G8R8A8_SRGB, VK_NULL_HANDLE, smp));
    CHECK_FALSE(viewer_setup_ready(true, true, rp, VK_FORMAT_B8G8R8A8_SRGB, view, VK_NULL_HANDLE));

    // All gates met -> READY.
    CHECK(viewer_setup_ready(true, true, rp, VK_FORMAT_B8G8R8A8_SRGB, view, smp));
}

TEST_CASE("S222: the S200 is_srgb_format pairing is honored by the gate")
{
    // The gate reuses quad::is_srgb_format — the S200 SRGB paired formats pass,
    // a non-SRGB color format fails (verified against the library's own seam).
    const VkRenderPass rp   = reinterpret_cast<VkRenderPass>(static_cast<std::uintptr_t>(1));
    const VkImageView  view = reinterpret_cast<VkImageView>(static_cast<std::uintptr_t>(2));
    const VkSampler    smp  = reinterpret_cast<VkSampler>(static_cast<std::uintptr_t>(3));
    CHECK(viewer_setup_ready(true, true, rp, VK_FORMAT_R8G8B8A8_SRGB, view, smp));
    CHECK_FALSE(viewer_setup_ready(true, true, rp, VK_FORMAT_R8G8B8A8_UNORM, view, smp));
}

TEST_CASE("S222: ViewerApp is move-only RAII (the S170 discipline)")
{
    static_assert(std::is_move_constructible<ViewerApp>::value,
                  "ViewerApp must be movable");
    static_assert(std::is_move_assignable<ViewerApp>::value,
                  "ViewerApp must be move-assignable");
    static_assert(!std::is_copy_constructible<ViewerApp>::value,
                  "ViewerApp must be non-copyable");
    static_assert(!std::is_copy_assignable<ViewerApp>::value,
                  "ViewerApp must be non-copy-assignable");
    CHECK(true);
}

TEST_CASE("S222: the auto-close deadline gate is pure + zero-guarded (viewer_deadline_elapsed)")
{
    // deadline 0 = NEVER (the no-auto-close default) — even well in the future.
    auto start = std::chrono::steady_clock::now();
    CHECK_FALSE(viewer_deadline_elapsed(start, 0));

    // A small positive deadline (e.g. 1 ms) fires after real time has elapsed.
    // The steady clock is monotonic + coarse-ish; sleeping briefly + re-checking
    // is deterministic enough for the gate contract (a 1ms deadline WILL have
    // passed after a 30ms sleep).
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(viewer_deadline_elapsed(start, 1));

    // A LARGE unelapsed deadline does NOT fire.
    auto now = std::chrono::steady_clock::now();
    CHECK_FALSE(viewer_deadline_elapsed(now, 60 * 60 * 1000ULL));  // 1 hour
}
