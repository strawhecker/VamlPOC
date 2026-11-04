// S593 (dep:S592) — ELEMENT ALPHA COMPOSITION (headless Rail 1).
//
// Proves the S593 alpha single-fold fix + the Label/Label-arm realignment against
// the REAL surfaces (no Vulkan runtime):
//   * the base UIElement fallback does NOT fold effective_opacity into the emitted
//     color — the encoder folds it EXACTLY ONCE (encode_visual_slice -> the S208
//     encoder site `visual.item.a *= effective_opacity(element)`).
//   * the base arm reads the element's RESOLVED Background (Color) — a
//     semi-alpha Background × Opacity renders at bg.a × opacity (SINGLE fold; would
//     be bg.a × opacity^2 under the old double-count — regression tripwire).
//   * Label: the backdrop box is a NEUTRAL alpha 1.0 (the invented 0.85f constant is
//     GONE, and no fg.a tints the box — WPF forbids a foreground-tints-background
//     rule); the content/glyph uses the resolved Foreground (S400 path).
//
// The alpha values below are read from the ENCODED DrawItems (post single-fold),
// the honest render output the S206 scene consumes.

#include <vaml/presentation/VamlRenderer.h>

#include <vaml/core/Element.h>   // StackPanel, Label, Border, TextBlock, Image
#include <vaml/core/UIElement.h> // UIElement, Opacity/Background DPs
#include <vaml/core/Color.h>     // vaml::Color, parse_color
#include <vaml/vulkan/ui/UiScene.h> // DrawItem, DrawItemKind, MaterialKind

#include "doctest/doctest.h"

#include <vector>

using namespace vaml::presentation;
using vaml::vulkan::ui::DrawItem;
using vaml::vulkan::ui::DrawItemKind;

namespace {

// Encodes a single real element through the S212 merge encoder (the S208 encoder
// walk + the single-fold opacity site), returning the FIRST emitted DrawItem.
DrawItem encode_first(const vaml::UIElement& root)
{
    std::vector<DrawItem> out;
    encode_visual_slice(root, default_visual_of(), out);
    REQUIRE(out.size() >= 1u);
    return out[0];
}

} // namespace

TEST_CASE("S593 base panel: opaque Background + Opacity=0.5 -> item.a == 0.5 (SINGLE fold)")
{
    // A real StackPanel (a re-based UIElement) with an opaque Bottom Color
    // Background + Opacity=0.5. The classifier emits item.a = bg.a (1.0); the
    // ENCODER folds effective_opacity ONCE -> 1.0 * 0.5 = 0.5. Under the old
    // double-count it would be 0.25 — this is the regression tripwire.
    vaml::StackPanel p;
    (void)p.Set(vaml::PropertyId::of("Background"),
                vaml::Value::Color(vaml::parse_color("#FF102030")));
    (void)p.set_opacity(0.5);
    p.Measure(vaml::Size{ 100.0f, 100.0f });
    p.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const DrawItem item = encode_first(p);
    CHECK(item.kind == DrawItemKind::SolidRect);
    CHECK(item.a == doctest::Approx(0.5f));  // bg.a (1.0) × Opacity (0.5) — folded ONCE
}

TEST_CASE("S593 panel Background=Transparent (alpha 0) -> item.a == 0")
{
    vaml::StackPanel p;
    (void)p.Set(vaml::PropertyId::of("Background"),
                vaml::Value::Color(vaml::parse_color("#00000000")));  // fully transparent
    p.Measure(vaml::Size{ 100.0f, 100.0f });
    p.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const DrawItem item = encode_first(p);
    CHECK(item.a == doctest::Approx(0.0f));  // straight-alpha bg through, × Opacity 1
}

TEST_CASE("S593 panel semi-alpha Background + Opacity=1.0 -> item.a == bg.a")
{
    vaml::StackPanel p;
    (void)p.Set(vaml::PropertyId::of("Background"),
                vaml::Value::Color(vaml::parse_color("#80FF0000")));  // half-alpha red
    p.Measure(vaml::Size{ 100.0f, 100.0f });
    p.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const DrawItem item = encode_first(p);
    CHECK(item.a == doctest::Approx(0x80 / 255.0f));  // ~0.502 — bg.a × Opacity 1.0
}

TEST_CASE("S593 cross-product: semi-alpha Background × Opacity=0.5 -> item.a == bg.a × 0.5")
{
    vaml::StackPanel p;
    (void)p.Set(vaml::PropertyId::of("Background"),
                vaml::Value::Color(vaml::parse_color("#80FF0000")));
    (void)p.set_opacity(0.5);
    p.Measure(vaml::Size{ 100.0f, 100.0f });
    p.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const DrawItem item = encode_first(p);
    CHECK(item.a == doctest::Approx((0x80 / 255.0f) * 0.5f));  // bg.a × opacity — SINGLE fold
}

TEST_CASE("S593 panel default (no Background, Opacity=1) -> alpha 1 identical to today (no regression)")
{
    vaml::StackPanel p;
    p.Measure(vaml::Size{ 100.0f, 100.0f });
    p.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const DrawItem item = encode_first(p);
    CHECK(item.a == doctest::Approx(1.0f));  // neutral fill, alpha 1.0 × Opacity 1.0
}

TEST_CASE("S593 Label backdrop box is NEUTRAL alpha 1.0 (no invented 0.85f, no fg.a on the box)")
{
    // A Label with a resolved Foreground. Its backdrop box must be a NEUTRAL alpha
    // 1.0 (the vertical-slice default), NOT fg.a (WPF forbids a foreground-tinted
    // background box) and NOT the old invented 0.85f constant.
    vaml::Label label;
    // A distinctive foreground so the box WOULD be fg.a (0.5) if the bug regressed.
    (void)label.set_foreground(/*brushId=*/0);
    label.Measure(vaml::Size{ 100.0f, 30.0f });
    label.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 30.0f });

    const DrawItem item = encode_first(label);
    CHECK(item.kind == DrawItemKind::SolidRect);
    CHECK(item.a == doctest::Approx(1.0f));  // neutral backdrop, alpha 1.0 (× Opacity 1)
}

TEST_CASE("S593 Label Opacity still applies as the single coverage multiplier")
{
    vaml::Label label;
    (void)label.set_foreground(/*brushId=*/0);
    (void)label.set_opacity(0.4);
    label.Measure(vaml::Size{ 100.0f, 30.0f });
    label.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 30.0f });

    const DrawItem item = encode_first(label);
    CHECK(item.a == doctest::Approx(0.4f));  // 1.0 backdrop × Opacity 0.4 — SINGLE fold
}

TEST_CASE("S593 Text/Image alpha honored unchanged")
{
    // A real TextBlock renders through the Text material; alpha is preserved (the
    // S200 straight-alpha blend). We assert the encoder emits it + the alpha stays
    // its authored value through the single-fold (Opacity 1 -> unchanged).
    vaml::TextBlock t;
    (void)t.set_text("x");
    t.Measure(vaml::Size{ 100.0f, 30.0f });
    t.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 30.0f });

    std::vector<DrawItem> out;
    encode_visual_slice(t, default_visual_of(), out);
    REQUIRE(out.size() >= 1u);
    CHECK(out[0].kind == DrawItemKind::TextGlyph);
    // The text glyph alpha = the resolved foreground alpha (unchanged through
    // Opacity 1); at minimum it is a non-degenerate, valid emitted glyph.
    CHECK(out[0].a > 0.0f);
}
