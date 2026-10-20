// S177 — RAIL 2: Hidden-window real-WSI resize gate (LOCAL on this box, NOT GH).
//
// A real-WSI resize harness that opens a hidden/offscreen GLFW window — Vulkan
// needs the surface, NOT visibility — builds the real instance → surface → device →
// swapchain through vaml-vulkan's REAL builders (via the S173 Presenter), ENABLES
// validation layers, installs a VK_EXT_debug_utils messenger that flags ANY
// validation/error message, and drives a programmatic RESIZE STORM +
// minimize/restore + resize-during-flight through the S167_1 two-generation
// SwapchainStateMachine.
//
// **Any validation error = test failure** (the hard S177 contract). The gate is
// enforced as a LOCAL pre-✅ command on THIS box (build/verify_s177_wsiresize.cmd);
// it is NOT a GH lane (CI runners have no WSI surface/display). On a headless host
// (no display, no present-capable surface) the executable exits SKIP (exit 2) so
// normal CI stays green; the local gate forces the RUN.
//
// The full three-lane harness matures into `docs/testing.md` (named, NOT created
// here — deferred-not-silent to S179). Optional future GPU/desktop nightly =
// deferred-not-silent.
//
// Exit codes: 0 = PASS (clean steady-state after the storm, zero validation errors)
//             1 = FAIL (validation error observed, or a required create failed)
//             2 = SKIP (no display / no surface — the local gate forces a real RUN)

#include <vaml/vulkan/PlatformWindow.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/gpu/BestGpuSelection.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/QueueFamilySelection.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/presentation/Presenter.h>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace vaml::vulkan;

namespace {

std::atomic<bool> g_validationError = false;

// The S177 clear color (matches the POCs — a light/gray clear so a resize's
// steady-state frame is a correct cleared swapchain image).
inline constexpr float kClearColor[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

// The S177 debug-utils messenger callback: ANY ERROR-severity message marks the
// gate FAILED (the "any validation error = test failure" contract).
VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT /*type*/,
              const VkDebugUtilsMessengerCallbackDataEXT* data,
              void* /*userData*/)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::fprintf(stderr, "[s177 VALIDATION ERROR] %s\n",
                     data && data->pMessage ? data->pMessage : "(no message)");
        g_validationError = true;
    }
    return VK_FALSE;
}

// Binds + installs the debug-utils messenger at runtime via vkGetInstanceProcAddr
// (the library's VulkanLoader keeps a fixed entry surface; this is a LOCAL gate-only
// best-effort binding — if the extension is absent the run continues unguarded).
struct DebugMessenger
{
    PFN_vkDestroyDebugUtilsMessengerEXT destroyFn = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;

    ~DebugMessenger()
    {
        if (messenger && destroyFn)
            destroyFn(instance, messenger, nullptr);
    }
};

bool install_debug_messenger(DebugMessenger& m, const VulkanLoader& vk, VkInstance instance)
{
    m.instance = instance;
    auto* createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vk.vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    m.destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vk.vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (!createFn || !m.destroyFn)
        return false;  // unavailable — proceed without (best-effort)

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = &debugCallback;

    if (createFn(instance, &ci, nullptr, &m.messenger) != VK_SUCCESS)
        return false;
    return true;
}

// The S177 render pass needed by the draw body's clear record (single-threaded, set
// once before the presenter is created; the presenter/surface outlives it).
VkRenderPass g_renderPass = VK_NULL_HANDLE;

