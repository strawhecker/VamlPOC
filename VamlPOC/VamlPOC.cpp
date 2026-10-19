// VamlPOC (Windows): the Win32 counterpart to VamlPOC-linux.
//
// Proves the same GPU lane as the Linux app on the Windows side:
//   creates a native Win32 window, builds a Vulkan instance, selects the BEST
//   GPU via the S163 auto-selection surface, creates a logical device +
//   graphics queue, and runs the window message loop.
//
// S163 migration: the pre-S162 hand-rolled instance/GPU loop and the local
// VamlPOC/VulkanLoader.h are GONE (NO-deadwood). Both POCs now call the
// IDENTICAL vaml::vulkan::gpu::select_best_gpu() library function — "identical
// code in both apps" is one library call, not two copies (Cat 1 library
// surface; the app keeps only its own window + loop + device consumption).
//
// It also links the vaml-core static library (consumer path) and prints the
// library version, keeping the two POCs capability-synced:
//   * Vulkan instance + best-GPU auto-selection + logical device
//   * a real native window on the GPU
//   * links vaml-core and prints its version
//
// The Linux twin (VamlPOC-linux/src/main.cpp) is the source of truth for this
// file's capabilities — keep the two in sync when extending the GPU path.

#include <vaml/version.h>
#include <vaml/vulkan/PlatformWindow.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/gpu/BestGpuSelection.h>

#include "framework.h"   // pulls <windows.h>; must come BEFORE glfw3.h to avoid the
                         // APIENTRY macro redefinition conflict between minwindef.h
                         // and GLFW (standard GLFW+Win32 include-order requirement).
#include <GLFW/glfw3.h>  // GLFW_* constants referenced by the S160 callback lambdas

#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vamlpoc {

constexpr const wchar_t* kWindowTitle = L"VamlPOC (Windows)";
constexpr int            kWindowW     = 800;
constexpr int            kWindowH     = 600;

// Vulkan handles chosen at init; destroyed in reverse order on exit. The
// loader, instance, window, and surface must OUTLIVE everything created from
// them. The S164 Swapchain (unique_ptr) is member #1 so it is destroyed FIRST
// (before the device — the documented swapchain-before-device ordering).
struct VulkanCtx
{
    VkDevice                                     device        = VK_NULL_HANDLE;
    VkQueue                                      graphicsQueue = VK_NULL_HANDLE;
    uint32_t                                     queueFamily   = 0;
    std::unique_ptr<vaml::vulkan::Swapchain>     swapchain;   // destroyed before device

    void destroy(const vaml::vulkan::VulkanLoader& vk)
    {
        swapchain.reset();  // swapchain -> its image views -> its depth -> swapchain
        if (device)
            vk.vkDestroyDevice(device, nullptr);
        device   = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
    }
};

