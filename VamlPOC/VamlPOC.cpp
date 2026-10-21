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
// S202_1: the icon asset pipeline — PNG + SVG are BOTH source formats, each
// rasterized ONCE into the S202 atlas (no vector-on-screen, no second renderer);
// output tagged with its texel count for S213_1 best-fit.
#include <vaml/vulkan/quad/TextureAtlas.h>
#include <vaml/vulkan/icon/IconPipeline.h>
#include <vaml/vulkan/icon/IconAtlas.h>
#include <vaml/vulkan/font/FontAtlas.h>
#include <vaml/vulkan/font/SdfAtlas.h>
// S205: the real UI shader set (SDF text + solid color + border) + descriptor
// sets on ONE shared layout (UiPipeline). Extends ShaderModule with the four
// embedded UI shaders; consumes the S204 SDF atlas + the S200 quad geometry.
#include <vaml/vulkan/ui/UiPipeline.h>

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
// them. The S169 Device (unique_ptr) owns the raw VkDevice + VkQueue — the APP
// never owns a raw Vulkan handle.
//
// S175 (Sprint 4B): the OPTIONAL framework (vaml-presentation) is dogfooded — the
// PRESENTER owns the swapchain machine (S167_1), the command pool + frame sync
// (S166), and the loop; the APP owns only the S165/S168 render objects (render
// pass, shader modules, pipeline) + the Device. The presenter is destroyed before
// the render pass/device (it wraps the machine + S166 resources which must be
// destroyed before them).
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
                vk, *ctx.device, colorFormat.format, depthFormat, sampleCount);
            ctx.vertexModule   = vaml::vulkan::ShaderModule::create(vk, *ctx.device, vaml::vulkan::tri_vertex_spirv());
            ctx.fragmentModule = vaml::vulkan::ShaderModule::create(vk, *ctx.device, vaml::vulkan::tri_fragment_spirv());

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
                ctx.pipeline = vaml::vulkan::Pipeline::create(vk, *ctx.device, spec);
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
            presenterOpts.width       = vamlpoc::kWindowW;
            presenterOpts.height      = vamlpoc::kWindowH;
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
                return vamlpoc::record_draw_frame(
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

// S175 fixture: a two-frame-per-tick composition dest-rect (the full window). The
// composition engine (vaml::composition) is dogfooded: pane/splitter resize = the
// dest-rect CHANGEs -> the viewport/scissor recompute -> ZERO object churn. This
// POC renders a SINGLE full-window pane; the splitter-app composition (multiple
// panes over a fixed virtual surface) is demonstrated in Composition_test.cpp +
// the Linux POC. The pure clip/reserve/viewport math is exercised headlessly.
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

    std::string gpuName;
    try
    {
        auto gpu = vamlpoc::createVulkanCtx(*vk, *instance, surface.get(), ctx);
        gpuName = gpu.name.empty() ? "<unnamed>" : gpu.name;
        std::printf("Running on GPU: %s; close the window to quit.\n", gpuName.c_str());
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Vulkan init error: %s\n", e.what());
        return 1;
    }

    // --- S202_1 live icon-pipeline proof (device-only; no surface required) --------
    // The S202_1 icon surface (icon::raster_icon + IconAtlas over the S202
    // TextureAtlas) only needs a DEVICE, so it proves live here even without a
    // surface: the HEADLINE risks are (1) the PNG decode path (stb_image) + the SVG
    // rasterize path (nanosvg) run on the real CPU-raster stack, (2) each source is
    // rasterized ONCE into an RGBA8 tile tagged with its TEXEL COUNT (the S213_1
    // best-fit input), (3) the icon tiles are uploaded into the S202 atlas via
    // upload_tile (each add_icon dirties ONLY its own tile — the S202 SUB-REGION
    // INVALIDATION preserved through the icon layer), and (4) the ADR-015 STRUCTURAL
    // SOURCE-SET RULE holds on a live driver. The presented sampling of a tile via
    // QuadScene UVs stays the display-backed manual step ([both apps, not CI]).
    if (vk->has_quad_loader() && vk->has_texture_loader() && vk->has_command_loader() && ctx.device)
    {
        using namespace vaml::vulkan::quad;
        using namespace vaml::vulkan::icon;
        // A 256x128 atlas as a 4x2 grid -> 32x64 tiles (256%4==0 && 128%2==0: exact).
        auto atlas = TextureAtlas::create(*vk, *ctx.device, 256, 128, 4, 2);
        if (atlas)
        {
            // Embedded fixtures (packaged assets — the ADR-015 boundary, never a path):
            // a 4x4 solid-red PNG + a 10x10 solid-red SVG square.
            using Bytes = std::vector<std::uint8_t>;
            const Bytes kPng = {
                137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,18,73,
                68,65,84,120,156,99,248,207,192,240,31,25,51,144,46,0,0,60,64,31,225,224,129,119,180,0,0,0,0,73,69,78,
                68,174,66,96,130,
            };
            const Bytes kSvg = {
                '<','s','v','g',' ','x','m','l','n','s','=','"','h','t','t','p',':','/','/','w','w','w','.','w','3','.',
                'o','r','g','/','2','0','0','0','/','s','v','g','"',' ','w','i','d','t','h','=','"','1','0','"',' ',
                'h','e','i','g','h','t','=','"','1','0','"','>','<','r','e','c','t',' ','w','i','d','t','h','=','"',
                '1','0','"',' ','h','e','i','g','h','t','=','"','1','0','"',' ','f','i','l','l','=','"','#','f','f',
                '0','0','0','0','"','/','>','<','/','s','v','g','>',
            };
            IconSourceSet sources;
            sources.emplace(1, IconSource{ IconFormat::Png, kPng });
            sources.emplace(2, IconSource{ IconFormat::Svg, kSvg });

            IconAtlas icons(*atlas);
            icons.set_sources(sources);
            // Rasterize the PNG + the SVG ONCE at the atlas tile size (32x64).
            IconErrorKind err = IconErrorKind::None;
            const bool pngOk = icons.add_icon(1, 32, 64, &err);
            const IconErrorKind pngErr = err;
            const bool svgOk = icons.add_icon(2, 32, 64, &err);
            const IconErrorKind svgErr = err;
            // Out-of-set key: the ADR-015 structural rule rejects it, no crash.
            const bool outOk = icons.add_icon(999, 32, 64, &err);
            const IconErrorKind outErr = err;
            icons.atlas().flush();
            // Each add_icon dirties ONLY its own tile; the texel count tag is the same
            // for both (32*64) — the S213_1 best-fit input.
            const IconSlot slotPng = icons.slot_for(1);
            const IconSlot slotSvg = icons.slot_for(2);
            if (pngOk && svgOk && !outOk && outErr == IconErrorKind::OutOfSet &&
                pngErr == IconErrorKind::None && svgErr == IconErrorKind::None &&
                slotPng.texelCount == 32u * 64u && slotSvg.texelCount == 32u * 64u &&
                slotPng.tileIndex != slotSvg.tileIndex)
                std::printf("[s202_1-live] icon pipeline: PNG + SVG rasterized ONCE into the S202 atlas "
                            "on %s (%u texels/tile PNG tile=%u SVG tile=%u; OutOfSet rejected), "
                            "flush uploaded %u tiles\n",
                            gpuName.c_str(), slotPng.texelCount, slotPng.tileIndex, slotSvg.tileIndex,
                            icons.slot_count());
            else
                std::fprintf(stderr, "[s202_1-live] icon pipeline MISMATCH on %s "
                            "(png=%d svg=%d outOk=%d outErr=%d slots=%u pngTx=%u svgTx=%u)\n",
                            gpuName.c_str(), pngOk, svgOk, outOk, static_cast<int>(outErr),
                            icons.slot_count(), slotPng.texelCount, slotSvg.texelCount);
        }
        else
        {
            std::fprintf(stderr, "[s202_1-live] IconAtlas::create (TextureAtlas) failed.\n");
        }
    }

    // --- S203+S204 live font/SDF-atlas proof (device-only; no surface required) --
    // The S203 FontAtlas (embedded TrueType parsed via stb_truetype + ASCII 32–126
    // coverage glyph atlas) + the S204 SdfAtlas (coverage → SDF into the S202 atlas
    // for soft-edge sampling) only need a DEVICE (over the S202 TextureAtlas), so
    // they prove live here even without a surface: the HEADLINE risks are
    // (1) the embedded default font PARSES (stbtt_InitFont) on the real CPU stack,
    // (2) each of the 95 printable-ASCII glyphs gets a rasterized COVERAGE tile
    // uploaded via the S202 sub-region upload (only its own tile dirties),
    // (3) S204 converts the coverage to a packed SDF tile per glyph (+ the
    // sdf_soft_alpha soft-edge value semantic), and (4) both are uploaded to a REAL
    // driver via flush(). The presented sampling of an SDF tile via QuadScene UVs
    // (the S205 text shader) stays the display-backed manual step.
    if (vk->has_quad_loader() && vk->has_texture_loader() && vk->has_command_loader() && ctx.device)
    {
        using namespace vaml::vulkan::quad;
        using namespace vaml::vulkan::font;
        // A 256x128 atlas as a 16x8 grid -> 16x16 tiles (256%16==0 && 128%8==0:
        // exact) — 128 tiles >= 95, the S203 glyph atlas requirement.
        auto atlas = TextureAtlas::create(*vk, *ctx.device, 256, 128, 16, 8);
        if (atlas)
        {
            FontAtlas glyphs(*atlas);
            const bool loaded = glyphs.load();           // parse embedded font + rasterize 95
            const std::uint32_t glyphTiles = glyphs.upload_glyphs();

            SdfAtlas sdfs(*atlas, glyphs);
            const bool sdfBuilt = sdfs.build();          // coverage -> SDF, 1 per glyph
            const std::uint32_t sdfTiles = sdfs.upload_sdfs();

            // A known glyph sanity: 'A' has a slot + a coverage tile + an SDF slot.
            const bool aHasGlyph = glyphs.has_slot(static_cast<std::uint32_t>('A'));
            const bool aHasSdf = sdfs.has_slot(static_cast<std::uint32_t>('A'));
            const bool metricsOk = glyph_metrics_valid(glyphs.slot_for(static_cast<std::uint32_t>('A')).metrics);

            const bool fontProof = loaded && glyphs.slot_count() == 95u && glyphTiles == 95u &&
                                   sdfBuilt && sdfs.has_slot(static_cast<std::uint32_t>('A')) && sdfTiles == 95u &&
                                   aHasGlyph && aHasSdf && metricsOk;
            if (fontProof)
                std::printf("[s203+s204-live] font/SDF atlas: embedded font parsed, "
                            "%u glyph coverage tiles + %u SDF tiles uploaded (spread=%u); "
                            "'A' has glyph+SDF+valid metrics\n",
                            glyphs.slot_count(), sdfTiles, sdfs.spread());
            else
                std::fprintf(stderr, "[s203+s204-live] font/SDF MISMATCH "
                            "(loaded=%d slots=%u glyphTiles=%u sdfBuilt=%d sdfTiles=%u aG=%d aS=%d m=%d)\n",
                            loaded, glyphs.slot_count(), glyphTiles,
                            sdfBuilt, sdfTiles, aHasGlyph, aHasSdf, metricsOk);
        }
        else
        {
            std::fprintf(stderr, "[s203+s204-live] FontAtlas/SdfAtlas (TextureAtlas) create failed.\n");
        }
    }

    // --- S205 live UI-pipeline proof (device-only; no surface required) ---------
    // The S205 real UI shader set (SDF text + solid color + border) + the SHARED
    // descriptor-set layout + the 128-byte shared UiPushConstants generalize the
    // S200-local descriptor seam into ONE layout; the three UiPipelines here are
    // built over a temporary SRGB render pass (device-only, no swapchain needed)
    // so the HEADLINE risks prove live: (1) the four embedded UI shaders
    // (ui_vertex + the three fragments) CREATE REAL VkShaderModule + graphics
    // pipelines on a real driver, (2) ONE shared VkDescriptorSetLayout (set 0
    // binding 0) + ONE shared VkPipelineLayout (the 128-byte push range) underpin
    // all three kinds, (3) ONE descriptor set allocated from the shared layout and
    // bound to the S204 SDF atlas's image view + sampler serves all three, and
    // (4) the S200 SRGB pairing + alpha blend hold. The presented sampling of the
    // three kinds on the swapchain is the S206 display-backed batching step
    // ([both apps, not CI]).
    if (vk->has_render_loader() && vk->has_texture_loader() && ctx.device)
    {
        using namespace vaml::vulkan::quad;
        using namespace vaml::vulkan::ui;
        // A device-only SRGB color render pass (no depth) matching the S200 pairing.
        auto rp = vaml::vulkan::RenderPass::create(*vk, *ctx.device, VK_FORMAT_B8G8R8A8_SRGB,
                                                   std::nullopt, VK_SAMPLE_COUNT_1_BIT);
        auto uiVert = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::ui_vertex_spirv());
        auto sdfFrag  = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::sdf_text_fragment_spirv());
        auto solidFrag = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::solid_color_fragment_spirv());
        auto borderFrag = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::border_fragment_spirv());
        auto monoFrag = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::monochrome_tint_fragment_spirv());
        if (rp && uiVert && sdfFrag && solidFrag && borderFrag && monoFrag)
        {
            // The shared spec: the three pipelines differ ONLY by fragment shader + kind.
            UiPipelineSpec spec{};
            spec.renderPass   = rp->native_handle();
            spec.colorFormat  = VK_FORMAT_B8G8R8A8_SRGB;  // the S200 SRGB pairing
            spec.hasDepth     = false;
            spec.sampleCount  = VK_SAMPLE_COUNT_1_BIT;
            spec.extent       = VkExtent2D{ 800, 600 };
            spec.vertexModule = uiVert->shader_module();

            spec.fragmentModule = solidFrag->shader_module();
            auto solid = UiPipeline::create(*vk, *ctx.device, spec, UiShaderKind::Solid);
            spec.fragmentModule = borderFrag->shader_module();
            auto border = UiPipeline::create(*vk, *ctx.device, spec, UiShaderKind::Border);
            spec.fragmentModule = sdfFrag->shader_module();
            auto sdf = UiPipeline::create(*vk, *ctx.device, spec, UiShaderKind::SdfText);
            spec.fragmentModule = monoFrag->shader_module();
            auto mono = UiPipeline::create(*vk, *ctx.device, spec, UiShaderKind::Monochrome);

            // ALL FOUR share the SAME shared set layout + pipeline layout. The S205_1
            // monochrome-tint pipeline rides the IDENTICAL shared layout (one set +
            // one 128-B push block serves it too — the S206 batching premise, and the
            // TintMode=Monochrome render path S400_1 maps onto; TintMode=Preserve is
            // the existing multicolor SDF-text/solid/quad kinds here).
            const bool threeCreated = solid && border && sdf && mono;
            const bool sharedLayout = solid && border && sdf && mono &&
                solid->descriptor_set_layout() == border->descriptor_set_layout() &&
                solid->descriptor_set_layout() == sdf->descriptor_set_layout() &&
                solid->descriptor_set_layout() == mono->descriptor_set_layout() &&
                solid->pipeline_layout() == border->pipeline_layout() &&
                solid->pipeline_layout() == sdf->pipeline_layout() &&
                solid->pipeline_layout() == mono->pipeline_layout();
            // The 128-byte shared push-constant range (the S200 uiScale offset 64).
            const auto range = ui_push_constant();
            const bool sharedRange = range.size == 128u && range.offset == 0u &&
                (range.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) &&
                (range.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT);
            const bool srgbPair = vaml::vulkan::quad::is_srgb_format(VK_FORMAT_B8G8R8A8_SRGB);

            // One descriptor set from the SHARED layout bound to the S204 SDF atlas.
            // (The three pipelines share this set — the S206 batching premise.)
            VkDescriptorPool pool = VK_NULL_HANDLE;
            VkDescriptorSet set = VK_NULL_HANDLE;
            bool setBound = false;
            if (sharedLayout && vk->vkCreateDescriptorPool)
            {
                VkDescriptorPoolSize poolSize{};
                poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSize.descriptorCount = 1;
                VkDescriptorPoolCreateInfo dpci{};
                dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                dpci.maxSets       = 1;
                dpci.poolSizeCount = 1;
                dpci.pPoolSizes    = &poolSize;
                if (vk->vkCreateDescriptorPool(ctx.device->device(), &dpci, nullptr, &pool) == VK_SUCCESS &&
                    vk->vkAllocateDescriptorSets)
                {
                    VkDescriptorSetAllocateInfo dai{};
                    dai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    dai.descriptorPool     = pool;
                    dai.descriptorSetCount = 1;
                    const VkDescriptorSetLayout sharedSetLayout = solid->descriptor_set_layout();
                    dai.pSetLayouts = &sharedSetLayout;
                    setBound = vk->vkAllocateDescriptorSets(ctx.device->device(), &dai, &set) == VK_SUCCESS;
                }
                if (setBound && vk->vkUpdateDescriptorSets)
                {
                    // Write the SHARED set-0-binding-0 = the atlas combined image
                    // sampler using ui_sampler() (the S200 linear/clamp decision) with
                    // a NULL image view here (device-only block): this exercises the
                    // exact descriptor-write path the SDF/solid/border shaders sample;
                    // the real atlas view binding is the S206/S400 display step.
                    VkSampler sampler = VK_NULL_HANDLE;
                    if (vk->vkCreateSampler)
                    {
                        const auto samplerInfo = ui_sampler();
                        vk->vkCreateSampler(ctx.device->device(), &samplerInfo, nullptr, &sampler);
                    }
                    VkDescriptorImageInfo info{};
                    const VkWriteDescriptorSet write =
                        ui_descriptor_write(set, info, VK_NULL_HANDLE, sampler);
                    vk->vkUpdateDescriptorSets(ctx.device->device(), 1, &write, 0, nullptr);
                    if (sampler && vk->vkDestroySampler)
                        vk->vkDestroySampler(ctx.device->device(), sampler, nullptr);
                }
            }

            if (threeCreated && sharedLayout && sharedRange && srgbPair)
                std::printf("[s205+s205_1-live] UI pipelines on %s: SDF-text + solid + border "
                            "+ monochrome-tint created over ONE shared set layout + ONE shared "
                            "128-B push range; descriptor set bound=%d\n",
                            gpuName.c_str(), setBound);
            else
                std::fprintf(stderr, "[s205+s205_1-live] UI pipeline MISMATCH on %s "
                            "(solid=%d border=%d sdf=%d mono=%d sharedLayout=%d sharedRange=%d srgb=%d set=%d)\n",
                            gpuName.c_str(), !!solid, !!border, !!sdf, !!mono,
                            sharedLayout, sharedRange, srgbPair, setBound);
            if (pool && vk->vkDestroyDescriptorPool)
                vk->vkDestroyDescriptorPool(ctx.device->device(), pool, nullptr);
        }
        else
        {
            std::fprintf(stderr, "[s205+s205_1-live] UI pipeline create failed "
                        "(rp=%d uiVert=%d sdf=%d solid=%d border=%d mono=%d)\n",
                        !!rp, !!uiVert, !!sdfFrag, !!solidFrag, !!borderFrag, !!monoFrag);
        }
    }

    // --- S175 change-driven loop (the framework owns it) ------------------------
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

        bool invalidated = false;

        // S175: a resize is a REQUEST to the presenter (the framework applies the
        // two-generation transaction + the S174 dest-rect recompute). The resize
        // flag marks the frame invalid so the presenter re-renders at the new size.
        if (ctx.resizePending)
        {
            ctx.resizePending = false;
            const auto dest = vamlpoc::full_window_dest(ctx.framebufferW, ctx.framebufferH);
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

    // Wait for any in-flight submit before destroying the CommandPool/FrameSync so
    // nothing references them during teardown.
    if (ctx.device && vk->vkDeviceWaitIdle)
        vk->vkDeviceWaitIdle(ctx.device->device());

    // Teardown (reverse order), matching Linux: device -> surface -> instance -> loader.
    ctx.destroy();
    return 0;
}
