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

#include "VulkanLoader.h"
#include "framework.h"

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
// Win32 window plumbing (the native window lane; Linux uses GLFW).
// ---------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    default:
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

bool createHostWindow(HWND& outWindow, HINSTANCE instance, int nCmdShow)
{
    const wchar_t* className = L"VamlPOCWindowClass";

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = instance;
    wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = className;

    if (!::RegisterClassExW(&wc))
        return false;

    RECT rect{0, 0, kWindowW, kWindowH};
    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    outWindow = ::CreateWindowExW(
        0, className, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, nullptr);

    if (!outWindow)
        return false;

    ::ShowWindow(outWindow, nCmdShow);
    ::UpdateWindow(outWindow);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // --- vaml-core consumer check (parity with Linux) -----------------------
    const auto ver = vaml::version();
    std::printf("VamlPOC (Windows): linking vaml-core %d.%d.%d (%s)\n",
                ver.major, ver.minor, ver.patch,
                std::string(vaml::engine_name()).c_str());

    HWND hwnd = nullptr;
    if (!vamlpoc::createHostWindow(hwnd, hInstance, nCmdShow))
    {
        std::fprintf(stderr, "Failed to create the host window.\n");
        return 1;
    }

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
        ::DestroyWindow(hwnd);
        return 1;
    }

    // --- Message loop -------------------------------------------------------
    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    // Teardown (reverse order), matching Linux.
    ctx.destroy(*vk);
    return static_cast<int>(msg.wParam);
}
