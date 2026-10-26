// vaml_viewer — the shared "soup-to-nuts" render host (S222, dep:S221).
//
// THE SINGLE SOURCE OF TRUTH for the viewer's LIVE render path. S222's contract:
// after `load_file` succeeds (S221), open a REAL window + the Vulkan stack +
// `VamlRenderer::create_device` and render the DECLARED tree through the FULL
// retained-render path (S174 CompositionEngine + S173 Presenter + S207/S208 scene
// encoder + S210 SceneRenderer) via ONLY the public surface (ADR-021). This
// DEMONSTRATES S212_1 — the library's create_device present objective — in the
// real window. Structurally builds on S212's vertical slice.
//
// This host is compiled by BOTH apps AND the test target (exactly the S221
// load_file seam pattern). It owns the WHOLE Vulkan stack + VamlRenderer behind
// one owner and is the genuine ADR-021 consumer path: it includes ONLY public
// <vaml/...> headers, never a header under src/<lib>/src.
//
// SPLIT (the standing S162/S200/S206/S212 precedent):
//   * PURE DECISION SEAMS (headless-testable, NO Vulkan runtime required) —
//       `viewer_stack_steps()`  — the serialized build order of the S222 stack,
//                                 proving the LIFETIMES contract (each borrowed
//                                 handle outlives the next owner; the last-built
//                                 VamlRenderer borrows them all).
//       `viewer_setup_ready()`  — the create_device PRECONDITION gate, REUSING
//                                 S173 `presenter_lifetimes_ok` + S200
//                                 `quad::is_srgb_format` + non-null atlas
//                                 view/sampler.
//     These PROVE the soup-to-nuts wiring contract headlessly (the S222 proof
//     surface — ctest without a live window/runtime).
//   * `ViewerApp` — the REAL owner (RAII): builds the platform window + the
//     Vulkan stack (Loader -> Instance -> Surface -> Device -> RenderPass ->
//     shaders -> three UiPipelines -> TextureAtlas/FontAtlas/SdfAtlas -> the
//     UiScene spec) and then `VamlRenderer::create_device` over it. Member order
//     == reverse destruction: the VamlRenderer (which borrows render-pass +
//     pipeline handles + atlas view/sampler + the tree) is declared LAST and
//     destroyed FIRST; everything it borrows is declared before it and destroyed
//     after. [both apps, not CI] — a live window + driver.
//
// SCOPE (recorded, NOT silent): real control MATERIALS (Text/Button -> S400/S401)
// do not exist yet, so the stack is wired for the SDF atlas (Ready-pass-ready)
// but the slice renders what IS renderable today — the real layout panels filled
// at their arranged final_rects via the S212 default visualOf classifier. Window
// title polish + pan/zoom + --help -> S224; live file-watch reload -> S222_1.
// NO MINIMAL REAL introduced here — every seam + owner is real, permanent,
// kept-and-extended.

#pragma once

