#pragma once
// S212 (dep:S211) — MERGE POINT: UI declared in VAML renders to window via the
// full S5 path (the first real vertical slice). vaml:presentation owns the
// vertical-slice RENDER HOST.
//
// The Sprint-5 accumulated hand-offs all RESOLVE here at once:
//   * S207/S208 — the scene encoder (a depth-first pre-order walk of the REAL core
//     visual tree into the S206 DrawItem list, draw-list-by-material, z-order via
//     tree order) FEEDS this host's CPU encode.
//   * S211 — the lock-free MarshalledScene postbox (UI thread mutates + publishes a
//     CPU-encoded snapshot; the render thread consumes the LATEST once per frame).
//   * S210 — the retained SceneRenderer (encode once, record every frame without
//     re-encode, recycle via the S209 fence-driven pool).
//   * S173 — the Presenter (the ONE owner of the change-driven loop, acquire/
//     submit/present + the S167_1 two-generation recovery).
//
// VamlRenderer COMPOSES those surfaces (never re-authors them) so the full path
//   VAML text -> VamlReader::Load -> UIElement root -> Measure/Arrange final_rects
//   -> encode_visual_tree (visualOf) -> postbox (mutate+publish) -> consume_changes
//   -> set_scene (re-encode-on-newer) -> record retained -> recycle -> present
// is ONE public surface. S222 (the official viewer) consumes ONLY this public
// surface ("consumer touches ONLY the public surface") — so the MERGE LOGIC is a
// library surface, never POC-internal code (S220 deletes the POCs).
//
// SPLIT (the S162/S200/S206/S210 standing precedent):
//   * PURE DECISION SEAMS + the CPU CORE (headless-testable, NO Vulkan runtime) —
//       encode_visual_slice(root, visualOf) (the merge's encoder -> the S206 draw list)
//       default_visual_of()               (the vertical-slice classifier: fill each
//                                         real UIElement panel at its final_rect)
//       render_loop_steps()               (the S210/S211 fixed per-frame loop)
//       should_reencode(dirty)            (the retain gate)
//       should_consume(hasNewer)          (the drain gate)
//       presenter_lifetimes_ok(...)       (the S173 no-handle lifetime gate)
//       VamlRenderer::create()            (the CPU/merge host: owns the postbox,
//                                         the tree reference, the visualOf seam, and
//                                         the arrange+encode+invalidate producer path)
//     These prove the vertical-slice loop contract in ctest WITHOUT a live runtime
//     (the S211 MarshalledScene + the S210 loop seams are reused, not re-authored).
//   * `VamlRenderer::create_device(...)`  — the REAL live owner (RAII, move-only):
//     builds + owns the SceneRenderer + the Presenter behind ONE host, and wires
//     the Presenter's DrawBody internally = ConsumeMarshalled -> EncodeIfDirty
//     (record retained) -> RecordRetained -> Recycle. Device-backed ([both apps,
//     not CI] per the S206/S210/S211 precedent).
//
// JUDGMENT (recorded, accepted at plan review): the honest first vertical slice
// renders the DECLARED LAYOUT by filling each real panel at its final_rect via the
// `visualOf` classifier seam (real controls — Text/Button -> S400/S401 — do not
// exist yet, so their material render is NOT backfilled here per the MINIMAL-REAL/
// NO-GAP sentinel contract; a real panel tree produces real, visible, arranged
// rectangles). That seam is the S207 forward seam real control materials replace in
// place (kept-and-extended, never a core virtual per ADR-011).
//
// LIFETIMES: for create_device the loader/device/surface/render-pass must OUTLIVE
// the host (borrowed; documented). The SceneRenderer + Presenter are owned
// (destroyed in reverse build order on host destruction).

#include <vaml/presentation/Presenter.h>        // Presenter, PresenterOptions
#include <vaml/presentation/dpi.h>              // S213: dpi::RenderPlan (the composed view surface)
#include <vaml/vulkan/ui/UiScene.h>             // DrawItem, UiScene::Spec
#include <vaml/vulkan/ui/UiSceneEncoder.h>      // ElementVisual, encode_visual_tree
#include <vaml/vulkan/ui/SceneRenderer.h>       // SceneRenderer (the retained owner)
#include <vaml/vulkan/ui/MarshalledScene.h>     // MarshalledScene (the postbox)

#include <vaml/core/UIElement.h>                // vaml::UIElement (the core visual tree)
#include <vaml/core/Transform.h>                // S208: vaml::Matrix2D (the rendered-view pan/zoom type)

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The vertical-slice classifier seam: maps a core UIElement to its ElementVisual
// (material + draw). Default = `classify_merge_element` (below). Replaced in place
// by real control materials (Text->S400, templates->S364) — never a core virtual
// (ADR-011). The encoder's walk + skip + grouping stays authoritative.
using VisualOf = std::function<vaml::vulkan::ui::ElementVisual(const vaml::UIElement&)>;

