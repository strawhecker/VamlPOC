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

#include "load_file.h"   // vaml::LoadResult (the S221 seam; owns the borrowed tree)

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

#include <chrono>
#include <cstdint>
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

    ~ViewerApp();
    ViewerApp(ViewerApp&&) noexcept;
    ViewerApp& operator=(ViewerApp&&) noexcept;
    ViewerApp(const ViewerApp&)            = delete;
    ViewerApp& operator=(const ViewerApp&) = delete;

    // Runs the change-driven loop until the window closes (or VAML_VIEWER_AUTO_
    // CLOSE_MS elapses — the deterministic headful auto-close for the local
    // gate). Returns 0 on a clean close. Never throws out of the window.
    int run_loop();

    // Headless-observable state (for the test + the CLI summary).
    [[nodiscard]] bool created() const noexcept { return renderer_ != nullptr; }
    [[nodiscard]] const vaml::presentation::VamlRenderer* renderer() const noexcept { return renderer_.get(); }

private:
    ViewerApp();

    // The owned tree (ADOPTED from LoadResult). Declared before renderer_ so it
    // is destroyed AFTER the host that borrows it.
    std::unique_ptr<vaml::DependencyObject> root_;

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

    ViewerAppConfig cfg_;
};

} // namespace vaml::viewer
