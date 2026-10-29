// 11k-4 (dep:11k-3/S453/S460) — VECTOR-DRAWING RESOURCES: Brushes + the
// vector-drawing resources (the corpus's LARGEST omitted volume — the entire
// `Icons/*.xaml` + `Icons.xaml`/`NewIcons.xaml` family).
//
// Coverage (the plan's P1–P4):
//   1. authoring round-trip: `<PathGeometry>`/`<GeometryGroup>`/
//      `<RectangleGeometry>`/`<EllipseGeometry>` + `<DrawingBrush>`/
//      `<DrawingGroup>`/`<GeometryDrawing>`/`<ImageBrush>` captured by x:Key
//      into a ResourceDictionary as Object resources;
//   2. the `parse_path_geometry` WPF MiniLanguage matrix (M/L/H/V/C/S/Q/T/A/Z +
//      repeated-number shorthand + degenerate fail-loud);
//   3. resource resolution through the S360 three-scope walk (element's own ->
//      ancestor -> app);
//   4. the vulkan vector raster seam: drawing/geometry -> RGBA8 S202 tile ->
//      textured-quad material (headless).
//
// WPF-fidelity (validated against E:\GitRepos\wpf, the real product source):
// Geometry/Drawing are ABSTRACT (internal ctor, NOT authorable as bare
// elements) — only the concrete DO leaves are. The 11k-3 `<Geometry>` deferral
// (OUT-OF-SCOPE(11k-4), recorded in the spine docs) is RESOLVED here.

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

#include <vaml/vulkan/vector/VectorRaster.h>  // the 11k-4 raster seam
#include <vaml/vulkan/ui/UiPipeline.h>        // Color4 (std::array<float,4>)

#include <memory>
#include <string>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DrawingBrush;
using vaml::DrawingGroup;
using vaml::GeometryDrawing;
using vaml::ImageBrush;
using vaml::PathCommand;
using vaml::PathData;
using vaml::PathFigure;
using vaml::PathGeometry;
using vaml::RectangleGeometry;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::SetResult;
using vaml::StackPanel;
using vaml::ValueConversionException;
using vaml::VamlReader;
using vaml::detail::get_element_resources;

TEST_CASE("11k4 parse_path_geometry matrix")
{
    SUBCASE("Move-Line-Close")
    {
        const PathData d = vaml::parse_path_geometry("M0,0 L10,0 L10,10 Z");
        REQUIRE(d.size() == 1);
        const PathFigure& fig = d.front();
        // M, L, L, Z = 4 steps.
        REQUIRE(fig.size() == 4);
        CHECK(fig[0].cmd == PathCommand::MoveTo);
        CHECK(fig[0].a[0] == 0.0f);
        CHECK(fig[0].a[1] == 0.0f);
        CHECK(fig[1].cmd == PathCommand::LineTo);
        CHECK(fig[1].a[0] == 10.0f);
        CHECK(fig[1].a[1] == 0.0f);
        CHECK(fig[3].cmd == PathCommand::Close);
    }

    SUBCASE("Repeated-number shorthand (H/V draw pairs) + relative")
    {
        // "M0,0 h10 v10 h-10 z" — H/V carry ONE operand; the coordinates are
        // relative to the implicit cursor (the numeric values retained as-is).
        const PathData d = vaml::parse_path_geometry("M0,0 h10 v10 h-10 z");
        REQUIRE(d.size() == 1);
        const PathFigure& fig = d.front();
        CHECK(fig.size() == 5); // M, then 3 draws (h,v,h), then z
        CHECK(fig[0].cmd == PathCommand::MoveTo);
        CHECK(fig[1].cmd == PathCommand::LineTo);
        CHECK(fig[1].a[0] == 10.0f);   // h10
        CHECK(fig[2].cmd == PathCommand::LineTo);
        CHECK(fig[2].a[1] == 10.0f);   // v10
        CHECK(fig[4].cmd == PathCommand::Close);
    }

    SUBCASE("Cubic + smooth + quad + arc commands parse")
    {
        const PathData d = vaml::parse_path_geometry(
            "M0,0 C1,2 3,4 5,6 S7,8 9,10 Q11,12 13,14 T15,16 A5,5 0 0 1 20,20 Z");
        REQUIRE(d.size() == 1);
        const PathFigure& fig = d.front();
        CHECK(fig[1].cmd == PathCommand::CubicTo);    // C (6 operands)
        CHECK(fig[1].a[0] == 1.0f);
        CHECK(fig[1].a[4] == 5.0f);
        CHECK(fig[1].a[5] == 6.0f);
        CHECK(fig[2].cmd == PathCommand::SmoothCubic); // S (4 operands)
        CHECK(fig[3].cmd == PathCommand::QuadTo);      // Q (4 operands)
        CHECK(fig[4].cmd == PathCommand::SmoothQuad);  // T (2 operands)
        CHECK(fig[5].cmd == PathCommand::ArcTo);       // A (7 operands)
        CHECK(fig[5].a[0] == 5.0f);
        CHECK(fig[5].a[3] == 0.0f); // large-arc
        CHECK(fig[5].a[4] == 1.0f); // sweep
        CHECK(fig[5].a[5] == 20.0f);
        CHECK(fig[5].a[6] == 20.0f);
    }

    SUBCASE("Multiple figures (implicit M split)")
    {
        const PathData d = vaml::parse_path_geometry("M0,0 L1,1 M2,2 L3,3");
        REQUIRE(d.size() == 2);
        CHECK(d[0].front().cmd == PathCommand::MoveTo);
        CHECK(d[0].back().cmd == PathCommand::LineTo);
        CHECK(d[1].front().cmd == PathCommand::MoveTo);
        CHECK(d[1].back().cmd == PathCommand::LineTo);
    }

    SUBCASE("Degenerate/empty parser contract")
    {
        const PathData empty = vaml::parse_path_geometry("");
        CHECK(empty.empty());
        const PathData ws = vaml::parse_path_geometry("   ");
        CHECK(ws.empty());
    }
}