// The DEFAULT vertical-slice classifier (the S207 seam): every REAL UIElement
// (today: the layout panels StackPanel/Grid/WrapPanel/Canvas, all re-based onto
// UIElement at S129/S131/S134/S136) contributes a SOLID fill at its ARRANGED
// final_rect, tinted by its S208 effective_opacity. A non-UIElement core node
// (a DependencyObject-only MINIMAL-REAL control seed — Button/Text etc., not yet
// re-based) contributes None (no draw; its subtree still walks). Kept-and-extended,
// replaced in place by real control materials at S400/S364. Pure + headless.
VAML_PRESENTATION_EXPORT
vaml::vulkan::ui::ElementVisual
classify_merge_element(const vaml::UIElement& element) noexcept;

// The default VisualOf: every visited UIElement -> classify_merge_element. Pure.
VAML_PRESENTATION_EXPORT VisualOf default_visual_of();

// The merge's ENCODER: runs the S207 encode_visual_tree over the arranged core
// tree rooted at `root` into the S206 draw list `out`. `visualOf` is the material
// authority seam (default_visual_of() unless overridden). The emitted list is
// z-ordered (pre-order) + material-grouped = the exact input S206's batch_plan /
// the S210 retained set_scene consume. Pure walker over the borrowed tree.
VAML_PRESENTATION_EXPORT void
encode_visual_slice(const vaml::UIElement& root, const VisualOf& visualOf,
                    std::vector<vaml::vulkan::ui::DrawItem>& out);

// The S210/S211 fixed per-frame RENDER-LOOP PLAN the host's DrawBody runs:
// WaitFence -> ConsumeMarshalled -> EncodeIfDirty -> RecordRetained -> SubmitPresent
// -> Recycle (ConsumeMarshalled ahead of EncodeIfDirty, in place — the S211 loop).
// Delegates to vaml::vulkan::ui::render_loop_steps (reused, not re-authored), so
// the host's DrawBody seam == the fixed sequence. Pure + headless.
VAML_PRESENTATION_EXPORT
const std::vector<vaml::vulkan::ui::RenderLoopStep>& render_loop_steps() noexcept;

// The retain + drain gates (reused from S210/S211): a dirty flag forces a re-encode
// (EncodeIfDirty); an unchanged frame does NOT re-encode. Pure + headless.
VAML_PRESENTATION_EXPORT bool should_reencode(bool dirty) noexcept;
VAML_PRESENTATION_EXPORT bool should_consume(bool hasNewer) noexcept;

// The S224 rendered-view PAN & ZOOM surface — THE S208/S213 HARNEESSING PURE SEAM.
//
// Composes the S208 `view` Matrix2D (the S224 viewer-level VIEW TRANSFORM — pan +
// zoom authored by the viewer, reused from vaml::core Transform.h) into the S213
// `dpi::RenderPlan` mvp so the ENTIRE retained scene pans/zooms together at the
// MVP level. `plan.mvp` is the S213 logical-canvas -> NDC projection; a
// 2D-affine `view` (translation ∘ scaling, in logical px) left-multiplies it:
//   mvp' = To4x4(view) · mvp
// so a logical content point is first mapped by the 2D view then projected —
// the standard "view ∘ model/projection" composition. `plan.uiScale` is LEFT
// UNTOUCHED (the DERIVED physical/logical DPI scale, S213's crisp invariant), so
// the composed view stays DPI-crisp AND the S208 clip scissor (which maps logical
// -> frame px via uiScale) stays ALIGNED with the scaled geometry. Because the
// view lives in the mvp and every element model stays identity (S212's default
// visualOf), ALL emitted elements pan/zoom together (multi-panel correct, no
// per-element injection) and only the ROOT clip (the full window) remains — which
// is correct under any view mvp (overflow is naturally framebuffer-culled).
//
// Pure + headless-testable (no Vulkan runtime) — the same S213 discipline. Seeing
// the view transform exercised on the S208 type + the S213 render-plan surface is
// exactly the S224 "viewer chrome harnesses library work" contract.
VAML_PRESENTATION_EXPORT void
compose_view_into_render_plan(const vaml::Matrix2D& view, dpi::RenderPlan& plan) noexcept;

// The S173 presenter-lifetime gate is REUSED (already declared in Presenter.h:
// presenter_lifetimes_ok); the host applies it in create_device. Not re-authored.

// ---------------------------------------------------------------------------
// The vertical-slice render host (RAII; move-only)
// ---------------------------------------------------------------------------

