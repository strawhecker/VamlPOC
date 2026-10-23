// vaml_viewer — the shared "soup-to-nuts" render host implementation (S222).
//
// Owns the whole Vulkan stack + `VamlRenderer::create_device` behind one owner and
// runs the change-driven loop until the window closes. Mirror the S177 WSIResize
// harness — the proven live-stack construction precedent (instance -> surface ->
// best-gpu -> device -> swapchain-negotiate -> render pass -> Presenter) — and the
// S212 vertical-slice host. See app.h for the split + the LIFETIMES contract +
// the NO-GAP + the S222 scope.

#include "app.h"

#include <vaml/core/UIElement.h>        // vaml::UIElement (the REAL visual tree root)
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/gpu/BestGpuSelection.h>
#include <vaml/vulkan/QueueFamilySelection.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/ui/SceneRenderer.h>   // ui::SceneRenderer alias (S210)

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <utility>

namespace vaml::viewer {

// ---------------------------------------------------------------------------
// Pure decision seams (headless-testable)
// ---------------------------------------------------------------------------

const std::vector<std::string>& viewer_stack_steps() noexcept
{
    // The S222 stack build order — the LIFETIMES contract serialized. Each step's
    // borrowed handles outlive the next owner; the owners are destroyed in REVERSE
    // (a stack), so the VamlRenderer (the LAST step) is destroyed FIRST.
    static const std::vector<std::string> steps = {
        "Loader",    // runtime entry points (process-lifetime)
        "Instance",  // on the loader
        "Surface",   // on the instance + window (window outlives it)
        "Gpu",       // best present-capable physical device (S163)
        "Device",    // logical device + graphics queue
        "RenderPass",// the S165 color(+depth) pass
        "Pipelines", // the S205 shared-layout UiPipelines (Solid/SDF/Border)
        "Atlas",     // the S202 TextureAtlas
        "Font",      // the S203 FontAtlas
        "Sdf",       // the S204 SdfAtlas
        "UiSceneSpec", // the S206 spec (shared layout + 3 pipelines + atlas)
        "Host",      // VamlRenderer::create_device (borrows every handle above)
    };
    return steps;
}

const char* viewer_step_name(std::size_t i) noexcept
{
    const auto& steps = viewer_stack_steps();
    return (i < steps.size()) ? steps[i].c_str() : "(out of range)";
}

bool viewer_setup_ready(bool hasSwapchainLoader,
                        bool hasCommandLoader,
                        VkRenderPass renderPass,
                        VkFormat colorFormat,
                        VkImageView atlasView,
                        VkSampler atlasSampler) noexcept
{
    // The S222 front-door gate, recomposing the S173/S200/S206 gates into ONE
    // check the live create() runs FIRST:
    //   (1) the S173 presenter-lifetime gate (swapchain + command loaders + a
    //       real render pass — the Presenter + VamlRenderer borrow them),
    //   (2) the S200 SRGB pairing (the swapchain color format must be SRGB for
    //       the UiScene + UiPipeline color attachment),
    //   (3) the S206 atlas contract (the SDF atlas view + sampler the shared
    //       set-0-binding-0 descriptor samples must be non-null).
    if (!vaml::presentation::presenter_lifetimes_ok(hasSwapchainLoader, hasCommandLoader, renderPass))
        return false;
    if (renderPass == VK_NULL_HANDLE || !vaml::vulkan::quad::is_srgb_format(colorFormat))
        return false;
    return atlasView != VK_NULL_HANDLE && atlasSampler != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// The REAL owner
// ---------------------------------------------------------------------------

// The change-driven loop body: forwards to the host's tick. Returns the frame
// slot drained, or nullopt when nothing was drawn (idle/park).
static std::optional<std::uint32_t> tick_host(vaml::presentation::VamlRenderer& host,
                                              bool invalidated, bool tickDue)
{
    return host.tick(invalidated, tickDue);
}

ViewerApp::ViewerApp() = default;
ViewerApp::ViewerApp(ViewerApp&&) noexcept = default;
ViewerApp& ViewerApp::operator=(ViewerApp&&) noexcept = default;
ViewerApp::~ViewerApp() = default;

std::unique_ptr<ViewerApp>
ViewerApp::create(const ViewerAppConfig& config, vaml::LoadResult load)
{
    // The loaded root must be a real UIElement (the core visual tree) to render.
    if (!load.ok || !load.root)
        return nullptr;  // no root -> nothing renderable yet
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(load.root.get());
    if (!uiRoot)
        return nullptr;  // the root is not a UIElement -> nothing renderable yet

    std::unique_ptr<ViewerApp> app(new ViewerApp());
    app->cfg_ = config;

    try
    {
        // ---- Loader (first; borrows nothing) ----
        // Constructed into the OWNED app->loader_ member (declared FIRST in the
        // class, destroyed LAST) — NOT a local that gets moved away. The S173
        // swapchain builders capture `&loader` into the Presenter (alive until
        // the app closes); that reference MUST point at the permanent loader, or
        // the first resize-time swapchain rebuild (a live tick) would call through
        // a moved-from/dead loader (null function pointers -> AV). This NO-GAP
        // lifetime fix is exactly what only the LIVE soup-to-nuts proof can catch.
        app->loader_ = std::make_unique<vaml::vulkan::VulkanLoader>();

        // ---- Window (GLFW; process-lifetime) ----
        // The window's Config uses `int` (GLFW contract); config.width/height are
        // uint32_t logical layout px -> narrow explicitly (never implicit narrowing,
        // rejected under /WX).
        app->window_ = vaml::vulkan::PlatformWindow::create(
            { config.title.c_str(),
              static_cast<int>(config.width),
              static_cast<int>(config.height) },
            {});
        if (!app->window_)
            return nullptr;

        // ---- Instance ----
        vaml::vulkan::VulkanInstance::Config icfg;
        icfg.enableValidation = true;   // a Debug build => validation layers ON
        app->instance_ = vaml::vulkan::VulkanInstance::create(*app->loader_, icfg);
        if (!app->instance_)
            return nullptr;

        // ---- Surface ----
        app->surface_ = vaml::vulkan::VulkanSurface::create(*app->loader_, *app->instance_, *app->window_);
        if (!app->surface_)
            return nullptr;

        // ---- Best present-capable GPU (S163) ----
        auto best = vaml::vulkan::gpu::select_best_gpu(
            *app->loader_, *app->instance_, app->surface_.get(),
            vaml::vulkan::gpu::preferred_gpu_env_name());
        if (!best || best->best.device == VK_NULL_HANDLE)
            return nullptr;

        // ---- Graphics+present queue family (S162) ----
        std::uint32_t famCount = 0;
        app->loader_->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, nullptr);
        std::vector<VkQueueFamilyProperties> fams(famCount);
        app->loader_->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, fams.data());
        vaml::vulkan::PresentSupportFn present = [&](std::uint32_t i) {
            VkBool32 s = VK_FALSE;
            return app->loader_->vkGetPhysicalDeviceSurfaceSupportKHR(best->best.device, i,
                                                                      app->surface_->surface(), &s) == VK_SUCCESS &&
                   s == VK_TRUE;
        };
        auto fam = vaml::vulkan::select_queue_family(fams.data(), famCount, present);
        if (!fam)
            return nullptr;

        // ---- Device (S169) ----
        app->device_ = vaml::vulkan::Device::create(*app->loader_, best->best, *fam);
        if (!app->device_)
            return nullptr;

        // ---- Negotiate the swapchain to learn the color + depth formats ----
        auto scCfg = vaml::vulkan::Swapchain::negotiate(
            *app->loader_, best->best.device, *app->surface_, config.width, config.height);
        const VkFormat colorFormat = scCfg ? scCfg->surfaceFormat.format : VK_FORMAT_B8G8R8A8_SRGB;
        const std::optional<VkFormat> depth = scCfg ? scCfg->depthFormat : std::nullopt;

        // ---- RenderPass (S165): color (+depth) at the negotiated formats ----
        app->renderPass_ = vaml::vulkan::RenderPass::create(
            *app->loader_, *app->device_, colorFormat, depth, VK_SAMPLE_COUNT_1_BIT);
        if (!app->renderPass_)
            return nullptr;

        // ---- S205 shader modules (the shared ui_vertex + the 3 UI fragments) ----
        app->uiVertex_ = vaml::vulkan::ShaderModule::create(*app->loader_, *app->device_,
                                                            vaml::vulkan::ui_vertex_spirv());
        app->sdfFrag_ = vaml::vulkan::ShaderModule::create(*app->loader_, *app->device_,
                                                           vaml::vulkan::sdf_text_fragment_spirv());
        app->solidFrag_ = vaml::vulkan::ShaderModule::create(*app->loader_, *app->device_,
                                                              vaml::vulkan::solid_color_fragment_spirv());
        app->borderFrag_ = vaml::vulkan::ShaderModule::create(*app->loader_, *app->device_,
                                                              vaml::vulkan::border_fragment_spirv());
        if (!app->uiVertex_ || !app->sdfFrag_ || !app->solidFrag_ || !app->borderFrag_)
            return nullptr;

        // ---- The S205 shared UiPipelineSpec (identical across kinds; SRGB) ----
        vaml::vulkan::ui::UiPipelineSpec plSpec;
        plSpec.renderPass     = app->renderPass_->render_pass();
        plSpec.colorFormat    = colorFormat;
        plSpec.hasDepth       = depth.has_value();
        plSpec.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
        plSpec.extent         = { config.width, config.height };
        plSpec.vertexModule   = app->uiVertex_->shader_module();

        // The three UiPipelines SHARE one descriptor-set layout + one pipeline
        // layout (the S205 shared-layout contract — each create() builds the same
        // shared layout; we read it from the FIRST and reuse the handles for all
        // three in the UiScene::Spec).
        plSpec.fragmentModule = app->solidFrag_->shader_module();
        app->solidPipeline_ = vaml::vulkan::ui::UiPipeline::create(
            *app->loader_, *app->device_, plSpec, vaml::vulkan::ui::UiShaderKind::Solid);
        if (!app->solidPipeline_)
            return nullptr;
        app->sharedLayout_     = app->solidPipeline_->pipeline_layout();
        app->sharedSetLayout_  = app->solidPipeline_->descriptor_set_layout();

        plSpec.fragmentModule = app->sdfFrag_->shader_module();
        app->sdfPipeline_ = vaml::vulkan::ui::UiPipeline::create(
            *app->loader_, *app->device_, plSpec, vaml::vulkan::ui::UiShaderKind::SdfText);
        if (!app->sdfPipeline_)
            return nullptr;

        plSpec.fragmentModule = app->borderFrag_->shader_module();
        app->borderPipeline_ = vaml::vulkan::ui::UiPipeline::create(
            *app->loader_, *app->device_, plSpec, vaml::vulkan::ui::UiShaderKind::Border);
        if (!app->borderPipeline_)
            return nullptr;

        // ---- The S202 TextureAtlas (the SDF glyph atlas grid; 4096x2048, 16 cols x 8 rows) ----
        // 4096 % 16 == 0 && 2048 % 8 == 0 -> the S202 exact-grid contract. >= 95
        // tiles fills every printable-ASCII glyph (S203 FontAtlas requirement).
        app->atlas_ = vaml::vulkan::quad::TextureAtlas::create(
            *app->loader_, *app->device_, 4096u, 2048u, 16u, 8u);
        if (!app->atlas_)
            return nullptr;

        // ---- The S203 FontAtlas (bitmap glyphs into the S202 atlas) + the S204
        // SDF rasterizer. Build + upload so the atlas is shader-ready. ----
        app->fontAtlas_ = std::make_unique<vaml::vulkan::font::FontAtlas>(*app->atlas_);
        if (!app->fontAtlas_->load() || app->fontAtlas_->upload_glyphs() == 0u)
            return nullptr;
        app->sdfAtlas_ = std::make_unique<vaml::vulkan::font::SdfAtlas>(*app->atlas_, *app->fontAtlas_);
        if (!app->sdfAtlas_->build() || app->sdfAtlas_->upload_sdfs() == 0u)
            return nullptr;

        // ---- The S206 UiScene::Spec (the shared layout + 3 pipelines + the SDF atlas) ----
        vaml::vulkan::ui::UiScene::Spec sceneSpec;
        sceneSpec.renderPass     = app->renderPass_->render_pass();
        sceneSpec.colorFormat    = colorFormat;
        sceneSpec.extent         = { config.width, config.height };
        sceneSpec.pipelineLayout = app->sharedLayout_;
        sceneSpec.setLayout      = app->sharedSetLayout_;
        sceneSpec.sdfPipeline    = app->sdfPipeline_->pipeline();
        sceneSpec.solidPipeline  = app->solidPipeline_->pipeline();
        sceneSpec.borderPipeline = app->borderPipeline_->pipeline();
        sceneSpec.atlasView      = app->atlas_->image_view();
        sceneSpec.atlasSampler   = app->atlas_->sampler();
        sceneSpec.inFlightCount  = 3;   // the S166 ring depth (the S209 pool)

        // ---- The S212 create_device PRECONDITION gate ----
        if (!viewer_setup_ready(app->loader_->has_swapchain_loader(),
                                app->loader_->has_command_loader(),
                                app->renderPass_->render_pass(),
                                colorFormat,
                                app->atlas_->image_view(),
                                app->atlas_->sampler()))
        {
            return nullptr;
        }

        // ---- VamlRenderer (S212) — the LAST owner (declared last, borrowed everything) ----
        vaml::presentation::VamlRendererDeviceOptions hostOpts;
        hostOpts.loader     = app->loader_.get();
        hostOpts.device     = app->device_.get();
        hostOpts.surface    = app->surface_.get();
        hostOpts.renderPass = app->renderPass_->render_pass();
        hostOpts.sceneSpec  = sceneSpec;
        // The S173 presenter declaration (width/height/sampleCount/userData). The
        // `draw` member is filled by the host's internally-wired DrawBody.
        hostOpts.presenter.width       = config.width;
        hostOpts.presenter.height      = config.height;
        hostOpts.presenter.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        hostOpts.clearR = 0.18f; hostOpts.clearG = 0.22f; hostOpts.clearB = 0.26f; hostOpts.clearA = 1.0f;

        app->renderer_ = vaml::presentation::VamlRenderer::create_device(hostOpts);
        if (!app->renderer_)
            return nullptr;

        // ---- Attach the REAL core visual tree + frame size (the soup-to-nuts producer) ----
        // ADOPT the loaded tree into the app (unique_ptr move preserves the object,
        // so `uiRoot` stays valid — the object does not move). The host BORROWS the
        // root; rooting it here keeps it alive for the app's lifetime.
        app->root_ = std::move(load.root);
        if (!app->renderer_->set_root(uiRoot))
            return nullptr;
        app->renderer_->set_frame_size(static_cast<float>(config.width),
                                       static_cast<float>(config.height));

        // PUBLISH the initial declaration: arrange + encode + publish the loaded
        // tree into the retained render path NOW (not lazily on the first frame).
        // Without this, the first run_loop tick finds no scene to record
        // (recorded=false), tick() returns nullopt WITHOUT submitting/presenting
        // or advancing its frame slot, and the NEXT tick blocks/crashes waiting
        // on a fence the never-submitted frame left unsignaled. This is the
        // NO-GAP soy-to-nuts step ONLY a live render can prove.
        app->renderer_->invalidate();

        // ---- S222_1: arm the OPTIONAL file-watch live reload ----
        // When config.filePath is non-empty, the host re-renders the declared tree
        // on a file mtime change. The reload re-runs the whole S5 path (re-adopt ->
        // set_root -> set_frame_size -> invalidate -> retained record/present) with
        // ZERO imperative tree-building. On a load failure the app KEEPS the
        // last-good tree + reports to stderr (never silent). S222 behavior is
        // unchanged when filePath is empty (no watcher is built).
        if (!config.filePath.empty())
        {
            app->reloader_ = std::make_unique<vaml::viewer::LiveReload>(config.filePath);
            app->reloader_->on_reloaded(
                [app = app.get()](std::unique_ptr<vaml::DependencyObject> newRoot) {
                    if (!app->apply_reload(std::move(newRoot)))
                    {
                        std::fprintf(stderr,
                                     "vaml_viewer: reload produced a non-UIElement root; keeping the last-good tree\n");
                    }
                });
            app->reloader_->on_reload_error(
                [path = config.filePath](const std::string& detail) {
                    std::fprintf(stderr, "vaml_viewer: reload error for '%s': %s (keeping the last-good tree)\n",
                                 path.c_str(), detail.c_str());
                });
            app->reloader_->on_file_disappeared(
                [path = config.filePath]() {
                    std::fprintf(stderr, "vaml_viewer: '%s' disappeared; keeping the last-good tree rendering\n",
                                 path.c_str());
                });
            app->reloader_->begin();
        }

        // The loader member is owned for the app's lifetime (declared FIRST,
        // destroyed LAST — it outlives the renderer + every borrowed handle).
        return app;
    }
    catch (const std::exception&)
    {
        return nullptr;   // any throw (e.g. loader not found) -> no-throw create
    }
}

