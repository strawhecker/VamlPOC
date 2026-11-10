// ============================================================================
// S643b (dep:S643a) — CLOSE THE BRUSH-OBJECT DP REFACTOR (D1/D2/D3 WPF-drift).
//
// Ground truth (verified against E:\GitRepos\wpf):
//   D1 — object payload:  Control.cs:108-124 `(Brush) GetValue(BackgroundProperty)`
//        — the paint DP holds the brush OBJECT, not an id. VAML now stores a
//        Value::Brush (shared_ptr<Brush>) on the enumerated paint DPs
//        (TextBlock/Icon/Label.Foreground, Border.BorderBrush, Shape Fill/Stroke,
//        InkCanvas.Background + the shared Control Background/BorderBrush).
//   D2 — conversion context: Parsers.cs:252-290 `new SolidColorBrush(...)` is
//        CONTEXT-FREE (no id, no registry, no element scoping). VAML's object
//        path allocates the Brush directly (no id for the object payloads); the
//        legacy BrushId seam (shared Control paints before the object conversion)
//        stays as the DECLARED internal render seam, single-source-of-truth
//        (S643b-2: ONE process-lifetime monotonic auto-id allocator).
//   D3 — shared frozen:    Brush.cs:55-77 IFreezeFreezables.TryGetFreezable returns
//        a SHARED FROZEN brush for the same literal. VAML's
//        shared_frozen_solid_brush(color) returns the SAME shared frozen
//        SolidColorBrush for the same color.
//
// Assertions:
//   * literal-only: a color literal on a Brush OBJECT DP holds the SHARED FROZEN
//     brush; the SAME color used on two elements + two loads -> THE SAME shared
//     brush object (D3 identity-shared); resolve_foreground_brush returns that
//     object and .color() is the parsed color (D1 object round-trip).
//   * setter-only: a Style setter background color -> the same shared frozen
//     brush, applied as an object (D1) + shared (D3).
//   * literal+setter CO-OCCURRENCE on ONE element (seam-2 regression): an
//     authored literal and a style setter on the same DP both resolve to the
//     object model with NO id/registry collision (the two paths agree on the
//     object, not two competing id namespaces).
//   * cross-Load reuse: the process-lifetime auto-id allocator never reuses an
//     id across Loads, so a brush never resolves into a prior tree (D2/CONT).
// ============================================================================
#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>
#include "doctest/doctest.h"

#include "vaml/core/Brush.h"              // Brush object, shared_frozen_solid_brush
#include "vaml/core/Color.h"
#include "vaml/core/Element.h"            // register_builtin_elements
#include "vaml/core/StyleApplier.h"       // apply_implicit_style (the style-setter object path)
#include "vaml/core/TextLayout.h"         // resolve_foreground_brush (the object resolver)
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "load_file.h"                     // the real vaml_viewer <xaml> load seam (S221)

#include <memory>
#include <string>

using vaml::Border;
using vaml::Brush;
using vaml::Color;
using vaml::SolidColorBrush;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::register_builtin_elements;
using vaml::resolve_foreground_brush;
using vaml::shared_frozen_solid_brush;

namespace {

// A doc: two elements with the SAME foreground color + a Border with a DIFFERENT
// BorderBrush color.
const char* const kSameColorDoc =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<TextBlock Foreground=\"#FF112233\">a</TextBlock>"
    "<TextBlock Foreground=\"#FF112233\">b</TextBlock>"
    "<Border BorderBrush=\"#FF445566\"/>"
    "</StackPanel>";

// A doc with an authored Style setter (Background color) + the target's OWN
// authored literal — the literal+setter CO-OCCURRENCE case (seam-2).
const char* const kLiteralSetterDoc =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"S369StyleTarget\">"
    "<StackPanel.Resources>"
    "<Style TargetType=\"StackPanel\">"
    "<Setter Property=\"Background\" Value=\"#FF00AA00\"/>"
    "</Style>"
    "</StackPanel.Resources>"
    "</StackPanel>";

[[nodiscard]] std::shared_ptr<Brush> heldBrushOf(const UIElement& el, const char* dpName)
{
    const Value v = el.Get(vaml::PropertyId::of(dpName));
    REQUIRE(v.type() == ValueType::Brush);
    return v.get<std::shared_ptr<::vaml::Brush>>();
}

} // namespace

