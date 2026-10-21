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
#include <vaml/vulkan/quad/QuadPipeline.h>
#include <vaml/vulkan/quad/QuadTexture.h>
#include <vaml/vulkan/quad/QuadScene.h>

// Sprint 4B (S175): the OPTIONAL presentation framework (vaml-presentation) is
// dogfooded into this POC. The app DECLARES its scene (the S165 triangle via a
// Presenter DrawBody) + its resize/recovery callbacks; the PRESENTER OWNS the
// loop (acquire/submit/present), the S166 command pool + frame sync, the S167_1
// SwapchainStateMachine + two-generation recovery, and the resize transaction.
// The app no longer touches the swapchain, the command pool, the frame sync, or
// the acquire/present handles — that is the S173 "app DECLARES; the framework
// owns loop/composition/recovery" contract.
#include <vaml/presentation/Presenter.h>
#include <vaml/composition/CompositionEngine.h>

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
    // S175: the S173 framework Presenter — the ONE owner of the loop, the S166
    // command pool + frame sync, and the S167_1 SwapchainStateMachine (behind
    // OnPresentationLost + the stale-ghost). The app NEVER owns a raw swapchain /
    // command pool / frame sync / framebuffer here anymore. Destroyed before the
    // render pass + device.
    std::unique_ptr<vaml::presentation::Presenter> presenter;
    // S167 resize state: the latest framebuffer (physical) px reported by the
    // S160 on_resize hook, + a pending flag the app forwards to the presenter as a
    // resize REQUEST (the framework applies the two-generation transaction).
    int                                          framebufferW  = 0;
    int                                          framebufferH  = 0;
    bool                                         resizePending = false;
    // S165 render resources (size-independent; survive every swapchain recreation).
    std::unique_ptr<vaml::vulkan::RenderPass>    renderPass;
    std::unique_ptr<vaml::vulkan::ShaderModule>  vertexModule;
    std::unique_ptr<vaml::vulkan::ShaderModule>  fragmentModule;
    std::unique_ptr<vaml::vulkan::Pipeline>      pipeline;

    void destroy()
    {
        // Destroy order: presenter (its owned machine + S166 resources + the loop) ->
        // pipeline -> shader modules -> render pass -> device. The App must have
        // device-waited before destroy() so the presenter's loop is idle (nothing in
        // flight references its machine/command pool/frame sync). The S169 Device's
        // dtor (vkDestroyDevice) runs LAST of the Vulkan objects.
        presenter.reset();  // machine -> command pool -> frame sync (reverse build)
        pipeline.reset();
        vertexModule.reset();
        fragmentModule.reset();
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

        // --- S175: the framework PRESENTER (the ONE owner of the loop) -------
        // The S173 framework wraps the S167_1 SwapchainStateMachine (built at the S168
        // sample count through the REAL builders) + the S166 CommandPool + FrameSync
        // behind ONE owner. The app DECLARES only its render objects (render pass +
        // shader modules + pipeline, above) and its DrawBody (below); it NEVER touches
        // the swapchain, command pool, frame sync, or acquire/present handles. The
        // render pass must outlive the presenter — both are destroyed before the device.
        if (ctx.renderPass && ctx.pipeline)
        {
            vaml::presentation::PresenterOptions presenterOpts;
            presenterOpts.width       = kWindowW;
            presenterOpts.height      = kWindowH;
            presenterOpts.sampleCount = sampleCount;
            presenterOpts.userData    = &ctx;
            presenterOpts.onPresentationLost = [](void* userData, vaml::vulkan::SwapchainState /*state*/) {
                auto* c = static_cast<VulkanCtx*>(userData);
                std::fprintf(stderr,
                             "[s175] OnPresentationLost: the framework does NOT auto-rebirth the device; "
                             "the app owns recovery (deferred-not-silent). Requesting teardown.\n");
                c->presenter.reset();  // no auto-rebirth — the app decides; for this POC, teardown.
            };
            presenterOpts.onFeedback = [](void* /*userData*/, vaml::vulkan::FeedbackKind fb) {
                std::printf("[s175] stale-ghost feedback: %s\n",
                            fb == vaml::vulkan::FeedbackKind::RetryableFrost ? "retryable-frost (Gen N live)" :
                            fb == vaml::vulkan::FeedbackKind::FatalWhite     ? "fatal-white" : "none");
            };
            // The app DECLARES its per-frame scene as a DrawBody: it RECORDS the S165
            // triangle onto the presenter-owned `cmd` for the acquired `imageIndex`. The
            // presenter owns acquire -> submit -> present.
            presenterOpts.draw = [](void* userData, vaml::presentation::Presenter& self,
                                    VkCommandBuffer cmd, std::uint32_t imageIndex) -> bool {
                auto* c = static_cast<VulkanCtx*>(userData);
                const auto& vkLoad = self.loader();
                if (!c->pipeline || !c->renderPass)
                    return false;
                const vaml::vulkan::Framebuffer* fb = self.machine().framebuffer(imageIndex);
                if (!fb)
                    return false;
                return record_draw_frame(
                    vkLoad, cmd,
                    c->pipeline->pipeline(), c->pipeline->pipeline_layout(),
                    c->renderPass->render_pass(), fb->framebuffer(),
                    self.extent(),
                    self.machine().depth_format().has_value(),
                    c->renderPass->config().uses_msaa_resolve());
            };

            ctx.presenter = vaml::presentation::Presenter::create(
                vk, *ctx.device, *surface, ctx.renderPass->render_pass(), presenterOpts);
            if (ctx.presenter)
                std::printf("[s175] Presenter created; machine gen0 built: %u images, extent %ux%u, state=%d, msaa=%u\n",
                            ctx.presenter->machine().image_count(), ctx.presenter->extent().width,
                            ctx.presenter->extent().height,
                            static_cast<int>(ctx.presenter->state()),
                            static_cast<unsigned>(ctx.presenter->machine().sample_count()));
            else
                std::fprintf(stderr, "[s175] Presenter::create failed (no Gen N to fall back to — startup)\n");
        }
    }
    return gpu;
}

