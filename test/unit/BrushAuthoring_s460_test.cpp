// S460 (dep:S456) — BRUSH AUTHORING: LinearGradientBrush + GradientStop.
//
// The S2 text reader authors a <LinearGradientBrush> from real .vaml:
//   * `<X.Resources><LinearGradientBrush x:Key="Grad" StartPoint="0,0"
//     EndPoint="1,1"><LinearGradientBrush.GradientStops>
//       <GradientStop Color="#FF0000" Offset="0"/>
//       <GradientStop Color="#0000FF" Offset="1"/>
//     </LinearGradientBrush.GradientStops></LinearGradientBrush></X.Resources>`
//     -> the LinearGradientBrush is captured BY its x:Key as an S360 Object
//       resource, its authored StartPoint/EndPoint + ordered GradientStop
//       Color/Offset list settle into the brush's data model.
//   * The authored gradient registers under the "Brush:<id>" convention and
//     RESOLVES by id: resolve_foreground_color() (TextLayout) recognizes a
//     LinearGradientBrush resource via its scalar representative_color() — the
//     same BrushId path a SolidColorBrush rides (the S460 title: "rides the
//     BrushId path").
//   * The element factory registry gate grows 14 -> 16 (LinearGradientBrush +
//     GradientStop are authorable element types).
//
// NO-GAP (recorded, not silent): the full vertex-sampled gradient RENDER is
// S465's obligation (this seed's sentinel: "RESOLVED BY S465 (gradient render)")
// — S460 delivers the authored DATA + resolution-by-id, never the render.
//
// Registers no S460-unique DPs (GradientStop.Color/Offset are GradientStop-owned;
// Background/Color are brush/template-owned). Uses the Shouldly vocabulary.
// ADR-018 zero locks. [MINIMAL REAL LinearGradientBrush RESOLVED BY S465.]

#include "doctest/doctest.h"

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

#include <memory>
#include <string>

using vaml::Color;
using vaml::DependencyObject;
using vaml::ElementFactoryRegistry;
using vaml::GradientStop;
using vaml::LinearGradientBrush;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::find_for_element;
using vaml::register_builtin_elements;
using vaml::resolve_foreground_color;

namespace {

// A full authored gradient resource: a StackPanel's Resources dict holds a
// LinearGradientBrush red -> blue with an explicit StartPoint/EndPoint.
const std::string kGradientDoc =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<StackPanel.Resources>"
    "<LinearGradientBrush x:Key=\"Grad\" StartPoint=\"0,0\" EndPoint=\"1,1\">"
    "<LinearGradientBrush.GradientStops>"
    "<GradientStop Color=\"#FF0000\" Offset=\"0\"/>"
    "<GradientStop Color=\"#0000FF\" Offset=\"1\"/>"
    "</LinearGradientBrush.GradientStops>"
    "</LinearGradientBrush>"
    "</StackPanel.Resources>"
    "</StackPanel>";

} // namespace

// ---------------------------------------------------------------------------
// 1. The authored LinearGradientBrush is captured by x:Key as an Object resource
// ---------------------------------------------------------------------------