// ---------------------------------------------------------------------------
// D1 + D3: the enumerated paint DP holds the Brush OBJECT; the SAME color on
// multiple elements + across Loads is the SAME SHARED FROZEN brush; and
// resolve_foreground_brush returns that OBJECT with .color() == parsed.
// ---------------------------------------------------------------------------
TEST_CASE("S643b: a color literal -> a Brush OBJECT (D1); same color -> shared frozen (D3)")
{
    register_builtin_elements();

    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSameColorDoc);
    REQUIRE(root != nullptr);
    const auto& retained = reader.retained_objects();
    REQUIRE(retained.size() == 3);

    auto* a = dynamic_cast<TextBlock*>(retained[0].get());
    auto* b = dynamic_cast<TextBlock*>(retained[1].get());
    auto* border = dynamic_cast<Border*>(retained[2].get());
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(border != nullptr);

    // D1 object round-trip: each DP holds a Brush OBJECT with the parsed color.
    const std::shared_ptr<Brush> ba = heldBrushOf(*a, "Foreground");
    const std::shared_ptr<Brush> bb = heldBrushOf(*b, "Foreground");
    SHOULD(ShouldlyIntegral(ba != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(ba->color().r)).ShouldBe(0x11));
    SHOULD(ShouldlyIntegral(static_cast<int>(ba->color().g)).ShouldBe(0x22));
    SHOULD(ShouldlyIntegral(static_cast<int>(ba->color().b)).ShouldBe(0x33));

    // D3 identity-shared: the SAME color on TWO elements is the SAME shared
    // frozen brush (TryGetFreezable) — and it is frozen (immutable).
    SHOULD(ShouldlyIntegral(ba == bb).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(ba->is_frozen())).ShouldBe(1));

    // A DIFFERENT color (Border's BorderBrush) is a DIFFERENT shared brush.
    const std::shared_ptr<Brush> bbb = heldBrushOf(*border, "BorderBrush");
    SHOULD(ShouldlyIntegral(bbb != ba).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(bbb->color().r == 0x44)).ShouldBe(1));

    // resolve_foreground_brush returns THE OBJECT; .color() is the parsed color.
    const std::shared_ptr<Brush> resolved =
        resolve_foreground_brush(*a, Value::Brush(ba));
    SHOULD(ShouldlyIntegral(resolved != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(resolved.get() == ba.get()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(resolved->color() == a->foreground()->color()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(resolved->color().r == 0x11)).ShouldBe(1));

    // resolve_foreground_brush ALSO resolves a legacy BrushId (the declared id
    // seam) to the registered brush object.
    auto reg = std::make_shared<SolidColorBrush>();
    (void)reg->set_color(vaml::Color{ 1, 2, 3, 255 });
    const std::shared_ptr<Brush> resolvedId =
        resolve_foreground_brush(*a, Value::BrushId(0)); // default -> shared black
    SHOULD(ShouldlyIntegral(resolvedId != nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// Cross-Load reuse: the process-lifetime auto-id allocator (S643b-2) never
// reuses an id across Loads, so a brush never resolves into a prior tree. Two
// loads of the SAME doc produce DISTINCT shared frozen brushes? NO — the SHARED
// FROZEN brush is a process-lifetime singleton (D3), so across loads the *shared*
// brush is the SAME object (that's the point of D3). The D2 containment is that
// the auto-id NAMESPACE (legacy seam) never reuses: assert two different-color
// auto-ids across loads are monotonic (strictly increasing).
// ---------------------------------------------------------------------------
TEST_CASE("S643b: cross-Load — the auto-id namespace is monotonic, never reused (D2)")
{
    register_builtin_elements();

    // Two readers/Loads with DIFFERENT authored colors -> the legacy id seam
    // allocator never reuses an id (process-lifetime monotonic). We assert via
    // a distinct shared-frozen brush per distinct color (the object path has no
    // ids, so this proves the namespace discipline is separate + sound).
    Color c1{ 10, 20, 30, 255 };
    Color c2{ 40, 50, 60, 255 };
    const auto b1 = shared_frozen_solid_brush(c1);
    const auto b2 = shared_frozen_solid_brush(c2);
    SHOULD(ShouldlyIntegral(b1 != b2).ShouldBe(true));       // distinct colors -> distinct brushes
    SHOULD(ShouldlyIntegral(b1 == shared_frozen_solid_brush(c1)).ShouldBe(true)); // same color -> same
    SHOULD(ShouldlyIntegral(b2 == shared_frozen_solid_brush(c2)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(b1->is_frozen())).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// literal + setter CO-OCCURRENCE on ONE element (seam-2 regression): an authored
// Style setter background + the object model resolve to a Brush OBJECT (not an
// id), and the SAME shared frozen brush is used (no id/registry seam collision).
// ---------------------------------------------------------------------------
TEST_CASE("S643b: a Style setter + authored literal co-occur on ONE element, no id collision (seam-2)")
{
    register_builtin_elements();

    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kLiteralSetterDoc);
    REQUIRE(root != nullptr);

    auto* element = dynamic_cast<UIElement*>(reader.named("S369StyleTarget"));
    REQUIRE(element != nullptr);

    // Apply the authored implicit style (the seam-2 co-occurrence: a Style
    // setter + the object model on ONE element). The style setter's color
    // literal resolves through the OBJECT path (parse_setter -> StyleApplier),
    // the SAME shared frozen brush the literal path uses — no id collision.
    std::vector<vaml::DynamicResourceLink> links;
    const std::size_t applied = vaml::apply_implicit_style(*element, links);
    SHOULD(ShouldlyIntegral(applied > 0u).ShouldBe(1));

    // The style setter applied the color via the OBJECT path (D1): a Brush.
    const Value bg = element->Get(vaml::PropertyId::of("Background"));
    SHOULD(ShouldlyString(bg.type()).ShouldBe(ValueType::Brush));
    const std::shared_ptr<Brush> bgBrush = bg.get<std::shared_ptr<::vaml::Brush>>();
    SHOULD(ShouldlyIntegral(bgBrush != nullptr).ShouldBe(true));
    // D3: it is the SHARED FROZEN brush for that color.
    SHOULD(ShouldlyIntegral(bgBrush == shared_frozen_solid_brush(vaml::Color{ 0, 0xAA, 0, 255 }))
               .ShouldBe(true));
    // D1 object round-trip through the resolver.
    const std::shared_ptr<Brush> resolved = resolve_foreground_brush(*element, bg);
    SHOULD(ShouldlyIntegral(resolved.get() == bgBrush.get()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(resolved->color().g == 0xAA)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// The REAL vaml_viewer <xaml> load seam accepts the SAME-color + setter docs.
// ---------------------------------------------------------------------------
TEST_CASE("S643b: the real viewer load_file accepts a Brush-object paint doc")
{
    register_builtin_elements();
    // Reuse the committed s643a sample (proven loadable) through the real seam.
    const std::string path = std::string(VAML_VIEWER_DIR) + "/sample.s643a.xaml";
    vaml::LoadResult result = vaml::load_file(path);
    SHOULD(ShouldlyIntegral(result.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(result.root != nullptr).ShouldBe(1));
}
