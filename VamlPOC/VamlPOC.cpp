// VamlPOC (Windows): the Win32 counterpart to VamlPOC-linux.
//
// Proves the same GPU lane as the Linux app on the Windows side:
//   creates a native Win32 window, builds a Vulkan instance + picks the GPU
//   (integrated/discrete), creates a logical device + graphics queue, and runs
//   the window message loop.
//
// It also links the vaml-core static library (consumer path) and prints the
// library version, keeping the two POOCs capability-synced:
//   * Vulkan instance + physical-device (GPU) selection + logical device
//   * a real native window on the GPU
//   * links vaml-core and prints its version
//
// The Linux twin (VamlPOC-linux/src/main.cpp) is the source of truth for this
// file's capabilities — keep the two in sync when extending the GPU path.

#include <vaml/version.h>
#include <vaml/vulkan/PlatformWindow.h>

#include "framework.h"   // pulls <windows.h>; must come BEFORE glfw3.h to avoid the
                         // APIENTRY macro redefinition conflict between minwindef.h
                         // and GLFW (standard GLFW+Win32 include-order requirement).
#include <GLFW/glfw3.h>  // GLFW_* constants referenced by the S160 callback lambdas
#include "VulkanLoader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vamlpoc {

constexpr const wchar_t* kWindowTitle = L"VamlPOC (Windows)";
constexpr int            kWindowW     = 800;
constexpr int            kWindowH     = 600;

// Vulkan handles chosen at init; destroyed in reverse order on exit.
struct VulkanCtx
{
    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    uint32_t         queueFamily    = 0;

    void destroy(const VulkanLoader& vk)
    {
        if (device)
            vk.vkDestroyDevice(device, nullptr);
        if (instance)
            vk.vkDestroyInstance(instance, nullptr);
        device   = VK_NULL_HANDLE;
        instance = VK_NULL_HANDLE;
    }
};

// Rough mirror of VamlPOC-linux main.cpp's createVulkanCtx(), but window
// creation is Win32 and the loader is runtime-resolved (VK_NO_PROTOTYPES).
// Note: GPU selection mirrors Linux (prefer integrated/discrete GPU). Surface
// creation/swapchain arrives with S160+ on both platforms.
VulkanCtx createVulkanCtx(const VulkanLoader& vk)
{
    VulkanCtx ctx;

    // --- Instance -----------------------------------------------------------
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VamlPOC";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo{};
    instInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;

    if (vk.vkCreateInstance(&instInfo, nullptr, &ctx.instance) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed (is a Vulkan driver installed?)");

    // --- Physical device: prefer an integrated/discrete GPU -----------------
    uint32_t devCount = 0;
    vk.vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
    if (devCount == 0)
        throw std::runtime_error("No Vulkan physical devices found");
    std::vector<VkPhysicalDevice> devices(devCount);
    vk.vkEnumeratePhysicalDevices(ctx.instance, &devCount, devices.data());

    for (auto dev : devices)
    {
        VkPhysicalDeviceProperties props{};
        vk.vkGetPhysicalDeviceProperties(dev, &props);
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

    // --- Queue family (graphics, no presentation check yet — no surface) ----
    uint32_t famCount = 0;
    vk.vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    vk.vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &famCount, fams.data());

    bool found = false;
    for (uint32_t i = 0; i < famCount; ++i)
    {
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            ctx.queueFamily = i;
            found           = true;
            break;
        }
    }
    if (!found)
        throw std::runtime_error("No graphics-capable queue family found");

    // --- Logical device -----------------------------------------------------
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = ctx.queueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos    = &queueInfo;

    if (vk.vkCreateDevice(ctx.physicalDevice, &devInfo, nullptr, &ctx.device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vk.vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.graphicsQueue);
    return ctx;
}

// ---------------------------------------------------------------------------
// Window lane: the window is created through vaml::vulkan::PlatformWindow (S160)
// — a real GLFW-backed window (Win32 on Windows / X11 on Linux) — instead of
// hand-rolled CreateWindowEx. Its resize + input callback surface is echoed
// here. (The Vulkan handle is still created from the GLFW window's native
// Win32 HWND via get_native_win32_handle at S162; this baseline already routes
// window creation + the loop through the library surface.)
// ---------------------------------------------------------------------------
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int /*nCmdShow*/)
{
    // --- vaml-core consumer check (parity with Linux) -----------------------
    const auto ver = vaml::version();
    std::printf("VamlPOC (Windows): linking vaml-core %d.%d.%d (%s)\n",
                ver.major, ver.minor, ver.patch,
                std::string(vaml::engine_name()).c_str());

    // --- Platform window (S160) ----------------------------------------------
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
        { "VamlPOC (Windows)", vamlpoc::kWindowW, vamlpoc::kWindowH }, std::move(callbacks));
    if (!window)
    {
        std::fprintf(stderr, "PlatformWindow::create failed.\n");
        return 1;
    }
    // GLFW UI-thread init on Windows requires the message-only window thread;
    // poll_events() pumps it. The GLFWwindow* is retrievable for the native
    // HWND at S162.
    (void)hInstance;

    // --- Vulkan loader + GPU init (parity with Linux) -----------------------
    // The loader (vulkan-1.dll) must outlive every Vulkan handle, so it is
    // held for the app's lifetime and destroyed last.
    vamlpoc::VulkanCtx ctx;
    std::unique_ptr<vamlpoc::VulkanLoader> vk;
    try
    {
        vk  = std::make_unique<vamlpoc::VulkanLoader>(); // LoadLibrary("vulkan-1.dll")
        ctx = vamlpoc::createVulkanCtx(*vk);             // instance -> GPU -> device
        std::printf("Running on GPU; close the window to quit.\n");
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Vulkan init error: %s\n", e.what());
        return 1;
    }

    // --- Message loop (mirrors glfwPollEvents; drives PlatformWindow) ---------
    while (!window->should_close())
        window->poll_events();

    // Teardown (reverse order), matching Linux.
    ctx.destroy(*vk);
    return 0;
}
