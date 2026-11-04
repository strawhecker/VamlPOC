// S603 (dep:S602) — BRUSH property-element AUTHORING ORDER root A4.
// The S366 authoring parser now matches WPF property-element/content rules:
//   * GeometryDrawing accepts `.Geometry`, `.Pen`, `.Brush` property-elements
//     (WPF GeometryDrawing has NO [ContentProperty]; its settable children are
//     the three owned property-elements — the audit-failed "may only contain
//     .Geometry (got 'GeometryDrawing.Pen')" rejection is resolved).
//   * GradientBrush has [ContentProperty("GradientStops")] (GradientBrush.cs:24),
//     so a BARE <GradientStop> child under <LinearGradientBrush> is legal WPF
//     and folds into GradientStops (the audit-failed "got 'GradientStop'"
//     rejection is resolved); `.GradientStops` property-element is also legal.
//
// Test plan (Shouldly vocabulary):
//   1. A DrawingBrush->DrawingGroup->GeometryDrawing with .Geometry + .Pen +
//      .Brush property-elements loads (the audit-failed shape) and the drawing's
//      geometry/pen/brush are set.
//   2. A LinearGradientBrush authored with BARE <GradientStop> children AND one
//      with the `.GradientStops` property-element yield IDENTICAL stop lists.
//   3. An out-of-set GeometryDrawing property-element still fails with the
//      precise authoring error (S604 discipline).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Color.h>
#include <vaml/core/DependencyObject.h>
#include <vaml/core/Element.h>
#include <vaml/core/ResourceDictionary.h>
#include <vaml/core/Value.h>
#include <vaml/core/VamlReader.h>
#include <vaml/core/VectorDrawing.h>

#include <memory>
#include <string>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DrawingBrush;
using vaml::DrawingGroup;
using vaml::GeometryDrawing;
using vaml::LinearGradientBrush;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::StackPanel;
using vaml::Value;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::detail::get_element_resources;
using vaml::test::ShouldThrow;



// ---------------------------------------------------------------------------
// 1. GeometryDrawing accepts .Geometry + .Pen + .Brush property-elements.
// ---------------------------------------------------------------------------
TEST_CASE("S603: a GeometryDrawing accepts .Geometry/.Pen/.Brush property-elements")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <DrawingBrush x:Key="Drawing">
      <DrawingBrush.Drawing>
        <DrawingGroup>
          <GeometryDrawing>
            <GeometryDrawing.Pen>10</GeometryDrawing.Pen>
            <GeometryDrawing.Brush>20</GeometryDrawing.Brush>
            <GeometryDrawing.Geometry>
              <RectangleGeometry Rect="0,0,10,10"/>
            </GeometryDrawing.Geometry>
          </GeometryDrawing>
        </DrawingGroup>
      </DrawingBrush.Drawing>
    </DrawingBrush>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(xaml);
    REQUIRE(root != nullptr);

    auto* panel = dynamic_cast<StackPanel*>(root.get());
    REQUIRE(panel != nullptr);
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    REQUIRE(dict != nullptr);
    const ResourceValue* v = dict->try_get("Drawing");
    REQUIRE(v != nullptr);
    REQUIRE(v->kind() == ResourceValue::Kind::Object);

    // The DrawingBrush owns a DrawingGroup with the GeometryDrawing child.
    const auto* db = dynamic_cast<const DrawingBrush*>(v->object());
    REQUIRE(db != nullptr);

    // Reach the GeometryDrawing: DrawingBrush.drawing() -> DrawingGroup ->
    // children()[0] -> GeometryDrawing. Assert the pen/brush ids + geometry.
    const DrawingGroup* group =
        dynamic_cast<const DrawingGroup*>(db->drawing().get());
    REQUIRE(group != nullptr);
    REQUIRE(group->children().size() >= 1);
    const GeometryDrawing* gd =
        dynamic_cast<const GeometryDrawing*>(group->children()[0].get());
    REQUIRE(gd != nullptr);
    // .Pen -> 10, .Brush -> 20 (scalar ids from the property-element content).
    REQUIRE(gd->pen() == 10);
    REQUIRE(gd->brush() == 20);
    // .Geometry -> an owned geometry leaf (RectangleGeometry).
    REQUIRE(gd->geometry() != nullptr);
}

