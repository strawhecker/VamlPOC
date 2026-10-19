// VamlPOC-linux: Linux counterpart to the Windows VamlPOC app.
//
// Proves the GPU lane end-to-end on the target machine (AMD RADV / Vulkan):
//   opens a GLFW window, creates a Vulkan instance, selects the BEST GPU via
//   the S163 auto-selection surface, creates a logical device + graphics
//   queue, and runs the message loop.
//
// S163 migration: the pre-S162 hand-rolled instance/GPU loop (createVulkanCtx)
// is GONE (NO-deadwood). Both POCs now call the IDENTICAL
// vaml::vulkan::gpu::select_best_gpu() library function — "identical code in
// both apps" is one library call, not two copies (Cat 1 library surface; the
// app keeps only its own window + loop + device consumption).
//
// It also links the vaml-core static library (consumer path) and prints the
// library version, so this binary demonstrates "apart from the lib but
// consuming it".
//
// Deliberately a single-TU, dependency-light program (no raw pointer managers,
// no RAII wrappers yet) — it is the Linux POC baseline that later sprints
// (S164+, swapchain/device RAII in S168) extend into the full VAML renderer.

#include <vaml/version.h>
#include <vaml/vulkan/PlatformWindow.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/gpu/BestGpuSelection.h>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kWindowW = 800;
constexpr uint32_t kWindowH = 600;

// Small RAII-free Vulkan context holder (baseline; S168 introduces wrappers).
// Holds handles long enough for the demo loop, destroys in reverse order.
struct VulkanCtx
{
    VkDevice                                     device        = VK_NULL_HANDLE;
    VkQueue                                      graphicsQueue = VK_NULL_HANDLE;
    uint32_t                                     queueFamily   = 0;
    std::unique_ptr<vaml::vulkan::Swapchain>     swapchain;   // destroyed before device
    // S165 render resources (constructed after the swapchain, destroyed before it).
    std::unique_ptr<vaml::vulkan::RenderPass>    renderPass;
    std::unique_ptr<vaml::vulkan::ShaderModule>  vertexModule;
    std::unique_ptr<vaml::vulkan::ShaderModule>  fragmentModule;
    std::unique_ptr<vaml::vulkan::Pipeline>      pipeline;
    std::vector<std::unique_ptr<vaml::vulkan::Framebuffer>> framebuffers;

    void destroy(const vaml::vulkan::VulkanLoader& vk)
    {
        // Destroy order: framebuffers -> pipeline -> shader modules -> render pass
        // -> swapchain -> device (the S165 LIFETIMES contract).
        framebuffers.clear();
        pipeline.reset();
        vertexModule.reset();
        fragmentModule.reset();
        renderPass.reset();
        swapchain.reset();  // swapchain -> its image views -> its depth -> swapchain
        if (device)
            vk.vkDestroyDevice(device, nullptr);
        device        = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
    }
};

using vaml::vulkan::PhysicalDeviceInfo;
using vaml::vulkan::PresentSupportFn;

