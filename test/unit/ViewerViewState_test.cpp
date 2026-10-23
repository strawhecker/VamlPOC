// S224 (dep:S223) — the viewer RENDERED-VIEW PAN & ZOOM model (headless Rail 1).
//
// Proves the S224 pan/zoom VIEW-STATE math (the rendered view, exercising the S208
// RenderTransform type + the S213 DPI/framebuffer-scale surface) WITHOUT a live
// window/Vulkan runtime, through the PURE view-transform seams
// (vaml_viewer/view_transform.h, compiled into the test target = the ADR-021
// consumer path, single source of truth across both apps + the test):
//   (1) `to_matrix(vs)` / `make_view_matrix` — the S208 Matrix2D fold (translate∘scale).
//   (2) `apply_pan(vs, dx, dy)` — drag accumulation.
//   (3) `zoom_about(vs, ax, ay, factor)` — anchor-preserving zoom, clamped [kMin,kMax].
//   (4) `viewport_to_content` — the cursor -> content mapping the zoom anchor uses.
// The viewer chrome harnesses the S208 Matrix2D type (reused, not duplicated) for
// the VIEW model — the S224 "viewer chrome harnesses library work" contract.

#include "view_transform.h"   // the S224 pure view-transform seams

#include "doctest/doctest.h"

#include <cmath>

using namespace vaml::viewer;

// Approx equality for the float/double view math (tolerance-based).
static bool approx(double a, double b, double eps = 1e-5)
{
    return std::fabs(a - b) <= eps;
}

TEST_CASE("S224: the default ViewState is identity (no pan/zoom)")
{
    ViewState vs;
    CHECK(vs.panX == 0.0);
    CHECK(vs.panY == 0.0);
    CHECK(vs.zoom == 1.0);
    const vaml::Matrix2D m = to_matrix(vs);
    CHECK(vaml::is_transform_identity(m));   // S208 identity fast-path
}

TEST_CASE("S224: make_view_matrix folds translation∘scaling via the S208 affine")
{
    const vaml::Matrix2D m = make_view_matrix(10.0, -5.0, 2.0);
    CHECK(approx(m.dx, 10.0f));
    CHECK(approx(m.dy, -5.0f));
    CHECK(approx(m.m11, 2.0f));   // scaling(zoom) linear part (then translated)
    CHECK(approx(m.m22, 2.0f));
    // A content point (3, 4) maps to  scale*pt + pan  under the S208 fold:
    //   v = zoom*pt + pan  ->  (2*3 + 10, 2*4 - 5) = (16, 3).
    float ox = 0.0f, oy = 0.0f;
    vaml::transform_point(m, 3.0f, 4.0f, ox, oy);
    CHECK(approx(ox, 16.0f));
    CHECK(approx(oy, 3.0f));
}

TEST_CASE("S224: apply_pan accumulates the drag delta (screen-space +Y down)")
{
    ViewState vs;
    apply_pan(vs, 30.0, -12.0);
    apply_pan(vs, -5.0, 4.0);
    CHECK(approx(vs.panX, 25.0));
    CHECK(approx(vs.panY, -8.0));
    CHECK(vs.zoom == 1.0);   // pan never touches zoom
}

TEST_CASE("S224: zoom_about keeps the anchored content fixed (anchor-preserving)")
{
    ViewState vs;
    vs.zoom = 1.0;
    // Anchor at (100, 50). Before zoom, the content point at the anchor is
    //   content = (anchor - pan)/zoom = (100, 50).
    const double ax = 100.0, ay = 50.0;
    double c0x = 0.0, c0y = 0.0;
    viewport_to_content(vs, ax, ay, c0x, c0y);
    CHECK(approx(c0x, 100.0));
    CHECK(approx(c0y, 50.0));

    // Zoom in by 2x about that anchor.
    zoom_about(vs, ax, ay, 2.0);
    CHECK(approx(vs.zoom, 2.0));

    // After zoom, the SAME content point must still map to the SAME screen spot:
    //   screen = zoom'*content + pan'  ==  (100, 50).
    double cx = 0.0, cy = 0.0;
    viewport_to_content(vs, ax, ay, cx, cy);
    CHECK(approx(cx, c0x));   // the content at the anchor is unchanged
    CHECK(approx(cy, c0y));
    // AND the mapped screen spot of that content == the original anchor.
    float sx = 0.0f, sy = 0.0f;
    const vaml::Matrix2D m = to_matrix(vs);
    vaml::transform_point(m, static_cast<float>(c0x), static_cast<float>(c0y), sx, sy);
    CHECK(approx(sx, ax));
    CHECK(approx(sy, ay));
}

TEST_CASE("S224: zoom_about clamps to [kMinZoom, kMaxZoom]")
{
    ViewState vs;
    zoom_about(vs, 0.0, 0.0, 1e9);
    CHECK(approx(vs.zoom, kMaxZoom));
    zoom_about(vs, 0.0, 0.0, 1e-9);
    CHECK(approx(vs.zoom, kMinZoom));
}

TEST_CASE("S224: zoom_about guards a non-finite/non-positive factor (no-op)")
{
    ViewState vs;
    vs.zoom = 2.0;
    zoom_about(vs, 0.0, 0.0, std::nan(""));
    CHECK(vs.zoom == 2.0);
    zoom_about(vs, 0.0, 0.0, -1.0);
    CHECK(vs.zoom == 2.0);
    zoom_about(vs, 0.0, 0.0, 0.0);
    CHECK(vs.zoom == 2.0);
}

TEST_CASE("S224: viewport_to_content maps the cursor into content space")
{
    ViewState vs;
    vs.panX = 10.0; vs.panY = 20.0; vs.zoom = 2.0;
    double cx = 0.0, cy = 0.0;
    viewport_to_content(vs, 30.0, 40.0, cx, cy);
    CHECK(approx(cx, (30.0 - 10.0) / 2.0));   // (v - pan)/zoom
    CHECK(approx(cy, (40.0 - 20.0) / 2.0));
}

TEST_CASE("S224: a sequence of drag + zoom composes (drag-to-zoom-then-pan-roundtrip)")
{
    // A realistic viewer gesture: drag 40,-30, then zoom 1.5x about the cursor.
    ViewState vs;
    apply_pan(vs, 40.0, -30.0);
    zoom_about(vs, 50.0, 60.0, 1.5);
    CHECK(approx(vs.zoom, 1.5));

    // The anchor (50,60) is still at the same screen spot after the zoom: the
    // content mapped to (50,60) before equals the content at (50,60) now.
    double beforeX = 0.0, beforeY = 0.0;
    ViewState preZoom;
    apply_pan(preZoom, 40.0, -30.0);
    viewport_to_content(preZoom, 50.0, 60.0, beforeX, beforeY);
    double afterX = 0.0, afterY = 0.0;
    viewport_to_content(vs, 50.0, 60.0, afterX, afterY);
    CHECK(approx(afterX, beforeX));
    CHECK(approx(afterY, beforeY));
}
