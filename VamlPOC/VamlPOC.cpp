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
#include <vaml/vulkan/SwapchainState.h>
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
    // S167 resize state: the latest framebuffer (physical) px reported by the
    // S160 on_resize hook, + a pending flag the draw loop checks each iteration.
    int                                          framebufferW  = 0;
    int                                          framebufferH  = 0;
    bool                                         resizePending = false;
    // S167_1: the presentation-facing owner (two-generation atomic swapchain +
    // image views + depth + per-image framebuffers + SwapchainState). Destroyed
    // before the device. The app NEVER owns a raw swapchain / framebuffer here.
    std::unique_ptr<vaml::vulkan::SwapchainStateMachine> swapchain;
    // S165 render resources (constructed after the swapchain, destroyed before it).
    std::unique_ptr<vaml::vulkan::RenderPass>    renderPass;
    std::unique_ptr<vaml::vulkan::ShaderModule>  vertexModule;
    std::unique_ptr<vaml::vulkan::ShaderModule>  fragmentModule;
    std::unique_ptr<vaml::vulkan::Pipeline>      pipeline;
    // S166 draw-loop resources (constructed after the framebuffers, destroyed before
    // the device — after the last submit + a device wait so nothing is in flight).
    // The per-image framebuffers are OWNED BY the SwapchainStateMachine (S167_1),
    // so they are no longer a VulkanCtx member here.
    std::unique_ptr<vaml::vulkan::CommandPool>   commandPool;
    std::unique_ptr<vaml::vulkan::FrameSync>     frameSync;

    void destroy(const vaml::vulkan::VulkanLoader& vk)
    {
        // Destroy order: FrameSync -> CommandPool -> pipeline -> shader modules ->
        // swapchain(s) -> render pass -> device. The draw loop must be idle before
        // these — destroy() is called after the loop's device-wait. The
        // SwapchainStateMachine's dtor destroys its per-image framebuffers FIRST
        // (they reference the render pass + the swapchain's image views), then its
        // swapchain — so the machine must be destroyed BEFORE the render pass, and
        // before the device (teardown-order-reverse).
        frameSync.reset();
        commandPool.reset();
        pipeline.reset();
        vertexModule.reset();
        fragmentModule.reset();
        swapchain.reset();  // framebuffers -> swapchain (reverse build order)
        renderPass.reset();
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
                       bool hasDepth,
                       bool hasResolve)
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
    const auto rpBegin = build_render_pass_begin(renderPass, framebuffer, extent, clear, hasDepth, hasResolve);
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

    // --- Swapchain + image views + depth + per-image framebuffers (S164/S165/S167_1)
    // Consumes the surface + the S163 device + the S164 loader entries. The
    // S164 decision seams negotiate the format/BPP/present-mode/extent/depth.
    if (surface && vk.has_swapchain_loader())
    {
        const auto cfg = vaml::vulkan::Swapchain::negotiate(
            vk, gpu.device, *surface, vamlpoc::kWindowW, vamlpoc::kWindowH);
        const VkSurfaceFormatKHR colorFormat = cfg ? cfg->surfaceFormat
                                                   : VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB,
                                                                        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        const std::optional<VkFormat> depthFormat = cfg ? cfg->depthFormat : std::nullopt;
        std::printf("[swapchain] negotiated BPP=%u present=%d extent=%ux%u count=%u depth=%s\n",
                    cfg ? cfg->surface_bytes_per_pixel() : 0u,
                    cfg ? static_cast<int>(cfg->presentMode) : -1,
                    cfg ? cfg->extent.width : 0u,
                    cfg ? cfg->extent.height : 0u,
                    cfg ? cfg->imageCount : 0u,
                    depthFormat.has_value() ? "yes" : "no");

        // --- S168: negotiate the MSAA sample count from the physical device ----
        // The largest candidate the device supports in BOTH the color and depth
        // framebuffer sample-count sets; 1_BIT (the non-MSAA path) when nothing
        // higher is mutually supported.
        VkPhysicalDeviceProperties props{};
        if (vk.vkGetPhysicalDeviceProperties)
            vk.vkGetPhysicalDeviceProperties(gpu.device, &props);
        const VkSampleCountFlagBits sampleCount = vaml::vulkan::choose_sample_count(
            props.limits.framebufferColorSampleCounts,
            props.limits.framebufferDepthSampleCounts);
        std::printf("[s168] MSAA sampleCount=%u\n", static_cast<unsigned>(sampleCount));

        // --- Render pass + shader modules + graphics pipeline (S165/S168) -----
        // These are size-independent (the render area comes from the extent at record
        // time) so they are built ONCE and SURVIVE every swapchain recreation (S167_1).
        // The render pass is needed by the SwapchainStateMachine's per-image framebuffer
        // builder. S168: the render pass + pipeline are built at the negotiated
        // sampleCount (a multisampled pass resolves into the swapchain image).
        if (vk.has_render_loader())
        {
            ctx.renderPass = vaml::vulkan::RenderPass::create(
                vk, ctx.device, colorFormat.format, depthFormat, sampleCount);
            ctx.vertexModule   = vaml::vulkan::ShaderModule::create(vk, ctx.device, vaml::vulkan::tri_vertex_spirv());
            ctx.fragmentModule = vaml::vulkan::ShaderModule::create(vk, ctx.device, vaml::vulkan::tri_fragment_spirv());

            if (ctx.renderPass && ctx.vertexModule && ctx.fragmentModule)
            {
                vaml::vulkan::PipelineSpec spec{};
                spec.renderPass     = ctx.renderPass->render_pass();
                spec.colorFormat    = colorFormat.format;
                spec.hasDepth       = depthFormat.has_value();
                spec.sampleCount    = sampleCount;
                spec.extent         = cfg ? cfg->extent : VkExtent2D{ vamlpoc::kWindowW, vamlpoc::kWindowH };
                spec.vertexModule   = ctx.vertexModule->shader_module();
                spec.fragmentModule = ctx.fragmentModule->shader_module();
                ctx.pipeline = vaml::vulkan::Pipeline::create(vk, ctx.device, spec);
            }
        }

        // --- SwapchainStateMachine: the S167_1 two-generation atomic owner ----
        // Builds Gen 0 (swapchain + image views + depth + per-image framebuffers) and
        // OWNS every presentation-facing handle + the SwapchainState machine. Callers
        // never touch a raw swapchain. The REAL builders use Swapchain::create +
        // Framebuffer::create (+ the render pass) and, at the S168 negotiated sample
        // count, build the multisampled target. If the initial generation cannot be
        // built there is no Gen N to fall back to — treat as startup failure, never
        // present a null handle.
        if (ctx.renderPass && ctx.pipeline)
        {
            ctx.swapchain = vaml::vulkan::SwapchainStateMachine::create(
                vaml::vulkan::SwapchainStateBuilders::real(
                    vk, ctx.device, gpu.device, *surface, ctx.renderPass->render_pass(), sampleCount),
                vamlpoc::kWindowW, vamlpoc::kWindowH, sampleCount);
            if (ctx.swapchain)
                std::printf("[s167_1] SwapchainStateMachine gen0 built: %u images, extent %ux%u, state=%d, msaa=%u\n",
                            ctx.swapchain->image_count(), ctx.swapchain->extent().width,
                            ctx.swapchain->extent().height, static_cast<int>(ctx.swapchain->state()),
                            static_cast<unsigned>(ctx.swapchain->sample_count()));
            else
                std::fprintf(stderr, "[s167_1] SwapchainStateMachine::create failed\n");

            // --- Command pool (one buffer per swapchain image) + FrameSync ----
            // These are FRAME-SLOT-indexed (independent of image count) so they survive
            // every swapchain recreation unchanged (S167_1 — the machine does NOT touch
            // them). S166.
            if (vk.has_command_loader() && ctx.swapchain)
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
    }
    return gpu;
}

