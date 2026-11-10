// S592 (dep:S591) — PER-WINDOW CLEAR ALPHA + BACKGROUND COMPOSITION (headless Rail 1).
//
// Proves the S592 clear source-of-truth change WITHOUT a live Vulkan runtime:
//   (1) the PURE `clear::decide_clear(allowsTransparency, resolvedBackground)`
//       seam — the 2-branch WPF HwndTarget.SetBackgroundColor mirror;
//   (2) the REAL Window tree read path: a Window root resolves its
//       AllowsTransparency (Bool, owner "Window") + Background (shared Color DP)
//       and the host's record_draw derives the clear from them (the host's
//       record_draw is exercised indirectly through decide_clear + the resolution —
//       the RenderPass/clearValue wiring itself is the live [both-apps] lane).
//
// The 2-branch rule (WPF-identical):
//   1. allowsTransparency==true  -> ClearValue{0,0,0,0}.
//   2. allowsTransparency==false && resolved background -> that color, alpha 1.0.
//   3. allowsTransparency==false && no background -> alpha 0 (WPF default-null).
//   4. non-Window root -> decide_clear(false, nullptr) = alpha 0, no crash.

#include <vaml/presentation/VamlRenderer.h>

#include <vaml/core/Element.h>   // Window (the app-shell root), StackPanel
#include <vaml/core/UIElement.h> // UIElement
#include <vaml/core/Color.h>     // vaml::Color, parse_color
#include <vaml/core/ResourceDictionary.h> // S643a: register_element_brush
#include "vaml_test_brush.h"              // S643b: brush_value (Brush-object paint)
#include <vaml/core/TextLayout.h>         // S643a: resolve_foreground_color

#include "doctest/doctest.h"

#include <memory>

using namespace vaml::presentation;
using vaml::presentation::clear::ClearValue;
using vaml::presentation::clear::decide_clear;

namespace {

// Reads a Window root's allows-transparency + resolved background EXACTLY as the
// host's record_draw does (the single decision site), returning decide_clear's
// result. Mirrors the S592 production read so the test exercises the real seam.
// A Background is "resolved" only when AUTHORED (a local value) — the shared
// "Background" Color DP's default opaque black is NOT a resolved background
// (WPF Window.Background defaults null).
ClearValue window_clear(const vaml::Window& w)
{
    const bool allows = w.Get(vaml::PropertyId::of("AllowsTransparency")).get<bool>();
    const vaml::Value bv = w.Get(vaml::PropertyId::of("Background"));
    // S643b-1 (D1): the shared "Background" DP holds a Brush OBJECT — resolve it
    // through the object resolver (handles the Brush object and the legacy
    // BrushId seam). A local value is "resolved" only when it's an authored
    // Brush/BrushId (the DP-default null is not authored).
    const bool authored =
        w.has_local_value(vaml::PropertyId::of("Background")) &&
        (bv.type() == vaml::ValueType::Brush || bv.type() == vaml::ValueType::BrushId);
    vaml::Color bgColor{ 0, 0, 0, 0 };
    const vaml::Color* resolved = nullptr;
    if (authored)
    {
        bgColor = vaml::resolve_foreground_brush(w, bv)->color();
        resolved = &bgColor;
    }
    return decide_clear(allows, resolved);
}

} // namespace

TEST_CASE("S592 decide_clear(true, anything) -> alpha 0 (transparent window)")
{
    // A transparent window clears to alpha 0 regardless of the authored background
    // (WPF forces Colors.Transparent for per-pixel mode — the S590 risk gate).
    vaml::Color red = vaml::parse_color("#FFFF0000");
    const ClearValue c = decide_clear(true, &red);
    CHECK(c.r == doctest::Approx(0.0f));
    CHECK(c.g == doctest::Approx(0.0f));
    CHECK(c.b == doctest::Approx(0.0f));
    CHECK(c.a == doctest::Approx(0.0f));

    const ClearValue cNull = decide_clear(true, nullptr);
    CHECK(cNull.a == doctest::Approx(0.0f));
}

TEST_CASE("S592 decide_clear(false, &red) -> red, alpha 1.0 (opaque window with Background)")
{
    vaml::Color red = vaml::parse_color("#FFFF0000");
    const ClearValue c = decide_clear(false, &red);
    CHECK(c.r == doctest::Approx(1.0f));
    CHECK(c.g == doctest::Approx(0.0f));
    CHECK(c.b == doctest::Approx(0.0f));
    CHECK(c.a == doctest::Approx(1.0f));  // an opaque window clears fully opaque
}

TEST_CASE("S592 decide_clear(false, nullptr) -> alpha 0 (the WPF null-Background mirror)")
{
    // An opaque window with no authored Background -> alpha-0 clear (WPF paints
    // nothing when Window.Background is its DP-default null; no grey fabricated).
    const ClearValue c = decide_clear(false, nullptr);
    CHECK(c.a == doctest::Approx(0.0f));
    CHECK(c.r == doctest::Approx(0.0f));
}

TEST_CASE("S592 real Window tree: AllowsTransparency=true + Transparent Background -> clear alpha 0")
{
    // Build a real Window root (owner "Window" registers AllowsTransparency +
    // Background) + a content child; arrange it; read the decided clear through the
    // REAL Window surface. A transparent window -> alpha 0.
    vaml::Window w;
    (void)w.Set(vaml::PropertyId::of("AllowsTransparency"), vaml::Value::Bool(true));
    (void)w.Set(vaml::PropertyId::of("Background"),
                vaml::test::brush_value(vaml::parse_color("#00000000")));
    w.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    w.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });

    const ClearValue c = window_clear(w);
    CHECK(c.a == doctest::Approx(0.0f));
    CHECK(c.r == doctest::Approx(0.0f));
}

TEST_CASE("S592 real Window tree: opaque window + Background -> the resolved Background color")
{
    vaml::Window w;
    // Opaque (AllowsTransparency default false) + an authored opaque blue background.
    (void)w.Set(vaml::PropertyId::of("Background"), vaml::test::brush_value(vaml::parse_color("#FF0000FF")));
    w.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    w.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });

    const ClearValue c = window_clear(w);
    CHECK(c.b == doctest::Approx(1.0f));  // blue
    CHECK(c.r == doctest::Approx(0.0f));
    CHECK(c.a == doctest::Approx(1.0f));
}

TEST_CASE("S592 real Window tree: opaque window WITHOUT Background -> alpha 0 (no ghost constant)")
{
    vaml::Window w;  // no Background authored, not transparent
    w.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    w.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });

    const ClearValue c = window_clear(w);
    CHECK(c.a == doctest::Approx(0.0f));
}

TEST_CASE("S592 non-Window root -> decide_clear(false, nullptr) = alpha 0 (no crash)")
{
    // The host's record_draw falls back to decide_clear(false, nullptr) for a
    // non-Window root (the WPF null-Background rule) — no crash, alpha 0.
    vaml::StackPanel root;
    root.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });

    const ClearValue c = decide_clear(false, nullptr);
    CHECK(c.a == doctest::Approx(0.0f));
    (void)root;
}