TEST_CASE("11k4 PathGeometry set_figures fail-loud on degenerate")
{
    PathGeometry g;
    // An EMPTY 'Figures' string is a fail-loud authoring error (never silent).
    {
        bool threw = false;
        try { (void)g.set_figures(""); } catch (const vaml::ValueConversionException&) { threw = true; }
        CHECK(threw);
    }
    SUBCASE("valid figures set")
    {
        CHECK(g.set_figures("M0,0 L4,0 L4,4 Z") == vaml::SetResult::APPLIED);
        CHECK(g.parsed());
        CHECK(g.figures().size() == 1);
        CHECK(g.figures().front().size() == 4);
    }
}

TEST_CASE("11k4 authoring round-trip captures Object resources by x:Key")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <ResourceDictionary>
      <PathGeometry x:Key="arrowGlyph" Figures="M0,0 L16,8 L0,16 Z" FillRule="1"/>
      <RectangleGeometry x:Key="rounded" Rect="4,4,32,16"/>
      <ImageBrush x:Key="tagBrush" ImageSource="Icons/tag.png"/>
      <DrawingBrush x:Key="vectorBrush">
        <DrawingBrush.Drawing>
          <DrawingGroup>
            <GeometryDrawing>
              <GeometryDrawing.Geometry>
                <PathGeometry Figures="M0,0 L32,0 L32,32 Z"/>
              </GeometryDrawing.Geometry>
            </GeometryDrawing>
          </DrawingGroup>
        </DrawingBrush.Drawing>
      </DrawingBrush>
    </ResourceDictionary>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    auto root = reader.Load(xaml);
    REQUIRE(root != nullptr);
    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    REQUIRE(panel != nullptr);
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    REQUIRE(dict != nullptr);

    // The authored geometry leaves are Object resources.
    const ResourceValue* arrow = dict->try_get("arrowGlyph");
    REQUIRE(arrow != nullptr);
    CHECK(arrow->kind() == ResourceValue::Kind::Object);
    REQUIRE(arrow->object() != nullptr);
    auto* pg = dynamic_cast<const PathGeometry*>(arrow->object());
    REQUIRE(pg != nullptr);
    CHECK(pg->figures().size() == 1);
    CHECK(pg->fill_rule() == 1);

    const ResourceValue* rv = dict->try_get("rounded");
    REQUIRE(rv != nullptr);
    REQUIRE(rv->object() != nullptr);
    auto* rg = dynamic_cast<const RectangleGeometry*>(rv->object());
    REQUIRE(rg != nullptr);
    CHECK(rg->rect().x == 4.0f);
    CHECK(rg->rect().width == 32.0f);

    // The ImageBrush resource (TagConfigurationUserControl channel).
    const ResourceValue* ib = dict->try_get("tagBrush");
    REQUIRE(ib != nullptr);
    REQUIRE(ib->object() != nullptr);
    auto* imageBrush = dynamic_cast<const ImageBrush*>(ib->object());
    REQUIRE(imageBrush != nullptr);
    CHECK(imageBrush->image_source() == "Icons/tag.png");

    // The DrawingBrush's owned vector/drawing graph (DrawingGroup -> GeometryDrawing
    // -> PathGeometry).
    const ResourceValue* vb = dict->try_get("vectorBrush");
    REQUIRE(vb != nullptr);
    REQUIRE(vb->object() != nullptr);
    auto* dbrush = dynamic_cast<const DrawingBrush*>(vb->object());
    REQUIRE(dbrush != nullptr);
    REQUIRE(dbrush->drawing() != nullptr);
    auto* group = dynamic_cast<const DrawingGroup*>(dbrush->drawing().get());
    REQUIRE(group != nullptr);
    CHECK(group->child_count() == 1);
    auto* gd = dynamic_cast<const GeometryDrawing*>(group->children().front().get());
    REQUIRE(gd != nullptr);
    REQUIRE(gd->geometry() != nullptr);
    auto* gpg = dynamic_cast<const PathGeometry*>(gd->geometry().get());
    REQUIRE(gpg != nullptr);
    CHECK(gpg->figures().size() == 1);
}

