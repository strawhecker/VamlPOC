#pragma once
// S162 — shared Vulkan prelude for vaml-vulkan.
//
// Every vaml-vulkan translation unit that needs the Vulkan types must include
// this header FIRST, before any <vulkan/vulkan.h> or <GLFW/glfw3.h> (with
// GLFW_INCLUDE_VULKAN) include. This vendored vulkan-headers version's
// vulkan_core.h references the StdVideo* types (used by the AV1/H264/H265
// codec structs) BEFORE their own #include lines, so those vk_video std
// headers MUST be pre-included first — the exact ordering documented by
// VmaImpl.cpp and the original VamlPOC/VulkanLoader.h.
//
// VK_NO_PROTOTYPES is defined so vaml-vulkan never links a Vulkan import
// library on Windows (the repo's standing SDK-less design): the headers
// declare the PFN_* function-pointer types but no vk* prototypes; the runtime
// loader (VulkanLoader) binds them from the platform loader at runtime. On
// Linux the loader (libvulkan.so) is linked via VAML_VULKAN_LINKED_LOADER and
// VulkanLoader.cpp binds the symbols directly.

#define VK_NO_PROTOTYPES

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
