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

    // ---- S165 render-pass/pipeline/framebuffer entry points (bound at load) ----
    // The recorded S162/S163/S164 "expanded by later sprints" continuation: S165 is
    // the first sprint to BUILD A GRAPHICS PIPELINE on the S164 swapchain's image
    // views, so the render-pass, shader-module, pipeline-layout, graphics-pipeline
    // and framebuffer entry points all bind HERE. has_render_loader() gates the
    // S165 surface. (The vkGetDeviceProcAddr seam's S166 expansion for the
    // command-buffer/draw/sync/present entries is RESOLVED below — the recorded
    // NO-GAP hand-off is closed at S166, not silent.)
    PFN_vkCreateRenderPass                          vkCreateRenderPass                          = nullptr;
    PFN_vkDestroyRenderPass                         vkDestroyRenderPass                         = nullptr;
    PFN_vkCreateShaderModule                        vkCreateShaderModule                        = nullptr;
    PFN_vkDestroyShaderModule                       vkDestroyShaderModule                       = nullptr;
    PFN_vkCreatePipelineLayout                      vkCreatePipelineLayout                      = nullptr;
    PFN_vkDestroyPipelineLayout                     vkDestroyPipelineLayout                     = nullptr;
    PFN_vkCreateGraphicsPipelines                   vkCreateGraphicsPipelines                   = nullptr;
    PFN_vkDestroyPipeline                           vkDestroyPipeline                           = nullptr;
    PFN_vkCreateFramebuffer                         vkCreateFramebuffer                         = nullptr;
    PFN_vkDestroyFramebuffer                        vkDestroyFramebuffer                        = nullptr;

    // True iff the S165 render-pass/pipeline/framebuffer entry points are bound.
    [[nodiscard]] bool has_render_loader() const noexcept
    {
        return vkCreateRenderPass != nullptr && vkCreatePipelineLayout != nullptr &&
               vkCreateGraphicsPipelines != nullptr && vkCreateFramebuffer != nullptr;
    }

    // ---- S166 command-buffer/sync/present entry points (bound at load) ----
    // The recorded S162/S163/S164/S165 "expanded by later sprints" continuation: S166 is
    // the first sprint to RECORD + SUBMIT + PRESENT a frame (the main-thread draw loop
    // that clears the framebuffer + draws the S165 triangle), so the command-pool /
    // command-buffer / command-recording entries, the semaphore/fence sync entries, and
    // the acquire/submit/present entries all bind HERE. vkGetDeviceProcAddr remains the
    // general seam S167+ extends for resize/recreation entries. has_command_loader()
    // gates the S166 surface.
    // --- command pool + command buffers ---
    PFN_vkCreateCommandPool        vkCreateCommandPool        = nullptr;
    PFN_vkDestroyCommandPool       vkDestroyCommandPool       = nullptr;
    PFN_vkAllocateCommandBuffers   vkAllocateCommandBuffers   = nullptr;
    PFN_vkFreeCommandBuffers       vkFreeCommandBuffers       = nullptr;
    PFN_vkResetCommandBuffer       vkResetCommandBuffer       = nullptr;
    PFN_vkBeginCommandBuffer       vkBeginCommandBuffer       = nullptr;
    PFN_vkEndCommandBuffer         vkEndCommandBuffer         = nullptr;
    // --- command recording (draw loop body) ---
    PFN_vkCmdBeginRenderPass       vkCmdBeginRenderPass       = nullptr;
    PFN_vkCmdEndRenderPass         vkCmdEndRenderPass         = nullptr;
    PFN_vkCmdSetViewport           vkCmdSetViewport           = nullptr;
    PFN_vkCmdSetScissor            vkCmdSetScissor            = nullptr;
    PFN_vkCmdBindPipeline          vkCmdBindPipeline          = nullptr;
    PFN_vkCmdPushConstants         vkCmdPushConstants         = nullptr;
    PFN_vkCmdDraw                  vkCmdDraw                  = nullptr;
    // --- semaphores + fences (sync) ---
    PFN_vkCreateSemaphore          vkCreateSemaphore          = nullptr;
    PFN_vkDestroySemaphore         vkDestroySemaphore         = nullptr;
    PFN_vkCreateFence              vkCreateFence              = nullptr;
    PFN_vkDestroyFence             vkDestroyFence             = nullptr;
    PFN_vkWaitForFences            vkWaitForFences            = nullptr;
    PFN_vkResetFences              vkResetFences              = nullptr;
    // S209 — the fence STATUS query (the retained-buffer lifetime driver): the
    // RetainedBufferPool asks "has this in-flight fence signaled?" to decide when a
    // retired generation is safe to free (no UAF under the 3-in-flight window).
    PFN_vkGetFenceStatus           vkGetFenceStatus           = nullptr;
    // --- acquire / submit / present / wait ---
    PFN_vkAcquireNextImageKHR      vkAcquireNextImageKHR      = nullptr;
    PFN_vkQueueSubmit              vkQueueSubmit              = nullptr;
    PFN_vkQueuePresentKHR          vkQueuePresentKHR          = nullptr;
    PFN_vkQueueWaitIdle            vkQueueWaitIdle            = nullptr;
    PFN_vkDeviceWaitIdle           vkDeviceWaitIdle           = nullptr;

    // True iff the S166 command/sync/present entry points are bound (the draw loop
    // requires the command-pool + acquire + submit + present seam to be live).
    [[nodiscard]] bool has_command_loader() const noexcept
    {
        return vkCreateCommandPool != nullptr && vkAllocateCommandBuffers != nullptr &&
               vkAcquireNextImageKHR != nullptr && vkQueueSubmit != nullptr &&
               vkQueuePresentKHR != nullptr;
    }

    // ---- S200 vertex/index-buffer + sampler + descriptor + copy entry points ----
    // The recorded S162..S166 "expanded by later sprints" continuation: S200 is
    // the first sprint to build VERTEX/INDEX BUFFERS (quad geometry), a SAMPLER
    // + a combined image sampler descriptor set (the S200-local minimal textured
    // quad), an INDEXED DRAW (vkCmdDrawIndexed — N quads = 6N indices), and a
    // CPU→GPU copy (staging the quad mesh + the procedural texture). These were
    // deliberately left unbound by the S166 draw loop (which used vkCmdDraw(3)
    // with no buffer); S200 binds them here. vkGetDeviceProcAddr remains the
    // general seam later sprints extend. has_quad_loader() gates the S200 vertex/
    // index/draw surface; has_texture_loader() gates the S200 sampler/descriptor/
    // copy/pipeline-barrier surface.
    // --- vertex/index buffers + memory ---
    PFN_vkCreateBuffer                 vkCreateBuffer                 = nullptr;
    PFN_vkDestroyBuffer                vkDestroyBuffer                = nullptr;
    PFN_vkGetBufferMemoryRequirements  vkGetBufferMemoryRequirements  = nullptr;
    PFN_vkBindBufferMemory             vkBindBufferMemory             = nullptr;
    PFN_vkMapMemory                    vkMapMemory                    = nullptr;
    PFN_vkUnmapMemory                  vkUnmapMemory                  = nullptr;
    PFN_vkFlushMappedMemoryRanges      vkFlushMappedMemoryRanges      = nullptr;
    // --- indexed quad draw + descriptor binding ---
    PFN_vkCmdBindVertexBuffers         vkCmdBindVertexBuffers         = nullptr;
    PFN_vkCmdBindIndexBuffer           vkCmdBindIndexBuffer           = nullptr;
    PFN_vkCmdDrawIndexed               vkCmdDrawIndexed               = nullptr;
    PFN_vkCmdBindDescriptorSets        vkCmdBindDescriptorSets        = nullptr;
    // --- sampler + descriptor set (the S200-local combined image sampler) ---
    PFN_vkCreateSampler                vkCreateSampler                = nullptr;
    PFN_vkDestroySampler               vkDestroySampler               = nullptr;
    PFN_vkCreateDescriptorSetLayout    vkCreateDescriptorSetLayout    = nullptr;
    PFN_vkDestroyDescriptorSetLayout   vkDestroyDescriptorSetLayout   = nullptr;
    PFN_vkAllocateDescriptorSets       vkAllocateDescriptorSets       = nullptr;
    PFN_vkUpdateDescriptorSets         vkUpdateDescriptorSets         = nullptr;
    PFN_vkCreateDescriptorPool         vkCreateDescriptorPool         = nullptr;
    PFN_vkDestroyDescriptorPool        vkDestroyDescriptorPool        = nullptr;
    // --- CPU→GPU copy (staging the quad mesh + the procedural texture) ---
    PFN_vkCmdPipelineBarrier           vkCmdPipelineBarrier           = nullptr;
    PFN_vkCmdCopyBuffer                vkCmdCopyBuffer                = nullptr;
    PFN_vkCmdCopyBufferToImage         vkCmdCopyBufferToImage         = nullptr;

    // True iff the S200 vertex/index-buffer + indexed-draw entry points are bound.
    [[nodiscard]] bool has_quad_loader() const noexcept
    {
        return vkCreateBuffer != nullptr && vkGetBufferMemoryRequirements != nullptr &&
               vkBindBufferMemory != nullptr && vkCmdBindVertexBuffers != nullptr &&
               vkCmdBindIndexBuffer != nullptr && vkCmdDrawIndexed != nullptr;
    }

    // True iff the S200 sampler/descriptor/copy/pipeline-barrier entry points are bound.
    [[nodiscard]] bool has_texture_loader() const noexcept
    {
        return vkCreateSampler != nullptr && vkCreateDescriptorPool != nullptr &&
               vkCreateDescriptorSetLayout != nullptr && vkCmdCopyBuffer != nullptr &&
               vkCmdPipelineBarrier != nullptr;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml::vulkan
