// S213 (dep:S212_1) — DPI + framebuffer scale: logical px layout, render scales by
// framebufferScale; high-DPI crisp (headless Rail 1).
//
// Proves the S213 pure decision seams (vaml::presentation::dpi) WITHOUT a live Vulkan
// runtime (the standing S160+ precedent): the layout is ALREADY logical-px (S123/S124),
// so DPI is a PRESENTATION/RENDER concern delivered as the DERIVED framebuffer scale —
// the one value that keeps the S208 clip scissor aligned with the scaled geometry at the
// PHYSICAL viewport. The "crisp headline" is: a logical rect at framebuffer scale s
// renders at EXACTLY logical × s physical px (no DPI gap, no blur, no misaligned clip).
//
// The device-backed host integration (`VamlRenderer::record_draw` computing the plan +
// calling the SceneRenderer's 3-arg record) is [both apps, not CI] per precedent; the
// pure math the host applies is proven headlessly here (the S213 proof surface).

#include <vaml/presentation/dpi.h>
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/vulkan/quad/QuadPipeline.h>   // quad::ui_projection (the S200 MVP) for parity

#include "doctest/doctest.h"

#include <cstdint>
#include <type_traits>

using namespace vaml::presentation;
using vaml::presentation::dpi::PhysicalRect;
using vaml::presentation::dpi::RenderPlan;

TEST_CASE("S213: framebuffer_scale_for derives physical/logical on X (the crisp ratio)")
{
    // 1600 phys / 800 logical = 2.0 — the classic 2x high-DPI scale.
    CHECK(dpi::framebuffer_scale_for(1600u, 1200u, 800.0f, 600.0f) == doctest::Approx(2.0f));
    // 1:1 (no DPI) -> identity.
    CHECK(dpi::framebuffer_scale_for(800u, 600u, 800.0f, 600.0f) == doctest::Approx(1.0f));
    // A fractional hiDPI (150%): 1200/800 = 1.5.
    CHECK(dpi::framebuffer_scale_for(1200u, 900u, 800.0f, 600.0f) == doctest::Approx(1.5f));
}

TEST_CASE("S213: framebuffer_scale_for guards zero/negative logical + zero physical")
{
    // A not-yet-set frame (logical 0) cannot divide -> identity 1.0 (the no-DPI default).
    CHECK(dpi::framebuffer_scale_for(1600u, 1200u, 0.0f, 600.0f) == doctest::Approx(1.0f));
    CHECK(dpi::framebuffer_scale_for(1600u, 1200u, 800.0f, 0.0f) == doctest::Approx(1.0f));
    // A negative logical extent guards too.
    CHECK(dpi::framebuffer_scale_for(1600u, 1200u, -800.0f, 600.0f) == doctest::Approx(1.0f));
    // A zero physical framebuffer collapses to identity (nothing to scale to anyway).
    CHECK(dpi::framebuffer_scale_for(0u, 0u, 800.0f, 600.0f) == doctest::Approx(1.0f));
}

TEST_CASE("S213: canvas_projection maps the logical canvas via ui_projection(Lw,Lh,1)")
{
    // The canvas projection IS the S200 UI-scale MVP over the logical canvas @ 1.0.
    const auto expected = vaml::vulkan::quad::ui_projection(800u, 600u, 1.0f);
    const auto got      = dpi::canvas_projection(800.0f, 600.0f);
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(got[i] == doctest::Approx(expected[i]));
}

TEST_CASE("S213: dpi_render_plan is the single self-consistent plan (mvp + uiScale)")
{
    // A 800x600 logical canvas in a 1600x1200 physical framebuffer:
    // mvp = canvas_projection(800,600); uiScale = framebuffer_scale = 2.0.
    const RenderPlan plan = dpi::dpi_render_plan(1600u, 1200u, 800.0f, 600.0f);
    CHECK(plan.uiScale == doctest::Approx(2.0f));
    const auto canvas = dpi::canvas_projection(800.0f, 600.0f);
    for (std::size_t i = 0; i < canvas.size(); ++i)
        CHECK(plan.mvp[i] == doctest::Approx(canvas[i]));

    // At 1:1 the plan is the identity UI-scale plan (uiScale 1).
    const RenderPlan identity = dpi::dpi_render_plan(800u, 600u, 800.0f, 600.0f);
    CHECK(identity.uiScale == doctest::Approx(1.0f));
}

TEST_CASE("S213: logical_to_physical is the crisp invariant — logical × scale == physical")
{
    // THE S213 CRISP HEADLINE: a logical rect at framebuffer scale s maps EXACTLY to
    // logical × s physical px, so a 1-logical-px element at scale 2 is EXACTLY 2
    // physical px — geometry + S208 clip align at the physical viewport (no blur, no
    // misalignment). 800x600 logical -> 1600x1200 physical (scale 2).
    const float s = dpi::framebuffer_scale_for(1600u, 1200u, 800.0f, 600.0f);
    PhysicalRect r = dpi::logical_to_physical(20.0f, 30.0f, 100.0f, 50.0f, s);
    CHECK(r.x == doctest::Approx(40.0f));   // 20 * 2
    CHECK(r.y == doctest::Approx(60.0f));   // 30 * 2
    CHECK(r.w == doctest::Approx(200.0f));  // 100 * 2
    CHECK(r.h == doctest::Approx(100.0f));  // 50 * 2

    // A 1-logical-px rect at scale 2 is EXACTLY 2 physical px (the crisp guarantee).
    PhysicalRect one = dpi::logical_to_physical(0.0f, 0.0f, 1.0f, 1.0f, s);
    CHECK(one.w == doctest::Approx(2.0f));
    CHECK(one.h == doctest::Approx(2.0f));
}

TEST_CASE("S213: VamlRenderer exposes the S213_1 dpiScale input (last_framebuffer_scale)")
{
    // The headless host carries the DPI surface: the scale is 1.0 by default (no frame/
    // framebuffer yet) — the invariant the S213_1 icon best-fit consumes (= logical size
    // × dpiScale = physical N) and the S222/S224 on-screen proof projects from.
    auto host = VamlRenderer::create();
    REQUIRE(host != nullptr);
    CHECK(host->last_framebuffer_scale() == doctest::Approx(1.0f));

    // Move-only RAII discipline holds alongside the new seam (the S170 invariant).
    static_assert(std::is_move_constructible<VamlRenderer>::value,
                  "VamlRenderer must be movable");
    static_assert(!std::is_copy_constructible<VamlRenderer>::value,
                  "VamlRenderer must be non-copyable");
}
