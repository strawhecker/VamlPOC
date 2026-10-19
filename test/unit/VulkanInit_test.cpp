// S162 — Vulkan instance decision seams (headless-safe).
//
// Tests the S162 instance DECISION SURFACE WITHOUT a live Vulkan runtime,
// display, or loader — the standing S160 precedent: CI proves the decision
// layer headless; the LIVE instance creation (vkCreateInstance actually
// firing) is proven manually via the POCs, NOT in ctest.
//
// This TU tests the two PURE seams only:
//   * required_instance_extensions(bool debug)
//   * choose_validation_layers(bool debugEnabled, candidates, available)
// No VkInstance, VulkanInstance, VulkanLoader, PlatformWindow or surface is
// constructed here — the vulkan.lib symbols ARE linked (vaml-vulkan) so this
// also proves the S162 public surface LINKS from an external consumer.
//
// Cases:
//   1. choose_validation_layers: debug-off -> empty (the structural gate).
//   2. debug-on + available -> the requested candidates.
//   3. unavailable candidate dropped (never an error).
//   4. debug-on but empty available/candidates -> still empty (never fabricates).
//   5. required_instance_extensions: debug=false base surface set has no
//      debug-utils; debug=true appends VK_EXT_DEBUG_UTILS, base preserved.
//   6. requirements never empty / always include the mandatory surface set.

#include <vaml/vulkan/VulkanInstance.h>

#include <cstddef>
#include <span>
#include <string>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for
// the TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S162 choose_validation_layers empty when debug disabled (gate)")
{
    const char* kCandidates[] = { "VK_LAYER_KHRONOS_validation" };
    const char* kAvailable[]  = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_api_dump" };

    auto layers = choose_validation_layers(
        /*debugEnabled=*/false,
        std::span<const char* const>(kCandidates, 1),
        std::span<const char* const>(kAvailable, 2));

    CHECK(layers.empty());  // release/disabled NEVER enables validation
}

TEST_CASE("S162 choose_validation_layers enables requested when debug + available")
{
    const char* kCandidates[] = { "VK_LAYER_KHRONOS_validation" };
    const char* kAvailable[]  = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_api_dump" };

    auto layers = choose_validation_layers(
        /*debugEnabled=*/true,
        std::span<const char* const>(kCandidates, 1),
        std::span<const char* const>(kAvailable, 2));

    REQUIRE(layers.size() == 1);
    CHECK(std::string(layers[0]) == "VK_LAYER_KHRONOS_validation");
}

TEST_CASE("S162 choose_validation_layers drops unavailable candidate (never fails)")
{
    const char* kCandidates[] = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_SOME_MISSING" };
    const char* kAvailable[]  = { "VK_LAYER_KHRONOS_validation" };

    auto layers = choose_validation_layers(
        /*debugEnabled=*/true,
        std::span<const char* const>(kCandidates, 2),
        std::span<const char* const>(kAvailable, 1));

    REQUIRE(layers.size() == 1);
    CHECK(std::string(layers[0]) == "VK_LAYER_KHRONOS_validation");  // missing dropped
}

TEST_CASE("S162 choose_validation_layers never fabricates when nothing available")
{
    const char* kCandidates[] = { "VK_LAYER_KHRONOS_validation" };

    auto layers = choose_validation_layers(
        /*debugEnabled=*/true,
        std::span<const char* const>(kCandidates, 1),
        std::span<const char* const>());

    CHECK(layers.empty());  // no validation layer present -> enable none
}

TEST_CASE("S162 required_instance_extensions adds debug-utils only in debug")
{
    auto base = required_instance_extensions(/*debug=*/false);
    auto debug = required_instance_extensions(/*debug=*/true);

    // Debug set = base + exactly the debug-utils extension appended.
    REQUIRE(debug.size() == base.size() + 1u);
    CHECK(std::string(debug.back()) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    for (std::size_t i = 0; i < base.size(); ++i)
        CHECK(std::string(debug[i]) == std::string(base[i]));
}

TEST_CASE("S162 required_instance_extensions always carries the surface extension")
{
    auto base = required_instance_extensions(/*debug=*/false);

    // The base surface set (VK_KHR_surface + a platform surface) must always
    // be requested, debug or not — a surface is mandatory for the swapchain.
    bool hasSurface = false;
    for (const char* e : base)
        if (std::string(e) == VK_KHR_SURFACE_EXTENSION_NAME)
            hasSurface = true;

    CHECK(hasSurface);
    CHECK(!base.empty());
}

TEST_CASE("S162 required_instance_extensions never leaks debug-utils in release")
{
    auto base = required_instance_extensions(/*debug=*/false);
    for (const char* e : base)
        CHECK(std::string(e) != VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}
