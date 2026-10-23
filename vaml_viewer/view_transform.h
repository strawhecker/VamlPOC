// vaml_viewer — VIEW TRANSFORM seams (S224, dep:S223).
//
// S224's contract: the RENDERED VIEW supports PAN (drag) + ZOOM (wheel) on the
// S208 RenderTransform / S213 DPI-framebuffer-scale surface. This header is the
// PURE VIEW-STATE decision layer (the standing S222 SPLIT precedent — pure
// inline seams proven headless in ctest, no Vulkan runtime, shared verbatim by
// BOTH apps + the test target = the ADR-021 consumer path).
//
// The mechanism (accepted at plan review): a VIEWER-LEVEL view transform
// composed into the S213 `dpi::RenderPlan` mvp via a small, real, permanent
// library addition (`VamlRenderer::set_view_transform` + the pure
// `compose_view_into_render_plan` seam). The VIEW MODEL here is the S208
// `vaml::Matrix2D` type itself (reused, NOT duplicated):
//     pan  = Matrix2D::translation(panX, panY)
//     zoom = Matrix2D::scaling(zoom)
//     view = translation ∘ scaling  (to_matrix = translate then scale about
//            the origin; zoom-about is folded by adjusting pan)
//
// This header owns ONLY the pure ViewState bookkeeping + the S208 fold:
//   * `ViewState { panX, panY, zoom }` — the live pan/zoom parameters.
//   * `apply_pan(vs, dx, dy)` — drag by cursor delta (logical px).
//   * `zoom_about(vs, anchorX, anchorY, factor)` — zoom preserving the point
//     under the cursor; clamps zoom to [kMinZoom, kMaxZoom].
//   * `to_matrix(vs)` — the S208 Matrix2D the host composes into the plan.
//   * `viewport_to_view(vs, vx, vy)` — maps a logical viewport offset back into
//     the VIEW's origin frame (used to anchor zoom about the cursor).
//
// All math is pure (no Vulkan, no window), on the S123/S124 logical-px frame, so
// it is headless-testable. The WIRING (drag/scroll dispatch into these seams +
// host->set_view_transform) lives in the app (app.{h,cpp}).
//
// NO MINIMAL REAL introduced — the view-transform seam is REAL permanent code.

#pragma once

#include <vaml/core/Transform.h>   // S208: vaml::Matrix2D (reused, not duplicated)

#include <cmath>

namespace vaml::viewer {

// The live pan/zoom parameter set (the S224 "rendered-view pan & zoom" state).
struct ViewState
{
    double panX = 0.0;   // logical px, +X right
    double panY = 0.0;   // logical px, +Y DOWN (screen space — Y grows down)
    double zoom = 1.0;   // uniform scale factor, clamped [kMinZoom, kMaxZoom]
};

// The zoom clamping bounds (S224 — a sane bounded zoom so the view never
// collapses to an unusable scale nor blows out of numerical range).
inline constexpr double kMinZoom = 0.1;
inline constexpr double kMaxZoom = 10.0;

// Clamps a proposed zoom into [kMinZoom, kMaxZoom]. Pure.
[[nodiscard]] inline double clamp_zoom(double z) noexcept
{
    if (z < kMinZoom) return kMinZoom;
    if (z > kMaxZoom) return kMaxZoom;
    return z;
}

// The pure make_view helper: a bounded clamp + the S208 composition in one
// step (kept as a seam so the clamp policy + the fold order are asserted once).
[[nodiscard]] inline vaml::Matrix2D make_view_matrix(double panX, double panY, double zoom) noexcept
{
    const double z = clamp_zoom(zoom);
    // view = translation ∘ scaling(zoom): a point is first scaled about the
    // origin, THEN translated. Column-vector order: multiply(translation, scaling).
    return vaml::multiply(vaml::Matrix2D::translation(static_cast<float>(panX),
                                                      static_cast<float>(panY)),
                          vaml::Matrix2D::scaling(static_cast<float>(z)));
}

// The S208 fold of the current ViewState into its Matrix2D. Pure + headless.
[[nodiscard]] inline vaml::Matrix2D to_matrix(const ViewState& vs) noexcept
{
    return make_view_matrix(vs.panX, vs.panY, vs.zoom);
}

// PAN (drag): the view translate by (dx, dy) in LOGICAL px (screen-space: +Y
// down, matching the S123/S124 top-left-origin layout). Does NOT touch zoom.
// Pure + headless.
inline void apply_pan(ViewState& vs, double dx, double dy) noexcept
{
    vs.panX += dx;
    vs.panY += dy;
}

// ZOOM-ABOUT (wheel): scales the view by `factor` aiming to KEEP the view-space
// point `(anchorX, anchorY)` (a LOGICAL VIEWPORT coordinate, e.g. the cursor
// position) FIXED under the cursor — the classic anchor-preserving zoom. The
// zoom is clamped [kMinZoom, kMaxZoom]. Pure + headless.
//
// Math (screen space, +Y down): the S208 fold maps content -> viewport as
//   v = zoom*content + pan.  We want a new zoom z' and a compensated pan' such
// that the content point currently at the viewport anchor `a` STAYS at `a`:
//   z'*content + pan' == a,  content = (a - pan)/z.
// Solving for pan':
//   pan' = a - z'*(a - pan)/z  =  pan*(z'/z) + a*(1 - z'/z).
// Using ratio = z'/z (with z' clamped), each pan axis becomes
//   pan = pan*ratio + a*(1 - ratio),
// which keeps the anchored content fixed even when the zoom is clamped (a wheel
// at the edge does not drift the content under the cursor).
inline void zoom_about(ViewState& vs, double anchorX, double anchorY, double factor) noexcept
{
    if (!(factor > 0.0))  // NaN/inf/<=0 guard (a NaN factor fails this)
        return;
    const double oldZoom = vs.zoom;
    const double newZoom = clamp_zoom(vs.zoom * factor);
    if (newZoom == oldZoom)
        return;
    const double ratio = newZoom / oldZoom;
    vs.panX = vs.panX * ratio + anchorX * (1.0 - ratio);
    vs.panY = vs.panY * ratio + anchorY * (1.0 - ratio);
    vs.zoom = newZoom;
}

// Maps a VIEWPORT/logical coordinate (e.g. the cursor position in the window's
// logical-px frame, before any view transform) into the VIEW's own origin frame
// — i.e. the logical content coordinate at that cursor. Used so the anchor the
// zoom preserves is expressed in content space. With the S208 fold
// v = zoom*content + pan, the inverse is content = (v - pan)/zoom. Pure.
inline void viewport_to_content(const ViewState& vs,
                                double vx, double vy,
                                double& outX, double& outY) noexcept
{
    const double z = vs.zoom > 0.0 ? vs.zoom : 1.0;
    outX = (vx - vs.panX) / z;
    outY = (vy - vs.panY) / z;
}

} // namespace vaml::viewer
