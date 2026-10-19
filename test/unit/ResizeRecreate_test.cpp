// S167 — resize-safe swapchain recreation: the headless decision seams.
//
// S167's LIVE path (vkCreateSwapchainKHR at a new size + rebuilding the
// app-owned framebuffers) is a real-GPU manual step ([both apps, not CI], the
// S164/S166 precedent). What CI proves here is the headless decision surface:
//   * should_recreate_swapchain(result) — the S167 trigger across every result
//     a resize/acquire/present pass can surface.
// The swapchain recreation renegotiation (choose_extent / negotiate_image_count
// at the new size) is already fully covered by the S164 Swapchain_test.cpp seams;
// no live Vulkan loader/device/surface is constructed here.

#include <vaml/vulkan/Swapchain.h>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for
// the TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S167 should_recreate_swapchain triggers ONLY on out-of-date/suboptimal")
{
    // The S167 resize triggers: out-of-date (most common during a window resize)
    // and suboptimal both mean "recreate at the new size".
    CHECK(should_recreate_swapchain(VK_ERROR_OUT_OF_DATE_KHR));
    CHECK(should_recreate_swapchain(VK_SUBOPTIMAL_KHR));
}

TEST_CASE("S167 should_recreate_swapchain is false on success + every other result")
{
    // A successful acquire/present means "keep drawing" — never recreate.
    CHECK_FALSE(should_recreate_swapchain(VK_SUCCESS));

    // Common non-recreate results must never spuriously trigger a recreate.
    CHECK_FALSE(should_recreate_swapchain(VK_NOT_READY));
    CHECK_FALSE(should_recreate_swapchain(VK_TIMEOUT));
    CHECK_FALSE(should_recreate_swapchain(VK_ERROR_DEVICE_LOST));
    CHECK_FALSE(should_recreate_swapchain(VK_ERROR_OUT_OF_HOST_MEMORY));
    CHECK_FALSE(should_recreate_swapchain(VK_ERROR_OUT_OF_DEVICE_MEMORY));
    CHECK_FALSE(should_recreate_swapchain(VK_ERROR_SURFACE_LOST_KHR));
    CHECK_FALSE(should_recreate_swapchain(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR));

    // A fabricated/unknown result value is also "keep drawing" (defensive).
    CHECK_FALSE(should_recreate_swapchain(static_cast<VkResult>(0xdead)));
}

TEST_CASE("S167 should_recreate_swapchain is a pure predicate (no state)")
{
    // Deliberately callable on transient values — the resize-loop caller can
    // branch on it directly, no retained state required.
    VkResult outOfDate = VK_ERROR_OUT_OF_DATE_KHR;
    CHECK(should_recreate_swapchain(outOfDate));
    outOfDate = VK_SUCCESS;
    CHECK_FALSE(should_recreate_swapchain(outOfDate));
}
