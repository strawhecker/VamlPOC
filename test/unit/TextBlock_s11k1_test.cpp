// 11k-1 (dep:S400) — TextBlock: the corpus dump's #1 blocker RESOLVED.
//
//   (A) A corpus-shaped `<TextBlock>label</TextBlock>` authored text leaf now
//       LOADS: it resolves to a real TextBlock (no "unknown element type
//       '<TextBlock>'" throw), its inline content lands on the Text DP (the
//       WPF IAddChild routing authored at 11k-1), and a FontSize attribute
//       round-trips.
//   (B) The TextBlock measures to its wrapped extent + classifies to
//       MaterialKind::Text (the S400 material carried over unchanged under the
//       WPF-true name).
//
// 11k-1 RETIRES the S087-invented `Text` name; the WPF-true `TextBlock`
// (TextBlock : FrameworkElement; in VAML the UIElement branch) is the authored
// text leaf. S167_1 corrective convention: the S400 `Text` history is preserved
// (never edited to hide the error), SUPERSEDED BY 11k-1. NO-GAP: the general
// content→DP routing + mixed/interleaved ordering is 11k-11's in-11k step —
// merely SCOPED OUT of this file, not silently deferred.
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/VamlReader.h>         // VamlReader::Load (the corpus entry)
#include <vaml/core/Element.h>            // TextBlock + register_builtin_elements
#include <vaml/core/UIElement.h>          // UIElement, Size, Rect, desired_size
#include <vaml/core/Geometry.h>           // Size / Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element (vertical slice)
#include <vaml/vulkan/ui/UiScene.h>       // MaterialKind

using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

// ---------------------------------------------------------------------------
// (A) CORPUS-SHAPED LOAD — `<TextBlock>label</TextBlock>`
// ---------------------------------------------------------------------------
TEST_CASE("11k-1: a corpus-shaped <TextBlock> leaf loads (no unknown-type throw)")
{
    register_builtin_elements();
    vaml::VamlReader reader;

    // The corpus dump's #1 blocker: <TextBlock> used to hit "unknown element
    // type '<TextBlock>'". It now resolves to a real TextBlock.
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<TextBlock FontSize=\"24\">label</TextBlock>");

    REQUIRE(root != nullptr);

    vaml::TextBlock* tb = dynamic_cast<vaml::TextBlock*>(root.get());
    REQUIRE(tb != nullptr);

    // Inline content -> the Text DP (the WPF IAddChild content routing).
    SHOULD(ShouldlyString(tb->text()).ShouldBe(std::string("label")));
    // The FontSize attribute round-trips through the Double converter.
    SHOULD(ShouldlyString(tb->font_size()).ShouldBe(24.0));

    // Re-based onto UIElement (participates in the visual tree + layout).
    (void)dynamic_cast<vaml::UIElement*>(tb);
}

// ---------------------------------------------------------------------------
// (B) MEASURE + CLASSIFY — the S400 material under the WPF-true name
// ---------------------------------------------------------------------------
TEST_CASE("11k-1: TextBlock measures to its wrapped extent + classifies to MaterialKind::Text")
{
    register_builtin_elements();
    vaml::TextBlock tb;
    (void)tb.set_text("AAAAA AAAA A"); // 5 + space + 4 + space + 1 glyphs

    // Foreground (BrushId) round-trip. NOTE: BrushId has no literal converter,
    // so it is authored programmatically (the WPF-faithful brush surface); the
    // LN inline-content + FontSize authoring is proven in test (A).
    (void)tb.set_foreground(7);
    SHOULD(ShouldlyIntegral(tb.foreground() == 7).ShouldBe(true));

    // A WIDE offer with INFINITE height -> one line.
    tb.Measure(vaml::Size{ 500.0f, vaml::Size::Infinite().height });
    const float singleLineH = tb.desired_size().height;
    // A NARROW offer with INFINITE height -> word-wrap -> taller.
    tb.Measure(vaml::Size{ 40.0f, vaml::Size::Infinite().height });
    const float wrappedH = tb.desired_size().height;

    SHOULD(ShouldlyString(singleLineH).ShouldBe(1.2f * 16.0f));
    SHOULD(ShouldlyString(wrappedH > singleLineH).ShouldBe(true));

    // Give it an arranged box so the classifier uses a real render slot.
    tb.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 20.0f });

    // The S400 material is carried under the WPF-true name: TextBlock ->
    // MaterialKind::Text (the SDF glyph-quad material).
    const vaml::vulkan::ui::ElementVisual ev = classify_merge_element(tb);
    SHOULD(ShouldlyIntegral(ev.kind == MaterialKind::Text).ShouldBe(true));
}