// S175 fixture: the composition dest-rect for the full window (a single-pane pane).
// The composition engine (vaml::composition) is dogfooded: pane/splitter resize =
// the dest-rect CHANGE -> viewport/scissor recompute -> ZERO object churn (the S174
// engineering-out). This POC renders a SINGLE full-window pane; the splitter-app
// composition (multiple panes over a fixed virtual surface) is the S175 dogfood
// direction covered in Composition_test.cpp. The pure clip/reserve/viewport math is
// exercised headlessly.
inline vaml::composition::DestRect full_window_dest(int w, int h)
{
    vaml::composition::DestRect r;
    r.x      = 0;
    r.y      = 0;
    r.width  = w > 0 ? static_cast<std::uint32_t>(w) : 0u;
    r.height = h > 0 ? static_cast<std::uint32_t>(h) : 0u;
    return r;
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

    // --- S200/S201 live quad proof (device-only; no surface required) -----------
    // Like the S165 device-only proof + the S165-live block above, the QUAD surface
    // (QuadPipeline + QuadTexture) only needs a DEVICE, so it proves live here even
    // without a surface: the critical S200 risk is that the EMBEDDED QUAD SPIR-V
    // (quad_vertex / quad_fragment) is accepted by a real driver (vkCreateShaderModule)
    // + a real quad pipeline create (alpha blend + SRGB color format + the 80-byte
    // QuadUniform push constant) + a real device-local texture create (the minimal
    // staging → device-local copy). The surface-dependent N-quad frame is the
    // display-backed manual step ([both apps, not CI]).
    if (vk->has_render_loader() && vk->has_quad_loader() && vk->has_texture_loader() && ctx.device)
    {
        using namespace vaml::vulkan;
        using namespace vaml::vulkan::quad;
        auto qrp = vaml::vulkan::RenderPass::create(
            *vk, *ctx.device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT);
        auto qvm = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, quad_vertex_spirv());
        auto qfm = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, quad_fragment_spirv());
        if (qrp && qvm && qfm)
        {
            vaml::vulkan::quad::QuadPipelineSpec qspec{};
            qspec.renderPass     = qrp->render_pass();
            qspec.colorFormat    = VK_FORMAT_B8G8R8A8_SRGB;  // the S200 SRGB pairing
            qspec.hasDepth       = true;
            qspec.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
            qspec.extent         = VkExtent2D{ 800, 600 };
            qspec.vertexModule   = qvm->shader_module();
            qspec.fragmentModule = qfm->shader_module();
            auto qpl = vaml::vulkan::quad::QuadPipeline::create(*vk, *ctx.device, qspec);
            auto qtex = vaml::vulkan::quad::QuadTexture::create_procedural(*vk, *ctx.device, 8, 8, 2);
            if (qpl && qtex)
                std::printf("[s200-live] quad pipeline (alpha blend + SRGB + 80B uniform) "
                            "+ device-local texture created on %s (embedded quad SPIR-V accepted)\n",
                            gpuName.c_str());
            else
                std::fprintf(stderr, "[s200-live] QuadPipeline/QuadTexture::create failed on %s\n",
                             gpuName.c_str());
        }
        else
        {
            std::fprintf(stderr, "[s200-live] Quad RenderPass/ShaderModule::create failed (quad SPIR-V rejected?)\n");
        }
    }

    // --- S175 change-driven loop (the framework owns it) ---------------------------------------------
    // The app NEVER touches the swapchain/command pool/frame sync/acquire/present.
    // It just DECLARES: an invalidation flag (a resize is a dest-rect change -> the
    // composition engine recomputes the viewport/scissor -> ZERO object churn; the
    // S174 engineering-out) + a tick, and calls presenter->tick(invalidated, tickDue).
    // The PRESENTER owns acquire -> record (via the app's DrawBody) -> submit ->
    // present + the resize/out-of-date recreation + OnPresentationLost recovery.
    // On the headless VM the presenter is null (no surface), so this is skipped
    // ([both apps, not CI] manual proof).
    bool tickDue = true;  // v1: the POC ticks every iteration (a would-be WPF app may
                          // tick only on a timer); the presenter is CHANGE-driven —
                          // idle-only iterations still skip the render when appropriate.
    while (!window->should_close())
    {
        window->poll_events();  // pumps GLFW + forwards into the callbacks
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            window->request_close();

        bool invalidated = false;

        // S175: a resize is a REQUEST to the presenter (the framework applies the
        // two-generation transaction + the S174 dest-rect recompute). The resize
        // flag marks the frame invalid so the presenter re-renders at the new size.
        if (ctx.resizePending)
        {
            ctx.resizePending = false;
            const auto dest = full_window_dest(ctx.framebufferW, ctx.framebufferH);
            if (!vaml::composition::clip_to_zero(dest.width, dest.height))
            {
                // A non-minimized resize -> the presenter refreshes the swapchain to
                // the new framebuffer size (the swapchain extent source), then the
                // S174 dest-rect recompute maps the virtual surface with ZERO churn.
                if (ctx.presenter)
                    ctx.presenter->request_resize(dest.width, dest.height);
            }
            // (A minimized/framebuffer-zero pane is clipped to zero — nothing to
            // draw, no churn (S174). The presenter parks on a zero extent next tick.)
            invalidated = true;
        }

        if (ctx.presenter)
            (void)ctx.presenter->tick(invalidated, tickDue);  // the framework owns the loop
    }

    // Wait for any in-flight submit before destroying the presenter + CommandPool/
    // FrameSync so nothing references them during teardown.
    if (ctx.device && vk->vkDeviceWaitIdle)
        vk->vkDeviceWaitIdle(ctx.device->device());

    ctx.destroy();
    return 0;
}
