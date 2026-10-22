#pragma once
// S213 (dep:S212_1) — DPI + framebuffer scale: LOGICAL-PX layout, render scales by
// framebufferScale; high-DPI crisp (vaml::presentation::dpi).
//
// The vertical-slice layout (Measure/Arrange + `final_rect()` + `set_frame_size`)
// is ALREADY authored in LOGICAL px — "logical px layout" is inherently satisfied by
// the S123/S124 core layout. DPI is therefore a PRESENTATION/RENDER concern ONLY:
// the rendered scene maps the LOGICAL canvas onto the PHYSICAL framebuffer by a
// DERIVED `framebufferScale = physical / logical`, giving crisp high-DPI output.
//
// WHY THE SCALE IS DERIVED, NEVER SET (the S213 crisp invariant, verified from
// source): `UiScene::record_draw` sets the viewport from the PHYSICAL swapchain
// extent (`viewport_scissor(extent_)`), and the S208 clip scissor maps logical→frame
// px via the `uiScale` passed to `record`. `quad::ui_projection(Lw, Lh, 1.0)` maps
// the LOGICAL canvas to NDC; with a physical viewport, `1 logical px → extent.width/Lw
// physical px = framebufferScale`. For the S208 clip to ALIGN with the scaled geometry,
// `uiScale` MUST equal `framebufferScale = physical/logical` — an independent settable
// DPI value would BREAK that alignment. So the scale is a pure function of the logical
// frame size + the physical framebuffer size; the consumer of this module supplies both
// and receives the one self-consistent render plan (mvp + uiScale).
//
// Split (the standing S162/S200/S206/S212 precedent):
//   * PURE DECISION SEAMS (headless-testable, NO Vulkan runtime) — everything in this
//     header is pure + linkable, proven in ctest:
//       framebuffer_scale_for(physicalW, physicalH, logicalW, logicalH)
//       canvas_projection(logicalW, logicalH)
//       RenderPlan { mvp, uiScale } + dpi_render_plan(...)
//       logical_to_physical(x, y, w, h, scale)
//     These PROVE the DPI/framebuffer-scale math headlessly (the S213 proof surface).
//   * The host (`VamlRenderer::record_draw`) CONSUMES dpi_render_plan to call the
//     S206 SceneRenderer's 3-arg `record(cmd, mvp, uiScale)` with the derived plan
//     (the uiScale consumer set_frame_size already carried — now made REAL).
//
// NO-GAP recorded (not silent): S213_1 (DPI-aware icon best-fit) consumes
// `VamlRenderer::last_framebuffer_scale()` as its `dpiScale`. The on-screen high-DPI
// proof belongs to S222 (viewer) / S224 (pan/zoom); S213 delivers the library-surface
// DPI + the headless proof. ADR-011 maintained (vaml-presentation; no core virtual).

#include <vaml/vulkan/ui/UiPipeline.h>  // ui::Mat4 (the S205 shared MVP alias)

#include <cstdint>

namespace vaml::presentation::dpi {

// The framebuffer scale (physical px ÷ logical px) for the X axis — the crisp
// high-DPI mapping: 1 logical px renders at exactly `framebufferScale` physical px.
// Guarded: a zero/negative logical extent cannot divide -> returns 1.0 (identity, the
// no-DPI default); a zero/negative physical extent returns 1.0 too. Pure + headless.
[[nodiscard]] float
framebuffer_scale_for(std::uint32_t physicalW, std::uint32_t physicalH,
                      float logicalW, float logicalH) noexcept;

// The canvas projection: the S200 orthographic MVP mapping the LOGICAL canvas
// (logicalW × logicalH) to NDC with uiScale 1.0 — `quad::ui_projection(Lw, Lh, 1.0)`.
// With the PHYSICAL viewport UiScene sets, this + the framebuffer scale ARE the crisp
// high-DPI pair (see the header comment). Pure + headless.
[[nodiscard]] vaml::vulkan::ui::Mat4
canvas_projection(float logicalW, float logicalH) noexcept;

// The single self-consistent render plan the S206/210 `record(cmd, mvp, uiScale)`
// consumer needs: `mvp` = canvas_projection(logicalW, logicalH) (the logical canvas →
// NDC) and `uiScale` = framebuffer_scale_for(...) (so the S208 clip scissor ALIGNS
// with the geometry at the physical viewport). Pure + headless.
struct RenderPlan
{
    vaml::vulkan::ui::Mat4 mvp{};  // the logical canvas -> NDC projection
    float uiScale = 1.0f;          // the DERIVED framebuffer scale (physical/logical)
};

// Computes RenderPlan for a LOGICAL frame size (logicalW × logicalH) drawn into a
// PHYSICAL framebuffer (physicalW × physicalH). This is the seam `VamlRenderer`'
// record_draw consumes: logical geometry + the physical framebuffer extent in, the one
// derived plan out. Pure + headless. See framebuffer_scale_for for the zero guards.
[[nodiscard]] RenderPlan
dpi_render_plan(std::uint32_t physicalW, std::uint32_t physicalH,
                float logicalW, float logicalH) noexcept;

// A logical-px rectangle scaled to physical framebuffer px by `scale`
// (logical_to_physical = logical × scale — the S213 crisp invariant: a 1-logical-px
// rect at scale s is EXACTLY s physical px, so geometry + clip align). The result is
// a value struct; callers may round to pixels at record. Pure + headless.
struct PhysicalRect
{
    float x = 0.0f, y = 0.0f;   // top-left in physical px
    float w = 0.0f, h = 0.0f;   // extent in physical px
};

[[nodiscard]] PhysicalRect
logical_to_physical(float x, float y, float w, float h, float scale) noexcept;

} // namespace vaml::presentation::dpi