// S167 — rebuild the swapchain + dependent framebuffers at a new framebuffer
// size (called from the draw loop on a resize/out-of-date signal). Assumes the
// caller has already vkDeviceWaitIdle'd the device (the draw loop does). The
// command pool + frame sync are FRAME-SLOT-indexed (independent of image count)
// so they survive the recreation unchanged; the pipeline + render pass are size-
// independent so they survive too. IDENTICAL in both apps.
//
// S167_1 (the corrective to S167): this is now a TRANSACTIONAL change. The
// SwapchainStateMachine builds Gen N+1 (swapchain + image views + depth + per-image
// framebuffers) into FRESH temporaries and COMMITS only on full success; on ANY
// failure only the stillborn N+1 is destroyed and Gen N stays live + presentable
// (machine lands Retryable). The old destroy-then-create Swapchain::recreate is
// GONE. Returns true if the machine is Ready to draw (a fresh Gen N+1 or Gen N
// still live); false if the rebuild failed (Retryable) — the loop keeps drawing
// Gen N under the stale-ghost frost and retries next time.
bool recreateSwapchain(const vaml::vulkan::VulkanLoader& vk, VulkanCtx& ctx)
{
    using namespace vaml::vulkan;
    (void)vk;  // S167_1: the machine owns the swapchain+framebuffer build (framebuffers
               // moved inside it); the loader is no longer needed here for a manual rebuild.
    if (!ctx.swapchain)
        return false;
    const std::uint32_t w = ctx.framebufferW > 0 ? static_cast<std::uint32_t>(ctx.framebufferW) : 1u;
    const std::uint32_t h = ctx.framebufferH > 0 ? static_cast<std::uint32_t>(ctx.framebufferH) : 1u;

    // Zero/minimized extent -> park into Suspended (nothing to present until a
    // resize re-drives). The machine's prepare_recreate handles this internally.
    if (!ctx.swapchain->prepare_recreate(w, h))
    {
        if (ctx.swapchain->state() == SwapchainState::Retryable)
            std::fprintf(stderr, "[s167_1] Gen N+1 rebuild failed at %ux%u -> Retryable (Gen N still live)\n", w, h);
        // Suspended -> quiet park; Fatal -> the feedback already surfaces it.
        return ctx.swapchain->state() == SwapchainState::Ready;
    }
    std::printf("[s167_1] transactional recreate COMMITTED: %u images, extent %ux%u, state=%d\n",
                ctx.swapchain->image_count(), ctx.swapchain->extent().width,
                ctx.swapchain->extent().height, static_cast<int>(ctx.swapchain->state()));
    return true;
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
    // The VulkanCtx is declared before the callbacks so the S167 on_resize hook
    // (which records the new framebuffer size + flags a pending swapchain
    // recreate) can capture it by reference.
    vamlpoc::VulkanCtx ctx;

    vaml::vulkan::WindowCallbacks callbacks;
    callbacks.on_resize = [&ctx](int w, int h) {
        std::printf("[resize] framebuffer %dx%d\n", w, h);
        // S167: record the new framebuffer px (the swapchain extent source) and
        // flag the draw loop to recreate the swapchain at the new size.
        ctx.framebufferW = w;
        ctx.framebufferH = h;
        ctx.resizePending = true;
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

        // S167_1: the loop is CHANGE-DRIVEN + asks state() every iteration. Draw
        // ONLY when the machine is Ready (Gen N valid + presentable). Suspended parks
        // (zero/minimized extent — nothing to present); Retryable keeps drawing Gen N
        // (under the stale-ghost frost) until a rebuild commits; Fatal stops (the
        // app owns recovery). The per-image framebuffers live INSIDE the machine.
        if (ctx.swapchain && ctx.commandPool && ctx.frameSync && ctx.pipeline &&
            vk->has_command_loader() &&
            ctx.swapchain->state() == vaml::vulkan::SwapchainState::Ready)
        {
            const auto& vkLoad = *vk;

            // S167_1 — when a resize is pending, run the two-generation atomic
            // recreate TRANSACTION (quiesce first, then prepare_recreate). The
            // machine lands Ready (commit) or Retryable/Suspended (Gen N still
            // live); Retryable/Suspended skip this frame but keep the loop alive.
            if (ctx.resizePending)
            {
                ctx.resizePending = false;
                if (vkLoad.vkDeviceWaitIdle)
                    vkLoad.vkDeviceWaitIdle(ctx.device);
                if (!vamlpoc::recreateSwapchain(vkLoad, ctx))
                    continue;
            }

            const std::uint32_t frames = ctx.frameSync->frame_count();
            const std::uint32_t f      = frameIndex % frames;

            // Acquire the next swapchain image (waiting imageAvailable[f]).
            std::uint32_t imageIndex = 0;
            VkResult acq = vkLoad.vkAcquireNextImageKHR(
                ctx.device, ctx.swapchain->handle(), UINT64_MAX,
                ctx.frameSync->image_available(f),
                VK_NULL_HANDLE, &imageIndex);

            // S167_1: follow the state machine. An out-of-date/suboptimal acquire
            // means the swapchain no longer matches the surface — quiesce + run the
            // recreate TRANSACTION, then retry next iteration. DEVICE/SURFACE_LOST
            // escalates the machine to Fatal (engine stops + OnPresentationLost; the
            // app decides recovery) — never draw from an invalid generation.
            if (vk->vkDeviceWaitIdle &&
                vaml::vulkan::next_state(
                    ctx.swapchain->state(), acq, vaml::vulkan::should_suspend_extent(ctx.swapchain->extent()))
                == vaml::vulkan::SwapchainState::Fatal)
            {
                ctx.swapchain->mark_fatal();
                continue;
            }
            if (vk->vkDeviceWaitIdle && vaml::vulkan::should_recreate_swapchain(acq))
            {
                vkLoad.vkDeviceWaitIdle(ctx.device);
                vamlpoc::recreateSwapchain(vkLoad, ctx);
                continue;
            }
            if (acq != VK_SUCCESS)
                continue;

            // Wait the in-flight fence for this frame's slot, then reset it.
            const VkFence inFlight = ctx.frameSync->in_flight_fence(f);
            vkLoad.vkWaitForFences(ctx.device, 1, &inFlight, VK_TRUE, UINT64_MAX);
            vkLoad.vkResetFences(ctx.device, 1, &inFlight);

            VkCommandBuffer cmd = ctx.commandPool->command_buffer(f);
            VkFramebuffer fb = ctx.swapchain->framebuffer(imageIndex)->framebuffer();

            if (!vamlpoc::record_draw_frame(vkLoad, cmd, ctx.pipeline->pipeline(),
                                            ctx.pipeline->pipeline_layout(),
                                            ctx.renderPass->render_pass(), fb,
                                            ctx.swapchain->extent(),
                                            ctx.swapchain->depth_format().has_value(),
                                            ctx.renderPass->config().uses_msaa_resolve()))
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
            VkSwapchainKHR swapchain = ctx.swapchain->handle();
            VkSemaphore renderDone = ctx.frameSync->render_finished(f);
            VkPresentInfoKHR present{};
            present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores    = &renderDone;
            present.swapchainCount     = 1;
            present.pSwapchains        = &swapchain;
            present.pImageIndices      = &imageIndex;
            VkResult pres = vkLoad.vkQueuePresentKHR(ctx.graphicsQueue, &present);

            // S167_1: an out-of-date/suboptimal present also means run the recreate
            // TRANSACTION at the new size (the S160 on_resize path may have raced
            // the present). DEVICE/SURFACE_LOST escalates to Fatal (engine stops).
            if (vk->vkDeviceWaitIdle &&
                vaml::vulkan::next_state(
                    ctx.swapchain->state(), pres, vaml::vulkan::should_suspend_extent(ctx.swapchain->extent()))
                == vaml::vulkan::SwapchainState::Fatal)
            {
                ctx.swapchain->mark_fatal();
                continue;
            }
            if (vk->vkDeviceWaitIdle && vaml::vulkan::should_recreate_swapchain(pres))
            {
                vkLoad.vkDeviceWaitIdle(ctx.device);
                vamlpoc::recreateSwapchain(vkLoad, ctx);
                continue;
            }

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