// Options for the LIVE create_device path (the device-gated integration). The
// borrowed loader/device/surface/render-pass must outlive the returned host.
struct VamlRendererDeviceOptions
{
    const vaml::vulkan::VulkanLoader* loader    = nullptr;  // borrowed; must outlive
    const vaml::vulkan::Device*       device    = nullptr;  // borrowed; must outlive
    const vaml::vulkan::VulkanSurface* surface  = nullptr;  // borrowed; must outlive
    VkRenderPass                      renderPass = VK_NULL_HANDLE;  // borrowed; must outlive
    // The S206 scene spec (the SHARED pipelines + set layout + atlas). The pipeline
    // handles must outlive the host.
    vaml::vulkan::ui::UiScene::Spec    sceneSpec{};
    // The S173 presenter declaration (width/height/sampleCount/userData/callbacks).
    // The `draw` member is IGNORED/filled by the host's internally-wired DrawBody.
    PresenterOptions                   presenter{};
    // The S166 clear color the host's DrawBody uses when beginning the render pass.
    float clearR = 0.18f, clearG = 0.22f, clearB = 0.26f, clearA = 1.0f;
};

// The vertical-slice RENDER HOST: composes the CPU encode + the S211 postbox +
// the S210 retained renderer + the S173 presenter behind ONE public surface. The
// app uses the UI-thread surface (set_root/set_frame_size/invalidate) and the
// render-loop surface (tick/request_resize/state); it never touches the loop.
//
// Two creation paths:
//   * create()        — the CPU/merge host (headless): owns the postbox + the
//                       tree reference + the visualOf seam + the arrange/encode
//                       producer path. Constructible WITHOUT Vulkan (the S222
//                       CPU half + the Rail-1 headless test).
//   * create_device() — the live owner: additionally builds + owns the retained
//                       SceneRenderer + the Presenter and wires the Presenter's
//                       DrawBody internally = consume_changes (ConsumeMarshalled) ->
//                       record retained (EncodeIfDirty + RecordRetained) -> recycle
//                       (Recycle). Device-backed ([both apps, not CI]).
class VAML_PRESENTATION_EXPORT VamlRenderer
{
public:
    // The CPU/merge host (headless, no Vulkan). Owns the S211 postbox + the visualOf
    // seam + the tree reference + the arrange/invalidate producer path. The app
    // attaches a tree (set_root) + a frame size (set_frame_size) and calls
    // invalidate() to arrange + encode + publish.
    [[nodiscard]] static std::unique_ptr<VamlRenderer> create();

    // The LIVE vertical-slice owner: builds + owns the SceneRenderer (over
    // opts.sceneSpec) + the Presenter (over opts.presenter), wiring the DrawBody
    // internally = ConsumeMarshalled -> EncodeIfDirty->RecordRetained -> Recycle.
    // Returns nullptr (no throw) if either owner cannot be built. The borrowed
    // loader/device/surface/renderPass + the sceneSpec handles must outlive.
    [[nodiscard]] static std::unique_ptr<VamlRenderer>
    create_device(const VamlRendererDeviceOptions& opts);

    ~VamlRenderer();
    VamlRenderer(VamlRenderer&&) noexcept;
    VamlRenderer& operator=(VamlRenderer&&) noexcept;
    VamlRenderer(const VamlRenderer&)            = delete;
    VamlRenderer& operator=(const VamlRenderer&) = delete;

    // ----------------------- UI-thread surface ----------------------------
    // Attaches the core visual tree root (BORROWED — the app owns it; must outlive
    // this host while in use). The root must be (or contain) a UIElement (real
    // panels). The host ARRANGES this tree (Measure/Arrange write the S123/S124
    // layout rects), so the pointer is MUTABLE. Returns false if `root` is null.
    [[nodiscard]] bool set_root(vaml::UIElement* root) noexcept;
    // The arrange/frame size (logical px) — the final_rect slot for the root's
    // Arrange. The app sets it to the window's backing size.
    void set_frame_size(float width, float height) noexcept;
    // S224 — the RENDERED-VIEW PAN & ZOOM surface: sets the viewer-level VIEW
    // TRANSFORM (the S208 Matrix2D the S224 viewer authors — translation ∘ scale).
    // It is composed into the S213 dpi::RenderPlan mvp at the next record_draw via
    // `compose_view_into_render_plan`, so the WHOLE retained scene pans/zooms
    // together at the MVP level (no per-element injection, no model mutation —
    // redraw-only, the retained buffers re-record unchanged). `invalidate()` is NOT
    // required (the scene/producer tree is unchanged); the render side is marked
    // dirty so the NEXT present re-records with the new view. Identity (the
    // default) is the no-op. Pure decision surface; headless-observable via
    // `view_transform()`.
    void set_view_transform(const vaml::Matrix2D& view) noexcept;
    // Reads back the currently composed view transform (identity by default).
    [[nodiscard]] const vaml::Matrix2D& view_transform() const noexcept { return view_; }
    // Overrides the material authority seam (default_visual_of() by default). Real
    // control materials replace it in place at S400/S364 — never a core virtual.
    void set_visual_of(VisualOf visualOf);
    // The UI-thread producer step: Measure + Arrange the attached tree at the frame
    // size, encode it via visualOf into the S206 draw list, then mute+publish the
    // snapshot to the postbox (the S211 producer side, race-free by affinity). Marks
    // the host dirty (a re-encode is pending). Returns the published scene version
    // (0 when nothing was published — no root or no frame size). In the live
    // create_device path, publish goes to the SceneRenderer's postbox so the
    // DrawBody's consume_changes drains it; in the headless create() path it goes
    // to this host's own postbox (observable via postbox()).
    std::uint64_t invalidate();

