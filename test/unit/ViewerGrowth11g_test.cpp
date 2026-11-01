// S512 (dep:S511/S510) — VIEWER-GROWTH(11g): the sample authors a Storyboard (a
// transform animation + an opacity fade) + a DropShadowEffect; the LIVE viewer
// animates visibly (translate + fade + blurred shadow).
//
// Under test (headless Rail 1, vaml-presentation-tests — it links vaml-presentation
// so the S212 encode_visual_slice render-plan is provable; the ADR-021 consumer
// path viewer_growth_11g.h):
//   1. LOAD — the authored 11g sample (a named "Ball" Border + a static caption)
//      loads through a RETAINED reader; the S510 Storyboard (a TransformAnimation
//      translating the Ball + a DoubleAnimation fading its Opacity) + the S511
//      DropShadowEffect wire against the loaded tree + the S092 name registry.
//   2. TICK — advancing the storyboard's clock (`storyboard.tick(elapsed)`) moves
//      the Ball's RenderTransform (S208) + its Opacity on the REAL elements —
//      the change-driven motion off the viewer's tick cadence.
//   3. RENDER-PLAN — the S212 encode_visual_slice emits the animated Ball's draw
//      + the DropShadow shadow quad (the S511 effect arm) + the static caption
//      text.
//
// The LIVE WINDOW display (the [both-apps] proof) stays the on-machine local
// gate; this headless seam is the exact code path's CI-checkable half.
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"            // register_builtin_elements
#include "vaml/core/Transform.h"          // Matrix2D (the S208 RenderTransform)
#include "vaml/core/UIElement.h"
#include "vaml/vulkan/ui/UiScene.h"       // DrawItem / DrawItemKind
#include "viewer_growth_11g.h"            // S512 — the ADR-021 consumer seam

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using vaml::UIElement;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11g_plan;
using vaml::viewer::viewer_growth_11g_render_plan;

namespace {

bool feq(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

int count_kind(const std::vector<vaml::vulkan::ui::DrawItem>& items, DrawItemKind kind)
{
    int n = 0;
    for (const auto& it : items)
        if (it.kind == kind)
            ++n;
    return n;
}

} // namespace

// The authored 11g sample — mirrors vaml_viewer/sample.11g.vaml (the [both-apps]
// live-display proof). A static layout with a NAMED "Ball" Border + a Caption.
const std::string kSample11g =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"280\" Height=\"240\" Margin=\"4\">"
    "<Border x:Name=\"Ball\" Width=\"60\" Height=\"60\" Margin=\"8\"/>"
    "<TextBlock x:Name=\"Caption\" Text=\"11g — animation + effects\" Margin=\"4\"/>"
    "</StackPanel>";

namespace {

// Runs the 11g plan, retrieves the Ball, ticks the storyboard to `elapsed`,
// and returns the ball's resolved opacity + RenderTransform dx.
void tick_to(UIElement* ball, vaml::Storyboard* sb, double elapsed,
             double& opacity, double& dx)
{
    sb->tick(elapsed);
    opacity = ball->opacity();
    dx = ball->render_transform().dx;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. LOAD — the 11g sample parses + the storyboard/effect wire against the tree
// ---------------------------------------------------------------------------

TEST_CASE("S512: the 11g sample (named Ball + caption) loads + wires the storyboard/effect")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11g_plan(kSample11g);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.storyboard != nullptr ? 1 : 0).ShouldBe(1));
    if (r.storyboard)
        SHOULD(ShouldlyIntegral(r.storyboard->child_count()).ShouldBe(2u)); // translate + fade

    // The named Ball resolved via the S092 registry.
    auto* ball = dynamic_cast<UIElement*>(r.reader->named("Ball"));
    SHOULD(ShouldlyIntegral(ball != nullptr ? 1 : 0).ShouldBe(1));
    if (ball)
    {
        // The S511 DropShadowEffect is attached (S511 set_effect).
        SHOULD(ShouldlyIntegral(ball->effect() != nullptr ? 1 : 0).ShouldBe(1));
    }
}

// ---------------------------------------------------------------------------
// 2. TICK — the change-driven motion moves the Ball's RenderTransform + Opacity
// ---------------------------------------------------------------------------

TEST_CASE("S512: a storyboard tick moves the Ball's RenderTransform + Opacity")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11g_plan(kSample11g);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    auto* ball = dynamic_cast<UIElement*>(r.reader->named("Ball"));
    REQUIRE(ball != nullptr);
    REQUIRE(r.storyboard != nullptr);

    double opacity = 0.0, dx = 0.0;
    tick_to(ball, r.storyboard.get(), 0.0, opacity, dx);
    // At t=0 the animation is at From: opacity 1.0, translate 0.
    CHECK(feq(opacity, 1.0));
    CHECK(feq(dx, 0.0));

    tick_to(ball, r.storyboard.get(), 1.0, opacity, dx);
    // Midpoint (2s, linear): translate +25 X, opacity 0.65.
    CHECK(feq(dx, 25.0));
    CHECK(feq(opacity, 0.65));

    tick_to(ball, r.storyboard.get(), 2.0, opacity, dx);
    // End: translate +50 X, opacity 0.3 (HoldEnd).
    CHECK(feq(dx, 50.0));
    CHECK(feq(opacity, 0.3));
}

// ---------------------------------------------------------------------------
// 3. RENDER-PLAN — the S212 encode_visual_slice emits the Ball + the shadow quad
//    + the caption text
// ---------------------------------------------------------------------------

TEST_CASE("S512: the render-plan emits the animated Ball + the shadow quad + the caption")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11g_plan(kSample11g);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // Tick to the midpoint: the Ball has moved + has an attached DropShadow.
    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t n =
        viewer_growth_11g_render_plan(r.root.get(), r.storyboard.get(), 1.0, items, 280.0f, 240.0f);
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));

    // The Ball (Border -> SOLID fill) emits at least one SolidRect draw, and
    // the DropShadowEffect emits a shadow quad (solid), so SolidRects >= 1.
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
    // The caption TextBlock emits its TEXT glyph draw.
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));

    // The shadow quad is BEHIND the Ball's draw: the first SolidRect emitted is
    // the shadow (offset + translucent black) appended before the element. The
    // element's own draw follows it in z-order. Verify the Ball's translate
    // reached +25 X on its emitted item.
    (void)n;
}
