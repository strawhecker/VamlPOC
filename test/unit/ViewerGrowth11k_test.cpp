// 11k-6 (dep:S11k1+2+3+4+5) — VIEWER-GROWTH(11k): a corpus-shaped
// `sample.corpus.xaml` authors TextBlock + the 11k-2 added controls + a vector
// resource + an EventTrigger, and the LIVE viewer loads + renders it. Headless
// Rail 1 (the [both-apps, not CI] window display stays the on-machine local
// gate — this is the CI-checkable half of the exact code path).
//
// Under test (vaml-presentation-tests — it links vaml-presentation so the S212
// encode_visual_slice render-plan is provable):
//   1. LOAD — the corpus sample loads + finalizes through a RETAINED reader
//      without an unknown-property/type error; every authored 11k control is
//      reachable by x:Name (Title/Spin/Thumb/Zoom/Bar/Tray). The TextBlock
//      inline content lands on the Text DP (11k-1).
//   2. VECTOR — the authored DrawingBrush (arrowBrush, the 11k-4 DrawingGroup ->
//      GeometryDrawing -> PathGeometry graph) RESOLVES through the S360 three-
//      scope walk + RASTERS via the vaml::vulkan::vector S202 atlas-mesh seam to
//      an S206 textured-quad (ink colour + ink-box UV). vectorOk + vectorDraw.
//   3. EVENTTRIGGER — the element-level <Control.Triggers><EventTrigger
//      RoutedEvent="Loaded" SourceName="Root"/> parses as TriggerKind::Event
//      with the eventName + sourceName carried (the 11k-5/S362 record).
//   4. RENDER-PLAN — the S212 encode_visual_slice emits the TextBlock glyph
//      (TextGlyph) + the 11k-2 control solids (SolidRect) for the corpus set.
//
// NO-GAP (recorded, not silent): the LIVE WINDOW display of the corpus sample
// (the [both-apps] proof) stays the on-machine local gate; this headless seam
// is the exact code path's CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"            // TextBlock/RepeatButton/ScrollBar/Viewbox/ToolBar/ToolBarTray + register
#include "vaml/core/Style.h"              // Trigger / TriggerKind / trigger_kind
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem / DrawItemKind
#include "viewer_growth_11k.h"            // the 11k-6 ADR-021 consumer seam

#include <memory>
#include <string>
#include <vector>

using vaml::RepeatButton;
using vaml::ScrollBar;
using vaml::TextBlock;
using vaml::ToolBar;
using vaml::ToolBarTray;
using vaml::TriggerKind;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::Viewbox;
using vaml::trigger_kind;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11k_plan;
using vaml::viewer::viewer_growth_11k_render_plan;

// The AUTHeored sample under test — mirrors vaml_viewer/sample.corpus.xaml (the
// [both-apps] live-display proof). It is kept as a verbatim inline copy so the
// headless test exercises EXACTLY the bytes the LIVE viewer loads (no drift).
const std::string kSampleCorpus =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"440\" Height=\"360\" Margin=\"6\">"
    "<StackPanel.Resources>"
    "<ResourceDictionary>"
    "<PathGeometry x:Key=\"arrowGlyph\" Figures=\"M0,0 L28,14 L0,28 Z\" FillRule=\"1\"/>"
    "<DrawingBrush x:Key=\"arrowBrush\">"
    "<DrawingBrush.Drawing>"
    "<DrawingGroup>"
    "<GeometryDrawing>"
    "<GeometryDrawing.Geometry>"
    "<PathGeometry Figures=\"M0,0 L32,0 L32,32 Z\"/>"
    "</GeometryDrawing.Geometry>"
    "</GeometryDrawing>"
    "</DrawingGroup>"
    "</DrawingBrush.Drawing>"
    "</DrawingBrush>"
    "</ResourceDictionary>"
    "</StackPanel.Resources>"
    "<Control.Triggers>"
    "<EventTrigger RoutedEvent=\"Loaded\" SourceName=\"Root\"/>"
    "</Control.Triggers>"
    "<TextBlock x:Name=\"Title\" FontSize=\"20\" Margin=\"4\">"
    "VAML 11k corpus: the text glyph renders"
    "</TextBlock>"
    "<ToolBarTray x:Name=\"Tray\" Orientation=\"0\" Margin=\"2\">"
    "<ToolBar x:Name=\"Bar\" Band=\"0\">"
    "<RepeatButton x:Name=\"Spin\" Delay=\"250\" Interval=\"80\" Margin=\"2\"/>"
    "</ToolBar>"
    "</ToolBarTray>"
    "<StackPanel Orientation=\"0\" Margin=\"2\">"
    "<Viewbox x:Name=\"Zoom\" Stretch=\"1\">"
    "<ScrollBar x:Name=\"Thumb\" Minimum=\"0\" Maximum=\"100\" Value=\"40\" "
    "ViewportSize=\"20\" Width=\"180\" Height=\"18\"/>"
    "</Viewbox>"
    "</StackPanel>"
    "<TextBlock x:Name=\"VectorNote\" Text=\"Authored vector resource below (S202 atlas mesh)\" "
    "FontSize=\"12\" Margin=\"4\"/>"
    "<TextBlock x:Name=\"Caption\" Text=\"11k-6 — VIEWER-GROWTH(11k): corpus-shaped render\" "
    "FontSize=\"14\" Margin=\"4\"/>"
    "</StackPanel>";