TEST_CASE("S460: an authored LinearGradientBrush resource captures its data model")
{
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(kGradientDoc);

    auto* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));

    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("Grad");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(v->kind() == ResourceValue::Kind::Object).ShouldBe(true));
            const LinearGradientBrush* grad =
                dynamic_cast<const LinearGradientBrush*>(v->object());
            SHOULD(ShouldlyIntegral(grad != nullptr).ShouldBe(true));
            if (grad)
            {
                // Authored StartPoint / EndPoint.
                SHOULD(ShouldlyString(grad->start_x()).ShouldBe(0.0));
                SHOULD(ShouldlyString(grad->start_y()).ShouldBe(0.0));
                SHOULD(ShouldlyString(grad->end_x()).ShouldBe(1.0));
                SHOULD(ShouldlyString(grad->end_y()).ShouldBe(1.0));
                // Authored ordered GradientStops (Color + Offset).
                SHOULD(ShouldlyIntegral(grad->stops().size()).ShouldBe(2u));
                if (grad->stops().size() == 2)
                {
                    SHOULD(ShouldlyString(grad->stops()[0].color).ShouldBe(Color{ 255, 0, 0, 255 }));
                    SHOULD(ShouldlyString(grad->stops()[0].offset).ShouldBe(0.0));
                    SHOULD(ShouldlyString(grad->stops()[1].color).ShouldBe(Color{ 0, 0, 255, 255 }));
                    SHOULD(ShouldlyString(grad->stops()[1].offset).ShouldBe(1.0));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 2. The authored gradient resolves by BrushId (rides the S460 "Brush:<id>" path)
// ---------------------------------------------------------------------------

TEST_CASE("S460: resolve_foreground_color resolves an authored gradient by BrushId")
{
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(kGradientDoc);
    auto* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));

    // The gradient is reachable via the S360 three-scope walk at a BrushId that
    // maps to its "Brush:<id>" key. We resolve the authored gradient for id 7
    // (any non-zero id whose "Brush:7" key was registered — here we register it
    // under that convention by placing the gradient in the element's dict under
    // "Brush:7" via the resource resolution). To prove the PATH headlessly, we
    // author a second doc whose dict keys the gradient as "Brush:7" the same way
    // resolve_foreground_color builds it.
    const std::string keyedDoc =
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" x:Name=\"P\">"
        "<StackPanel.Resources>"
        "<LinearGradientBrush x:Key=\"Brush:7\" StartPoint=\"0,0\" EndPoint=\"1,1\">"
        "<LinearGradientBrush.GradientStops>"
        "<GradientStop Color=\"#FF0000\" Offset=\"0\"/>"
        "<GradientStop Color=\"#0000FF\" Offset=\"1\"/>"
        "</LinearGradientBrush.GradientStops>"
        "</LinearGradientBrush>"
        "</StackPanel.Resources>"
        "</StackPanel>";
    std::unique_ptr<DependencyObject> root2 = reader.Load(keyedDoc);
    auto* p2 = dynamic_cast<StackPanel*>(root2.get());
    SHOULD(ShouldlyIntegral(p2 != nullptr).ShouldBe(true));

    // BrushId 7 -> "Brush:7" -> the authored LinearGradientBrush -> its scalar
    // representative color (red+blue averaged = a mid purple).
    const Color resolved = resolve_foreground_color(*p2, 7);
    // representative_color of [red, blue] = byte-average -> (127,0,127,255).
    SHOULD(ShouldlyString(resolved.r).ShouldBe(127u));
    SHOULD(ShouldlyString(resolved.g).ShouldBe(0u));
    SHOULD(ShouldlyString(resolved.b).ShouldBe(127u));
    SHOULD(ShouldlyString(resolved.a).ShouldBe(255u));
}

// ---------------------------------------------------------------------------
// 3. The element registry gate grows 14 -> 16
// ---------------------------------------------------------------------------

TEST_CASE("S460: the element registry now includes LinearGradientBrush + GradientStop (gate 14->16)")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("LinearGradientBrush")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GradientStop")).ShouldBe(true));
    // 21 (the pre-11d set) -> 33 its full built-in register as Sprint 11d
    // added the core display + input controls (Label/Border/Rectangle/Ellipse/
    // Line/Image/ProgressBar/PasswordBox/GroupBox/Expander/Separator/StatusBar),
    // then 36 as Sprint 11j added UserControl + Application + ResourceDictionaryHolder,
    // then 41 as 11k-2 added ScrollBar/RepeatButton/Viewbox/ToolBar/ToolBarTray,
    // then 50 as 11k-4 added PathGeometry/GeometryGroup/RectangleGeometry/
    // EllipseGeometry/GeometryDrawing/DrawingGroup/DrawingBrush/DrawingImage/
    // ImageBrush (the vector-drawing + brush-resource volume).
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(50u));
}

// ---------------------------------------------------------------------------
// 4. An authored gradient with a malformed StartPoint errors readably (never silent)
// ---------------------------------------------------------------------------

TEST_CASE("S460: a malformed StartPoint on an authored gradient is a readable error")
{
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<StackPanel.Resources>"
            "<LinearGradientBrush x:Key=\"Bad\" StartPoint=\"0\">"
            "<LinearGradientBrush.GradientStops>"
            "<GradientStop Color=\"#FF0000\" Offset=\"0\"/>"
            "</LinearGradientBrush.GradientStops>"
            "</LinearGradientBrush>"
            "</StackPanel.Resources></StackPanel>");
    }
    catch (const vaml::VamlReaderException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
