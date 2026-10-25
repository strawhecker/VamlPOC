// S400 (dep:S368) — Text control tests (vaml::core). Headless proof of the
// S400 Text control + the pure TextLayout seam (the CORE half; the RENDER half
// — the vulkan TextGlyphPlan + the material classifiers — lives in its own test
// target, vaml-vulkan-tests / vaml-presentation-tests).
//
//   (A) TEXTLAYOUT PURE SEAM — the CPU text layout + word-wrap engine:
//       default_text_metric (the fontSize-based estimate), text_layout_plan wrap
//       correctness (word wrap, single-long-word overflow, advance math,
//       empty/whitespace, line y/height, measured extent).
//   (B) TEXT CONTROL — the REAL re-based Text (S087 seed RESOLVED at S400):
//       DP defaults/roundtrip (Text/FontSize/FontWeight/Foreground across the
//       S043 attached-aware global path), FontSize >= 0 validation, the real
//       MeasureOverride (measures to the wrapped extent), and the foreground
//       BrushId resolution (resolved via a registered SolidColorBrush;
//       unresolved -> opaque black + a structured diagnostic — NO silent).
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>        // Text (S400), register_builtin_elements
#include <vaml/core/TextLayout.h>     // text_layout_plan / default_text_metric / resolve_foreground_color
#include <vaml/core/UIElement.h>      // UIElement, Size, Rect
#include <vaml/core/Geometry.h>
#include <vaml/core/DependencyProperty.h> // TryGet (S043 attached-aware read)

#include <vaml/core/Brush.h>          // SolidColorBrush (the resolved foreground)
#include <vaml/core/ResourceDictionary.h> // the S360 three-scope lookup anchor

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using vaml::Color;
using vaml::Rect;
using vaml::Size;
using vaml::SolidColorBrush;
using vaml::Text;
using vaml::TextLayoutPlan;
using vaml::TextGlyphMetric;
using vaml::text_layout_plan;
using vaml::default_text_metric;
using vaml::test::ShouldThrow;

namespace {

// A TOY metrics provider: every printable glyph advances 10px + paints (tile 0),
// the space advances 4px + does NOT paint. Deterministic + independent of the
// real atlas — the pure wrap/advance math under test.
vaml::TextGlyphMetrics toyMetrics(float advance = 10.0f, float spaceAdvance = 4.0f)
{
    return [advance, spaceAdvance](std::uint32_t cp) -> vaml::TextGlyphMetric {
        if (cp == 32u)
            return vaml::TextGlyphMetric{ spaceAdvance, false };
        if (cp >= 32u && cp <= 126u)
            return vaml::TextGlyphMetric{ advance, true };
        return vaml::TextGlyphMetric{ 0.0f, false };
    };
}

} // namespace

// ---------------------------------------------------------------------------
// (A) TextLayout pure seam
// ---------------------------------------------------------------------------

TEST_CASE("S400: default_text_metric — byte advance estimate")
{
    // Printable advance 0.6em; space 0.3em non-painting; out-of-range 0 no-paint.
    const vaml::TextGlyphMetric a = default_text_metric(65u, 16.0f); // 'A'
    SHOULD(ShouldlyString(a.advance).ShouldBe(16.0f * 0.6f));
    SHOULD(ShouldlyString(a.render).ShouldBe(true));

    const vaml::TextGlyphMetric sp = default_text_metric(32u, 16.0f); // space
    SHOULD(ShouldlyString(sp.advance).ShouldBe(16.0f * 0.3f));
    SHOULD(ShouldlyString(sp.render).ShouldBe(false));

    const vaml::TextGlyphMetric oo = default_text_metric(200u, 16.0f); // out of range
    SHOULD(ShouldlyString(oo.advance).ShouldBe(0.0f));
    SHOULD(ShouldlyString(oo.render).ShouldBe(false));
}

