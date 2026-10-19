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
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/FrameSync.h>
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

// The S166 clear color (light/gray) so the blue S165 triangle is visible against it.
inline constexpr float kClearColor[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

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
    // S165 render resources (constructed after the swapchain, destroyed before it).
    std::unique_ptr<vaml::vulkan::RenderPass>    renderPass;
    std::unique_ptr<vaml::vulkan::ShaderModule>  vertexModule;
    std::unique_ptr<vaml::vulkan::ShaderModule>  fragmentModule;
    std::unique_ptr<vaml::vulkan::Pipeline>      pipeline;
    std::vector<std::unique_ptr<vaml::vulkan::Framebuffer>> framebuffers;
    // S166 draw-loop resources (constructed after the framebuffers, destroyed before
    // the device — after the last submit + a device wait so nothing is in flight).
    std::unique_ptr<vaml::vulkan::CommandPool>   commandPool;
    std::unique_ptr<vaml::vulkan::FrameSync>     frameSync;

    void destroy(const vaml::vulkan::VulkanLoader& vk)
    {
        // Destroy order: FrameSync -> CommandPool -> framebuffers -> pipeline ->
        // shader modules -> render pass -> swapchain -> device. The draw loop must be
        // idle before these — destroy() is called after the loop's device-wait.
        frameSync.reset();
        commandPool.reset();
        framebuffers.clear();
        pipeline.reset();
        vertexModule.reset();
        fragmentModule.reset();
        renderPass.reset();
        swapchain.reset();  // swapchain -> its image views -> its depth -> swapchain
        if (device)
            vk.vkDestroyDevice(device, nullptr);
        device   = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
    }
};

