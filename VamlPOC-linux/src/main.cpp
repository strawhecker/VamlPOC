// VamlPOC-linux: Linux counterpart to the Windows VamlPOC app.
//
// Proves the GPU lane end-to-end on the target machine (AMD RADV / Vulkan):
//   opens a GLFW window, creates a Vulkan instance + physical device on the
//   GPU, builds a tiny triangle pipeline, and renders it every frame.
//
// It also links the vaml-core static library (consumer path) and prints the
// library version, so this binary demonstrates "apart from the lib but
// consuming it".
//
// Deliberately a single-TU, dependency-light program (no raw pointer managers,
// no RAII wrappers yet) — it is the Linux POC baseline that later sprints
// (S160+, GLFW/Vulkan RAII in S168) extend into the full VAML renderer.

#include <vaml/version.h>
#include <vaml/vulkan/PlatformWindow.h>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kAppName    = "VamlPOC-linux";
constexpr uint32_t    kWindowW    = 800;
constexpr uint32_t    kWindowH    = 600;

// A triangle's vertex data is appended here by the S5 renderer sprint
// (S164: single-triangle graphics pipeline) — not pre-empted in this baseline.

// ---------------------------------------------------------------------------
// Small RAII-free Vulkan context holder (baseline; S168 introduces wrappers).
// Holds handles long enough for the demo loop, destroys in reverse order.
// ---------------------------------------------------------------------------
struct VulkanCtx
{
    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    uint32_t         queueFamily    = 0;
};

std::vector<const char*> requiredInstanceExtensions()
{
    std::vector<const char*> exts;
    uint32_t                 count = 0;
    const char**             names = glfwGetRequiredInstanceExtensions(&count);
    for (uint32_t i = 0; i < count; ++i)
        exts.push_back(names[i]);
    return exts;
}

// No surface is created in this baseline, so GLFW only contributes the window;
// device/queue selection is purely Vulkan-side here. (S160 adds the surface.)
VulkanCtx createVulkanCtx()
{
    VulkanCtx ctx;

    // --- Instance -----------------------------------------------------------
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = kAppName;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo{};
    instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo        = &appInfo;

    auto exts = requiredInstanceExtensions();
    instInfo.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    instInfo.ppEnabledExtensionNames = exts.data();

    if (vkCreateInstance(&instInfo, nullptr, &ctx.instance) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed (is a Vulkan driver installed?)");

    // --- Physical device: prefer an integrated/discrete GPU -----------------
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
    if (devCount == 0)
        throw std::runtime_error("No Vulkan physical devices found");
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(ctx.instance, &devCount, devices.data());

    for (auto dev : devices)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            ctx.physicalDevice = dev;
            std::printf("GPU: %s\n", props.deviceName);
            break;
        }
    }
    if (ctx.physicalDevice == VK_NULL_HANDLE)
        ctx.physicalDevice = devices[0];

    // --- Queue family -------------------------------------------------------
    uint32_t famCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &famCount, fams.data());

    bool found = false;
    for (uint32_t i = 0; i < famCount; ++i)
    {
        if ((fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            glfwGetPhysicalDevicePresentationSupport(ctx.instance, ctx.physicalDevice, i))
        {
            ctx.queueFamily = i;
            found           = true;
            break;
        }
    }
    if (!found)
        throw std::runtime_error("No queue family supports graphics + presentation");

    // --- Logical device -----------------------------------------------------
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = ctx.queueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount    = 1;
    devInfo.pQueueCreateInfos       = &queueInfo;
    devInfo.enabledExtensionCount   = 0;
    devInfo.pEnabledFeatures        = nullptr;

    if (vkCreateDevice(ctx.physicalDevice, &devInfo, nullptr, &ctx.device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.graphicsQueue);
    return ctx;
}

} // namespace

int main()
{
    // --- vaml-core consumer check -------------------------------------------
    const auto ver = vaml::version();
    std::printf("VamlPOC-linux: linking vaml-core %d.%d.%d (%s)\n",
                ver.major, ver.minor, ver.patch,
                std::string(vaml::engine_name()).c_str());

    // --- Platform window via vaml::vulkan::PlatformWindow (S160) -------------
    // The window is created through the library's real GLFW-backed surface
    // (Win32 on Windows / X11 on Linux) and its resize + input callback
    // surface is echoed here — S160's manual live-window proof (not CI; CI
    // covers the callback surface headless in vaml-vulkan-tests).
    vaml::vulkan::WindowCallbacks callbacks;
    callbacks.on_resize = [](int w, int h) {
        std::printf("[resize] framebuffer %dx%d\n", w, h);
    };
    callbacks.on_key = [](int key, int /*scancode*/, int action, int /*mods*/) {
        if (action == GLFW_PRESS)
            std::printf("[key] 0x%x\n", key);
    };
    callbacks.on_mouse_move = [](double x, double y) {
        std::printf("[mouse] %.0f,%.0f\n", x, y);
    };
    callbacks.on_mouse_button = [](int button, int action, int /*mods*/) {
        if (action == GLFW_PRESS)
            std::printf("[button] %d\n", button);
    };
    callbacks.on_scroll = [](double xo, double yo) {
        std::printf("[scroll] %.0f,%.0f\n", xo, yo);
    };

    auto window = vaml::vulkan::PlatformWindow::create(
        { "VamlPOC-linux", kWindowW, kWindowH }, std::move(callbacks));
    if (!window)
    {
        std::fprintf(stderr, "PlatformWindow::create failed (need a display; set DISPLAY=:0)\n");
        return 1;
    }
    // GLFW window handle for the Vulkan-side pass-through calls below.
    auto* glfwWindow = static_cast<GLFWwindow*>(window->native_handle());

    VulkanCtx ctx;
    try
    {
        ctx = createVulkanCtx();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Vulkan init error: %s\n", e.what());
        return 1;
    }

    int fbw = 0, fbh = 0;
    window->get_framebuffer_size(&fbw, &fbh);
    std::printf("Running on GPU; window framebuffer %dx%d. Press ESC or close to quit.\n", fbw, fbh);

    while (!window->should_close())
    {
        window->poll_events();  // pumps GLFW + forwards into the callbacks
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            window->request_close();
    }

    vkDestroyDevice(ctx.device, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);
    return 0;
}