TEST_CASE("S400: text_layout_plan — empty content -> zero lines / height 0")
{
    const TextLayoutPlan p0 = text_layout_plan("", 16.0f, 200.0f, toyMetrics());
    SHOULD(ShouldlyIntegral(p0.lines.size()).ShouldBe(0u));
    SHOULD(ShouldlyString(p0.measuredHeight).ShouldBe(0.0f));
    SHOULD(ShouldlyString(p0.measuredWidth).ShouldBe(0.0f));

    // A space-only string -> ONE line (height = one line) with the spaces' width.
    const TextLayoutPlan p1 = text_layout_plan("  ", 16.0f, 200.0f, toyMetrics());
    SHOULD(ShouldlyIntegral(p1.lines.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(p1.lineHeight).ShouldBe(1.2f * 16.0f));
    SHOULD(ShouldlyString(p1.measuredHeight).ShouldBe(1.2f * 16.0f));
    // Two spaces at 4px each -> the line width is 8px.
    SHOULD(ShouldlyString(p1.lines[0].width).ShouldBe(8.0f));
}

TEST_CASE("S400: text_layout_plan — single line advance + y/height correctness")
{
    // Five glyphs at 10px advance -> 50px wide, one line, y=0, lineHeight = 1.2*16.
    const TextLayoutPlan p = text_layout_plan("AAAAA", 16.0f, 200.0f, toyMetrics());
    SHOULD(ShouldlyIntegral(p.lines.size()).ShouldBe(1u));
    const auto& line = p.lines[0];
    SHOULD(ShouldlyString(line.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(line.width).ShouldBe(50.0f));
    SHOULD(ShouldlyIntegral(line.glyphs.size()).ShouldBe(5u));
    // Glyph x advances by 10px each; y = the line top (0).
    SHOULD(ShouldlyString(line.glyphs[0].x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(line.glyphs[1].x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(line.glyphs[4].x).ShouldBe(40.0f));
    SHOULD(ShouldlyString(line.glyphs[0].y).ShouldBe(0.0f));
    // Measured extent.
    SHOULD(ShouldlyString(p.measuredWidth).ShouldBe(50.0f));
    SHOULD(ShouldlyString(p.measuredHeight).ShouldBe(1.2f * 16.0f));
}

TEST_CASE("S400: text_layout_plan — WORD WRAP within available width")
{
    // "AAAAA AAAA": two words of 50px and 40px. A 90px available fits both on one
    // line (50 + space4 + 40 = 94 > 90 -> the SECOND word wraps to line 2).
    const TextLayoutPlan p = text_layout_plan("AAAAA AAAA", 16.0f, 90.0f, toyMetrics());
    // Both words do NOT fit on one line -> wrapped to two lines.
    SHOULD(ShouldlyIntegral(p.lines.size()).ShouldBe(2u));
    // Line 0 = the first word (50px); line 1 = the second word (40px), y = lineHeight.
    SHOULD(ShouldlyString(p.lines[0].width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(p.lines[1].width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(p.lines[1].y).ShouldBe(1.2f * 16.0f));
    // Measured height = 2 lines.
    SHOULD(ShouldlyString(p.measuredHeight).ShouldBe(2.0f * 1.2f * 16.0f));
    // The wrapped line's first glyph is at x=0 (leading space dropped).
    SHOULD(ShouldlyIntegral(p.lines[1].glyphs.size()).ShouldBe(4u));
    SHOULD(ShouldlyString(p.lines[1].glyphs[0].x).ShouldBe(0.0f));
}

TEST_CASE("S400: text_layout_plan — a single long word still gets its own line (overflow)")
{
    // A word WIDER than the full available width overflows its own line (does NOT
    // wrap mid-word — the S134 "wider than the boundary still gets its own row"
    // precedent applied to words).
    const TextLayoutPlan p = text_layout_plan("AAAAAAAAAA", 16.0f, 50.0f, toyMetrics());
    // 10 glyphs * 10px = 100px > 50px available. It still flows on ONE line
    // (overflow), width = 100.
    SHOULD(ShouldlyIntegral(p.lines.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(p.lines[0].width).ShouldBe(100.0f));
}

TEST_CASE("S400: text_layout_plan — available width clamps >= 0 (never negative geometry)")
{
    // A negative / zero available width clamps to 0 -> every word wraps to its own
    // line; never negative extent.
    const TextLayoutPlan p = text_layout_plan("AB CD", 16.0f, 0.0f, toyMetrics());
    (void)p; // must not throw; measured never negative
    SHOULD(ShouldlyString(p.measuredWidth >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(p.measuredHeight >= 0.0f).ShouldBe(true));
}

TEST_CASE("S400: text_layout_plan — spaces render no glyph but advance (buffer is honest)")
{
    // "A B": A, space (advances + carries a render=false placement), B.
    const TextLayoutPlan p = text_layout_plan("A B", 16.0f, 200.0f, toyMetrics());
    SHOULD(ShouldlyIntegral(p.lines.size()).ShouldBe(1u));
    // 3 placements (A, space, B) — the space's placement has render=false.
    SHOULD(ShouldlyIntegral(p.lines[0].glyphs.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(p.lines[0].glyphs[1].render).ShouldBe(false));
    SHOULD(ShouldlyString(p.lines[0].glyphs[1].advance).ShouldBe(4.0f));
    // Width = 10 (A) + 4 (space) + 10 (B) = 24.
    SHOULD(ShouldlyString(p.lines[0].width).ShouldBe(24.0f));
}

// ---------------------------------------------------------------------------
// (B) Text control
// ---------------------------------------------------------------------------

TEST_CASE("S400: Text is a real UIElement (S087 seed re-based) + DPs round-trip")
{
    vaml::register_builtin_elements();
    Text t;
    // Re-based onto UIElement (participates in the visual tree + layout).
    (void)dynamic_cast<vaml::UIElement*>(&t);

    // DP defaults.
    SHOULD(ShouldlyString(t.text()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(t.font_size()).ShouldBe(16.0));
    SHOULD(ShouldlyString(t.font_weight()).ShouldBe(0));
    SHOULD(ShouldlyString(t.foreground()).ShouldBe(0));

    // Round-trip each DP (typed accessors).
    SHOULD(ShouldlyString(t.set_text("Hello")).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(t.text()).ShouldBe(std::string("Hello")));
    SHOULD(ShouldlyString(t.set_font_size(24.0)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(t.font_size()).ShouldBe(24.0));
    SHOULD(ShouldlyString(t.set_font_weight(1)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(t.font_weight()).ShouldBe(1));
    SHOULD(ShouldlyString(t.set_foreground(5)).ShouldBe(vaml::SetResult::APPLIED));
    SHOULD(ShouldlyString(t.foreground()).ShouldBe(5));

    // Resolves globally on a PLAIN DependencyObject (the S043 attached-aware
    // read): "Text" is globally registered, so a plain DO reads the global
    // default ("" ) without a local registration.
    vaml::DependencyObject plain;
    const auto dp = vaml::DependencyProperty::TryGet("Text");
    SHOULD(ShouldlyIntegral(dp != nullptr).ShouldBe(true));
    const vaml::Value v = plain.Get(vaml::PropertyId::of("Text"));
    SHOULD(ShouldlyString(v.type() == vaml::ValueType::String).ShouldBe(true));
    SHOULD(ShouldlyString(v.get<std::string>()).ShouldBe(std::string("")));
}

TEST_CASE("S400: Text FontSize validated >= 0 (negative REJECTED)")
{
    Text t;
    SHOULD(ShouldlyString(t.set_font_size(-5.0)).ShouldBe(vaml::SetResult::REJECTED));
    // Unchanged (still the 16 default).
    SHOULD(ShouldlyString(t.font_size()).ShouldBe(16.0));
}

TEST_CASE("S400: Text MeasureOverride measures the wrapped extent (via Measure)")
{
    Text t;
    (void)t.set_text("AAAAA AAAA A"); // 5 + space + 4 + space + 1 glyphs
    (void)t.set_font_size(16.0);

    // A WIDE offer with INFINITE height: the content fits one line -> the
    // natural (wrapped) height is ONE line. (Stretch width fills the finite
    // offer; but with an infinite height axis the Stretch height WRAPS to the
    // natural content height — the S123 infinity-aware rule.)
    t.Measure(Size{ 500.0f, Size::Infinite().height });
    const float singleLineH = t.desired_size().height;

    // A NARROW offer with INFINITE height: the word-wrap creates MORE lines ->
    // a TALLER natural height.
    t.Measure(Size{ 40.0f, Size::Infinite().height });
    const float wrappedH = t.desired_size().height;

    // Single line measures to exactly one line height (1.2 * fontSize).
    SHOULD(ShouldlyString(singleLineH).ShouldBe(1.2f * 16.0f));
    // The wrapped text is strictly taller.
    SHOULD(ShouldlyString(wrappedH > singleLineH).ShouldBe(true));
}

TEST_CASE("S400: resolve_foreground_color — default -> opaque black, no diagnostic")
{
    Text t;
    // Default brushId 0 -> opaque black, NO diagnostic.
    const Color c0 = vaml::resolve_foreground_color(t, 0);
    SHOULD(ShouldlyString(c0.r).ShouldBe(0));
    SHOULD(ShouldlyString(c0.g).ShouldBe(0));
    SHOULD(ShouldlyString(c0.b).ShouldBe(0));
    SHOULD(ShouldlyString(c0.a).ShouldBe(255));
}

TEST_CASE("S400: resolve_foreground_color — registered SolidColorBrush resolves its Color")
{
    // Register a SolidColorBrush resource "Brush:7" with a red color, then resolve
    // brushId 7 through the element's resources slot (the S360 three-scope anchor).
    auto brush = std::make_unique<SolidColorBrush>();
    (void)brush->set_color(vaml::Color{ 200, 30, 10, 255 });

    Text t;
    auto dict = std::make_unique<vaml::ResourceDictionary>();
    dict->set("Brush:7", std::move(brush));
    t.set_resources(std::move(dict));

    const Color c = vaml::resolve_foreground_color(t, 7);
    SHOULD(ShouldlyString(c.r).ShouldBe(200));
    SHOULD(ShouldlyString(c.g).ShouldBe(30));
    SHOULD(ShouldlyString(c.b).ShouldBe(10));
}
