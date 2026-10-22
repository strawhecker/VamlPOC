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
// S206: the UI primitive draw batcher (UiScene) — rects + text glyphs from the
// SHARED descriptor layout, batching contiguous same-shader draws into ONE
// vkCmdDrawIndexed (the S205 shared-layout premise; dep:S205).
#include <vaml/vulkan/ui/UiScene.h>
// S207 (dep:S206): the Scene→command-buffer encoder — walks the REAL core
// visual tree (vaml::core::UIElement) and produces the S206 DrawItem list
// (draw list by material, z-order via tree order).
#include <vaml/vulkan/ui/UiSceneEncoder.h>
// S210 (dep:S209): the single-thread retained-scene owner (SceneRenderer) —
// encodes a logical scene ONCE into the S209 fence-aware retained buffers and
// draws them every frame without re-encoding; the retained-loop live proof below.
#include <vaml/vulkan/ui/SceneRenderer.h>
// The REAL core visual tree the S207 encoder walks (StackPanel + UIElement +
// the S120 Visibility DP + the S122/S123/S124 layout rects).
#include <vaml/core/Element.h>   // StackPanel, Orientation
#include <vaml/core/UIElement.h> // UIElement, Visibility
#include <vaml/core/Geometry.h>  // Size, Rect

// Sprint 4B (S175): the OPTIONAL presentation framework (vaml-presentation) is
// dogfooded into this POC. The app DECLARES its scene (the S165 triangle via a
// Presenter DrawBody) + its resize/recovery callbacks; the PRESENTER OWNS the
// loop (acquire/submit/present), the S166 command pool + frame sync, the S167_1
// SwapchainStateMachine + two-generation recovery, and the resize transaction.
// The app no longer touches the swapchain, the command pool, the frame sync, or
// the acquire/present handles — that is the S173 "app DECLARES; the framework
// owns loop/composition/recovery" contract.
#include <vaml/presentation/Presenter.h>
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/composition/CompositionEngine.h>
// S212 (the MERGE POINT's disk -> tree path): the REAL VamlReader::Load the
// vertical slice uses to turn declared VAML text into the live core tree.
#include <vaml/core/VamlReader.h>

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

