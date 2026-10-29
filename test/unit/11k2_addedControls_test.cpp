// 11k-2 — the missing WPF controls the corpus consumes that the roadmap never
// authored (dep: ScrollViewer S405, the toolkit's NumericUpDown/NumUpDown +
// ColorPickerSwatch + the app's ToolbarControl basis):
//
//   RepeatButton — the toolkit NumericUpDown/NumUpDown spin-button base
//                  (ButtonBase-faithful Click + Delay/Interval + IsPressed).
//   ScrollBar    — the scrollbar chrome ScrollViewer S405 drives
//                  (RangeBase-faithful Value/Min/Max + Orientation +
//                  ViewportSize + the bubbling "Scroll" routed event).
//   Viewbox      — the scale-contain single-child container (ColorPickerSwatch)
//                  (Decorator-faithful Stretch/StretchDirection + content
//                  child); classifies to NONE — "Viewbox -> its content".
//   ToolBar      — the app's ToolbarControl basis (ItemsControl-faithful
//                  Orientation/Band/BandIndex/IsOverflowOpen/overflow surface).
//   ToolBarTray  — the ToolBar host (Orientation/IsLocked).
//
// Each is a REAL UIElement re-base registered as a built-in + classified
// (RepeatButton/ScrollBar/ToolBar/ToolBarTray -> Solid; Viewbox -> None, the
// content child carries the material). The full ButtonBase/RangeBase/
// ItemsControl/Control abstract-base re-bind is 11k-11's S649 step (recorded
// NO-GAP not silent); the repeat TIMER loop + the full scrollbar thumb chrome
// are the S649 continuation (recorded NO-GAP not silent).
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/VamlReader.h>         // VamlReader::Load (the corpus entry)
#include <vaml/core/Element.h>            // the new controls + register_builtin_elements
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/UIElement.h>          // UIElement, Size, Rect, desired_size
#include <vaml/core/Geometry.h>           // Size / Rect
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element (vertical slice)
#include <vaml/vulkan/ui/UiScene.h>       // MaterialKind / ElementVisual

using vaml::RepeatButton;
using vaml::ScrollBar;
using vaml::ToolBar;
using vaml::ToolBarTray;
using vaml::Viewbox;
using vaml::Orientation;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