#include "load_file.h"     // vaml::LoadResult (the S221 seam; owns the borrowed tree)
#include "compiled_open.h" // S293: CompiledOpenResult / load_compiled_file (the compiled .vbaml seam)
#include "live_reload.h"   // vaml::viewer::LiveReload (S222_1 — the file-watch reload coordinator)
#include "view_transform.h"// S224: vaml::viewer::ViewState + the pan/zoom seams (pure)
#include "viewer_binding.h"// S256: ViewerDemoViewModel + run_viewer_binding_plan (the binding reload seam)
#include "app_shell.h"     // S440: run_app_shell_plan + AppShellPlanResult (the app-shell load seam)
#include "app_shell_compiled.h" // S444: load_compiled_app_shell_file/embedded + AppShellCompiledPlanResult

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/PlatformWindow.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/quad/QuadPipeline.h>     // quad::is_srgb_format (S200)
#include <vaml/vulkan/quad/TextureAtlas.h>     // quad::TextureAtlas (S202)
#include <vaml/vulkan/font/FontAtlas.h>        // font::FontAtlas (S203)
#include <vaml/vulkan/font/SdfAtlas.h>         // font::SdfAtlas (S204)
#include <vaml/vulkan/ui/UiPipeline.h>         // ui::UiPipeline (S205)
#include <vaml/vulkan/ui/UiScene.h>            // ui::UiScene::Spec (S206)
#include <vaml/vulkan/ui/SceneRenderer.h>      // ui::SceneRenderer (S210)
#include <vaml/presentation/Presenter.h>       // presenter_lifetimes_ok (S173)
#include <vaml/presentation/VamlRenderer.h>    // vaml::presentation::VamlRenderer (S212)
#include <vaml/tools/VBamlReader.h>            // S293: vaml::tools::VBamlReader (the compiled retained reader)

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vaml::viewer {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The S222 stack build order — the LIFETIMES CONTRACT, serialized. The steps
// run in this ORDER at create() (each borrowed handle outlives the next owner)
// and the owners are destroyed in REVERSE (a stack, last-built first-destroyed):
//   Loader   -> the runtime entry points (bound once, process-lifetime).
//   Instance -> created on the loader.
//   Surface  -> created on the instance + window (window outlives it).
//   Gpu      -> the best present-capable physical device chosen (S163).
//   Device   -> the logical device + graphics queue (borrows the gpu/instance).
//   RenderPass -> the S165 color(+depth) pass (borrows loader/device).
//   Pipelines  -> the S205 shared-layout UiPipelies (borrow render-pass/device).
//   Atlas    -> the S202 TextureAtlas (borrows loader/device).
//   Font     -> the S203 FontAtlas (borrows the atlas).
//   Sdf      -> the S204 SdfAtlas (borrows the atlas + font).
//   UiScene  -> the S206 spec (borrows the shared layout + 3 pipelines + atlas).
//   Host     -> VamlRenderer::create_device (borrows loader/device/surface/
//              render-pass + every sceneSpec handle). Declared last => destroyed
//              first; everything it borrows is declared before it.
// Pure + headless-testable — proves the wiring order without a live stack.
const std::vector<std::string>& viewer_stack_steps() noexcept;

// A single step name accessor (readable log + test assertions).
const char* viewer_step_name(std::size_t i) noexcept;

// The create_device PRECONDITION GATE (the S222 wiring contract, headless):
// the host may only be built when
//   (1) S173 `presenter_lifetimes_ok` holds — swapchain + command loaders live
//       and a real render pass is present (the Presenter + VamlRenderer borrow
//       them),
//   (2) the swapchain color format is SRGB (`quad::is_srgb_format`, S200 — the
//       UiScene + UiPipeline SRGB pairing), and
//   (3) the atlas view + sampler are non-null (the S204 SDF atlas the shared
//       set-0-binding-0 sampler samples — UiScene::create requires them).
// Pure + headless-testable — the S173/S200/S206 gates recomposed into ONE
// front-door check the live create() runs first.
bool viewer_setup_ready(bool hasSwapchainLoader,
                        bool hasCommandLoader,
                        VkRenderPass renderPass,
                        VkFormat colorFormat,
                        VkImageView atlasView,
                        VkSampler atlasSampler) noexcept;

// The auto-close deadline gate (the VAML_VIEWER_AUTO_CLOSE_MS seam): returns
// true once `start` (a steady-clock time point captured at the first loop
// iteration) is at least `deadlineMs` ms in the past. `deadlineMs == 0` -> never
// (the no-auto-close default). Pure + headless-testable — this is the single
// pure seam the live run_loop()'s deterministic auto-close gate uses.
inline bool viewer_deadline_elapsed(
    const std::chrono::steady_clock::time_point& start,
    std::uint64_t deadlineMs) noexcept
{
    if (deadlineMs == 0)
        return false;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    return static_cast<std::uint64_t>(elapsed) >= deadlineMs;
}

// ---------------------------------------------------------------------------
// The REAL "soup-to-nuts" render host (RAII; move-only)
// ---------------------------------------------------------------------------

// The config the app supplies to open the window + build the stack.
struct ViewerAppConfig
{
    std::string title  = "VAML Viewer";
    std::uint32_t width  = 900;   // logical layout px (set_frame_size)
    std::uint32_t height = 600;
    // S222_1 (file-watch live reload): when NON-EMPTY, the host arms a LiveReload
    // on this file path and re-renders the declared tree on a change (the whole S5
    // path re-runs with ZERO imperative tree-building). When EMPTY (the default) the
    // host behaves exactly as S222 (no watcher, no behavior change).
    std::string filePath;
};

// The REAL owner: builds the whole Vulkan stack + the VamlRenderer behind one
// owner and runs the change-driven loop until the window closes. Constructed
// ONLY via create(); returns nullptr (no throw) on ANY build failure so callers
// never juggle a half-constructed stack. Member order == reverse destruction (see
// viewer_stack_steps + the header comment). [both apps, not CI] — a live driver
// + window are required (the DISPLAY proof is never a CI lane, per the standing
// S160+ precedent).
class ViewerApp
{
public:
    // Builds the stack + the VamlRenderer over `load` (which MUST be ok and own
    // a root that dynamic_casts to a UIElement — the REAL core visual tree). The
    // app ADOPTS the loaded tree (root_) so the host's borrowed root stays alive
    // for the app's lifetime. `load` is taken BY VALUE (moved in) so the tree's
    // unique_ptr ownership transfers to the app. Returns nullptr on any step
    // failure (no throw).
    [[nodiscard]] static std::unique_ptr<ViewerApp>
    create(const ViewerAppConfig& config, vaml::LoadResult load);

    // S293 — the COMPILED-open variant. Builds the stack + the VamlRenderer
    // over a COMPILED `.vbaml` open (CompiledOpenResult, the S7 VBamlReader
    // tree). `compiled` MUST be ok and own a root that dynamic_casts to a
    // UIElement. The app ADOPTS the compiled root (root_) + the RETAINED S7
    // reader (compiledReader_) so the tree's borrowed non-root objects stay
    // alive for the app's lifetime + the S286 named() registry stays live.
    // Returns nullptr on any step failure (no throw).
    [[nodiscard]] static std::unique_ptr<ViewerApp>
    create_compiled(const ViewerAppConfig& config, CompiledOpenResult compiled);

    // S440 — the APP-SHELL launch variant (the `--app` mode, Sprint 11 v1 app).
    // Builds the stack + the VamlRenderer over the in-repo app-shell: runs
    // `run_app_shell_plan()` (loads the authored MainView.vaml through a RETAINED
    // VamlReader + finalize_visual_tree — the S120/S121 hand-off that RENDERS the
    // declared shell children), adopts the shell tree (the S440 REAL Window root)
    // + RETAINS the app-shell reader so the S443 {Binding}/{Command} wiring stays
    // armed, and renders it. Returns nullptr on any failure (no throw). A plan
    // failure reports its structured `detail` to stderr (never silent).
    [[nodiscard]] static std::unique_ptr<ViewerApp>
    create_app_shell(const ViewerAppConfig& config);

    // S444 — the COMPILED APP-SHELL launch variant (the `--compiled --app`
    // mode). Builds the stack + the VamlRenderer over the COMPILED app-shell:
    // `compiled` is the S444 AppShellCompiledPlanResult (the MainView.vbaml
    // decoded through the S7 VBamlReader + the S440 finalize hand-off). The app
    // ADOPTS the compiled root (root_) + RETAINS the S7 reader
    // (compiledReader_) so the tree's borrowed non-root objects + the S286
    // named() registry stay alive. Returns nullptr on any failure (no throw).
    // A plan failure reports its structured `detail` to stderr (never silent).
    [[nodiscard]] static std::unique_ptr<ViewerApp>
    create_compiled_app_shell(const ViewerAppConfig& config,
                              vaml::viewer::AppShellCompiledPlanResult compiled);

    ~ViewerApp();
    ViewerApp(ViewerApp&&) noexcept;
    ViewerApp& operator=(ViewerApp&&) noexcept;
    ViewerApp(const ViewerApp&)            = delete;
    ViewerApp& operator=(const ViewerApp&) = delete;

    // Runs the change-driven loop until the window closes (or VAML_VIEWER_AUTO_
    // CLOSE_MS elapses — the deterministic headful auto-close for the local
    // gate). Returns 0 on a clean close. Never throws out of the window.
    int run_loop();

    // S223 REOPEN seam — a thin public wrapper over apply_reload: adopts a
    // freshly-loaded `newRoot` and re-runs the whole S5 path (the S222_1 reopen
    // machinery: set_root + set_frame_size + invalidate). This is the SINGLE
    // reopen HANDLER shared by BOTH drop-reopen and file-watch reload (no
    // drift). Returns false if `newRoot` is null/not a UIElement (the app keeps
    // its last-good tree — never a blank/garbage render).
    [[nodiscard]] bool reopen(std::unique_ptr<vaml::DependencyObject> newRoot);

    // Headless-observable state (for the test + the CLI summary).
    [[nodiscard]] bool created() const noexcept { return renderer_ != nullptr; }
    [[nodiscard]] const vaml::presentation::VamlRenderer* renderer() const noexcept { return renderer_.get(); }
    // S224 — the RENDERED-VIEW PAN & ZOOM seam (the rendered VIEW, not editor chrome):
    // the viewer keeps the S224 ViewState (pan/zoom/live) + a small number of input
    // hooks the app's window callbacks call. They fold the S224 math into the S208
    // Matrix2D and push it into the S213 render surface via renderer_->set_view_transform.
    // Headless-observable so the pan/zoom model is testable WITHOUT a window/runtime.
    [[nodiscard]] const ViewState& view_state() const noexcept { return view_; }
    // Mouse button hook (S160 dispatch_mouse_button → routed into the window's
    // callbacks); (x, y) is the button event position in logical viewport px.
    // Button == GLFW_MOUSE_BUTTON_LEFT; down=true begins a drag (arming pan),
    // down=false ends it. The chain is: button-down captures the drag origin +
    // arming; mouse-move with button held pans; the wheel zooms about the cursor.
    void on_mouse_button_drag(double x, double y, bool buttonLeftDown);
    // Mouse move (S160 dispatch_mouse_move): pans when a left-button drag is armed.
    void on_mouse_drag(double x, double y);
    // Scroll wheel (S160 dispatch_scroll): zooms about the current cursor (x, y).
    void on_scroll_zoom(double x, double y, double yoffset);

private:
    ViewerApp();

    // The owned tree (ADOPTED from LoadResult). Declared before renderer_ so it
    // is destroyed AFTER the host that borrows it.
    std::unique_ptr<vaml::DependencyObject> root_;

    // S256 — the retained VIEWER BINDING state: the app-constructed VM + the
    // retained reader that keeps the S252 live bindings (reader-owned, per-Load)
    // armed across the reload path. The reload binds via the S256 seam
    // (run_viewer_binding_plan) so a VM change re-pushes the visible dim through
    // the S243 live wire. The VM is the S250 app-owned pattern (NO DI); the app
    // owns it, the reader borrows it.
    std::shared_ptr<vaml::viewer::ViewerDemoViewModel> bindingVM_;
    std::unique_ptr<vaml::VamlReader> bindingReader_;   // S256: retained reader (keeps the live bindings)
    // S440 — the retained APP-SHELL reader (the --app mode). Keeps the S120/S121
    // finalized visual tree + any S443 {Binding}/{Command} wiring armed across the
    // app's lifetime (mirrors the bindingReader_ retention pattern). Null outside
    // --app mode.
    std::unique_ptr<vaml::VamlReader> appShellReader_;

    // S293 — the retained COMPILED reader (the S7 VBamlReader). When the app was
    // opened via create_compiled, `compiledReader_` owns the non-root retained
    // objects the tree borrows (via parent links) + the S286 named() registry,
    // so both stay alive for the app's lifetime. Null in the text-open mode.
    std::unique_ptr<vaml::tools::VBamlReader> compiledReader_;

    // ---- The S222 Vulkan stack (member order == reverse destruction) ----
    // Constructed in build order; destroyed in REVERSE (last-declared first).
    // The renderer_ (declared LAST) borrows the render pass + pipeline handles +
    // atlas view/sampler + the root; everything it borrows is declared before it.
    // The loader_ (declared FIRST) is destroyed LAST — it must outlive the renderer
    // and every borrowed handle (the VamlRenderer + the whole stack borrow it).
    std::unique_ptr<vaml::vulkan::VulkanLoader>  loader_;
    std::unique_ptr<vaml::vulkan::PlatformWindow>  window_;
    std::unique_ptr<vaml::vulkan::VulkanInstance>  instance_;
    std::unique_ptr<vaml::vulkan::VulkanSurface>   surface_;
    std::unique_ptr<vaml::vulkan::Device>          device_;
    std::unique_ptr<vaml::vulkan::RenderPass>      renderPass_;
    std::unique_ptr<vaml::vulkan::ShaderModule>    uiVertex_;
    std::unique_ptr<vaml::vulkan::ShaderModule>    sdfFrag_;
    std::unique_ptr<vaml::vulkan::ShaderModule>    solidFrag_;
    std::unique_ptr<vaml::vulkan::ShaderModule>    borderFrag_;
    std::unique_ptr<vaml::vulkan::ui::UiPipeline>  solidPipeline_;
    std::unique_ptr<vaml::vulkan::ui::UiPipeline>  sdfPipeline_;
    std::unique_ptr<vaml::vulkan::ui::UiPipeline>  borderPipeline_;
    std::unique_ptr<vaml::vulkan::quad::TextureAtlas>  atlas_;
    std::unique_ptr<vaml::vulkan::font::FontAtlas>     fontAtlas_;
    std::unique_ptr<vaml::vulkan::font::SdfAtlas>      sdfAtlas_;
    // The SHARED S205 pipeline layout + set layout (borrowed from the first
    // UiPipeline; they are identical across all three — the shared-layout contract).
    VkPipelineLayout     sharedLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout sharedSetLayout_ = VK_NULL_HANDLE;

    // The HOST (declared last => destroyed FIRST). Borrows every live handle above.
    std::unique_ptr<vaml::presentation::VamlRenderer> renderer_;

    // S222_1: the OPTIONAL file-watch reloader (built ONLY when config.filePath is
    // non-empty). Declared after renderer_ so it is destroyed before the host that
    // borrows the tree it re-adopts. The reloaded tree is handed to apply_reload_.
    std::unique_ptr<vaml::viewer::LiveReload> reloader_;

    ViewerAppConfig cfg_;

    // S223: the pending FILE-DROP slot. The GLFW on_drop callback (registered in
    // create()) stores the chosen `.vaml` path here (a DROP sets it, never clears);
    // run_loop() drains it AFTER a poll_events() tick and reopens the file. This is
    // the single cross-thread-free handoff (ADR-018: all on the UI thread — GLFW
    // fires the drop callback inside poll_events()). `pendingDropApplied_` tracks
    // whether the CURRENT pending drop was already reopened this loop, so a drop
    // can't be reapplied on every frame.
    std::optional<std::string> pendingDrop_;
    bool pendingDropApplied_ = false;

    // S224 — the RENDERED-VIEW PAN & ZOOM state (rendered view, not editor chrome).
    // The VIEW MODEL + input bookkeeping. All single-threaded on the UI thread
    // (ADR-018 — the pan/zoom hooks fire inside poll_events() / the loop; no locks).
    ViewState view_;                 // the live pan/zoom parameters (pure seam)
    bool dragArmed_ = false;         // a left-button drag is in progress (pans on move)
    double dragLastX_ = 0.0;         // the last mouse x (logical px) during a drag
    double dragLastY_ = 0.0;         // the last mouse y (logical px) during a drag
    double cursorX_ = 0.0;           // the last reported cursor x (logical px)
    double cursorY_ = 0.0;           // the last reported cursor y (logical px)

    // S222_1: applies a freshly Loaded tree to the retained renderer — the whole S5
    // path re-runs with ZERO imperative tree-building: adopt the new root (the object
    // does not move, so the UIElement pointer stays valid), set_root + set_frame_size,
    // then invalidate() to arrange + encode + publish (the exact S222 producer path,
    // re-invoked — NOT hand-built). Returns false if the new root is not a UIElement.
    bool apply_reload(std::unique_ptr<vaml::DependencyObject> newRoot);

    // S256: loads + BINDS the source `text` through the S256 seam
    // (run_viewer_binding_plan — the app-constructed VM + retained reader + the
    // S252 BindExtensions markup resolution) and, on a clean bind, ADOPTS the bound
    // tree (root_/bindingVM_/bindingReader_) + re-runs the whole S5 path (set_root +
    // set_frame_size + invalidate). Returns false (last-good KEPT, reported) on a
    // load/bind failure — never silent, never a blank window. This is the live
    // "you SEE data binding appear in the window" path: the bound `{Binding Dim}`
    // lands on the target Opacity DP, which the S208 encoder folds into the alpha.
    bool bind_and_render(const std::string& text);

    // The SHARED stack-builder (S293 refactor): builds the whole Vulkan stack +
    // the VamlRenderer + attaches `root` (a UIElement) + arms the optional reloader,
    // and ADOPTS `compiledReader` (the S7 VBamlReader) into compiledReader_ when
    // non-null (the compiled-open retained reader — keeps the tree's non-root
    // borrowed objects + the S286 named() registry alive). Both create() and
    // create_compiled() delegate here (no drift). Returns nullptr on any failure
    // (no throw).
    [[nodiscard]] static std::unique_ptr<ViewerApp>
    create_impl(const ViewerAppConfig& config,
                std::unique_ptr<vaml::DependencyObject> root,
                vaml::UIElement* uiRoot,
                std::unique_ptr<vaml::tools::VBamlReader> compiledReader);

    // S224 — pushes the current ViewState (as the S208 Matrix2D) into the S213
    // render surface (renderer_->set_view_transform). Called by the pan/zoom input
    // hooks after folding a delta/zoom. The host's record_draw composes it into the
    // plan mvp next present — redraw-only, no re-encode.
    void push_view_transform();
};

} // namespace vaml::viewer
