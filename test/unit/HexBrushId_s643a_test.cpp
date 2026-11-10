// ============================================================================
// S643a/S643b — HEX-COLOR LITERAL ON PAINT DPs (the S643a gap-closure RESOLVED
// at S643b-1 into the WPF-faithful Brush OBJECT read).
//
// Ground truth (verified against E:\GitRepos\wpf): WPF's Generated
// BrushConverter -> Brush.Parse -> Parsers.ParseBrush is a PURE TYPE-LEVEL,
// CONTEXT-FREE converter: it just `new SolidColorBrush(ParseHexColor(...))`
// each time (Parsers.cs:252-290) — NO id registry, NO element scoping, NO dedup
// map, NO collision concern. Non-obvious facts:
//   (1) ParseHexColor accepts #RRGGBB/#AARRGGBB/#RGB/#ARGB with the high-alpha
//       ALPHA-FIRST convention (9->AARRGGBB, 5->ARGB single-digit alpha-first),
//       so S602's parse_color fix for #RGB/#ARGB is the building block;
//   (2) at S643b-1 (closes D1) the enumerated paint DPs (TextBlock.Foreground,
//       Border.BorderBrush, Shape.Fill/Stroke, Icon/Label.Foreground,
//       InkCanvas.Background) hold a REAL Brush OBJECT (Value::Brush), matching
//       WPF's `Control.cs:108-124 (Brush) GetValue(...)` — so a color literal on
//       them allocates a fresh SolidColorBrush and holds it DIRECTLY in the DP,
//       exactly WPF's context-free `new SolidColorBrush`. The legacy id seam
//       (BrushId) remains only for the shared Control Background/BorderBrush +
//       GeometryDrawing (the internal render seam).
//
// Assertions:
//   * the color literals load with NO throw in BOTH the S2 text reader and the
//     S7 compiled reader;
//   * each enumerated paint DP carries a Value::Brush OBJECT whose color() is
//     the PARSED Color (the D1 object-round-trip, not an id);
//   * a NUMERIC literal on a Brush OBJECT DP is NOT WPF-faithful -> fails loud
//     (MalformedValue) — a Brush object has no numeric form;
//   * a malformed color literal fails LOUD (MalformedValue) — never silent;
//   * the real vaml_viewer <xaml> parse check (vaml::load_file) — the smoke
//     .xaml at vaml_viewer/sample.s643a.xaml is proven loadable by the real
//     actual-frame producer.
// ============================================================================
#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Brush.h"
#include "vaml/core/Color.h"
#include "vaml/core/Element.h"            // register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/TextLayout.h"         // resolve_foreground_color
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/tools/VBamlReader.h"       // S7 compiled reader
#include "vaml/tools/Vamlc.h"             // compile_to_bytes

// The real vaml_viewer <xaml> load seam (compiled into this target by
// test/unit/CMakeLists.txt S221; ../../vaml_viewer is on the include path).
#include "load_file.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using vaml::Border;
using vaml::Brush;
using vaml::Color;
using vaml::DependencyObject;
using vaml::ElementFactoryRegistry;
using vaml::PropertyId;
using vaml::StackPanel;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::register_builtin_elements;
using vaml::test::ShouldThrow;

namespace {

// The smoke document — three paint DPs authored with color literals. Rendered
// in both readers; each enumerated paint DP (TextBlock.Foreground,
// Border.BorderBrush) becomes a REAL Brush OBJECT (S643b-1 D1).
const char* const kS643Doc = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <TextBlock Foreground="#FFE8E8E8">lit</TextBlock>
  <Border BorderBrush="#FF123456"/>
</StackPanel>
)";

// Numeric-literal control: a numeric literal on a Brush OBJECT DP is NOT
// WPF-faithful (a Brush object has no numeric form) -> fails loud. Used to
// prove the divergence is rejected, never silently coerced.
const char* const kS643NumericDoc = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <TextBlock Foreground="42">lit</TextBlock>
</StackPanel>
)";

// A malformed color literal on a Brush DP — neither an int nor a parse_color
// form — must fail LOUD (MalformedValue), never silently.
const char* const kS643MalformedDoc = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <TextBlock Foreground="#XYZ">lit</TextBlock>
</StackPanel>
)";

// S643b-1: the enumerated paint DP holds a Brush OBJECT (Value::Brush). Returns
// the held brush's color (D1 object-round-trip).
[[nodiscard]] Color brushColorOf(const DependencyObject& obj, const char* dpName)
{
    const Value v = obj.Get(PropertyId::of(dpName));
    SHOULD(ShouldlyString(v.type()).ShouldBe(ValueType::Brush));
    const std::shared_ptr<Brush> b = v.get<std::shared_ptr<::vaml::Brush>>();
    SHOULD(ShouldlyIntegral(b != nullptr).ShouldBe(true));
    return b->color();
}

} // namespace