// ---------------------------------------------------------------------------
// (A) REAL UIElement re-base + registered built-ins
// ---------------------------------------------------------------------------
TEST_CASE("11k-2: each new control is a REAL UIElement + a registered built-in")
{
    register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    SHOULD(ShouldlyIntegral(g.is_registered("ScrollBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("RepeatButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Viewbox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ToolBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ToolBarTray")).ShouldBe(true));

    // Each resolves to a concrete re-based UIElement (not a generic blob).
    {
        auto e = g.create("ScrollBar");
        SHOULD(ShouldlyIntegral(dynamic_cast<ScrollBar*>(e.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(true));
    }
    {
        auto e = g.create("RepeatButton");
        SHOULD(ShouldlyIntegral(dynamic_cast<RepeatButton*>(e.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(true));
    }
    {
        auto e = g.create("Viewbox");
        SHOULD(ShouldlyIntegral(dynamic_cast<Viewbox*>(e.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(true));
    }
    {
        auto e = g.create("ToolBar");
        SHOULD(ShouldlyIntegral(dynamic_cast<ToolBar*>(e.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(true));
    }
    {
        auto e = g.create("ToolBarTray");
        SHOULD(ShouldlyIntegral(dynamic_cast<ToolBarTray*>(e.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// (B) DP round-trip + the routing events
// ---------------------------------------------------------------------------
TEST_CASE("11k-2: ScrollBar DP round-trip + value-clamp + the 'Scroll' routed event")
{
    register_builtin_elements();
    ScrollBar sb;
    (void)sb.set_minimum(0.0);
    (void)sb.set_maximum(100.0);
    (void)sb.set_value(50.0);
    SHOULD(ShouldlyString(sb.value()).ShouldBe(50.0));

    // A value outside the range CLAMPS (never escapes the range).
    (void)sb.set_value(1000.0);
    SHOULD(ShouldlyString(sb.value()).ShouldBe(100.0));
    (void)sb.set_value(-50.0);
    SHOULD(ShouldlyString(sb.value()).ShouldBe(0.0));

    // Orientation (default Vertical) + ViewportSize round-trip.
    SHOULD(ShouldlyIntegral(sb.orientation() == Orientation::Vertical).ShouldBe(true));
    (void)sb.set_orientation(Orientation::Horizontal);
    SHOULD(ShouldlyIntegral(sb.orientation() == Orientation::Horizontal).ShouldBe(true));
    (void)sb.set_viewport_size(25.0);
    SHOULD(ShouldlyString(sb.viewport_size()).ShouldBe(25.0));

    // The bubbling "Scroll" routed event fires on a REAL value change.
    int fired = 0;
    [[maybe_unused]] auto token =
        sb.add_routed_handler(vaml::RoutedEvent::registered("Scroll", vaml::RouteEventDirection::Bubble),
                             [&](vaml::RoutedEventArgs&) { ++fired; });
    (void)sb.set_value(30.0); // a real change -> 1
    (void)sb.set_value(30.0); // no change -> 0 more
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));
}

TEST_CASE("11k-2: RepeatButton Click routed event + Delay/Interval/IsPressed round-trip")
{
    register_builtin_elements();
    RepeatButton rbtn;

    // Delay / Interval / IsPressed DPs round-trip.
    (void)rbtn.set_delay(250);
    (void)rbtn.set_interval(100);
    (void)rbtn.set_is_pressed(true);
    SHOULD(ShouldlyIntegral(rbtn.delay()).ShouldBe(250));
    SHOULD(ShouldlyIntegral(rbtn.interval()).ShouldBe(100));
    SHOULD(ShouldlyIntegral(static_cast<int>(rbtn.is_pressed())).ShouldBe(1));

    // The Click routed event (the S320 ClickEvent identity) fires on raise_click.
    int clicked = 0;
    [[maybe_unused]] auto ctoken =
        rbtn.add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++clicked; });
    (void)rbtn.raise_click();
    SHOULD(ShouldlyIntegral(clicked).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (C) Viewbox — the single-content child measures + classifies to NONE
// ---------------------------------------------------------------------------
TEST_CASE("11k-2: Viewbox measures its single content child + classifies to None (its content)")
{
    register_builtin_elements();
    Viewbox vb;
    (void)vb.set_stretch(1);   // IconStretch::Fill
    (void)vb.set_stretch_direction(0);
    SHOULD(ShouldlyIntegral(vb.stretch()).ShouldBe(1));

    // A content child (a TextBlock, its own material).
    auto content = std::make_unique<vaml::TextBlock>();
    (void)content->set_text("swatch");
    vaml::UIElement* contentRaw = content.get();
    vb.set_content(std::move(content));
    SHOULD(ShouldlyIntegral(vb.content() == contentRaw).ShouldBe(true));

    // The Viewbox measures to its content child's desired extent.
    vb.Measure(vaml::Size{ 300.0f, vaml::Size::Infinite().height });
    SHOULD(ShouldlyString(vb.desired_size().width).ShouldBe(contentRaw->desired_size().width));
    vb.Arrange(vaml::Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // classify -> MaterialKind::None ("Viewbox -> its content"): the Viewbox
    // contributes NO box; the content child (a TextBlock) carries the material.
    // (Without the explicit Viewbox arm, the non-Text fallback would have
    // emitted a Solid box FOR the Viewbox — the arm proves otherwise.)
    const vaml::vulkan::ui::ElementVisual ev = classify_merge_element(vb);
    SHOULD(ShouldlyIntegral(ev.kind == MaterialKind::None).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (D) ToolBar + ToolBarTray — strip layout + DPs
// ---------------------------------------------------------------------------
TEST_CASE("11k-2: ToolBar lays its children along the orientation + its DPs round-trip")
{
    register_builtin_elements();
    ToolBar tb;

    // Orientation default Horizontal + the ToolBar DPs round-trip.
    SHOULD(ShouldlyIntegral(tb.orientation() == Orientation::Horizontal).ShouldBe(true));
    (void)tb.set_band(1);
    (void)tb.set_band_index(2);
    (void)tb.set_is_overflow_open(true);
    (void)tb.set_has_overflow_items(true);
    SHOULD(ShouldlyIntegral(tb.band()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(tb.band_index()).ShouldBe(2));
    SHOULD(ShouldlyIntegral(static_cast<int>(tb.is_overflow_open())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(tb.has_overflow_items())).ShouldBe(1));

    // Children stack along the horizontal strip.
    vaml::TextBlock* a = tb.create_child<vaml::TextBlock>();
    vaml::TextBlock* b = tb.create_child<vaml::TextBlock>();
    (void)a; (void)b;
    tb.Measure(vaml::Size{ 200.0f, vaml::Size::Infinite().height });
    tb.Arrange(vaml::Rect{ 0.0f, 0.0f, 200.0f, 30.0f });

    // The second child is placed AFTER the first along the strip.
    const bool after = (b->final_rect().x >= a->final_rect().x);
    SHOULD(ShouldlyIntegral(static_cast<int>(after)).ShouldBe(1));
}

TEST_CASE("11k-2: ToolBarTray lays its ToolBar children along the tray orientation")
{
    register_builtin_elements();
    ToolBarTray tray;

    SHOULD(ShouldlyIntegral(tray.orientation() == Orientation::Horizontal).ShouldBe(true));
    (void)tray.set_is_locked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(tray.is_locked())).ShouldBe(1));

    ToolBar* t1 = tray.create_child<ToolBar>();
    ToolBar* t2 = tray.create_child<ToolBar>();
    (void)t1; (void)t2;
    tray.Measure(vaml::Size{ 300.0f, vaml::Size::Infinite().height });
    tray.Arrange(vaml::Rect{ 0.0f, 0.0f, 300.0f, 40.0f });

    const bool after = (t2->final_rect().x >= t1->final_rect().x);
    SHOULD(ShouldlyIntegral(static_cast<int>(after)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (E) corpus-shaped load + classify for the new controls
// ---------------------------------------------------------------------------
TEST_CASE("11k-2: a corpus-shaped doc authoring all five controls loads + classifies")
{
    register_builtin_elements();
    vaml::VamlReader reader;

    // A corpus-shaped doc authoring the missing controls (no unknown-type/
    // unknown-prop throw).
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(
        "<ToolBarTray Orientation=\"Horizontal\">"
        "  <ToolBar Band=\"0\"><RepeatButton Delay=\"100\" Interval=\"50\"/></ToolBar>"
        "  <ToolBar><Viewbox Stretch=\"1\"><ScrollBar Minimum=\"0\" Maximum=\"10\" Value=\"5\"/></Viewbox></ToolBar>"
        "</ToolBarTray>");

    REQUIRE(root != nullptr);
    SHOULD(ShouldlyIntegral(dynamic_cast<ToolBarTray*>(root.get()) != nullptr).ShouldBe(true));

    // Hand every declared child into its parent's S121 visual-children list (the
    // S440 finalize transfer) so the authored subtree is reachable via child_at.
    (void)reader.finalize_visual_tree();

    // The classified materials: ToolBarTray/ToolBar/RepeatButton/ScrollBar ->
    // Solid; Viewbox -> None (its content renders).
    ToolBarTray* tray = dynamic_cast<ToolBarTray*>(root.get());
    tray->Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    tray->Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 40.0f });

    const vaml::vulkan::ui::ElementVisual trayEv = classify_merge_element(*tray);
    SHOULD(ShouldlyIntegral(trayEv.kind == MaterialKind::Solid).ShouldBe(true));

    ToolBar* tb = tray->child_at(0) ? dynamic_cast<ToolBar*>(tray->child_at(0)) : nullptr;
    ToolBar* tb2 = tray->child_at(1) ? dynamic_cast<ToolBar*>(tray->child_at(1)) : nullptr;
    REQUIRE(tb != nullptr);
    REQUIRE(tb2 != nullptr);

    const vaml::vulkan::ui::ElementVisual tbEv = classify_merge_element(*tb);
    const vaml::vulkan::ui::ElementVisual tb2Ev = classify_merge_element(*tb2);
    SHOULD(ShouldlyIntegral(tbEv.kind == MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tb2Ev.kind == MaterialKind::Solid).ShouldBe(true));

    // The first ToolBar's RepeatButton child classifies Solid.
    RepeatButton* rbtn = tb->child_at(0) ? dynamic_cast<RepeatButton*>(tb->child_at(0)) : nullptr;
    REQUIRE(rbtn != nullptr);
    const vaml::vulkan::ui::ElementVisual rbEv = classify_merge_element(*rbtn);
    SHOULD(ShouldlyIntegral(rbEv.kind == MaterialKind::Solid).ShouldBe(true));

    // The Viewbox (inside the second ToolBar) classifies None — its content (a
    // ScrollBar) carries the material (Solid).
    Viewbox* vb = tb2->child_at(0) ? dynamic_cast<Viewbox*>(tb2->child_at(0)) : nullptr;
    REQUIRE(vb != nullptr);
    const vaml::vulkan::ui::ElementVisual vbEv = classify_merge_element(*vb);
    SHOULD(ShouldlyIntegral(vbEv.kind == MaterialKind::None).ShouldBe(true));

    // The Viewbox's declared child is its single-content child (the reader
    // finalizes it into the S121 visual-children list; the ScrollBar is the
    // Viewbox's content).
    ScrollBar* sb = vb->child_at(0) ? dynamic_cast<ScrollBar*>(vb->child_at(0)) : nullptr;
    REQUIRE(sb != nullptr);
    SHOULD(ShouldlyString(sb->value()).ShouldBe(5.0));
    const vaml::vulkan::ui::ElementVisual sbEv = classify_merge_element(*sb);
    SHOULD(ShouldlyIntegral(sbEv.kind == MaterialKind::Solid).ShouldBe(true));
}
