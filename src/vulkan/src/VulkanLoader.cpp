// S162 — VulkanLoader implementation.
//
// Binds the S162 instance/surface entry points from the platform Vulkan loader.
// Windows: ::LoadLibraryW("vulkan-1.dll") + ::GetProcAddress. Linux: loads the
// linked libvulkan.so via ::dlopen + ::dlsym (VAML_VULKAN_LINKED_LOADER links
// the stable loader ABI; we still bind symbols at runtime via dlsym so a
// missing symbol fails with a named error rather than a link-time hard miss).

#include <vaml/vulkan/VulkanLoader.h>

#include <string>
#include <stdexcept>

#if defined(_WIN32)
#  define VAML_VK_LOADER_WIN32 1
#  include <windows.h>
#else
#  define VAML_VK_LOADER_POSIX 1
#  include <dlfcn.h>
#endif

namespace vaml::vulkan {

struct VulkanLoader::Impl
{
#if defined(VAML_VK_LOADER_WIN32)
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif

    ~Impl()
    {
#if defined(VAML_VK_LOADER_WIN32)
        if (module)
            ::FreeLibrary(module);
#else
        if (module)
            ::dlclose(module);
#endif
        module = nullptr;
    }
};

VulkanLoader::VulkanLoader()
    : impl_(std::make_unique<Impl>())
{
#if defined(VAML_VK_LOADER_WIN32)
    impl_->module = ::LoadLibraryW(L"vulkan-1.dll");
#else
    // Prefer the versioned soname first, then the unversioned dev link.
    impl_->module = ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!impl_->module)
        impl_->module = ::dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif
    if (!impl_->module)
        throw std::runtime_error(
            "Vulkan loader not found. Install a Vulkan runtime (GPU vendor "
            "driver or the LunarG runtime) / libvulkan-dev on Linux.");

    auto resolve = [this](const char* name) {
#if defined(VAML_VK_LOADER_WIN32)
        auto* fn = reinterpret_cast<void*>(::GetProcAddress(impl_->module, name));
#else
        void* fn = ::dlsym(impl_->module, name);
#endif
        if (!fn)
            throw std::runtime_error(std::string("Missing Vulkan entry point: ") + name);
        return fn;
    };

    vkGetInstanceProcAddr              = reinterpret_cast<PFN_vkGetInstanceProcAddr>(resolve("vkGetInstanceProcAddr"));
    vkGetDeviceProcAddr                = reinterpret_cast<PFN_vkGetDeviceProcAddr>(resolve("vkGetDeviceProcAddr"));
    vkCreateInstance                   = reinterpret_cast<PFN_vkCreateInstance>(resolve("vkCreateInstance"));
    vkDestroyInstance                  = reinterpret_cast<PFN_vkDestroyInstance>(resolve("vkDestroyInstance"));
    vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(resolve("vkEnumerateInstanceLayerProperties"));
    vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(resolve("vkEnumerateInstanceExtensionProperties"));
    vkEnumeratePhysicalDevices         = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(resolve("vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceProperties      = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(resolve("vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(resolve("vkGetPhysicalDeviceQueueFamilyProperties"));
    vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(resolve("vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(resolve("vkGetPhysicalDeviceMemoryProperties"));
    vkDestroySurfaceKHR                = reinterpret_cast<PFN_vkDestroySurfaceKHR>(resolve("vkDestroySurfaceKHR"));

    // ---- S163 device entry points ----
    vkCreateDevice   = reinterpret_cast<PFN_vkCreateDevice>(resolve("vkCreateDevice"));
    vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(resolve("vkGetDeviceQueue"));
    vkDestroyDevice  = reinterpret_cast<PFN_vkDestroyDevice>(resolve("vkDestroyDevice"));
}

VulkanLoader::VulkanLoader(VulkanLoader&&) noexcept = default;
VulkanLoader& VulkanLoader::operator=(VulkanLoader&&) noexcept = default;
VulkanLoader::~VulkanLoader() = default;

} // namespace vaml::vulkan
