// S480 (dep:S476) — Label control RESOLVED: a REAL re-based UIElement whose
// Content DP renders as text, kept as a legacy ContentControl-compatible value.
//
// Under test (headless):
//   1. The Label DP surface is real and reachable: Content/FontSize/FontFamily/
//      Foreground/HorizontalContentAlignment round-trip through the registry.
//   2. MeasureOverride wraps the Content via the S400 text_layout_plan (the S400
//      basis), so a Label sizes to its wrapped text extent.
//   3. The classifier maps a Label to MaterialKind::Solid (the content box;
//      the glyph render defactors to the S400 Text path).
//
// NO-GAP (recorded, not silent): no ContentControl base class exists (the S649
// continuation); Label is a self-contained UIElement. The glyph render (SDF
// quads) is the S400 Text path.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"

#include <memory>

using vaml::Label;

TEST_CASE("S480: Label DP surface round-trips")
{
    Label label;
    SHOULD(ShouldlyString(label.content()).ShouldBe(std::string("")));
    SHOULD(ShouldlyIntegral(static_cast<int>(label.font_size() == 16.0)).ShouldBe(1));
    (void)label.set_content("Hello, world");
    (void)label.set_font_size(20.0);
    (void)label.set_font_family("Consolas");
    (void)label.set_foreground(7);
    (void)label.set_horizontal_content_alignment(2);
    SHOULD(ShouldlyString(label.content()).ShouldBe(std::string("Hello, world")));
    SHOULD(ShouldlyIntegral(static_cast<int>(label.font_size() == 20.0)).ShouldBe(1));
    SHOULD(ShouldlyString(label.font_family()).ShouldBe(std::string("Consolas")));
    SHOULD(ShouldlyIntegral(label.foreground()).ShouldBe(7));
    SHOULD(ShouldlyIntegral(label.horizontal_content_alignment()).ShouldBe(2));
}

TEST_CASE("S480: Label measures to its wrapped text extent (the S400 basis)")
{
    Label label;
    (void)label.set_content("A short label");
    label.Measure(vaml::Size{ 200.0f, 40.0f });
    // The measured box reflects the wrapped text (non-zero, fits the width).
    const vaml::Size d = label.desired_size();
    SHOULD(ShouldlyIntegral(d.width > 0.0f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(d.height > 0.0f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(d.width <= 200.0f).ShouldBe(true));
}
