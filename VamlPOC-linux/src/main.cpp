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
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/SwapchainState.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/Pipeline.h>
#include <vaml/vulkan/Framebuffer.h>
#include <vaml/vulkan/CommandBuffer.h>
#include <vaml/vulkan/FrameSync.h>
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

// The S166 clear color (light/gray) so the blue S165 triangle is visible against it.
inline constexpr float kClearColor[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

// Small RAII-closing Vulkan context holder. The S169 Device (unique_ptr) owns
// the raw VkDevice + VkQueue — the app NEVER owns a raw Vulkan handle. Holds
// handles long enough for the demo loop, destroys in reverse order.
struct VulkanCtx
{
    // S169: the logical-device + graphics-queue RAII owner (owns the raw
    // VkDevice/VkQueue; the app borrows device()/graphics_queue()/queue_family()).
    std::unique_ptr<vaml::vulkan::Device>    device;
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

    void destroy()
    {
        // Destroy order: FrameSync -> CommandPool -> pipeline -> shader modules ->
        // swapchain(s) -> render pass -> device. The draw loop must be idle before
        // these — destroy() is called after the loop's device-wait. The
        // SwapchainStateMachine's dtor destroys its per-image framebuffers FIRST
        // (they reference the render pass + the swapchain's image views), then its
        // swapchain — so the machine must be destroyed BEFORE the render pass, and
        // before the device (teardown-order-reverse). The S169 Device's dtor
        // (vkDestroyDevice) runs LAST of the Vulkan objects.
        frameSync.reset();
        commandPool.reset();
        pipeline.reset();
        vertexModule.reset();
        fragmentModule.reset();
        swapchain.reset();  // framebuffers -> swapchain (reverse build order)
        renderPass.reset();
        device.reset();     // S169: vkDestroyDevice, the device must outlive all borrowed handles
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
    const std::uint32_t familyIndex = *queueFamily;

    // --- Logical device + graphics queue (S169: the Device RAII owner) ----
    // S169 deletes the raw vkCreateDevice/vkGetDeviceQueue block from the app.
    // The Device owner constructs the logical device on the chosen physical
    // device, enables the S164 swapchain extension, and owns the VkDevice +
    // graphics VkQueue. `familyIndex` is passed through. Returns nullptr (no
    // throw) on failure.
    ctx.device = vaml::vulkan::Device::create(vk, gpu, familyIndex);
    if (!ctx.device)
        throw std::runtime_error("Device::create failed");

    // --- Swapchain + image views + depth + per-image framebuffers (S164/S165/S167_1)
    // Consumes the surface + the S163 device + the S164 loader entries. The
    // framebuffer size comes from the window; the S164 decision seams negotiate
    // the format/BPP/present-mode/extent/depth.
    if (surface && vk.has_swapchain_loader())
    {
        const auto cfg = Swapchain::negotiate(vk, gpu.device, *surface, kWindowW, kWindowH);
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
        VkPhysicalDeviceProperties props{};
        if (vk.vkGetPhysicalDeviceProperties)
            vk.vkGetPhysicalDeviceProperties(gpu.device, &props);
        const VkSampleCountFlagBits sampleCount = choose_sample_count(
            props.limits.framebufferColorSampleCounts,
            props.limits.framebufferDepthSampleCounts);
        std::printf("[s168] MSAA sampleCount=%u\n", static_cast<unsigned>(sampleCount));

        // --- Render pass + shader modules + graphics pipeline (S165/S168) -----
        // These are size-independent (the render area comes from the extent at record
        // time) so they are built ONCE and SURVIVE every swapchain recreation (S167_1).
        // The render pass is needed by the SwapchainStateMachine's per-image framebuffer
        // builder. S168: built at the negotiated sample count (multisampled + resolve).
        if (vk.has_render_loader())
        {
            ctx.renderPass = RenderPass::create(vk, *ctx.device, colorFormat.format, depthFormat, sampleCount);
            ctx.vertexModule   = ShaderModule::create(vk, *ctx.device, tri_vertex_spirv());
            ctx.fragmentModule = ShaderModule::create(vk, *ctx.device, tri_fragment_spirv());

            if (ctx.renderPass && ctx.vertexModule && ctx.fragmentModule)
            {
                PipelineSpec spec{};
                spec.renderPass     = ctx.renderPass->render_pass();
                spec.colorFormat    = colorFormat.format;
                spec.hasDepth       = depthFormat.has_value();
                spec.sampleCount    = sampleCount;
                spec.extent         = cfg ? cfg->extent : VkExtent2D{ kWindowW, kWindowH };
                spec.vertexModule   = ctx.vertexModule->shader_module();
                spec.fragmentModule = ctx.fragmentModule->shader_module();
                ctx.pipeline = Pipeline::create(vk, *ctx.device, spec);
            }
        }

        // --- SwapchainStateMachine: the S167_1 two-generation atomic owner ----
        // Builds Gen 0 (swapchain + image views + depth + per-image framebuffers [+
        // S168 multisampled target]) and OWNS every presentation-facing handle + the
        // SwapchainState machine. Callers never touch a raw swapchain. The REAL
        // builders use Swapchain::create + Framebuffer::create (+ the render pass) and,
        // at the S168 negotiated sample count, build the multisampled target. If the
        // initial generation cannot be built there is no Gen N to fall back to — treat
        // as startup failure, never present a null handle.
        if (ctx.renderPass && ctx.pipeline)
        {
            ctx.swapchain = SwapchainStateMachine::create(
                SwapchainStateBuilders::real(
                    vk, *ctx.device, *surface, ctx.renderPass->render_pass(), sampleCount),
                kWindowW, kWindowH, sampleCount);
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
                ctx.commandPool = CommandPool::create(
                    vk, *ctx.device, ctx.device->queue_family(), ctx.swapchain->image_count());
                ctx.frameSync = FrameSync::create(
                    vk, *ctx.device, choose_concurrency(ctx.swapchain->image_count()));
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

// S167_1 — rebuild the swapchain + dependent framebuffers at a new framebuffer
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

int main()
{
    // --- vaml-core consumer check -------------------------------------------
    const auto ver = vaml::version();
    std::printf("VamlPOC-linux: linking vaml-core %d.%d.%d (%s)\n",
                ver.major, ver.minor, ver.patch,
                std::string(vaml::engine_name()).c_str());

    // --- Platform window via vaml::vulkan::PlatformWindow (S160) -------------
    // The VulkanCtx is declared before the callbacks so the S167 on_resize hook
    // (which records the new framebuffer size + flags a pending swapchain
    // recreate) can capture it by reference.
    VulkanCtx ctx;

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
            *vk, *ctx.device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
        auto vm = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::tri_vertex_spirv());
        auto fm = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::tri_fragment_spirv());
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
            auto pl = vaml::vulkan::Pipeline::create(*vk, *ctx.device, spec);
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
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            window->request_close();

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
                    vkLoad.vkDeviceWaitIdle(ctx.device->device());
                if (!recreateSwapchain(vkLoad, ctx))
                    continue;
            }

            const std::uint32_t frames = ctx.frameSync->frame_count();
            const std::uint32_t f      = frameIndex % frames;

            // Acquire the next swapchain image (waiting imageAvailable[f]).
            std::uint32_t imageIndex = 0;
            VkResult acq = vkLoad.vkAcquireNextImageKHR(
                ctx.device->device(), ctx.swapchain->handle(), UINT64_MAX,
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
                vkLoad.vkDeviceWaitIdle(ctx.device->device());
                recreateSwapchain(vkLoad, ctx);
                continue;
            }
            if (acq != VK_SUCCESS)
                continue;

            // Wait the in-flight fence for this frame's slot, then reset it.
            const VkFence inFlight = ctx.frameSync->in_flight_fence(f);
            vkLoad.vkWaitForFences(ctx.device->device(), 1, &inFlight, VK_TRUE, UINT64_MAX);
            vkLoad.vkResetFences(ctx.device->device(), 1, &inFlight);

            VkCommandBuffer cmd = ctx.commandPool->command_buffer(f);
            VkFramebuffer fb = ctx.swapchain->framebuffer(imageIndex)->framebuffer();

            if (!record_draw_frame(vkLoad, cmd, ctx.pipeline->pipeline(),
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
            if (vkLoad.vkQueueSubmit(ctx.device->graphics_queue(), 1, &submit, inFlight) != VK_SUCCESS)
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
            VkResult pres = vkLoad.vkQueuePresentKHR(ctx.device->graphics_queue(), &present);

            // S167_1: an out-of-date/suboptimal present also means run the recreate
            // TRANSACTION at the new size (the S160 on_resize path may have raced the
            // present). DEVICE/SURFACE_LOST escalates to Fatal (engine stops).
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
                vkLoad.vkDeviceWaitIdle(ctx.device->device());
                recreateSwapchain(vkLoad, ctx);
                continue;
            }

            ++frameIndex;
        }
    }

    // Wait for any in-flight submit before destroying the CommandPool/FrameSync so
    // nothing references them during teardown.
    if (ctx.device && vk->vkDeviceWaitIdle)
        vk->vkDeviceWaitIdle(ctx.device->device());

    ctx.destroy();
    return 0;
}