bool ViewerApp::apply_reload(std::unique_ptr<vaml::DependencyObject> newRoot)
{
    if (!newRoot)
        return false;
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(newRoot.get());
    if (!uiRoot)
        return false;   // not a UIElement -> cannot render; the app keeps the last-good tree

    // ADOPT the new tree (unique_ptr move preserves the object, so `uiRoot` stays
    // valid — the object does not move). The host BORROWS the root; rooting it here
    // keeps it alive for the app's lifetime, and the OLD tree is released below.
    root_ = std::move(newRoot);

    if (!renderer_)
        return true;   // no host (headless create() path) — the adoption is enough
    if (!renderer_->set_root(uiRoot))
        return false;
    renderer_->set_frame_size(static_cast<float>(cfg_.width), static_cast<float>(cfg_.height));
    // The whole S5 path re-runs: arrange + encode + publish (the S222 producer path,
    // re-invoked — NOT hand-built). The retained renderer records + presents the
    // re-declared tree; the swapchain/presenter/device are NOT recreated.
    (void)renderer_->invalidate();
    return true;
}

int ViewerApp::run_loop()
{
    if (!renderer_ || !window_)
        return 1;

    // The deterministic headful auto-close for the local gate: when
    // VAML_VIEWER_AUTO_CLOSE_MS is set, the loop closes after the elapsed time
    // (so [both apps, not CI] live demo is scriptable deterministically).
    // Used by both MSVC (the S163 secure-variant precedent via _dupenv_s) and
    // clang/POSIX (std::getenv — C4996 is an MSVC-only deprecation, so no guard
    // is needed on the Linux CI lane).
    std::uint64_t deadlineMs = 0;
#if defined(_MSC_VER)
    char* envBuf = nullptr;
    std::size_t envLen = 0;
    const char* autoClose = nullptr;
    if (_dupenv_s(&envBuf, &envLen, "VAML_VIEWER_AUTO_CLOSE_MS") == 0 && envBuf)
        autoClose = envBuf;
    if (autoClose)
        deadlineMs = static_cast<std::uint64_t>(std::strtoull(autoClose, nullptr, 10));
    if (envBuf)
        std::free(envBuf);
#else
    const char* autoClose = std::getenv("VAML_VIEWER_AUTO_CLOSE_MS");
    if (autoClose)
        deadlineMs = static_cast<std::uint64_t>(std::strtoull(autoClose, nullptr, 10));
#endif
    const auto loopStart = std::chrono::steady_clock::now();

    // The change-driven loop: poll the window events, then tick the host. The
    // first frame is invalidated (the initial arrange+encode+publish).
    // The change-driven loop: poll the window events, then tick the host. The
    // first frame is invalidated (the initial arrange+encode+publish).
    // lastResizeW/H track the last size forwarded to request_resize so a resize
    // is requested ONLY on an actual change (never a spurious per-frame rebuild —
    // each request_resize queues a full S167_1 swapchain recreation).
    std::uint32_t lastResizeW = 0u, lastResizeH = 0u;
    while (!window_->should_close())
    {
        window_->poll_events();

        // Resize: forward the window's current framebuffer size as a logical frame
        // (S213 DPI derives the scale from it). The Presenter handles the swapchain
        // recreation transactionally (S167_1). Request only on a genuine change.
        int fbw = 0, fbh = 0;
        window_->get_framebuffer_size(&fbw, &fbh);
        if (fbw > 0 && fbh > 0 &&
            (static_cast<std::uint32_t>(fbw) != lastResizeW ||
             static_cast<std::uint32_t>(fbh) != lastResizeH))
        {
            renderer_->request_resize(static_cast<std::uint32_t>(fbw), static_cast<std::uint32_t>(fbh));
            lastResizeW = static_cast<std::uint32_t>(fbw);
            lastResizeH = static_cast<std::uint32_t>(fbh);
        }

        // The change-driven gate: render (the first frame is always a change;
        // subsequent frames tick for the retained re-render + consume-ahead).
        (void)tick_host(*renderer_, /*invalidated=*/true, /*tickDue=*/true);

        // S222_1: poll the file-watch live reload (only when armed — config.filePath
        // was non-empty). A Changed/Appeared event re-runs the whole S5 path
        // (apply_reload -> set_root -> set_frame_size -> invalidate); the retained
        // renderer records + presents the re-declared tree. A load failure keeps
        // the last-good tree + reports to stderr (never silent). Idle polls are the
        // KeepLast no-op. No new locks (ADR-018): single-threaded, UI thread.
        if (reloader_)
            (void)reloader_->check();

        // The deadline (VAML_VIEWER_AUTO_CLOSE_MS): once elapsed, close the window.
        if (autoClose && viewer_deadline_elapsed(loopStart, deadlineMs))
        {
            window_->request_close();
        }
    }
    return 0;
}

} // namespace vaml::viewer