// Builds the instance + surface + best-GPU selection + logical device through
// the vaml-vulkan library surface (S162 + S163). Returns the chosen GPU's
// PhysicalDeviceInfo so the caller can report it.
PhysicalDeviceInfo createVulkanCtx(const vaml::vulkan::VulkanLoader& vk,
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
    devInfo.pEnabledFeatures        = nullptr;

    if (vk.vkCreateDevice(gpu.device, &devInfo, nullptr, &ctx.device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vk.vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.graphicsQueue);

    // --- Swapchain + image views + depth (S164) -----------------------------
    // Consumes the surface + the S163 device + the S164 loader entries. The
    // framebuffer size comes from the window; the S164 decision seams negotiate
    // the format/BPP/present-mode/extent/depth.
    if (surface && vk.has_swapchain_loader())
    {
        const auto cfg = Swapchain::negotiate(vk, gpu.device, *surface, kWindowW, kWindowH);
        std::printf("[swapchain] negotiated BPP=%u present=%d extent=%ux%u count=%u depth=%s\n",
                    cfg ? cfg->surface_bytes_per_pixel() : 0u,
                    cfg ? static_cast<int>(cfg->presentMode) : -1,
                    cfg ? cfg->extent.width : 0u,
                    cfg ? cfg->extent.height : 0u,
                    cfg ? cfg->imageCount : 0u,
                    (cfg && cfg->depthFormat) ? "yes" : "no");

        ctx.swapchain = Swapchain::create(vk, ctx.device, gpu.device, *surface, kWindowW, kWindowH);
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

        // --- Render pass + shader modules + graphics pipeline + framebuffers (S165)
        // Consumes the swapchain's color/depth formats + extent + image views.
        // No command buffer / no present yet — that is S166.
        if (vk.has_render_loader())
        {
            const auto colorFormat = ctx.swapchain->color_format();
            const auto depthFormat = ctx.swapchain->depth_format();

            ctx.renderPass = RenderPass::create(vk, ctx.device, colorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT);
            ctx.vertexModule   = ShaderModule::create(vk, ctx.device, tri_vertex_spirv());
            ctx.fragmentModule = ShaderModule::create(vk, ctx.device, tri_fragment_spirv());

            if (ctx.renderPass && ctx.vertexModule && ctx.fragmentModule)
            {
                PipelineSpec spec{};
                spec.renderPass     = ctx.renderPass->render_pass();
                spec.colorFormat    = colorFormat;
                spec.hasDepth       = depthFormat.has_value();
                spec.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
                spec.extent         = ctx.swapchain->extent();
                spec.vertexModule   = ctx.vertexModule->shader_module();
                spec.fragmentModule = ctx.fragmentModule->shader_module();
                ctx.pipeline = Pipeline::create(vk, ctx.device, spec);

                if (ctx.pipeline)
                {
                    for (VkImageView view : ctx.swapchain->image_views())
                    {
                        ctx.framebuffers.push_back(Framebuffer::create(
                            vk, ctx.device, ctx.renderPass->render_pass(),
                            ctx.swapchain->extent(),
                            std::vector<VkImageView>{ view },
                            ctx.swapchain->depth_image_view() ? std::optional<VkImageView>(ctx.swapchain->depth_image_view())
                                                              : std::nullopt));
                    }
                    std::printf("[s165] render pass + pipeline + %zu framebuffer(s): color %s, depth %s, %ux%u\n",
                                ctx.framebuffers.size(),
                                colorFormat == VK_FORMAT_B8G8R8A8_SRGB ? "B8G8R8A8_SRGB" : "other",
                                depthFormat.has_value() ? "yes" : "no",
                                ctx.swapchain->extent().width, ctx.swapchain->extent().height);
                }
                else
                {
                    std::fprintf(stderr, "[s165] Pipeline::create failed (invalid SPIR-V or state?)\n");
                }
            }
            else
            {
                std::fprintf(stderr, "[s165] RenderPass/ShaderModule::create failed\n");
            }
        }
    }
    return gpu;
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

    // --- Vulkan loader + instance + surface (S162/S163 library surface) ------
    auto vk       = std::make_unique<vaml::vulkan::VulkanLoader>();
    auto instance = vaml::vulkan::VulkanInstance::create(*vk, {});
    if (!instance)
    {
        std::fprintf(stderr, "VulkanInstance::create failed (is a Vulkan driver installed?)\n");
        return 1;
    }
    auto surface = vaml::vulkan::VulkanSurface::create(*vk, *instance, *window);

    VulkanCtx ctx;
    std::string gpuName;
    try
    {
        auto gpu = createVulkanCtx(*vk, *instance, surface.get(), ctx);
        gpuName = gpu.name;
        std::printf("Running on GPU: %s\n", gpu.name.c_str());
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Vulkan init error: %s\n", e.what());
        return 1;
    }

    int fbw = 0, fbh = 0;
    window->get_framebuffer_size(&fbw, &fbh);
    std::printf("Running on GPU; window framebuffer %dx%d. Press ESC or close to quit.\n", fbw, fbh);

    // --- S165 LIVE proof (device-only; no surface required) ------------------
    // On the headless llvmpipe Xwayland VM the VulkanSurface is null, so the app
    // skips swapchain creation ([both apps, not CI]). The S165 device-bound
    // create() paths (RenderPass / ShaderModule / Pipeline) only need a DEVICE,
    // so they prove live here even without a surface: the critical S165 risk is
    // that the EMBEDDED SPIR-V is accepted by a real driver (vkCreateShaderModule)
    // + a real render pass + pipeline create succeed. (Surface-dependent
    // framebuffers + the draw loop are S166.)
    if (vk->has_render_loader() && ctx.device)
    {
        auto rp = vaml::vulkan::RenderPass::create(
            *vk, ctx.device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
        auto vm = vaml::vulkan::ShaderModule::create(*vk, ctx.device, vaml::vulkan::tri_vertex_spirv());
        auto fm = vaml::vulkan::ShaderModule::create(*vk, ctx.device, vaml::vulkan::tri_fragment_spirv());
        if (rp && vm && fm)
        {
            vaml::vulkan::PipelineSpec spec{};
            spec.renderPass     = rp->render_pass();
            spec.colorFormat    = VK_FORMAT_B8G8R8A8_SRGB;
            spec.hasDepth       = true;
            spec.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
            spec.extent         = VkExtent2D{ 800, 600 };
            spec.vertexModule   = vm->shader_module();
            spec.fragmentModule = fm->shader_module();
            auto pl = vaml::vulkan::Pipeline::create(*vk, ctx.device, spec);
            if (pl)
                std::printf("[s165-live] render pass + shader modules + pipeline created on %s (embedded SPIR-V accepted)\n",
                            gpuName.c_str());
            else
                std::fprintf(stderr, "[s165-live] Pipeline::create failed on %s\n", gpuName.c_str());
        }
        else
        {
            std::fprintf(stderr, "[s165-live] RenderPass/ShaderModule::create failed on %s (embedded SPIR-V rejected?)\n",
                         gpuName.c_str());
        }
    }

    while (!window->should_close())
    {
        window->poll_events();  // pumps GLFW + forwards into the callbacks
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            window->request_close();
    }

    ctx.destroy(*vk);
    return 0;
}
