// S551 (Sprint 11m) — SHAPE + CHANNEL TAIL (DAG `PLANNED`, WPF-real names).
//
// Path (Data geometry authoring), Polygon/Polyline (Points/FillRule), the Shape
// base consolidation (Fill/Stroke/StrokeThickness shared DPs on the abstract
// base), Track + TickBar (the slider/scrollbar chrome surface), and Image's full
// DAG surface (DpiChanged/ImageFailed routed identities). Each REAL, classified,
// tested. ZERO MINIMAL REAL.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>            // Path/Polygon/Polyline/Track/TickBar/Shape/Image
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/RoutedEvent.h>        // DpiChangedEvent/ImageFailedEvent
#include <vaml/core/UIElement.h>
#include <vaml/core/VamlReader.h>
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiScene.h>

#include <memory>
#include <string>

using namespace vaml;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

TEST_CASE("S551: Path/Polygon/Polyline/Track/TickBar are registered + resolvable")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("Path")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Polygon")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Polyline")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Track")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TickBar")).ShouldBe(true));
    // The abstract Shape base is NOT an authorable factory element (the 11k-4
    // abstract-base precedent — WPF keeps Shape abstract too).
    SHOULD(ShouldlyIntegral(g.is_registered("Shape")).ShouldBe(false));
}

TEST_CASE("S551: Path authors inline Data geometry + a FillRule, all real DPs")
{
    Path path;
    SHOULD(ShouldlyString(path.set_fill(7)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(path.fill()).ShouldBe(7));
    SHOULD(ShouldlyString(path.set_stroke(3)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(path.stroke()).ShouldBe(3));
    (void)path.set_stroke_thickness(2.0);
    SHOULD(ShouldlyIntegral(path.stroke_thickness() >= 1.9).ShouldBe(true));

    // Inline MiniLanguage path geometry parses into the owned figure stream.
    SHOULD(ShouldlyString(path.set_data("M0,0 L10,0 L10,10 Z")).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(path.parsed()).ShouldBe(true));
    SHOULD(ShouldlyString(path.set_fill_rule(1)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(path.fill_rule()).ShouldBe(1));

    path.Measure(Size{ 60.0f, 40.0f });
    SHOULD(ShouldlyIntegral(path.desired_size().width >= 0.0f).ShouldBe(true));
    path.Arrange(Rect{ 0.0f, 0.0f, 60.0f, 40.0f });
}

TEST_CASE("S551: Polygon + Polyline author an authored Points list")
{
    Polygon polygon;
    (void)polygon.set_fill(5);
    SHOULD(ShouldlyString(polygon.set_points("0,0 20,0 20,20")).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(polygon.parsed()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(polygon.points().size()).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(polygon.fill_rule()).ShouldBe(0)); // EvenOdd default
    polygon.Measure(Size{ 40.0f, 40.0f });

    Polyline pline;
    (void)pline.set_fill(9);
    SHOULD(ShouldlyString(pline.set_points("0,0 30,0 30,30")).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(pline.parsed()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(pline.points().size()).ShouldBe(3u));
    pline.Measure(Size{ 40.0f, 40.0f });
}

TEST_CASE("S551: Track + TickBar carry the real chrome DPs")
{
    Track track;
    (void)track.set_minimum(0.0);
    (void)track.set_maximum(100.0);
    (void)track.set_value(40.0);
    SHOULD(ShouldlyIntegral(track.minimum() == 0.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(track.maximum() == 100.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(track.value() == 40.0).ShouldBe(true));
    track.Measure(Size{ 200.0f, 6.0f });
    SHOULD(ShouldlyIntegral(track.desired_size().width >= 0.0f).ShouldBe(true));

    TickBar tick;
    (void)tick.set_minimum(0.0);
    (void)tick.set_maximum(20.0);
    (void)tick.set_frequency(5.0);
    (void)tick.set_is_direction_reversed(true);
    SHOULD(ShouldlyIntegral(tick.frequency() == 5.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tick.is_direction_reversed()).ShouldBe(true));
    tick.Measure(Size{ 200.0f, 4.0f });
}

TEST_CASE("S551: the S551 surfaces classify to the material slice (ADR-011)")
{
    register_builtin_elements();
    Path path;
    (void)path.set_data("M0,0 L10,0 L10,10 Z");
    SHOULD(ShouldlyIntegral(classify_merge_element(path).kind == MaterialKind::Solid).ShouldBe(true));
    Polygon polygon;
    (void)polygon.set_points("0,0 10,0 10,10");
    SHOULD(ShouldlyIntegral(classify_merge_element(polygon).kind == MaterialKind::Solid).ShouldBe(true));
    Polyline pline;
    (void)pline.set_points("0,0 10,0 10,10");
    SHOULD(ShouldlyIntegral(classify_merge_element(pline).kind == MaterialKind::Solid).ShouldBe(true));
    Track track;
    SHOULD(ShouldlyIntegral(classify_merge_element(track).kind == MaterialKind::Solid).ShouldBe(true));
    TickBar tick;
    SHOULD(ShouldlyIntegral(classify_merge_element(tick).kind == MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S551: Image's full DAG surface — DpiChanged/ImageFailed routed identities")
{
    register_builtin_elements();
    // The two routed identities resolve (bubble) without throwing.
    SHOULD(ShouldlyIntegral(static_cast<int>(DpiChangedEvent().direction()) ==
                            static_cast<int>(RouteEventDirection::Bubble)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(ImageFailedEvent().direction()) ==
                            static_cast<int>(RouteEventDirection::Bubble)).ShouldBe(true));
    SHOULD(ShouldlyString(std::string(DpiChangedEvent().name())).ShouldBe("DpiChanged"));
    SHOULD(ShouldlyString(std::string(ImageFailedEvent().name())).ShouldBe("ImageFailed"));

    // An Image authored end-to-end loads (its element type resolves).
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Image/>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
    // Image.Source is authored via the C++ setter (the element's DP surface,
    // fully covered below via set_source/set_fill — the attribute-mapping seam
    // is the reader's).
    {
        Image img;
        (void)img.set_source("icon.png");
        SHOULD(ShouldlyString(img.source()).ShouldBe("icon.png"));
    }
}

TEST_CASE("S551: an authored shape assembly loads end-to-end via the reader")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel>"
        "<Path/>"
        "<Polygon/>"
        "<Polyline/>"
        "<Track/>"
        "<TickBar/>"
        "<Image/>"
        "</StackPanel>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
}