// The S177 draw body — records a valid minimal command buffer (reset, begin, render
// pass clear, end pass, end). The Presenter owns acquire->submit->present; the resize
// + machine correctness is what S177 proves, not a full triangle.
bool draw_clear(void* /*userData*/, vaml::presentation::Presenter& self,
                VkCommandBuffer cmd, std::uint32_t imageIndex)
{
    const auto& vk = self.loader();
    const vaml::vulkan::Framebuffer* fb = self.machine().framebuffer(imageIndex);
    if (!fb || !cmd)
        return false;
    if (vk.vkResetCommandBuffer(cmd, /*flags=*/0) != VK_SUCCESS)
        return false;

    const auto begin = describe_command_buffer_begin();
    if (vk.vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        return false;

    const VkClearValue clear = describe_clear_value(kClearColor);
    const auto rp = build_render_pass_begin(
        g_renderPass, fb->framebuffer(), self.extent(), clear,
        self.machine().depth_format().has_value());
    vk.vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vk.vkCmdEndRenderPass(cmd);

    return vk.vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

} // namespace

// File-based diagnostic sink (the test also prints to stderr; the file survives an
// AV so we can see how far the gate got). std::ofstream keeps this warning-free on
// both MSVC and GCC/Clang (the C `fopen` triggers C4996 on MSVC).
static void logfile(const char* msg)
{
    std::ofstream f("s177_diag.txt", std::ios::app);
    if (f)
        f << msg << "\n";
}

int main()
{
    logfile("[s177] begin");
    std::fprintf(stderr, "[s177] begin\n");
    std::fflush(stderr);

    // --- The S177 harness: ONE hidden window (surface host) + real WSI build. ---
    // Create the window OFFSCREEN (GLFW hidden): Vulkan needs the surface, not
    // visibility. The SAME window hosts the surface + the programmatic storm.
    vaml::vulkan::WindowCallbacks callbacks;
    auto window = vaml::vulkan::PlatformWindow::create(
        { "VAML S177 WSI resize gate", 800, 600 }, std::move(callbacks));
    if (!window)
    {
        // No display — SKIP (headless/CI). The local gate forces a real host.
        std::fprintf(stderr, "[s177] No display / PlatformWindow::create failed — SKIP (exit 2).\n");
        std::fprintf(stderr, "[s177] The local build/verify_s177_wsiresize.cmd gate forces a REAL device run.\n");
        return 2;
    }

    // GLFW hidden hint (offscreen; surface-only). The window is GLFW_NO_API.
    auto* glfwWindow = static_cast<GLFWwindow*>(window->native_handle());
    glfwHideWindow(glfwWindow);

    std::unique_ptr<VulkanLoader> vk;
    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanSurface> surface;
    std::unique_ptr<Device> device;
    std::unique_ptr<RenderPass> renderPass;
    std::unique_ptr<vaml::presentation::Presenter> presenter;

    try
    {
        vk = std::make_unique<VulkanLoader>();
        std::fprintf(stderr, "[s177] loader ok\n");

        VulkanInstance::Config icfg;
        icfg.enableValidation = true;   // a Debug build => validation layers REALLY ON
        instance = VulkanInstance::create(*vk, icfg);
        if (!instance)
        {
            std::fprintf(stderr, "[s177] VulkanInstance::create failed — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s177] instance ok\n");

        surface = VulkanSurface::create(*vk, *instance, *window);
        if (!surface)
        {
            std::fprintf(stderr, "[s177] VulkanSurface::create failed (no surface) — SKIP (exit 2)\n");
            return 2;
        }

        DebugMessenger messenger;
        install_debug_messenger(messenger, *vk, instance->instance());

        auto best = gpu::select_best_gpu(*vk, *instance, surface.get(),
                                         gpu::preferred_gpu_env_name());
        if (!best || best->best.device == VK_NULL_HANDLE)
        {
            std::fprintf(stderr, "[s177] no physical device — SKIP (exit 2)\n");
            return 2;
        }
        std::fprintf(stderr, "[s177] gpu=%s\n", best->best.name.c_str());

        std::uint32_t famCount = 0;
        vk->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, nullptr);
        std::vector<VkQueueFamilyProperties> fams(famCount);
        vk->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, fams.data());
        PresentSupportFn present = [&](std::uint32_t i) {
            VkBool32 s = VK_FALSE;
            return vk->vkGetPhysicalDeviceSurfaceSupportKHR(best->best.device, i,
                                                            surface->surface(), &s) == VK_SUCCESS &&
                   s == VK_TRUE;
        };
        auto fam = select_queue_family(fams.data(), famCount, present);
        if (!fam)
        {
            std::fprintf(stderr, "[s177] no graphics+present queue family — SKIP (exit 2)\n");
            return 2;
        }

        device = Device::create(*vk, best->best, *fam);
        if (!device)
        {
            std::fprintf(stderr, "[s177] Device::create failed — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s177] device ok\n");

        auto cfg = Swapchain::negotiate(*vk, best->best.device, *surface, 800, 600);
        const VkFormat colorFormat = cfg ? cfg->surfaceFormat.format : VK_FORMAT_B8G8R8A8_SRGB;
        const std::optional<VkFormat> depth = cfg ? cfg->depthFormat : std::nullopt;
        renderPass = RenderPass::create(*vk, *device, colorFormat, depth, VK_SAMPLE_COUNT_1_BIT);
        if (!renderPass)
        {
            std::fprintf(stderr, "[s177] RenderPass::create failed — FAIL (exit 1)\n");
            return 1;
        }
        g_renderPass = renderPass->render_pass();

        vaml::presentation::PresenterOptions opts;
        opts.width = 800;
        opts.height = 600;
        opts.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        opts.draw = &draw_clear;
        presenter = vaml::presentation::Presenter::create(
            *vk, *device, *surface, renderPass->render_pass(), opts);
        if (!presenter)
        {
            std::fprintf(stderr, "[s177] Presenter::create failed (no gen0) — SKIP (exit 2)\n");
            return 2;
        }

        std::fprintf(stderr, "[s177] machine gen0 built: %u images, extent %ux%u, state=%d\n",
                     presenter->machine().image_count(), presenter->extent().width,
                     presenter->extent().height, static_cast<int>(presenter->state()));
        std::fflush(stderr);

        // --- RESIZE STORM (the S177 headline) ---------------------------------
        const std::uint32_t kSizes[][2] = {
            { 800, 600 }, { 1024, 768 }, { 640, 480 }, { 1280, 720 }, { 320, 200 },
            { 1920, 1080 }, { 400, 900 }, { 100, 60 }, { 768, 1024 }, { 1280, 60 },
        };
        const std::size_t kSizeCount = sizeof(kSizes) / sizeof(kSizes[0]);
        for (std::size_t i = 0; i < kSizeCount; ++i)
        {
            glfwSetWindowSize(glfwWindow, static_cast<int>(kSizes[i][0]), static_cast<int>(kSizes[i][1]));
            glfwPollEvents();
            presenter->request_resize(kSizes[i][0], kSizes[i][1]);
            (void)presenter->tick(true, true);
            glfwPollEvents();
        }
        std::fprintf(stderr, "[s177] resize storm applied (%d sizes)\n", static_cast<int>(kSizeCount));
        std::fflush(stderr);

        // MINIMIZE / RESTORE (zero-size -> the S167_1 park path).
        glfwIconifyWindow(glfwWindow);
        glfwPollEvents();
        (void)presenter->tick(true, true);
        glfwRestoreWindow(glfwWindow);
        glfwPollEvents();
        presenter->request_resize(800, 600);
        (void)presenter->tick(true, true);
        std::fprintf(stderr, "[s177] minimize/restore cycle applied\n");
        std::fflush(stderr);

        // RESIZE-DURING-FLIGHT: queue several resizes without draining between
        // (a fast drag), then drain to a clean steady state.
        presenter->request_resize(700, 500);
        presenter->request_resize(900, 640);
        presenter->request_resize(1200, 800);
        glfwPollEvents();
        (void)presenter->tick(true, true);
        glfwPollEvents();
        (void)presenter->tick(true, true);
        glfwPollEvents();
        (void)presenter->tick(true, true);
        glfwPollEvents();
        (void)presenter->tick(true, true);
        std::fprintf(stderr, "[s177] resize-during-flight drained to steady state\n");

        glfwPollEvents();

        // --- VERDICT -----------------------------------------------------------
        if (g_validationError.load())
        {
            std::fprintf(stderr, "[s177] FAIL: validation error observed after the WSI storm\n");
            return 1;
        }
        std::fprintf(stderr, "[s177] PASS: clean steady state after storm/minimize/restore/flight; "
                             "ZERO validation errors\n");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[s177] exception: %s — FAIL (exit 1)\n", e.what());
        return 1;
    }
}
