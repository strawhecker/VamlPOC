#pragma once
// VamlPOC (Windows): minimal Vulkan loader — runtime-resolves the Vulkan
// entry points from the platform loader (vulkan-1.dll) instead of linking an
// import library. This is the deliberate "vendor headers only / no SDK" choice.
//
// Mirrors the Linux app's GLFW+Vulkan path: we need the same small set of
// core instance/device/queue APIs. Because VK_NO_PROTOTYPES is defined before
// the Vulkan headers, the headers declare but do not define the functions; we
// load them at runtime here.
//
// Capability parity with VamlPOC-linux/src/main.cpp (keep in sync):
//   vkCreateInstance, vkEnumeratePhysicalDevices, vkGetPhysicalDeviceProperties,
//   vkGetPhysicalDeviceQueueFamilyProperties, vkCreateDevice, vkGetDeviceQueue,
//   vkDestroyDevice, vkDestroyInstance.

#define VK_NO_PROTOTYPES

// The video std headers must be included BEFORE vulkan_core.h: the generated
// Vulkan headers reference the StdVideo* types in some structs before their
// own #include lines, so we pre-include the codec std headers here to define
// them first. (These types are unused by the app; they only satisfy the core
// header's AV1/H264/H265 struct definitions.)
#include <vk_video/vulkan_video_codecs_common.h>
#include <vk_video/vulkan_video_codec_h264std.h>
#include <vk_video/vulkan_video_codec_h264std_encode.h>
#include <vk_video/vulkan_video_codec_h264std_decode.h>
#include <vk_video/vulkan_video_codec_h265std.h>
#include <vk_video/vulkan_video_codec_h265std_encode.h>
#include <vk_video/vulkan_video_codec_h265std_decode.h>
#include <vk_video/vulkan_video_codec_av1std.h>
#include <vk_video/vulkan_video_codec_av1std_encode.h>
#include <vk_video/vulkan_video_codec_av1std_decode.h>
#include <vk_video/vulkan_video_codec_vp9std.h>
#include <vk_video/vulkan_video_codec_vp9std_decode.h>

#include <vulkan/vulkan.h>

#include <windows.h>
#include <cstdio>
#include <stdexcept>

namespace vamlpoc {

// RAII wrapper around the Vulkan loader library (vulkan-1.dll).
class VulkanLoader
{
public:
    VulkanLoader()
    {
        module_ = ::LoadLibraryW(L"vulkan-1.dll");
        if (!module_)
            throw std::runtime_error(
                "vulkan-1.dll not found. Install a Vulkan runtime (e.g. the "
                "GPU vendor's driver or the LunarG runtime).");

        vkCreateInstance = resolve<PFN_vkCreateInstance>("vkCreateInstance");
        vkEnumeratePhysicalDevices =
            resolve<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        vkGetPhysicalDeviceProperties =
            resolve<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
        vkGetPhysicalDeviceQueueFamilyProperties =
            resolve<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                "vkGetPhysicalDeviceQueueFamilyProperties");
        vkCreateDevice = resolve<PFN_vkCreateDevice>("vkCreateDevice");
        vkGetDeviceQueue = resolve<PFN_vkGetDeviceQueue>("vkGetDeviceQueue");
        vkDestroyDevice = resolve<PFN_vkDestroyDevice>("vkDestroyDevice");
        vkDestroyInstance = resolve<PFN_vkDestroyInstance>("vkDestroyInstance");
    }

    ~VulkanLoader()
    {
        if (module_)
            ::FreeLibrary(module_);
    }

    VulkanLoader(const VulkanLoader&)            = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;

    PFN_vkCreateInstance                         vkCreateInstance              = nullptr;
    PFN_vkEnumeratePhysicalDevices               vkEnumeratePhysicalDevices    = nullptr;
    PFN_vkGetPhysicalDeviceProperties            vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkCreateDevice                           vkCreateDevice                = nullptr;
    PFN_vkGetDeviceQueue                         vkGetDeviceQueue              = nullptr;
    PFN_vkDestroyDevice                          vkDestroyDevice               = nullptr;
    PFN_vkDestroyInstance                        vkDestroyInstance             = nullptr;

private:
    template <typename T>
    T resolve(const char* name)
    {
        auto* fn = reinterpret_cast<T>(::GetProcAddress(module_, name));
        if (!fn)
            throw std::runtime_error(std::string("Missing Vulkan entry point: ") + name);
        return fn;
    }

    HMODULE module_ = nullptr;
};

} // namespace vamlpoc