    // ------------------------ render-loop surface -------------------------
    // The CPU/merge postbox (create() path). The S211 lock-free producer/consumer
    // transport; the headless test + the CPU-only consumer inspect + drain it.
    [[nodiscard]] vaml::vulkan::ui::MarshalledScene& postbox() noexcept { return postbox_; }
    [[nodiscard]] const vaml::vulkan::ui::MarshalledScene& postbox() const noexcept { return postbox_; }

    // The change-driven iteration: forwards to the owned Presenter (the live path).
    // Returns nullopt when nothing was drawn (idle/park/stop) — the S173 contract.
    [[nodiscard]] std::optional<std::uint32_t> tick(bool invalidated, bool tickDue);

    // A resize REQUEST forwarded to the owned Presenter (applied next tick, the
    // S167_1 two-generation transaction). No-op in the headless create() path.
    void request_resize(std::uint32_t width, std::uint32_t height);

    // The owned live resources (null in the headless create() path).
    [[nodiscard]] vaml::vulkan::ui::SceneRenderer* renderer() const noexcept { return sceneRenderer_.get(); }
    [[nodiscard]] Presenter* presenter() const noexcept { return presenter_.get(); }
    [[nodiscard]] vaml::vulkan::SwapchainState state() const noexcept;

    // State accessors (headless-observable).
    [[nodiscard]] bool has_root() const noexcept { return root_ != nullptr; }
    [[nodiscard]] float frame_width()  const noexcept { return frameW_; }
    [[nodiscard]] float frame_height() const noexcept { return frameH_; }
    // S213 — the DERIVED framebuffer scale (physical/logical px) of the LAST
    // record_draw: the S213 DPI/Framebuffer-scale seam. 1 logical px rendered at
    // exactly `last_framebuffer_scale()` physical px (high-DPI crisp). Since the
    // scale is DERIVED (never set independently — see dpi.h), this accessor is the
    // OBSERVABILITY + the S213_1 `dpiScale` input: the DPI-aware icon best-fit
    // (S213_1) multiplies a logical icon size by this to get the physical N it
    // best-fits against. 1.0 before the first record (no frame/framebuffer yet).
    [[nodiscard]] float last_framebuffer_scale() const noexcept { return lastFramebufferScale_; }
    // True after an invalidate() that published (a re-encode is pending until the
    // render side consumes + re-encodes).
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

private:
    VamlRenderer();

    // The internally-wired S173 DrawBody context method (live path). Owns the
    // command-buffer record: reset/begin the presenter's `cmd`, begin the render
    // pass against the acquired image's framebuffer, then run the fixed loop
    // steps — consume_changes (ConsumeMarshalled; re-encode ONLY on a newer
    // version) -> record retained (RecordRetained) -> recycle (Recycle) — and end
    // the pass. Returns true iff the body recorded + the presenter may submit.
    bool record_draw(vaml::presentation::Presenter& self,
                     VkCommandBuffer cmd, std::uint32_t imageIndex);

    // The CPU/merge state (always present; device-independent). Mutable because the
    // host Measure/Arranges the attached tree during invalidate().
    vaml::UIElement* root_ = nullptr;  // BORROWED; app-owned
    float frameW_ = 0.0f;
    float frameH_ = 0.0f;
    VisualOf visualOf_;
    vaml::vulkan::ui::MarshalledScene postbox_;   // S211 (headless path)
    bool dirty_ = false;
    // S213 — the derived framebuffer scale of the last record_draw (1.0 before any).
    float lastFramebufferScale_ = 1.0f;
    // S224 — the rendered-view VIEW TRANSFORM (pan/zoom) composed into the S213
    // render plan mvp at record. Identity by default (no view transform).
    vaml::Matrix2D view_{};

    // The live composition state (null in the headless create() path).
    std::unique_ptr<vaml::vulkan::ui::SceneRenderer> sceneRenderer_;
    std::unique_ptr<vaml::presentation::Presenter>  presenter_;
    VkRenderPass      renderPass_ = VK_NULL_HANDLE;
    float             clear_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};

} // namespace vaml::presentation
