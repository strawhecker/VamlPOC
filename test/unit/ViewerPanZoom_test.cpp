// S224 (dep:S223) — the RENDERED-VIEW PAN & ZOOM at the LIBRARY surface
// (vaml-presentation-tests, headless Rail 1).
//
// Proves the S224 pan/zoom surfaces the VIEWER CHROME harnesses — the S208
// RenderTransform type composed into the S213 DPI/framebuffer-scale render plan —
// WITHOUT a live Vulkan runtime (the standing S160+ / S213 discipline):
//   (1) `compose_view_into_render_plan(view, plan)` — the PURE S224 seam that
//       folds a S208 Matrix2D view into a S213 dpi::RenderPlan mvp, leaving the
//       S213 uiScale (the derived DPI scale) UNTOUCHED (the crisp invariant +
//       the S208 clip-scissor alignment hold under any view).
//   (2) the identity view is a TRUE NO-OP (mvp unchanged) — the preserve-batching
//       default.
//   (3) the view mvp is actually multiplied in (a translated/scaled scene maps
//       differently through the composed mvp than through the plain one).
//   (4) `VamlRenderer::set_view_transform` + `view_transform()` — the host surface
//       the viewer's `ViewerApp::push_view_transform` calls; observable headlessly.
//     Composing the view exercises the S213 render-plan surface the way S224's
//     "viewer chrome harnesses library work, not editor chrome" contract demands.

#include <vaml/presentation/VamlRenderer.h>
#include <vaml/presentation/dpi.h>
#include <vaml/core/Transform.h>   // S208: vaml::Matrix2D

#include "doctest/doctest.h"

#include <cmath>
#include <cstdint>

using namespace vaml::presentation;

static bool mat4_approx(const vaml::vulkan::ui::Mat4& a,
                        const vaml::vulkan::ui::Mat4& b, float eps = 1e-4f)
{
    for (std::size_t i = 0; i < 16; ++i)
        if (std::fabs(a[i] - b[i]) > eps)
            return false;
    return true;
}

TEST_CASE("S224: compose_view_into_render_plan leaves the S213 uiScale (DPI) untouched")
{
    // The crisp invariant: the derived DPI scale is NEVER changed by the view —
    // the S208 clip-scissor alignment + the high-DPI crispness must hold under a view.
    dpi::RenderPlan plan = dpi::dpi_render_plan(1600, 1200, 800, 600);   // uiScale 2.0
    const float uiScaleBefore = plan.uiScale;
    CHECK(plan.uiScale == 2.0f);

    vaml::Matrix2D view = vaml::Matrix2D::translation(40.0f, -30.0f);
    compose_view_into_render_plan(view, plan);

    CHECK(plan.uiScale == uiScaleBefore);   // UNTOUCHED — the S213 crisp invariant holds
    CHECK(plan.uiScale == 2.0f);
}

TEST_CASE("S224: an identity view is a TRUE NO-OP (mvp unchanged, preserve-batching default)")
{
    // The default (no pan/zoom) must leave the plain render plan EXACTLY intact, so
    // the pre-S224 frame (and its batching) is bitwise-identical.
    dpi::RenderPlan a = dpi::dpi_render_plan(1600, 1200, 800, 600);
    dpi::RenderPlan b = dpi::dpi_render_plan(1600, 1200, 800, 600);
    compose_view_into_render_plan(vaml::Matrix2D::identity(), b);
    CHECK(mat4_approx(b.mvp, a.mvp));   // mvp unchanged
    CHECK(b.uiScale == a.uiScale);
}

TEST_CASE("S224: compose_view_into_render_plan actually multiplies the view into the mvp")
{
    dpi::RenderPlan a = dpi::dpi_render_plan(1600, 1200, 800, 600);
    dpi::RenderPlan b = dpi::dpi_render_plan(1600, 1200, 800, 600);

    // A translation view: the composed mvp must DIFFER from the plain one (the
    // whole scene pans). A non-identity view transforms the projection.
    vaml::Matrix2D view = vaml::Matrix2D::translation(50.0f, -25.0f);
    compose_view_into_render_plan(view, b);

    // The translation lives in the mvp's 4th column (column-major: w... z ... xy),
    // i.e. the x/y translate is baked into the projection -> b.mvp != a.mvp.
    bool differs = false;
    for (std::size_t i = 0; i < 16; ++i)
        if (std::fabs(a.mvp[i] - b.mvp[i]) > 1e-4f)
            differs = true;
    CHECK(differs);
}

TEST_CASE("S224: a scaled view scales the composed projection (zoom enters the mvp)")
{
    dpi::RenderPlan base = dpi::dpi_render_plan(1600, 1200, 800, 600);
    dpi::RenderPlan zoomed = dpi::dpi_render_plan(1600, 1200, 800, 600);
    compose_view_into_render_plan(vaml::Matrix2D::scaling(2.0f), zoomed);
    CHECK(zoomed.uiScale == base.uiScale);   // DPI scale still untouched
    // The zoomed mvp differs from the base (higher scale = more of the scene visible).
    bool differs = false;
    for (std::size_t i = 0; i < 16; ++i)
        if (std::fabs(base.mvp[i] - zoomed.mvp[i]) > 1e-4f)
            differs = true;
    CHECK(differs);
}

TEST_CASE("S224: VamlRenderer::set_view_transform + view_transform() is headless-observable")
{
    // The host surface the viewer's ViewerApp::push_view_transform calls and the
    // test reads — observable WITHOUT a Vulkan runtime (the S212 create() path).
    auto host = VamlRenderer::create();
    REQUIRE(host != nullptr);

    // Default: identity (no view).
    CHECK(vaml::is_transform_identity(host->view_transform()));

    vaml::Matrix2D view = vaml::Matrix2D::translation(12.0f, -7.0f);
    host->set_view_transform(view);
    CHECK(host->view_transform() == view);
    CHECK_FALSE(vaml::is_transform_identity(host->view_transform()));
}
