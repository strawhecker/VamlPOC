#pragma once
// S162 — Vulkan window surface (real, permanent).
//
// A real VkSurfaceKHR owner for a PlatformWindow, created cross-platform via
// glfwCreateWindowSurface (Win32/X11/Wayland from the one vendored GLFW tree —
// matching PlatformWindow's Win32+X11 contract and reusing GLFW's own surface
// extension selection). RAII: destroys the surface on destruction.
//
// Surface present-capability is consumed NOW at S162's queue-family selection
// (select_queue_family asks the physical device whether its family supports
// the surface), and the surface itself is consumed by S164 (swapchain).
//
// LIFETIMES: the VulkanInstance AND the PlatformWindow must OUTLIVE the
// VulkanSurface; the VulkanSurface must outlive the S164 swapchain + image
// views. Destroy order: swapchain -> surface -> instance -> loader -> window.
// This is documented (not hidden) — callers own the ordering.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanInstance.h>

#include <memory>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

class PlatformWindow;

// The "surface" half of the S162 title — see header comment.
class VAML_VULKAN_EXPORT VulkanSurface
{
public:
    // Creates a VkSurfaceKHR for `window` through `instance`. Returns nullptr
    // (no throw) on failure, mirroring PlatformWindow/VulkanInstance::create.
    // `instance` and `window` must outlive the returned object.
    [[nodiscard]] static std::unique_ptr<VulkanSurface>
    create(const VulkanLoader& loader, VulkanInstance& instance, PlatformWindow& window);

    ~VulkanSurface();
    VulkanSurface(VulkanSurface&&) noexcept;
    VulkanSurface& operator=(VulkanSurface&&) noexcept;
    VulkanSurface(const VulkanSurface&)            = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    [[nodiscard]] VkSurfaceKHR native_handle() const noexcept { return surface_; }
    [[nodiscard]] VkSurfaceKHR surface()        const noexcept { return surface_; }

private:
    VulkanSurface(const VulkanLoader& loader, VkInstance instance, VkSurfaceKHR surface);

    const VulkanLoader* loader_ = nullptr;
    VkInstance instance_        = VK_NULL_HANDLE;
    VkSurfaceKHR surface_       = VK_NULL_HANDLE;
};

} // namespace vaml::vulkan