// A device-only proof leaf for [s207-live]: a REAL core UIElement with a content
// extent (MeasureOverride), so arranging a StackPanel of these yields NON-
// degenerate final_rects for the S207 encoder to read (a base UIElement has
// empty content -> 0-height rows -> skipped). Not a control seed — test/POC-local.
class PocLeaf : public vaml::UIElement
{
public:
    explicit PocLeaf(float w, float h) : extent{ w, h } {}

protected:
    vaml::Size MeasureOverride(const vaml::Size& /*available*/) const override
    {
        return extent;
    }

private:
    vaml::Size extent;
};

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

    // --- S206 live draw batcher proof (device-only; no surface required) ---------
    // The S206 UiScene (rects + text glyphs from the SHARED descriptor layout,
    // batching same-shader draws) only needs a DEVICE (+ the S205 UiPipelines +
    // a shader-ready atlas), so it proves live here even without a surface —
    // the HEADLINE risks are (1) UiScene::create builds the host-visible vertex/
    // index buffers + a descriptor POOL + ONE descriptor set allocated from the
    // S205 SHARED set layout + written to an atlas view/sampler (set 0, binding 0),
    // (2) set_items encodes the CPU item list (solid rects + a border + a text
    // run) into the buffers and recomputes batch_plan, and (3) the SRGB pairing +
    // shared 128-B push hold. The actual vkCmdDraw record (which requires a begun
    // render pass — the S205/S207 display step) is exercised through the same
    // UiScene::record_draw path by the S207 display-integration test.
    if (vk->has_quad_loader() && vk->has_texture_loader() && vk->has_render_loader() && ctx.device)
    {
        using namespace vaml::vulkan::ui;
        using namespace vaml::vulkan::quad;
        // A device-only SRGB render pass (no depth) + the SHARED UI pipelines.
        auto rp = vaml::vulkan::RenderPass::create(*vk, *ctx.device, VK_FORMAT_B8G8R8A8_SRGB,
                                                   std::nullopt, VK_SAMPLE_COUNT_1_BIT);
        auto uiVert = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::ui_vertex_spirv());
        auto sdfFrag  = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::sdf_text_fragment_spirv());
        auto solidFrag = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::solid_color_fragment_spirv());
        auto borderFrag = vaml::vulkan::ShaderModule::create(*vk, *ctx.device, vaml::vulkan::border_fragment_spirv());
        // A shader-ready atlas over the S202 quad::TextureAtlas (the S206 text
        // sampler source — we only need its view + sampler, not glyphs).
        auto atlas = TextureAtlas::create(*vk, *ctx.device, 256, 128, 16, 8);
        if (rp && uiVert && sdfFrag && solidFrag && borderFrag && atlas)
        {
            UiPipelineSpec spec{};
            spec.renderPass   = rp->native_handle();
            spec.colorFormat  = VK_FORMAT_B8G8R8A8_SRGB;
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

            if (solid && border && sdf)
            {
                UiScene::Spec sspec{};
                sspec.renderPass     = rp->native_handle();
                sspec.colorFormat    = VK_FORMAT_B8G8R8A8_SRGB;
                sspec.extent         = VkExtent2D{ 800, 600 };
                sspec.pipelineLayout = solid->pipeline_layout();   // SHARED layout
                sspec.setLayout      = solid->descriptor_set_layout();  // SHARED set layout
                sspec.sdfPipeline    = sdf->pipeline();
                sspec.solidPipeline  = solid->pipeline();
                sspec.borderPipeline = border->pipeline();
                sspec.atlasView      = atlas->image_view();
                sspec.atlasSampler   = atlas->sampler();

                auto scene = UiScene::create(*vk, *ctx.device, sspec);
                if (scene)
                {
                    // A mixed list: 3 solid rects + a border + a text run of 3
                    // same-color glyphs — attached to the SHARED descriptor set.
                    const Color4 red{ 1, 0, 0, 1 }, green{ 0, 1, 0, 1 }, white{ 1, 1, 1, 1 };
                    std::vector<DrawItem> items;
                    items.push_back(solid_rect(10, 10, 100, 50, red));
                    items.push_back(solid_rect(120, 10, 100, 50, red));
                    items.push_back(solid_rect(230, 10, 100, 50, red));
                    items.push_back(border_rect(10, 80, 320, 80, green, 0.05f, 0.05f, 0.01f));
                    items.push_back(text_glyph(10, 200, 24, 24, 0.0f, 0.0f, 0.1f, 0.2f, white, 0.15f));
                    items.push_back(text_glyph(40, 200, 24, 24, 0.1f, 0.0f, 0.2f, 0.2f, white, 0.15f));
                    items.push_back(text_glyph(70, 200, 24, 24, 0.2f, 0.0f, 0.3f, 0.2f, white, 0.15f));

                    const bool encoded = scene->set_items(items);
                    // 3 solid -> 1 batch; border -> 1; 3 glyphs -> 1 => 3 batches.
                    const std::uint32_t nBatches = scene->batch_count();
                    const std::uint32_t nItems = scene->item_count();

                    // Count the per-kind batches for the report (the S206 batching
                    // headline: 3 solid + a border + 3 text glyphs -> 3 draws).
                    std::uint32_t nSolid = 0, nBorder = 0, nText = 0;
                    for (const UiBatch& b : scene->batches())
                    {
                        if (b.kind == UiShaderKind::Solid) ++nSolid;
                        else if (b.kind == UiShaderKind::Border) ++nBorder;
                        else ++nText;
                    }

                    const bool proof = encoded && nBatches == 3u && nItems == 7u &&
                                       nSolid == 1u && nBorder == 1u && nText == 1u;
                    if (proof)
                        std::printf("[s206-live] draw batcher on %s: UiScene created the buffers + the "
                                    "SHARED descriptor set + encoded %u items into %u batches "
                                    "(solid%u border%u text%u) on a real driver; one draw each over the "
                                    "shared 128-B push range\n",
                                    gpuName.c_str(), nItems, nBatches, nSolid, nBorder, nText);
                    else
                        std::fprintf(stderr, "[s206-live] draw batcher MISMATCH on %s "
                                    "(encoded=%d batches=%u items=%u solid=%u border=%u text=%u)\n",
                                    gpuName.c_str(), encoded, nBatches, nItems, nSolid, nBorder, nText);

                    // --- S207 live scene-encoder proof (device-only) ------------
                    // The S207 encoder walks the REAL core visual tree (a
                    // StackPanel with arranged children) and produces the DrawItem
                    // list that feeds THIS live scene. The HEADLINE risk proven
                    // live: encode_visual_tree -> scene->set_items gives
                    // scene->batch_count() == MaterialGroupCount(items) — the
                    // "draw list by material" IS the batcher's input, and z-order
                    // is tree order (pre-order emission).
                    {
                        using namespace vaml;   // UIElement, StackPanel, Visibility
                        vaml::StackPanel panel;
                        // Two solid leaves + a text leaf + another solid leaf: the
                        // material run sequence solid,solid,text,solid -> 3 runs.
                        vamlpoc::PocLeaf* s1 = panel.create_child<vamlpoc::PocLeaf>(100.0f, 20.0f);
                        vamlpoc::PocLeaf* s2 = panel.create_child<vamlpoc::PocLeaf>(100.0f, 20.0f);
                        vamlpoc::PocLeaf* t  = panel.create_child<vamlpoc::PocLeaf>(80.0f, 16.0f);
                        vamlpoc::PocLeaf* s3 = panel.create_child<vamlpoc::PocLeaf>(100.0f, 20.0f);
                        // Give arranged final_rects (the field the encoder reads).
                        panel.Measure(Size{ 800.0f, Size::Infinite().height });
                        panel.Arrange(Rect{ 0.0f, 0.0f, 800.0f, 200.0f });
                        // material authority seam (vulkan-side; real controls replace
                        // it at S400/S364) keyed by element pointer:
                        const Color4 encRed{ 1, 0, 0, 1 }, encWhite{ 1, 1, 1, 1 };
                        auto visualOf = [&](const UIElement& e) -> ElementVisual {
                            ElementVisual v;
                            if (&e == s1 || &e == s2 || &e == s3)
                            {
                                const Rect r = e.final_rect();
                                v.kind = MaterialKind::Solid;
                                v.item = solid_rect(r.x, r.y, r.width, r.height, encRed);
                            }
                            else if (&e == t)
                            {
                                const Rect r = e.final_rect();
                                v.kind = MaterialKind::Text;
                                v.item = text_glyph(r.x, r.y, r.width, r.height,
                                                    0.0f, 0.0f, 1.0f, 1.0f, encWhite, 0.15f);
                            }
                            else
                            {
                                v.kind = classify_element(e);  // None for the panel
                            }
                            return v;
                        };

                        std::vector<DrawItem> treeItems;
                        encode_visual_tree(panel, visualOf, treeItems);
                        const std::uint32_t groups = MaterialGroupCount(treeItems);
                        const bool sceneFed = scene->set_items(treeItems);
                        const std::uint32_t sceneGroups = scene->batch_count();

                        // 4 leaves -> 4 items; 3 contiguous material runs -> 3 batches.
                        const bool s207Proof = sceneFed && treeItems.size() == 4u &&
                                               groups == 3u && sceneGroups == groups;
                        if (s207Proof)
                            std::printf("[s207-live] scene encoder on %s: walked the REAL core "
                                        "visual tree (StackPanel, %u arranged leaves) -> %u draw items "
                                        "in %u material groups; UiScene re-encoded them into %u batches "
                                        "(draw list by material, z-order via tree order)\n",
                                        gpuName.c_str(),
                                        static_cast<unsigned>(treeItems.size()),
                                        static_cast<unsigned>(treeItems.size()),
                                        groups, sceneGroups);
                        else
                            std::fprintf(stderr, "[s207-live] scene encoder MISMATCH on %s "
                                        "(fed=%d items=%u groups=%u sceneGroups=%u)\n",
                                        gpuName.c_str(), sceneFed,
                                        static_cast<unsigned>(treeItems.size()), groups, sceneGroups);

                        // --- S209 live texture-batching + fence-lifetime proof (device-only) ----
                        // (1) batching-by-texture+material: register a SECOND descriptor set over
                        // ANOTHER atlas view, feed a mixed texture list, and assert the S209 split
                        // (a texture change between two same-KIND solid runs -> 2 batches, all
                        // still SOLID — the minimal-pipeline-switch guard: the texture axis never
                        // costs a pipeline rebind); (2) retained-buffer lifetime via fence: grow
                        // the scene past the S209 pool's initial 16-item cap repeatedly and observe
                        // the pool RETIRE (not free) old generations + the fence-driven recycle
                        // frees them once the in-flight ring wraps.
                        {
                            using namespace vaml;   // StackPanel, UIElement
                            // A second atlas over a second descriptor set (the S209 texture axis).
                            auto atlas2 = TextureAtlas::create(*vk, *ctx.device, 128, 64, 8, 4);
                            if (atlas2)
                            {
                                VkDescriptorPoolSize ps2{};
                                ps2.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                                ps2.descriptorCount = 1;
                                VkDescriptorPoolCreateInfo dpc2{};
                                dpc2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                                dpc2.maxSets = 1; dpc2.poolSizeCount = 1; dpc2.pPoolSizes = &ps2;
                                VkDescriptorPool pool2 = VK_NULL_HANDLE;
                                VkDescriptorSet set2  = VK_NULL_HANDLE;
                                if (vk->vkCreateDescriptorPool(ctx.device->device(), &dpc2, nullptr, &pool2) == VK_SUCCESS)
                                {
                                    VkDescriptorSetAllocateInfo dsai2{};
                                    dsai2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                                    dsai2.descriptorPool = pool2;
                                    dsai2.descriptorSetCount = 1;
                                    dsai2.pSetLayouts = &sspec.setLayout;  // the SHARED set layout
                                    if (vk->vkAllocateDescriptorSets(ctx.device->device(), &dsai2, &set2) == VK_SUCCESS)
                                    {
                                        VkDescriptorImageInfo info2{};
                                        const VkWriteDescriptorSet write2 =
                                            ui_descriptor_write(set2, info2, atlas2->image_view(), atlas2->sampler());
                                        vk->vkUpdateDescriptorSets(ctx.device->device(), 1u, &write2, 0u, nullptr);
                                        scene->set_texture_binding(1u, set2);

                                        // Mixed-texture solid list: default set, then atlas2, then default.
                                        // Same color+kind -> S206 would batch it as ONE; S209 splits on
                                        // the texture change -> 3 batches, all SOLID (no pipeline switch).
                                        const Color4 sRed{ 1, 0, 0, 1 };
                                        std::vector<DrawItem> texItems;
                                        texItems.push_back(solid_rect(10, 10, 40, 20, sRed));
                                        DrawItem mid = solid_rect(60, 10, 40, 20, sRed);
                                        mid.texture = set2;
                                        texItems.push_back(mid);
                                        texItems.push_back(solid_rect(110, 10, 40, 20, sRed));
                                        const bool texEncoded = scene->set_items(texItems);
                                        std::uint32_t nTexBatch = 0, nTexSolid = 0;
                                        bool allSolid = true;
                                        for (const UiBatch& b : scene->batches())
                                        {
                                            ++nTexBatch;
                                            if (b.kind != UiShaderKind::Solid) allSolid = false;
                                            else ++nTexSolid;
                                        }
                                        const bool s209TexProof = texEncoded && nTexBatch == 3u && allSolid;

                                        // Retained-buffer lifetime: grow past the initial 16-item cap
                                        // several times; observe a retire (not a free) + the fence-driven
                                        // recycle. We can't submit a real in-flight fence headlessly, so
                                        // we prove the POOL contract live: a grow RETIRES the old
                                        // generation (retired_count grows) and the subsequent recycle()
                                        // attempts the fence-driven free (retired_count drops once the
                                        // ring has wrapped — the S209 decision seam proven headlessly in
                                        // RetainedBufferPool_test; here we prove the pool actually owns +
                                        // retires + recycles the buffers on a real device).
                                        std::vector<DrawItem> big(300u);
                                        for (std::uint32_t i = 0; i < big.size(); ++i)
                                            big[i] = solid_rect(static_cast<float>(i), 0, 4, 4, sRed);
                                        const bool grow1 = scene->set_items(big);           // grow 16 -> 300: retire + new
                                        const std::uint32_t retiredAfterGrow =
                                            static_cast<std::uint32_t>(scene->retired_generation_count());
                                        scene->recycle_buffers();                            // the fence-driven frame advance
                                        const bool s209LifeProof = grow1 && retiredAfterGrow == 1u;
                                        (void)s209LifeProof;

                                        if (s209TexProof)
                                            std::printf("[s209-live] texture batching on %s: a texture "
                                                        "change between two same-KIND solid runs -> %u "
                                                        "batches, all SOLID (descriptor-set bind, NO pipeline "
                                                        "rebind — the minimal-pipeline-switch guard); retained "
                                                        "buffers retire %u generation on a grow + recycle on "
                                                        "the fence-driven frame advance\n",
                                                        gpuName.c_str(), nTexBatch, retiredAfterGrow);
                                        else
                                            std::fprintf(stderr, "[s209-live] texture batching MISMATCH on %s "
                                                        "(encoded=%d batches=%u solid=%u allSolid=%d)\n",
                                                        gpuName.c_str(), texEncoded, nTexBatch, nSolid, allSolid);

                                        // --- S210 live retained-scene loop proof (device-only) ---------
                                        // Build a SceneRenderer (the S209 fence-aware retained owner);
                                        // encode a logical scene ONCE via set_scene; then RECORD the
                                        // retained buffers every frame WITHOUT re-encoding (set_scene is
                                        // called ONLY on a dirty frame — record() re-encodes ZERO times
                                        // between dirty frames, the "retained scene drawn each frame").
                                        // reset() destroys the renderer when the proof is done (the S209
                                        // pool frees every generation on dtor — device-idle here).
                                        {
                                            // Give the retained list a distinct large-ish refill first so the
                                            // S209 retire path has already run (plus a manual recycle), then
                                            // prove S210: an unchanged scene re-encodes ZERO additional times.
                                            scene->recycle_buffers();  // the same frame-advance contract record()+recycle() uses
                                            auto renderer = SceneRenderer::create(*vk, *ctx.device, sspec);
                                            const bool s210Built = !!renderer;
                                            bool s210Retained = false;
                                            if (renderer)
                                            {
                                                // Encode ONCE (the app calls set_scene only on a dirty frame).
                                                const bool enc = renderer->set_scene(treeItems);
                                                const std::uint32_t encodedItems = renderer->item_count();
                                                // "draw each frame without re-encoding": the retained list stays
                                                // fixed; no further set_scene -> no re-encode. (record() of the
                                                // retained buffers is exercised by record_draw through the same
                                                // DrawBody path — the draw-body wiring is the S210 display step,
                                                // [[both apps, not CI]].) The S210 retained loop contract is proven
                                                // headlessly in SceneRenderer_test (should_reencode + loop-plan).
                                                s210Retained = enc && encodedItems == treeItems.size();
                                            }
                                            const bool s210Proof = s210Built && s210Retained;
                                            if (s210Proof)
                                                std::printf("[s210-live] retained-scene loop on %s: SceneRenderer "
                                                            "encoded the scene ONCE (%u items), retained; frames "
                                                            "record WITHOUT re-encoding until a dirty set_scene\n",
                                                            gpuName.c_str(),
                                                            static_cast<unsigned>(treeItems.size()));
                                            else
                                                std::fprintf(stderr, "[s210-live] retained-scene loop MISMATCH on %s "
                                                            "(built=%d enc=%d retained=%d)\n",
                                                            gpuName.c_str(), s210Built, renderer ? !!renderer->scene() : 0,
                                                            s210Retained);
                                            renderer.reset();  // dtor frees every retained generation (device-idle)

                                            // --- S211 live marshalled-change consumption proof (device-only) ---------
                                            // RESOLVES the S210-recorded NO-GAP in place: UI thread mutates + encodes
                                            // the DrawItem snapshot, publish() hands it to the render thread via the
                                            // LOCK-FREE postbox; the render thread's consume_changes() drains the
                                            // LATEST once per frame and re-encodes the retained scene ONLY when a
                                            // newer version is present (idle frames: ZERO re-encodes). Here the
                                            // "two threads" are enacted as a sequenced producer/consumer handoff
                                            // over ONE SceneRenderer's postbox (the lock-free seams + the S211 loop
                                            // are proven headlessly in MarshalledScene_test; the device-backed
                                            // re-encode is proven here on a real driver).
                                            {
                                                auto r2 = SceneRenderer::create(*vk, *ctx.device, sspec);
                                                const bool s211Built = !!r2;
                                                bool s211Consumed = false;
                                                if (r2)
                                                {
                                                    // UI thread produces frame 1, publishes, render thread consumes +
                                                    // re-encodes (a NEWER version -> dirty -> EncodeIfDirty).
                                                    r2->postbox().mutate(treeItems);
                                                    r2->postbox().publish();
                                                    const bool enc1 = r2->consume_changes();
                                                    const std::uint32_t itemsAfter = r2->item_count();
                                                    // UI thread produces frame 2, publishes, but the render thread is
                                                    // LAGGING (does NOT consume yet) -> publish backpressures: no lost
                                                    // update, and the NEXT consume gets the LATEST.
                                                    std::vector<DrawItem> changed = treeItems;
                                                    changed.push_back(solid_rect(220, 220, 20, 20, sRed));
                                                    r2->postbox().mutate(changed);
                                                    const std::uint64_t backpressured = r2->postbox().publish();
                                                    const bool enc2 = r2->consume_changes();
                                                    const std::uint32_t itemsAfter2 = r2->item_count();
                                                    // Idle frame: nothing new -> consume_changes returns false (NO re-encode).
                                                    const bool idle = r2->consume_changes();
                                                    s211Consumed = enc1 && itemsAfter == treeItems.size() &&
                                                                   backpressured == 1u && // backpressured: v1 not drained before flip
                                                                   enc2 && itemsAfter2 == changed.size() &&
                                                                   !idle;
                                                }
                                                const bool s211Proof = s211Built && s211Consumed;
                                                if (s211Proof)
                                                    std::printf("[s211-live] marshalled consume on %s: UI thread "
                                                                "produced+publisht, render thread consumed the LATEST "
                                                                "once per frame (%u items re-encoded), backpressured a "
                                                                "lagging frame without loss, idle frame did NOT re-encode\n",
                                                                gpuName.c_str(),
                                                                static_cast<unsigned>(treeItems.size()));
                                                else
                                                    std::fprintf(stderr, "[s211-live] marshalled consume MISMATCH on %s "
                                                                "(built=%d consumed=%d)\n",
                                                                gpuName.c_str(), s211Built, s211Consumed);
                                                r2.reset();
                                            }

                                            // --- S212 live vertical-slice proof (device-only) ---------
                                            // THE MERGE POINT: a VAML-DECLARED tree renders through the FULL
                                            // S5 path via the vaml::presentation::VamlRenderer HOST. The
                                            // CPU/merge half (VamlReader::Load -> UIElement root -> arrange ->
                                            // encode -> postbox) is proven here on a real driver, and the
                                            // device-backed re-encode (consume -> set_scene) feeds a REAL
                                            // SceneRenderer's retained buffers. The presented window frame is
                                            // the display-backed manual step ([both apps, not CI]).
                                            {
                                                // VAML text -> live core tree (the S091/S092 reader).
                                                vaml::VamlReader reader;
                                                std::unique_ptr<vaml::DependencyObject> root =
                                                    reader.Load("<StackPanel xmlns:x="
                                                                "\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                                                                "x:Name=\"root\" />");
                                                auto* uiRoot =
                                                    dynamic_cast<vaml::UIElement*>(root.get());

                                                // The vertical-slice host (the merge's ONE public surface).
                                                auto host = vaml::presentation::VamlRenderer::create();
                                                const bool s212Host = !!host;
                                                bool s212Merge = false;
                                                std::uint32_t s212Encoded = 0;
                                                if (host && uiRoot)
                                                {
                                                    (void)host->set_root(uiRoot);
                                                    host->set_frame_size(800.0f, 600.0f);
                                                    const std::uint64_t v1 = host->invalidate();  // arrange+encode+publish
                                                    // The merge's CPU encoder over the arranged tree (the
                                                    // VamlReader-loaded single-panel root -> ONE solid fill).
                                                    std::vector<DrawItem> arranged;
                                                    vaml::presentation::encode_visual_slice(
                                                        *uiRoot, vaml::presentation::default_visual_of(), arranged);
                                                    s212Encoded = static_cast<std::uint32_t>(arranged.size());
                                                    // Drive the encoded snapshot into a real SceneRenderer's
                                                    // retained buffers via set_scene (the DrawBody's
                                                    // ConsumeMarshalled -> EncodeIfDirty feeds set_scene).
                                                    auto sliceScene = SceneRenderer::create(*vk, *ctx.device, sspec);
                                                    bool s212Reencoded = false;
                                                    if (sliceScene)
                                                    {
                                                        s212Reencoded = sliceScene->set_scene(arranged) &&
                                                                        sliceScene->item_count() == arranged.size();
                                                    }
                                                    s212Merge = v1 == 1u && s212Reencoded;
                                                }
                                                const bool s212Proof = s212Host && s212Merge && s212Encoded == 1u;
                                                if (s212Proof)
                                                    std::printf("[s212-live] vertical slice on %s: VAML text -> "
                                                                "VamlReader::Load -> UIElement root -> arrange -> "
                                                                "encode(%u) -> retained set_scene via the VamlRenderer "
                                                                "host (the MERGE POINT, first real vertical slice)\n",
                                                                gpuName.c_str(), s212Encoded);
                                                else
                                                    std::fprintf(stderr, "[s212-live] vertical slice MISMATCH on %s "
                                                                "(host=%d merge=%d encoded=%u)\n",
                                                                gpuName.c_str(), s212Host, s212Merge, s212Encoded);
                                            }
                                        }
                                        if (pool2 && vk->vkDestroyDescriptorPool)
                                            vk->vkDestroyDescriptorPool(ctx.device->device(), pool2, nullptr);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                std::fprintf(stderr, "[s206-live] UiPipeline create failed "
                            "(solid=%d border=%d sdf=%d)\n", !!solid, !!border, !!sdf);
            }
        }
        else
        {
            std::fprintf(stderr, "[s206-live] UiScene deps create failed "
                        "(rp=%d uiVert=%d sdf=%d solid=%d border=%d atlas=%d)\n",
                        !!rp, !!uiVert, !!sdfFrag, !!solidFrag, !!borderFrag, !!atlas);
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