// ---------------------------------------------------------------------------
// S2 text reader: Brush-id color literals are converted to REAL Brush OBJECTs
// (S643b-1 D1) and the object holds the parsed Color. The non-root children are
// RETAINED READER-OWNED until finalize_visual_tree() (S440) — we navigate
// retained_objects() (document order: TextBlock, then Border).
// ---------------------------------------------------------------------------
TEST_CASE("S643b: S2 text reader — color literals on paint DPs become Brush OBJECTs (D1)")
{
    register_builtin_elements();

    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(kS643Doc);
    REQUIRE(root != nullptr);

    const auto& retained = reader.retained_objects();
    REQUIRE(retained.size() == 2);

    // TextBlock.Foreground="#FFE8E8E8" -> (E8,E8,E8,FF)=(232,232,232,255).
    auto* tb = dynamic_cast<TextBlock*>(retained[0].get());
    REQUIRE(tb != nullptr);
    const vaml::Value fgv = tb->Get(vaml::PropertyId::of("Foreground"));
    SHOULD(ShouldlyString(fgv.type()).ShouldBe(vaml::ValueType::Brush));
    const Color fg = fgv.get<std::shared_ptr<::vaml::Brush>>()->color();
    SHOULD(ShouldlyString(fg.r).ShouldBe(232));
    SHOULD(ShouldlyString(fg.g).ShouldBe(232));
    SHOULD(ShouldlyString(fg.b).ShouldBe(232));
    SHOULD(ShouldlyString(fg.a).ShouldBe(255));
    // The object also round-trips through the typed accessor (a SINGLE brush).
    SHOULD(ShouldlyIntegral(tb->foreground() != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tb->foreground()->color().r == 232).ShouldBe(1));

    // Border.BorderBrush="#FF123456" -> (12,34,56,FF) as a Brush OBJECT.
    auto* border = dynamic_cast<Border*>(retained[1].get());
    REQUIRE(border != nullptr);
    const Color bb = brushColorOf(*border, "BorderBrush");
    SHOULD(ShouldlyString(bb.r).ShouldBe(0x12));
    SHOULD(ShouldlyString(bb.g).ShouldBe(0x34));
    SHOULD(ShouldlyString(bb.b).ShouldBe(0x56));
    SHOULD(ShouldlyString(bb.a).ShouldBe(0xFF));
    // Fresh-object-per-conversion: a second element's brush is a SEPARATE
    // object (never deduped/reused — WPF's new-per-conversion).
    SHOULD(ShouldlyIntegral(tb->foreground() != border->border_brush()).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// S7 compiled reader: the same doc through the real vamlcc + VBamlReader.
// ---------------------------------------------------------------------------
TEST_CASE("S643b: S7 compiled reader — color literals become Brush OBJECTs (D1)")
{
    register_builtin_elements();

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS643Doc);
    vaml::tools::VBamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(bytes);
    REQUIRE(root != nullptr);

    const auto& retained = reader.retained_objects();
    REQUIRE(retained.size() == 2);

    auto* tb = dynamic_cast<TextBlock*>(retained[0].get());
    REQUIRE(tb != nullptr);
    const Color fg = brushColorOf(*tb, "Foreground");
    SHOULD(ShouldlyString(fg.r).ShouldBe(232));
    SHOULD(ShouldlyString(fg.g).ShouldBe(232));
    SHOULD(ShouldlyString(fg.b).ShouldBe(232));
    SHOULD(ShouldlyString(fg.a).ShouldBe(255));

    auto* border = dynamic_cast<Border*>(retained[1].get());
    REQUIRE(border != nullptr);
    const Color bb = brushColorOf(*border, "BorderBrush");
    SHOULD(ShouldlyString(bb.r).ShouldBe(0x12));
    SHOULD(ShouldlyString(bb.g).ShouldBe(0x34));
    SHOULD(ShouldlyString(bb.b).ShouldBe(0x56));
    SHOULD(ShouldlyString(bb.a).ShouldBe(0xFF));
}

// ---------------------------------------------------------------------------
// A numeric literal on a Brush OBJECT DP is NOT WPF-faithful (a Brush object
// has no numeric form) -> fails LOUD (MalformedValue), never silent.
// ---------------------------------------------------------------------------
TEST_CASE("S643b: a numeric literal on a Brush OBJECT DP throws MalformedValue (never silent)")
{
    register_builtin_elements();

    VamlReader reader;
    SHOULD(ShouldThrow<vaml::ValueConversionException>([&] {
        (void)reader.Load(kS643NumericDoc);
    }));
}

// ---------------------------------------------------------------------------
// Malformed literal fails LOUD (never silent) — the structured MalformedValue.
// ---------------------------------------------------------------------------
TEST_CASE("S643b: a malformed color literal on a Brush DP throws MalformedValue (never silent)")
{
    register_builtin_elements();

    VamlReader reader;
    SHOULD(ShouldThrow<vaml::ValueConversionException>([&] {
        (void)reader.Load(kS643MalformedDoc);
    }));
}

// ---------------------------------------------------------------------------
// The real vaml_viewer <xaml> parse check: sample.s643a.xaml must load through
// the REAL actual-frame load seam (vaml::load_file), proving the smoke .xaml is
// loadable by the real actual-frame producer.
// ---------------------------------------------------------------------------
TEST_CASE("S643a: the real viewer load_file accepts sample.s643a.xaml (hex-color BrushId literals)")
{
    register_builtin_elements();

    // VAML_VIEWER_DIR is the compile-defined repo path to vaml_viewer/ (set in
    // test/unit/CMakeLists.txt for the S643a gate, mirroring the S643
    // VAML_UI_TESTER_DIR convention) — the COMMITTED smoke .xaml is proven
    // loadable by the real actual-frame producer's seam.
    const std::string path = std::string(VAML_VIEWER_DIR) + "/sample.s643a.xaml";
    vaml::LoadResult result = vaml::load_file(path);
    SHOULD(ShouldlyIntegral(result.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(result.root != nullptr).ShouldBe(1));
}