// Builds the instance + surface + best-GPU selection + logical device through
// the vaml-vulkan library surface (S162 + S163). Returns the chosen GPU's
// PhysicalDeviceInfo so the caller can report it.
vaml::vulkan::PhysicalDeviceInfo createVulkanCtx(
    const vaml::vulkan::VulkanLoader& vk,
    const vaml::vulkan::VulkanInstance& instance,
    const vaml::vulkan::VulkanSurface* surface,
    VulkanCtx& ctx)
{
    using namespace vaml::vulkan;

    // --- Best-GPU auto-selection (S163): identical library call on both POCs.
    const std::string preferred = vaml::vulkan::gpu::preferred_gpu_env_name();
    auto best = vaml::vulkan::gpu::select_best_gpu(vk, instance, surface, preferred);
    if (!best)
        throw std::runtime_error("No Vulkan physical devices found");
    const PhysicalDeviceInfo& gpu = best->best;
    if (gpu.device == VK_NULL_HANDLE)
        throw std::runtime_error("Best-GPU selection returned no device");

    // --- Queue family: graphics (unified preferred, graphics-only fallback).
    std::uint32_t famCount = 0;
    vk.vkGetPhysicalDeviceQueueFamilyProperties(gpu.device, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    vk.vkGetPhysicalDeviceQueueFamilyProperties(gpu.device, &famCount, fams.data());

    PresentSupportFn present = [&](std::uint32_t i) {
        if (!surface || !vk.vkGetPhysicalDeviceSurfaceSupportKHR)
            return false;
        VkBool32 s = VK_FALSE;
        return vk.vkGetPhysicalDeviceSurfaceSupportKHR(gpu.device, i, surface->surface(), &s) == VK_SUCCESS &&
               s == VK_TRUE;
    };
    auto queueFamily = select_queue_family(fams.data(), famCount, present);
    if (!queueFamily)
        throw std::runtime_error("No graphics-capable queue family found");
    ctx.queueFamily = *queueFamily;

    // --- Logical device + graphics queue (S163 loader device entries).
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = ctx.queueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    // S164: enable VK_KHR_swapchain at device creation (else vkCreateSwapchainKHR
    // fails). The swapchain extension is the ONE device extension S164 requires.
    const char* kSwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo devInfo{};
    devInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount    = 1;
    devInfo.pQueueCreateInfos       = &queueInfo;
    devInfo.enabledExtensionCount   = 1;
    devInfo.ppEnabledExtensionNames = &kSwapchainExtension;

    if (vk.vkCreateDevice(gpu.device, &devInfo, nullptr, &ctx.device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vk.vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.graphicsQueue);

    // --- Swapchain + image views + depth (S164) -----------------------------
    // Consumes the surface + the S163 device + the S164 loader entries. The
    // framebuffer size comes from the window; the S164 decision seams negotiate
    // the format/BPP/present-mode/extent/depth.
    if (surface && vk.has_swapchain_loader())
    {
        const auto cfg = vaml::vulkan::Swapchain::negotiate(
            vk, gpu.device, *surface, vamlpoc::kWindowW, vamlpoc::kWindowH);
        std::printf("[swapchain] negotiated BPP=%u present=%d extent=%ux%u count=%u depth=%s\n",
                    cfg ? cfg->surface_bytes_per_pixel() : 0u,
                    cfg ? static_cast<int>(cfg->presentMode) : -1,
                    cfg ? cfg->extent.width : 0u,
                    cfg ? cfg->extent.height : 0u,
                    cfg ? cfg->imageCount : 0u,
                    (cfg && cfg->depthFormat) ? "yes" : "no");

        ctx.swapchain = vaml::vulkan::Swapchain::create(
            vk, ctx.device, gpu.device, *surface, vamlpoc::kWindowW, vamlpoc::kWindowH);
        if (!ctx.swapchain)
            std::fprintf(stderr, "[swapchain] Swapchain::create failed\n");
        else
            std::printf("[swapchain] created: %u images, format %s, %u BPP, present %d, extent %ux%u\n",
                        ctx.swapchain->image_count(),
                        ctx.swapchain->color_format() == VK_FORMAT_B8G8R8A8_SRGB ? "B8G8R8A8_SRGB" : "other",
                        ctx.swapchain->bytes_per_pixel(),
                        static_cast<int>(ctx.swapchain->present_mode()),
                        ctx.swapchain->extent().width,
                        ctx.swapchain->extent().height);
    }
    return gpu;
}

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
    (void)hInstance;

    // --- Vulkan loader + instance + surface + best-GPU init (parity with Linux)
    // The loader (vulkan-1.dll) must outlive every Vulkan handle; the instance
    // and window must outlive the surface (S162/S163/surface header lifetimes).
    auto vk  = std::make_unique<vaml::vulkan::VulkanLoader>();
    auto instance = vaml::vulkan::VulkanInstance::create(*vk, {});
    if (!instance)
    {
        std::fprintf(stderr, "VulkanInstance::create failed (is a Vulkan driver installed?)\n");
        return 1;
    }
    auto surface = vaml::vulkan::VulkanSurface::create(*vk, *instance, *window);

    vamlpoc::VulkanCtx ctx;
    try
    {
        auto gpu = vamlpoc::createVulkanCtx(*vk, *instance, surface.get(), ctx);
        std::printf("Running on GPU: %s; close the window to quit.\n",
                    gpu.name.empty() ? "<unnamed>" : gpu.name.c_str());
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Vulkan init error: %s\n", e.what());
        return 1;
    }

    // --- Message loop (mirrors glfwPollEvents; drives PlatformWindow) ---------
    while (!window->should_close())
        window->poll_events();

    // Teardown (reverse order), matching Linux: device -> surface -> instance -> loader.
    ctx.destroy(*vk);
    return 0;
}
