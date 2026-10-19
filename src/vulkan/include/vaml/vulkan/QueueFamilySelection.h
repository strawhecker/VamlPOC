#pragma once
// S162 — physical-device + queue-family selection (real, permanent).
//
// The "physical device + queue family selection" half of the S162 title. Splits
// into:
//   * PURE DECISION SEAM (headless-testable) — select_queue_family(...), the
//     S162 proof surface proven in ctest WITHOUT a live Vulkan runtime via a
//     fabricated VkQueueFamilyProperties array + a mock present-support lambda.
//   * PhysicalDeviceInfo + enumerate_physical_devices(...) — the real
//     enumeration surface S163 (best-GPU scoring) consumes.
//
// SCOPE (NO-GAP, recorded not silent): S162 owns FAMILY selection — choosing
// the graphics+present queue family for a GIVEN device. Best-GPU scoring/
// ranking (DISCRETE>INTEGRATED>OTHER, device-local memory, apiVersion
// tiebreak) and the VAML_PREFERRED_GPU override are S163's explicit step,
// which consumes enumerate_physical_devices() + select_queue_family().

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAM (headless-testable)
// ---------------------------------------------------------------------------

// Present-support predicate abstraction so select_queue_family can be tested
// without a live surface/physical device. Returns true iff the family at
// `familyIndex` presents to the target (a swapchain) surface.
using PresentSupportFn = std::function<bool(std::uint32_t familyIndex)>;

// Selects the queue family to create a graphics+present logical device from.
// Given an array of `familyProperties` (length `familyCount`) and a
// present-support predicate, returns the index of the first family with
// VK_QUEUE_GRAPHICS_BIT that ALSO supports presentation to the surface;
// falls back to the first GRAPHICS-ONLY family if none supports presentation;
// returns std::nullopt if there is no graphics family at all.
//
// Prefer a single family doing both (== a WPF-style graphics+present queue,
// and what the current single-thread renderer needs); the graphics-only
// fallback is the documented correctness posture when a device lacks a
// unified family. Also exposed: find_present_family(...) (present-capable
// index regardless of graphics) for S163's device scoring.
VAML_VULKAN_EXPORT std::optional<std::uint32_t>
select_queue_family(const VkQueueFamilyProperties* familyProperties,
                    std::uint32_t familyCount,
                    const PresentSupportFn& presentSupport);

// The index of the first family that supports presentation (regardless of
// graphics), or std::nullopt. Used by S163 to filter devices to
// present-capable ones. PresentSupport may be the "always-present" mock (any
// family is present-capable) when no surface is involved in scoring.
VAML_VULKAN_EXPORT std::optional<std::uint32_t>
find_present_family(std::uint32_t familyCount, const PresentSupportFn& presentSupport);

// ---------------------------------------------------------------------------
// Real physical-device enumeration surface (S163 consumes)
// ---------------------------------------------------------------------------

// A physical device + its static properties, as enumerated by S162. Real,
// permanent (NOT MINIMAL REAL / NOT a stub): S163 scores/ranks these.
struct PhysicalDeviceInfo
{
    VkPhysicalDevice          device   = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    std::string               name;       // properties.deviceName
    VkPhysicalDeviceType      type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    // Device-local memory in BYTES (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT heap
    // sum from vkGetPhysicalDeviceMemoryProperties). S163's best-GPU ranking
    // orders equal-type devices by device-local memory (descending). Populated
    // by enumerate_physical_devices; 0 if the loader's memory-properties entry
    // is unavailable (e.g. a pre-S163 loader).
    VkDeviceSize deviceLocalBytes = 0;
};

// Enumerates every physical device of `instance`, returning their static
// properties. Returns an empty vector if enumeration fails. Real surface:
// S163 (best-GPU scoring) filters + ranks these.
VAML_VULKAN_EXPORT std::vector<PhysicalDeviceInfo>
enumerate_physical_devices(const VulkanLoader& loader, const VulkanInstance& instance);

} // namespace vaml::vulkan