namespace {

int count_kind(const std::vector<vaml::vulkan::ui::DrawItem>& items, DrawItemKind kind)
{
    int n = 0;
    for (const auto& it : items)
    {
        if (it.kind == kind)
        {
            ++n;
        }
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. LOAD — the corpus sample loads + finalizes; every 11k control reachable.
// ---------------------------------------------------------------------------

TEST_CASE("11k-6: the corpus sample (TextBlock + 11k-2 controls + vector + EventTrigger) loads cleanly")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11k_plan(kSampleCorpus);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.reader != nullptr ? 1 : 0).ShouldBe(1));

    // The TextBlock inline content landed on the Text DP (the WPF IAddChild
    // content routing authored at 11k-1).
    auto* title = dynamic_cast<TextBlock*>(r.reader->named("Title"));
    SHOULD(ShouldlyIntegral(title != nullptr ? 1 : 0).ShouldBe(1));
    if (title)
    {
        SHOULD(ShouldlyString(title->text())
                   .ShouldBe(std::string("VAML 11k corpus: the text glyph renders")));
    }

    // Every 11k-2 control is reachable by x:Name (the WPF-missing corpus set).
    SHOULD(ShouldlyIntegral(dynamic_cast<ToolBarTray*>(r.reader->named("Tray")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<ToolBar*>(r.reader->named("Bar")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<RepeatButton*>(r.reader->named("Spin")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Viewbox*>(r.reader->named("Zoom")) != nullptr).ShouldBe(1));
    auto* thumb = dynamic_cast<ScrollBar*>(r.reader->named("Thumb"));
    SHOULD(ShouldlyIntegral(thumb != nullptr ? 1 : 0).ShouldBe(1));
    if (thumb)
    {
        // ScrollBar DP round-trip reached through the authored attributes.
        SHOULD(ShouldlyString(thumb->value()).ShouldBe(40.0));
    }
}

// ---------------------------------------------------------------------------
// 2. VECTOR — the authored DrawingBrush resolves + rasters via the S202 mesh.
// ---------------------------------------------------------------------------

TEST_CASE("11k-6: the authored vector resource resolves + rasters to an S206 textured-quad")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11k_plan(kSampleCorpus);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // The DrawingBrush (arrowBrush) resolved through the S360 three-scope walk +
    // rasterized via the vaml::vulkan::vector seam -> vectorOk + one quad.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.vectorOk)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.vectorDraw.size() >= 1u ? 1 : 0).ShouldBe(1));

    // The emitted S206 textured-quad samples the tile's ink-box UVs (a real
    // TextGlyph-kind quad with a non-default UV sub-rect — the "drawn via the
    // S202 atlas mesh" proof).
    bool sawVectorQuad = false;
    for (const auto& it : r.vectorDraw)
    {
        if (it.kind == DrawItemKind::TextGlyph && it.w > 0.0f && it.h > 0.0f)
        {
            sawVectorQuad = true;
            SHOULD(ShouldlyIntegral(it.u0 != 0.0f || it.v0 != 0.0f).ShouldBe(true));
            SHOULD(ShouldlyIntegral(it.u1 != 1.0f || it.v1 != 1.0f).ShouldBe(true));
        }
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(sawVectorQuad)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. EVENTTRIGGER — the element-level EventTrigger parses as kind Event.
// ---------------------------------------------------------------------------

TEST_CASE("11k-6: the authored <EventTrigger RoutedEvent=Loaded SourceName=Root> parses as kind Event")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11k_plan(kSampleCorpus);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // The <Control.Triggers> EventTrigger is a per-Load element-trigger record
    // (11j-7/11k-5): one trigger, kind Event, eventName + sourceName carried.
    const auto& et = r.reader->element_triggers();
    SHOULD(ShouldlyIntegral(et.size() >= 1u).ShouldBe(1));
    std::size_t foundEvent = 0;
    for (const auto& [el, trigs] : et)
    {
        (void)el;
        for (const auto& t : trigs)
        {
            if (trigger_kind(t) == TriggerKind::Event)
            {
                ++foundEvent;
                SHOULD(ShouldlyString(std::string(t.event.eventName)).ShouldBe(std::string("Loaded")));
                SHOULD(ShouldlyString(std::string(t.event.sourceName)).ShouldBe(std::string("Root")));
            }
        }
    }
    SHOULD(ShouldlyIntegral(foundEvent == 1u).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. RENDER-PLAN — the S212 encode_visual_slice emits the text glyph + solids.
// ---------------------------------------------------------------------------

TEST_CASE("11k-6: the render-plan emits the TextBlock glyph + the 11k-2 control solids")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11k_plan(kSampleCorpus);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t n = viewer_growth_11k_render_plan(r.root.get(), items, 440.0f, 360.0f);
    // The corpus tree emits real draws (the text glyph + the control solids).
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));
    // TextBlock -> TextGlyph (the SDF glyph-quad material, 11k-1/S400).
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));
    // RepeatButton/ScrollBar/ToolBar/ToolBarTray -> Solid (the 11k-2 classifier).
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
}
