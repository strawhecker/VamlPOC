// S595 (dep:S594) — `Window.AllowsTransparency` BECOMES LIVE CONSUMED SURFACE +
// VIEWER-GROWTH (headless Rail 1).
//
// Compiles the REAL viewer seam (vaml_viewer/viewer_transparency.h, ADR-021) into
// the presentation test target and proves:
//   1. LOAD: the SplashWindow-shaped `sample.transparent.vaml` loads; the Window
//      root's `AllowsTransparency` resolves true (the S595 LIVE-consumed surface).
//   2. RENDER (S592 clear): the loaded transparent window -> clear alpha 0 (the
//      see-through path) via `viewer_transparency_clear` -> `clear::decide_clear`.
//   3. OS-HINT (S594): the loaded transparency -> featureOn -> `window_transparency_hints`
//      returns the per-pixel DWM hints; an opaque window stays opaque.
//   4. ELEMENT ALPHA (S593): a half-alpha panel under the transparent window keeps
//      partial alpha through the single-fold + straight-alpha path.

#include "viewer_transparency.h"   // the S595 seam (vaml_viewer, ADR-021)

#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice / default_visual_of
#include <vaml/vulkan/PlatformWindow.h>     // window_transparency_hints (S594)
#include <vaml/vulkan/ui/UiScene.h>         // DrawItem, DrawItemKind
#include <vaml/core/Element.h>              // Window, StackPanel, Label
#include <vaml/core/Color.h>                // parse_color

#include <GLFW/glfw3.h>   // GLFW_TRANSPARENT_FRAMEBUFFER / GLFW_ALPHA_BITS (macro pinning)

#include "doctest/doctest.h"

#include <memory>
#include <vector>

using namespace vaml::viewer;
using vaml::presentation::clear::ClearValue;

namespace {

// The sample text (the SplashWindow-shaped transparent doc). Mirrors the real
// `sample.transparent.vaml` file, authored as a Window with AllowsTransparency.
const char* kTransparentSample = R"(
<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" AllowsTransparency="True">
  <StackPanel Background="Transparent"/>
</Window>
)";

} // namespace

TEST_CASE("S595 plan loads the transparent sample + resolves AllowsTransparency=true (LIVE surface)")
{
    ViewerTransparencyPlanResult plan = run_viewer_transparency_plan(kTransparentSample);
    REQUIRE(plan.ok);
    REQUIRE(plan.root != nullptr);
    // The root is a real Window (the app-shell root) with AllowsTransparency resolved
    // true — the named, consumed surface (no longer dead).
    CHECK(dynamic_cast<vaml::Window*>(plan.root.get()) != nullptr);
    CHECK(plan.allowsTransparency == true);
    // The reader is retained (named()/props stay armed).
    CHECK(plan.reader != nullptr);
}

TEST_CASE("S595 decide_clear: a transparent window clears to alpha 0 (see-through)")
{
    // The loaded transparent window's clear decision (the S592 clear of the S595
    // transparent sample): allowsTransparency -> clear alpha 0 (compositor shows
    // through), regardless of the authored background.
    const ClearValue c = viewer_transparency_clear(/*allows=*/true, std::optional<vaml::Color>{});
    CHECK(c.a == doctest::Approx(0.0f));

    // Even with an authored (semi) Background, a transparent window clears alpha 0
    // (WPF forces Colors.Transparent for per-pixel mode).
    const ClearValue semi = viewer_transparency_clear(true, vaml::parse_color("#80FF0000"));
    CHECK(semi.a == doctest::Approx(0.0f));
}

TEST_CASE("S595 window_transparency_hints(featureOn) resolves from the consumed transparency")
{
    // The S594 OS hint is driven by the S595 LIVE surface: an AllowsTransparency
    // window -> featureOn -> the per-pixel DWM hints (pinned to the GLFW macros).
    const bool featureOn = viewer_transparency_os_flag(/*allows=*/true);
    CHECK(featureOn == true);
    const vaml::vulkan::WindowTransparencyHints h = vaml::vulkan::window_transparency_hints(featureOn);
    CHECK(h.count == 2);
    CHECK(h.entries[0].hint == GLFW_TRANSPARENT_FRAMEBUFFER);
    CHECK(h.entries[0].value == GLFW_TRUE);
    CHECK(h.entries[1].hint == GLFW_ALPHA_BITS);
    CHECK(h.entries[1].value == 8);

    // An OPAQUE window (AllowsTransparency false) stays opaque (the NON-conditional rule).
    CHECK(viewer_transparency_os_flag(false) == false);
    CHECK(vaml::vulkan::window_transparency_hints(false).empty());
}

TEST_CASE("S595 half-alpha panel keeps partial alpha through the single-fold (S593)")
{
    // A real Window root (AllowsTransparency true) + a half-opacity StackPanel child:
    // encode_visual_slice emits the window + the panel; the panel's item.a is its
    // authored Background semi-alpha × Opacity (single fold, S593). The transparent
    // window composites over the see-through clear (alpha 0).
    vaml::Window w;
    (void)w.Set(vaml::PropertyId::of("AllowsTransparency"), vaml::Value::Bool(true));
    auto panel = std::make_unique<vaml::StackPanel>();
    vaml::StackPanel* panelRaw = panel.get();
    (void)panelRaw->Set(vaml::PropertyId::of("Background"),
                        vaml::Value::Color(vaml::parse_color("#80FF0000")));  // half-alpha red
    (void)panelRaw->set_opacity(0.5);
    w.set_content(std::move(panel));   // the Window's single-content slot (arranges the child to fill)
    w.Measure(vaml::Size{ 420.0f, 240.0f });
    w.Arrange(vaml::Rect{ 0.0f, 0.0f, 420.0f, 240.0f });

    std::vector<vaml::vulkan::ui::DrawItem> out;
    vaml::presentation::encode_visual_slice(w, vaml::presentation::default_visual_of(), out);
    // window + panel (panel is a child, pre-order z-order).
    REQUIRE(out.size() >= 2u);
    // The panel's item.a = bg.a (0x80/255) × Opacity 0.5 — the S593 single-fold.
    CHECK(out[1].a == doctest::Approx((0x80 / 255.0f) * 0.5f));

    // The transparent window's own clear (S592) is alpha 0 — see-through.
    const ClearValue c = viewer_transparency_clear(true, std::optional<vaml::Color>{});
    CHECK(c.a == doctest::Approx(0.0f));
}