// The S166 per-frame record body — IDENTICAL in both apps. Consumes the S166
// draw_record_plan() (the fixed ordered step sequence) + the S165 seams (tri_mvp,
// viewport_scissor, push_constant_range) + build_render_pass_begin. Returns false on
// a record failure.
bool record_draw_frame(const vaml::vulkan::VulkanLoader& vk,
                       VkCommandBuffer cmd,
                       VkPipeline pipeline,
                       VkPipelineLayout layout,
                       VkRenderPass renderPass,
                       VkFramebuffer framebuffer,
                       VkExtent2D extent,
                       bool hasDepth)
{
    using namespace vaml::vulkan;
    if (!cmd || !pipeline || !layout || !renderPass || !framebuffer)
        return false;
    if (vk.vkResetCommandBuffer(cmd, /*flags=*/0) != VK_SUCCESS)
        return false;

    const auto begin = describe_command_buffer_begin();
    if (vk.vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        return false;

    // The S166 fixed ordered record (see draw_record_plan()).
    const auto vs = viewport_scissor(extent);
    VkClearValue clear = describe_clear_value(kClearColor);  // light/gray; blue triangle over it
    const auto rpBegin = build_render_pass_begin(renderPass, framebuffer, extent, clear, hasDepth);
    vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vk.vkCmdSetViewport(cmd, 0, 1, &vs.viewport);
    vk.vkCmdSetScissor(cmd, 0, 1, &vs.scissor);
    const auto mvp = tri_mvp(extent.width, extent.height);
    vk.vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), mvp.data());
    vk.vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vk.vkCmdDraw(cmd, 3, 1, 0, 0);  // the S165 triangle
    vk.vkCmdEndRenderPass(cmd);

    return vk.vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

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

        // --- Render pass + shader modules + graphics pipeline + framebuffers (S165)
        // Consumes the swapchain's color/depth formats + extent + image views.
        // No command buffer / no present yet — that is S166.
        if (vk.has_render_loader())
        {
            const auto colorFormat = ctx.swapchain->color_format();
            const auto depthFormat = ctx.swapchain->depth_format();

            ctx.renderPass = vaml::vulkan::RenderPass::create(
                vk, ctx.device, colorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT);
            ctx.vertexModule   = vaml::vulkan::ShaderModule::create(vk, ctx.device, vaml::vulkan::tri_vertex_spirv());
            ctx.fragmentModule = vaml::vulkan::ShaderModule::create(vk, ctx.device, vaml::vulkan::tri_fragment_spirv());

            if (ctx.renderPass && ctx.vertexModule && ctx.fragmentModule)
            {
                vaml::vulkan::PipelineSpec spec{};
                spec.renderPass     = ctx.renderPass->render_pass();
                spec.colorFormat    = colorFormat;
                spec.hasDepth       = depthFormat.has_value();
                spec.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
                spec.extent         = ctx.swapchain->extent();
                spec.vertexModule   = ctx.vertexModule->shader_module();
                spec.fragmentModule = ctx.fragmentModule->shader_module();
                ctx.pipeline = vaml::vulkan::Pipeline::create(vk, ctx.device, spec);

                if (ctx.pipeline)
                {
                    for (VkImageView view : ctx.swapchain->image_views())
                    {
                        ctx.framebuffers.push_back(vaml::vulkan::Framebuffer::create(
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

                    // --- Command pool (one buffer per swapchain image) + FrameSync
                    // (in-flight semaphores + fences) — S166. Consumes the S166 loader
                    // entries + the graphics queue family.
                    if (vk.has_command_loader())
                    {
                        ctx.commandPool = vaml::vulkan::CommandPool::create(
                            vk, ctx.device, ctx.queueFamily, ctx.swapchain->image_count());
                        ctx.frameSync = vaml::vulkan::FrameSync::create(
                            vk, ctx.device,
                            vaml::vulkan::choose_concurrency(ctx.swapchain->image_count()));
                        if (ctx.commandPool && ctx.frameSync)
                            std::printf("[s166] command pool (%u buffers) + frame sync (%u in-flight frames)\n",
                                        ctx.commandPool->count(), ctx.frameSync->frame_count());
                        else
                            std::fprintf(stderr, "[s166] CommandPool/FrameSync::create failed\n");
                    }
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

    // --- S166 main-thread draw loop (single-thread proof) -----------------------
    // When the surface + command loader + all render resources are live, each loop
    // iteration records + submits + presents ONE frame: acquire -> wait in-flight
    // fence -> reset -> record (the S166 draw_record_plan body) -> submit (waits the
    // image-available semaphore, signals render-finished + the fence) -> present
    // (waits render-finished). On the headless VM the swapchain is null, so this is
    // skipped ([both apps, not CI] manual proof).
    std::uint32_t frameIndex = 0;
    while (!window->should_close())
    {
        window->poll_events();  // pumps GLFW + forwards into the callbacks

        if (ctx.swapchain && ctx.commandPool && ctx.frameSync && ctx.pipeline &&
            !ctx.framebuffers.empty() && vk->has_command_loader())
        {
            const auto& vkLoad = *vk;
            const std::uint32_t frames = ctx.frameSync->frame_count();
            const std::uint32_t f      = frameIndex % frames;

            // Acquire the next swapchain image (waiting imageAvailable[f]).
            std::uint32_t imageIndex = 0;
            VkResult acq = vkLoad.vkAcquireNextImageKHR(
                ctx.device, ctx.swapchain->swapchain(), UINT64_MAX,
                ctx.frameSync->image_available(f),
                VK_NULL_HANDLE, &imageIndex);
            if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR)
                continue;  // skip this iteration (e.g. out-of-date during resize)

            // Wait the in-flight fence for this frame's slot, then reset it.
            const VkFence inFlight = ctx.frameSync->in_flight_fence(f);
            vkLoad.vkWaitForFences(ctx.device, 1, &inFlight, VK_TRUE, UINT64_MAX);
            vkLoad.vkResetFences(ctx.device, 1, &inFlight);

            VkCommandBuffer cmd = ctx.commandPool->command_buffer(f);
            VkFramebuffer fb = ctx.framebuffers[imageIndex]->framebuffer();

            if (!vamlpoc::record_draw_frame(vkLoad, cmd, ctx.pipeline->pipeline(),
                                            ctx.pipeline->pipeline_layout(),
                                            ctx.renderPass->render_pass(), fb,
                                            ctx.swapchain->extent(),
                                            ctx.swapchain->depth_format().has_value()))
            {
                continue;
            }

            // Submit: waits imageAvailable (color-attachment stage), signals
            // renderFinished + the in-flight fence.
            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSemaphore waitSem = ctx.frameSync->image_available(f);
            VkSemaphore signalSem = ctx.frameSync->render_finished(f);
            VkSubmitInfo submit{};
            submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.waitSemaphoreCount   = 1;
            submit.pWaitSemaphores      = &waitSem;
            submit.pWaitDstStageMask    = &waitStage;
            submit.commandBufferCount   = 1;
            submit.pCommandBuffers      = &cmd;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores    = &signalSem;
            if (vkLoad.vkQueueSubmit(ctx.graphicsQueue, 1, &submit, inFlight) != VK_SUCCESS)
                continue;

            // Present: waits renderFinished on the swapchain's acquired image.
            VkSwapchainKHR swapchain = ctx.swapchain->swapchain();
            VkSemaphore renderDone = ctx.frameSync->render_finished(f);
            VkPresentInfoKHR present{};
            present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores    = &renderDone;
            present.swapchainCount     = 1;
            present.pSwapchains        = &swapchain;
            present.pImageIndices      = &imageIndex;
            vkLoad.vkQueuePresentKHR(ctx.graphicsQueue, &present);  // result benign: skip on OOD

            ++frameIndex;
        }
    }

    // Wait for any in-flight submit before destroying the CommandPool/FrameSync so
    // nothing references them during teardown.
    if (ctx.device && vk->vkDeviceWaitIdle)
        vk->vkDeviceWaitIdle(ctx.device);

    // Teardown (reverse order), matching Linux: device -> surface -> instance -> loader.
    ctx.destroy(*vk);
    return 0;
}
