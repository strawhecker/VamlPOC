#pragma once
// S162 — runtime Vulkan loader for vaml-vulkan.
//
// Runtime-resolves the Vulkan entry points from the platform Vulkan loader
// instead of linking a Vulkan import library — a REAL, permanent on-load
// component (NOT a STUB / NOT MINIMAL REAL): it is the loader binding that
// every later sprint (S163 device selection, S164 swapchain, S165+ render,
// S169 RAII) consumes. The loader, instance, window, and every Vk handle it
// created MUST outlive all Vulkan objects made from them; callers own that
// lifetime contract (mirrors the original VamlPOC/VulkanLoader.h and the
// VamlPOC-linux baseline).
//
// Resolution model per OS:
//   * Windows — loads vulkan-1.dll at runtime (VK_NO_PROTOTYPES; repo's
//     standing SDK-less design, no import library).
//   * Linux — the loader library (libvulkan.so) is linked by the build via
//     VAML_VULKAN_LINKED_LOADER (FindVulkan.cmake, pkg-config `vulkan`, used
//     for the LOADER ONLY — never the API-header authority); the vk* symbols
//     are bound from that linked library via ::dlsym.
//
// S162 resolves the instance/surface entry points only — exactly the surface
// S162 needs. The device/queue entry points (vkCreateDevice, vkGetDeviceQueue,
// swapchain, etc.) are EXPANDED by later sprints (S163/S164/S165) as their
// surfaces are authored; this loader deliberately exposes only vkGetInstanceProcAddr
// / vkGetDeviceProcAddr as the general seam those sprints extend (NO-GAP:
// the expansion is scheduled, not a silent backfill).

#include <vaml/vulkan/Vulkan.h>

#include <functional>
#include <memory>
#include <string>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// The S162 instance/surface loader surface (see header comment). RAII + move-only:
// owns the platform loader module handle and binds the entry points once.
// `VulkanLoader` must OUTLIVE ever Vulkan handle created through it.
class VAML_VULKAN_EXPORT VulkanLoader
{
public:
    // Throws std::runtime_error if the platform Vulkan loader cannot be found
    // or an entry point cannot be resolved. An application should typically
    // construct one of these once and keep it for the process lifetime.
    VulkanLoader();

    ~VulkanLoader();
    VulkanLoader(const VulkanLoader&)            = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;
    VulkanLoader(VulkanLoader&&) noexcept;
    VulkanLoader& operator=(VulkanLoader&&) noexcept;

    // ---- S162 entry points (bound at load) ----
    PFN_vkGetInstanceProcAddr                vkGetInstanceProcAddr                = nullptr;
    PFN_vkGetDeviceProcAddr                  vkGetDeviceProcAddr                  = nullptr;
    PFN_vkCreateInstance                     vkCreateInstance                     = nullptr;
    PFN_vkDestroyInstance                    vkDestroyInstance                    = nullptr;
    PFN_vkEnumerateInstanceLayerProperties   vkEnumerateInstanceLayerProperties   = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
    PFN_vkEnumeratePhysicalDevices           vkEnumeratePhysicalDevices           = nullptr;
    PFN_vkGetPhysicalDeviceProperties        vkGetPhysicalDeviceProperties        = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties   = nullptr;
    PFN_vkDestroySurfaceKHR                  vkDestroySurfaceKHR                  = nullptr;

    // ---- S163 device entry points (bound at load) ----
    // S163 deletes the POC-local loader (the pre-S162 VamlPOC/VulkanLoader.h)
    // and migrates both POCs onto this library loader, so the device/queue
    // creation entries that loader used to own move HERE (the recorded
    // S162/roadmap "expanded by later sprints" resolution — the first sprint
    // to create a device on the chosen GPU). vkGetDeviceProcAddr remains the
    // general seam S164/S165 extend for swapchain/render entries.
    PFN_vkCreateDevice                       vkCreateDevice                       = nullptr;
    PFN_vkGetDeviceQueue                     vkGetDeviceQueue                     = nullptr;
    PFN_vkDestroyDevice                      vkDestroyDevice                      = nullptr;

    // True iff a device-context entry point was resolved (see header comment;
    // true once S163's device entries are bound at load).
    [[nodiscard]] bool has_device_loader() const noexcept { return vkCreateDevice != nullptr && vkGetDeviceQueue != nullptr; }

    // ---- S164 swapchain/image-view/depth entry points (bound at load) ----
    // The recorded S162/S163 "expanded by later sprints" continuation: S164 is
    // the first sprint to BUILD A SWAPCHAIN on the S163 device, so the surface-
    // capability queries, the swapchain KHR functions, the image-view functions
    // and the raw device-memory functions (the single depth image's memory)
    // all bind HERE. vkGetDeviceProcAddr remains the general seam S165 extends
    // for render-pass/pipeline entries. has_swapchain_loader() is the S164 gate.
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties       vkGetPhysicalDeviceFormatProperties       = nullptr;  // depth-format support probe
    PFN_vkCreateSwapchainKHR                      vkCreateSwapchainKHR                      = nullptr;
    PFN_vkDestroySwapchainKHR                     vkDestroySwapchainKHR                     = nullptr;
    PFN_vkGetSwapchainImagesKHR                   vkGetSwapchainImagesKHR                   = nullptr;
    PFN_vkCreateImageView                         vkCreateImageView                         = nullptr;
    PFN_vkDestroyImageView                        vkDestroyImageView                        = nullptr;
    PFN_vkCreateImage                             vkCreateImage                             = nullptr;
    PFN_vkDestroyImage                            vkDestroyImage                            = nullptr;
    PFN_vkGetImageMemoryRequirements              vkGetImageMemoryRequirements              = nullptr;
    PFN_vkAllocateMemory                          vkAllocateMemory                          = nullptr;
    PFN_vkFreeMemory                              vkFreeMemory                              = nullptr;
    PFN_vkBindImageMemory                         vkBindImageMemory                         = nullptr;

    // True iff the S164 swapchain/image-view/depth entry points are bound.
    [[nodiscard]] bool has_swapchain_loader() const noexcept
    {
        return vkCreateSwapchainKHR != nullptr && vkGetSwapchainImagesKHR != nullptr &&
               vkCreateImageView != nullptr;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml::vulkan
