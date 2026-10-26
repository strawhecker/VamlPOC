// S162 — VulkanInstance implementation.
//
// Splits the instance creation into the pure decision seams + the real
// creation path. The seams must remain free + purely testable (no loader, no
// runtime); create() consumes them to construct the VkInstance.

#include <vaml/vulkan/VulkanInstance.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

// The canonical validation layer name (LunarG — the layer the Vulkan loader
// ships with the runtime/GPU driver). The only candidate vaml-vulkan requests.
constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
constexpr const char* kDebugUtilsExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

// True iff `name` is present in the NUL-terminated name list `available`.
bool availableContains(std::span<const char* const> available, const char* name)
{
    for (const char* a : available)
    {
        if (a && std::strcmp(a, name) == 0)
            return true;
    }
    return false;
}

// True in a release build (NDEBUG defined) — the debug-only validation gate.
bool isReleaseBuild()
{
#ifdef NDEBUG
    return true;
#else
    return false;
#endif
}

} // namespace

namespace vaml::vulkan {

std::vector<const char*> required_instance_extensions(bool debug)
{
    // The base surface extensions vaml-vulkan needs for a platform surface.
    // This mirrors the deterministic, compile-time backend selection the build
    // makes (FindGLFW/FindVulkan force Win32 on Windows, X11 on Linux — S160's
    // contract) and is deliberately NOT a glfwGetRequiredInstanceExtensions
    // runtime call: that call requires a live/initialized GLFW and returns
    // empty headless, which would break the pure decision seam's headless
    // testability (S160 precedent). The surface factory (glfwCreateWindowSurface
    // in VulkanSurface.cpp) uses the same platform extensions, so the set is
    // consistent.
    // The platform surface extension names are macros that live in the
    // platform-CONDITIONAL Vulkan headers (vulkan_win32.h / vulkan_xlib.h),
    // only included when the matching VK_USE_PLATFORM_*_KHR is defined at
    // include time (it isn't here). Use them as string literals matching the
    // exact names GLFW enables for its backend so the enabled set is
    // consistent with glfwCreateWindowSurface (VulkanSurface.cpp).
    std::vector<const char*> exts;
    exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
    exts.push_back("VK_KHR_win32_surface");
#else
    exts.push_back("VK_KHR_xlib_surface");
#endif

    if (debug)
        exts.push_back(kDebugUtilsExtension);

    return exts;
}

std::vector<const char*>
choose_validation_layers(bool debugEnabled,
                         std::span<const char* const> candidates,
                         std::span<const char* const> available)
{
    std::vector<const char*> chosen;
    if (!debugEnabled)
        return chosen;  // structural debug-only gate
    for (const char* c : candidates)
    {
        if (c && availableContains(available, c))
            chosen.push_back(c);
    }
    return chosen;
}

std::unique_ptr<VulkanInstance>
VulkanInstance::create(const VulkanLoader& loader, Config config)
{
    // --- Debug gate: validation is hard-off unless enableValidation && !NDEBUG.
    const bool debug = config.enableValidation && !isReleaseBuild();

    // --- Enumerate AVAILABLE instance layers + extensions to filter against.
    //
    // S446 hardening: the VkLayerProperties/VkExtensionProperties name fields
    // (p.layerName/p.extensionName) are only valid for the lifetime of the
    // `props` vector that backs them. The previous code stored those char*
    // directly into the `available*` vectors, so when the enclosing block
    // scopes ended (destroying `props`), the later availableContains() checks
    // dereferenced DANGLING pointers — a heap-use-after-free the S446 ASan
    // lane (verify_s446_asan.cmd, msvc-asan under /fsanitize=address) caught.
    // We now OWN the names as copies that live for the whole create() call.
    std::vector<std::string> availableLayerNames;
    std::vector<const char*> availableLayers;
    {
        uint32_t count = 0;
        loader.vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        loader.vkEnumerateInstanceLayerProperties(&count, props.data());
        availableLayerNames.reserve(count);
        availableLayers.reserve(count);
        for (auto& p : props)
        {
            availableLayerNames.emplace_back(p.layerName);
            availableLayers.push_back(availableLayerNames.back().c_str());
        }
    }

    std::vector<std::string> availableExtensionNames;
    std::vector<const char*> availableExtensions;
    {
        uint32_t count = 0;
        loader.vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> props(count);
        loader.vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
        availableExtensionNames.reserve(count);
        availableExtensions.reserve(count);
        for (auto& p : props)
        {
            availableExtensionNames.emplace_back(p.extensionName);
            availableExtensions.push_back(availableExtensionNames.back().c_str());
        }
    }

    // --- Choose layers (debug-gated + availability-filtered).
    const char* kValidationCandidates[] = { kValidationLayerName };
    auto layers = choose_validation_layers(
        debug,
        std::span<const char* const>(kValidationCandidates, 1),
        availableLayers);

    // --- Choose extensions. The base surface extensions (VK_KHR_surface + the
    // GLFW-selected platform surface) are REQUIRED for a swapchain surface and
    // are carried through UNCONDITIONALLY (never silently dropped). The one
    // explicitly optional extension we request — VK_EXT_DEBUG_UTILS in debug —
    // is kept ONLY when available: the robust "no validation errors" posture
    // (the S167 messenger works whether or not the driver exposes it).
    auto requested = required_instance_extensions(debug);
    std::vector<const char*> extensions;
    extensions.reserve(requested.size());
    for (const char* e : requested)
    {
        const bool isOptionalDebugUtils =
            std::strcmp(e, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
        if (!isOptionalDebugUtils || availableContains(availableExtensions, e))
            extensions.push_back(e);
    }

    // --- vkCreateInstance.
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = config.appName ? config.appName : "VAML";
    appInfo.applicationVersion = config.version;
    appInfo.pEngineName        = config.engineName ? config.engineName : "vaml-vulkan";
    appInfo.engineVersion      = config.version;
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    VkInstance inst = VK_NULL_HANDLE;
    if (loader.vkCreateInstance(&ci, nullptr, &inst) != VK_SUCCESS)
        return nullptr;

    return std::unique_ptr<VulkanInstance>(
        new VulkanInstance(loader, inst, std::move(layers), std::move(extensions)));
}

VulkanInstance::VulkanInstance(const VulkanLoader& loader, VkInstance instance,
                               std::vector<const char*> layers,
                               std::vector<const char*> extensions)
    : loader_(&loader)
    , instance_(instance)
    , enabledLayers_(std::move(layers))
    , enabledExtensions_(std::move(extensions))
{
}

VulkanInstance::VulkanInstance(VulkanInstance&&) noexcept = default;
VulkanInstance& VulkanInstance::operator=(VulkanInstance&&) noexcept = default;

VulkanInstance::~VulkanInstance()
{
    if (instance_ && loader_)
        loader_->vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