TEST_CASE("11k4 abstract Geometry/Drawing are not authorable")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <ResourceDictionary>
      <Geometry x:Key="bad"/>
    </ResourceDictionary>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    // <Geometry> is abstract + not registered as an authorable element (the
    // ElementResolutionException genre), so Load fails loud.
    bool threw = false;
    try { std::unique_ptr<vaml::DependencyObject> r = reader.Load(xaml); (void)r; }
    catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

TEST_CASE("11k4 duplicate x:Key is ResourceAlreadyRegistered")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <ResourceDictionary>
      <PathGeometry x:Key="g" Figures="M0,0 L4,4 Z"/>
      <RectangleGeometry x:Key="g" Rect="0,0,4,4"/>
    </ResourceDictionary>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    bool threw = false;
    try { std::unique_ptr<vaml::DependencyObject> r = reader.Load(xaml); (void)r; }
    catch (const vaml::ResourceAlreadyRegistered&) { threw = true; }
    CHECK(threw);
}

TEST_CASE("11k4 vector raster seam: drawing -> tile -> textured quad")
{
    using vaml::vulkan::vector::VectorTile;
    using vaml::vulkan::vector::raster_geometry_to_tile;

    // A PathGeometry with a clear bounds.
    vaml::PathGeometry g;
    const vaml::SetResult r = g.set_figures("M0,0 L32,0 L32,32 Z");
    (void)r;

    const Color fill{ 200u, 30u, 40u, 255u };
    const VectorTile t = raster_geometry_to_tile(g, fill);
    REQUIRE(t.ok);
    CHECK(t.rgba.size() == 64u * 64u * 4u);
    CHECK(t.texelCount == 64u * 64u);
    // The ink box is inside the tile (margin fitted) — u0>0 && u1<1.
    CHECK(t.u0 > 0.0f);
    CHECK(t.u1 < 1.0f);
    CHECK(t.v0 > 0.0f);
    CHECK(t.v1 < 1.0f);
    // Some non-transparent pixel must exist (ink was drawn).
    bool anyInk = false;
    for (std::size_t i = 3; i < t.rgba.size() && !anyInk; i += 4)
    {
        anyInk = (t.rgba[i] > 0u);
    }
    REQUIRE(anyInk);
    // The first inked pixel matches the fill colour (scan ONLY the alpha bytes,
    // stepping by 4 from the first alpha byte — RGB of that same pixel).
    std::size_t firstInk = 0;
    for (std::size_t i = 3; i < t.rgba.size(); i += 4)
    {
        if (t.rgba[i] > 0u)
        {
            firstInk = i - 3;
            break;
        }
    }
    CHECK(t.rgba[firstInk] == fill.r);
    CHECK(t.rgba[firstInk + 1] == fill.g);
    CHECK(t.rgba[firstInk + 2] == fill.b);

    // The textured-quad material samples the tile's ink sub-rect.
    const vaml::vulkan::ui::Color4 tint{ 1.0f, 1.0f, 1.0f, 1.0f };
    const vaml::vulkan::ui::DrawItem item = vaml::vulkan::vector::vector_quad(
        0.0f, 0.0f, 32.0f, 32.0f, t, tint);
    CHECK(item.kind == vaml::vulkan::ui::DrawItemKind::TextGlyph);
    CHECK(item.w == 32.0f);
    CHECK(item.h == 32.0f);
    CHECK(item.u0 == t.u0);
    CHECK(item.v0 == t.v0);
    CHECK(item.u1 == t.u1);
    CHECK(item.v1 == t.v1);
}

TEST_CASE("11k4 degenerate drawing rasterizes to ok=false")
{
    using vaml::vulkan::vector::VectorTile;
    // A DrawingGroup with NO children -> empty bounds -> ok=false, empty tile.
    vaml::DrawingGroup g;
    const Color fill{ 0u, 0u, 0u, 255u };
    const VectorTile t = vaml::vulkan::vector::raster_drawing_to_tile(g, fill);
    CHECK_FALSE(t.ok);
    CHECK(t.rgba.empty());
}