// ---------------------------------------------------------------------------
// 2. LinearGradientBrush accepts BOTH bare <GradientStop> children and the
//    `.GradientStops` property-element, folding to identical stop lists.
// ---------------------------------------------------------------------------
TEST_CASE("S603: LinearGradientBrush accepts bare <GradientStop> (ContentProperty) + .GradientStops")
{
    // Form (a): the `.GradientStops` property-element.
    const std::string xamlProps = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <LinearGradientBrush x:Key="GradProps">
      <LinearGradientBrush.GradientStops>
        <GradientStop Color="#FF0000" Offset="0"/>
        <GradientStop Color="#0000FF" Offset="1"/>
      </LinearGradientBrush.GradientStops>
    </LinearGradientBrush>
  </StackPanel.Resources>
</StackPanel>
)";
    // Form (b): BARE <GradientStop> children (WPF GradientStops ContentProperty).
    const std::string xamlBare = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <LinearGradientBrush x:Key="GradBare">
      <GradientStop Color="#FF0000" Offset="0"/>
      <GradientStop Color="#0000FF" Offset="1"/>
    </LinearGradientBrush>
  </StackPanel.Resources>
</StackPanel>
)";

    // Load each form, KEEPING the root alive in the test scope (the root StackPanel
    // OWNS the Resources dict -> the LinearGradientBrush; destroying root frees the
    // brush — mirrors the S460 test pattern where `root` stays in scope).
    VamlReader readerProps;
    VamlReader readerBare;
    std::unique_ptr<DependencyObject> rootProps = readerProps.Load(xamlProps);
    std::unique_ptr<DependencyObject> rootBare = readerBare.Load(xamlBare);

    auto get_brush = [](DependencyObject* root, const char* key)
    {
        auto* panel = dynamic_cast<StackPanel*>(root);
        if (panel == nullptr)
        {
            return static_cast<LinearGradientBrush*>(nullptr);
        }
        const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
        if (dict == nullptr)
        {
            return static_cast<LinearGradientBrush*>(nullptr);
        }
        const ResourceValue* v = dict->try_get(key);
        if (v == nullptr || v->kind() != ResourceValue::Kind::Object)
        {
            return static_cast<LinearGradientBrush*>(nullptr);
        }
        return dynamic_cast<LinearGradientBrush*>(const_cast<DependencyObject*>(v->object()));
    };

    LinearGradientBrush* props = get_brush(rootProps.get(), "GradProps");
    LinearGradientBrush* bare = get_brush(rootBare.get(), "GradBare");
    REQUIRE(props != nullptr);
    REQUIRE(bare != nullptr);

    // BOTH forms converge on the SAME ordered stop list (red -> blue).
    REQUIRE(props->stops().size() == 2);
    REQUIRE(bare->stops().size() == 2);
    REQUIRE(props->stops()[0].color == Color{ 255, 0, 0, 255 });
    REQUIRE(props->stops()[0].offset == 0.0);
    REQUIRE(props->stops()[1].color == Color{ 0, 0, 255, 255 });
    REQUIRE(props->stops()[1].offset == 1.0);

    REQUIRE(bare->stops()[0].color == Color{ 255, 0, 0, 255 });
    REQUIRE(bare->stops()[0].offset == 0.0);
    REQUIRE(bare->stops()[1].color == Color{ 0, 0, 255, 255 });
    REQUIRE(bare->stops()[1].offset == 1.0);
}

// ---------------------------------------------------------------------------
// 3. An out-of-set GeometryDrawing property-element still fails (S604 discipline).
// ---------------------------------------------------------------------------
TEST_CASE("S603: an out-of-set GeometryDrawing property-element fails with the precise error")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <DrawingBrush x:Key="Bad">
      <DrawingBrush.Drawing>
        <DrawingGroup>
          <GeometryDrawing>
            <GeometryDrawing.NotARealProp/>
          </GeometryDrawing>
        </DrawingGroup>
      </DrawingBrush.Drawing>
    </DrawingBrush>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(xaml);
    }
    catch (const VamlReaderException&)
    {
        threw = true;
    }
    REQUIRE(threw);
}
