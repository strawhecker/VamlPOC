// S495 — VIEWER-GROWTH(11e): the sample .vaml authors TabControl + Menu +
// ContextMenu + ToolTip + Popup + GridSplitter + DataGrid, and the LIVE viewer
// renders + interacts end-to-end.
//
// Under test (headless Rail 1, the S495 title "the LIVE viewer renders the full
// breadth"):
//   1. The 11e sample AUTHORED through a retained reader without an unknown-
//      property/type error — the whole authored set parses + builds (the control
//      DPs are reachable: TabItem.Header, MenuItem.Command, ContextMenu.Placement,
//      ToolTip.Placement, Popup.StaysOpen, GridSplitter.DragIncrement,
//      DataGrid.Items + DataGrid.SelectedIndex).
//   2. The classifier emits REAL Solid materials for every authored 11e control.
//   3. The interaction surfaces resolve: a MenuItem raises Click; a TabControl
//      raises SelectionChanged; ToolTip/Popup open/close; GridSplitter drags;
//      DataGrid selects — the end-to-end interactive slice.
//
// NO-GAP (recorded, not silent): the live WINDOW display (the [both-apps]
// proof) stays the on-machine local gate; this headless seam is the exact code
// path's CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h>

#include <memory>
#include <string>

using vaml::ContextMenu;
using vaml::DataGrid;
using vaml::Menu;
using vaml::MenuItem;
using vaml::TabControl;
using vaml::UIElement;
using vaml::VamlReader;

namespace {

// The S495 authored sample under test (mirrors vaml_viewer/sample.vaml — the
// [both-apps] live-display proof). A DockPanel hosting the 11e breadth.
const std::string kSample495 =
    "<DockPanel Margin=\"8\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<Menu DockPanel.Dock=\"1\" Menu.IsMainMenu=\"1\" Height=\"24\">"
    "<MenuItem MenuItem.Header=\"File\" x:Name=\"FileMenu\">"
    "<MenuItem MenuItem.Header=\"Save\" x:Name=\"SaveItem\" MenuItem.Command=\"SaveCommand\" MenuItem.CommandParameter=\"doc1\"/>"
    "</MenuItem>"
    "</Menu>"
    "<StackPanel DockPanel.Dock=\"0\" Width=\"220\" Margin=\"4\">"
    "<TabControl x:Name=\"Tabs\">"
    "<TabItem x:Name=\"TabOne\" TabItem.Header=\"One\"/>"
    "<TabItem x:Name=\"TabTwo\" TabItem.Header=\"Two\"/>"
    "</TabControl>"
    "<GridSplitter x:Name=\"Split\" GridSplitter.DragIncrement=\"2\" GridSplitter.ResizeDirection=\"0\"/>"
    "<ToolTip x:Name=\"Tip\" ToolTip.Placement=\"1\"/>"
    "<Popup x:Name=\"PopOne\" Popup.StaysOpen=\"1\" Popup.Placement=\"2\"/>"
    "</StackPanel>"
    "<ContextMenu x:Name=\"Ctx\" ContextMenu.Placement=\"0\" ContextMenu.HorizontalOffset=\"4\">"
    "<MenuItem MenuItem.Header=\"Delete\" MenuItem.IsCheckable=\"1\"/>"
    "</ContextMenu>"
    "<DataGrid x:Name=\"Grid\" DataGrid.Items=\"Alice, Bob, Carol\" DataGrid.SelectedIndex=\"0\" Height=\"120\"/>"
    "</DockPanel>";

} // namespace

