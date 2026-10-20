// S169 — Device decision seams (headless).
//
// Tests S169's PURE decision seams with fabricated Vulkan values — NO live Vulkan
// runtime, loader, physical device, or logical device (the S160/S162/S163/S164/
// S165/S166/S167_1/S168 precedent: CI proves the decision layer headless; the LIVE
// Device::create() — the vkCreateDevice + vkGetDeviceQueue path — is manual via the
// POCs, [both apps, not CI]).
//
// What CI proves here EVERY commit (the S169 proof surface):
//   * describe_device_queue — the single graphics queue create info: family index,
//     count 1, priority pass-through, sType.
//   * device_extensions — exactly { VK_KHR_SWAPCHAIN_EXTENSION_NAME } when the
//     swapchain is enabled; empty when disabled (a headless device).
//   * build_device_create_info — wires the queue-info ptr + the extension ptr array
//     into the device create info (including the empty-extension case).

#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/Vulkan.h>

#include <cstdint>
#include <cstring>
#include <memory>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S169 describe_device_queue is a single graphics queue at the family")
{
    // describe_device_queue stores pQueuePriorities POINTING INTO the priority
    // argument, so the caller must pass a named lvalue that outlives the struct
    // (the real Device::create holds `queuePriority` across the call).
    const float priority = 0.75f;
    const auto  q        = describe_device_queue(/*queueFamily=*/3, priority);
    CHECK(q.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    CHECK(q.queueFamilyIndex == 3u);
    CHECK(q.queueCount == 1u);
    REQUIRE(q.pQueuePriorities != nullptr);
    CHECK(*q.pQueuePriorities == doctest::Approx(0.75f));
}

TEST_CASE("S169 device_extensions enables exactly the swapchain extension")
{
    const auto on = device_extensions(/*useSwapchain=*/true);
    REQUIRE(on.size() == 1u);
    CHECK(std::strcmp(on[0], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0);

    // A headless device (no swapchain) requests no device extensions.
    const auto off = device_extensions(/*useSwapchain=*/false);
    CHECK(off.empty());
}

TEST_CASE("S169 build_device_create_info wires queue + extension arrays")
{
    const auto queueInfo = describe_device_queue(2, 1.0f);
    const auto extensions = device_extensions(/*useSwapchain=*/true);

    const auto info = build_device_create_info(queueInfo, extensions);
    CHECK(info.sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    CHECK(info.queueCreateInfoCount == 1u);
    REQUIRE(info.pQueueCreateInfos != nullptr);
    CHECK(info.pQueueCreateInfos->queueFamilyIndex == 2u);
    CHECK(info.enabledExtensionCount == 1u);
    REQUIRE(info.ppEnabledExtensionNames != nullptr);
    CHECK(std::strcmp(info.ppEnabledExtensionNames[0], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0);
}

TEST_CASE("S169 build_device_create_info handles an empty extension list")
{
    const auto queueInfo = describe_device_queue(0, 1.0f);
    const auto extensions = device_extensions(/*useSwapchain=*/false);

    const auto info = build_device_create_info(queueInfo, extensions);
    CHECK(info.enabledExtensionCount == 0u);
    CHECK(info.ppEnabledExtensionNames == nullptr);
}
