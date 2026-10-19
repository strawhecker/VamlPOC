// S161: single TU that instantiates VulkanMemoryAllocator's implementation.
//
// VMA is a single-header library; exactly ONE TU in the library must define
// VMA_IMPLEMENTATION before including it, so the vma functions are compiled
// into vaml-vulkan (not the consumer). Real, permanent — S163+ (device memory)
// and S202 (image upload / staging→device-local) consume the allocator.
//
// HEADER ORDER: this vendored vulkan-headers version's vulkan_core.h references
// the StdVideo* types (used by AV1/H264/H265 codec structs) before their own
// #include lines, so those vk_video std headers MUST be pre-included first (the
// same ordering VamlPOC/VulkanLoader.h documents). VMA then includes vulkan.h
// and sees them. The types are unused by VMA; they only satisfy the core
// header's struct definitions.
//
// LOADER: the runtime loader is resolved by the caller (S162) at device-creation
// time via VmaVulkanFunctions — no global vk* import is needed in this TU, so
// VMA_STATIC_VULKAN_FUNCTIONS and VMA_DYNAMIC_VULKAN_FUNCTIONS are kept 0.

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

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0   // no global vk* import — caller injects
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#include <vk_mem_alloc.h>
