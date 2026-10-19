// S162 — VulkanSurface implementation.
//
// Cross-platform surface via glfwCreateWindowSurface: GLFW returns the
// platform surface (Win32/X11/...) whose associated instance extensions
// required_instance_extensions() requested, so the instance already enabled
// them. Destroying a VkSurfaceKHR requires the creating VkInstance
// (vkDestroySurfaceKHR(instance, surface, ...)), so the surface holds a
// reference to the loader AND the instance.

#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/PlatformWindow.h>

// GLFW's surface factory needs the Vulkan headers (this file includes the
// S162 prelude via VulkanLoader.h, so vulkan.h is already visible) + the
// native window accessors.
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace vaml::vulkan {

std::unique_ptr<VulkanSurface>
VulkanSurface::create(const VulkanLoader& loader, VulkanInstance& instance, PlatformWindow& window)
{
    auto* glfwWindow = static_cast<GLFWwindow*>(window.native_handle());
    if (!glfwWindow)
        return nullptr;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance.instance(), glfwWindow, nullptr, &surface) != VK_SUCCESS)
        return nullptr;

    return std::unique_ptr<VulkanSurface>(new VulkanSurface(loader, instance.instance(), surface));
}

VulkanSurface::VulkanSurface(const VulkanLoader& loader, VkInstance inst, VkSurfaceKHR surface)
    : loader_(&loader)
    , instance_(inst)
    , surface_(surface)
{
}

VulkanSurface::VulkanSurface(VulkanSurface&&) noexcept = default;
VulkanSurface& VulkanSurface::operator=(VulkanSurface&&) noexcept = default;

VulkanSurface::~VulkanSurface()
{
    if (surface_ && instance_ && loader_)
        loader_->vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