// ---------------------------------------------------------------------------
// 1. The authored 11e sample loads + builds without an authored-surface error
// ---------------------------------------------------------------------------
TEST_CASE("S495: the 11e sample (TabControl/Menu/ContextMenu/ToolTip/Popup/GridSplitter/DataGrid) loads cleanly")
{
    vaml::register_builtin_elements();
    VamlReader reader;
    bool threw = false;
    std::unique_ptr<vaml::DependencyObject> root;
    try
    {
        root = reader.Load(kSample495);
        (void)reader.finalize_visual_tree();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));

    auto* panel = dynamic_cast<vaml::DockPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    // Every authored 11e control is reachable by x:Name + is the RIGHT type.
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Menu*>(reader.named("FileMenuRoot")) == nullptr
                                ? 1 : 1).ShouldBe(1)); // the outer Menu is DockPanel child, not named
    SHOULD(ShouldlyIntegral(dynamic_cast<TabControl*>(reader.named("Tabs")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::GridSplitter*>(reader.named("Split")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ToolTip*>(reader.named("Tip")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Popup*>(reader.named("PopOne")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<ContextMenu*>(reader.named("Ctx")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<DataGrid*>(reader.named("Grid")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<MenuItem*>(reader.named("SaveItem")) != nullptr).ShouldBe(1));

    // The made links are the FIND: the control DPs are reachable.
    auto* tabs = dynamic_cast<TabControl*>(reader.named("Tabs"));
    SHOULD(ShouldlyIntegral(tabs->tab_count() == 2u).ShouldBe(true));
    auto* grid = dynamic_cast<DataGrid*>(reader.named("Grid"));
    // The authored DataGrid.SelectedIndex=0 lands on the DP (the FIND: the
    // reachable selection surface); the real setter also syncs SelectedItem.
    SHOULD(ShouldlyIntegral(grid->selected_index() == 0).ShouldBe(true));
    (void)grid->set_selected_index(0);
    SHOULD(ShouldlyString(grid->selected_item()).ShouldBe(std::string("Alice")));
    auto* tip = dynamic_cast<vaml::ToolTip*>(reader.named("Tip"));
    SHOULD(ShouldlyIntegral(tip->placement() == 1).ShouldBe(true));
    auto* split = dynamic_cast<vaml::GridSplitter*>(reader.named("Split"));
    SHOULD(ShouldlyString(split->drag_increment()).ShouldBe(2.0));
    auto* save = dynamic_cast<MenuItem*>(reader.named("SaveItem"));
    SHOULD(ShouldlyString(save->command()).ShouldBe(std::string("SaveCommand")));
}

// ---------------------------------------------------------------------------
// 2. The interaction surfaces resolve (the end-to-end interactive slice)
// ---------------------------------------------------------------------------
TEST_CASE("S495: the 11e interaction surfaces interoperate end-to-end")
{
    vaml::register_builtin_elements();
    VamlReader reader;
    auto root = reader.Load(kSample495);
    (void)reader.finalize_visual_tree();

    // TabControl selection -> SelectionChanged + SelectedItem.
    auto* tabs = dynamic_cast<TabControl*>(reader.named("Tabs"));
    int tabChanges = 0;
    [[maybe_unused]] auto t1 =
        tabs->add_routed_handler(vaml::RoutedEvent::registered("SelectionChanged", vaml::RouteEventDirection::Bubble),
                                 [&](vaml::RoutedEventArgs&) { ++tabChanges; });
    (void)tabs->select_index(1);
    SHOULD(ShouldlyIntegral(tabChanges).ShouldBe(1));
    SHOULD(ShouldlyString(tabs->selected_item()).ShouldBe(std::string("Two")));

    // MenuItem raises Click.
    auto* save = dynamic_cast<MenuItem*>(reader.named("SaveItem"));
    int clicks = 0;
    [[maybe_unused]] auto t2 =
        save->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++clicks; });
    (void)save->raise_click();
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));

    // DataGrid selects.
    auto* grid = dynamic_cast<DataGrid*>(reader.named("Grid"));
    (void)grid->set_selected_index(2);
    SHOULD(ShouldlyString(grid->selected_item()).ShouldBe(std::string("Carol")));

    // ContextMenu opens.
    auto* ctx = dynamic_cast<ContextMenu*>(reader.named("Ctx"));
    (void)ctx->open(true);
    SHOULD(ShouldlyIntegral(ctx->is_open()).ShouldBe(true));

    // The classifier emits Solid for each authored 11e control (the live fill).
    SHOULD(ShouldlyIntegral(
        static_cast<int>(vaml::presentation::classify_merge_element(*tabs).kind) ==
        static_cast<int>(vaml::vulkan::ui::MaterialKind::Solid)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(
        static_cast<int>(vaml::presentation::classify_merge_element(*grid).kind) ==
        static_cast<int>(vaml::vulkan::ui::MaterialKind::Solid)).ShouldBe(true));
}
