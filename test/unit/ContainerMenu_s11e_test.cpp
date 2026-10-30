// Sprint 11e (S490–S494) — the container + menu controls. Each is a REAL
// re-based UIElement registered as a built-in + classified (Solid):
//
//   S490 — TabControl + TabItem (tab strip + content selection + routed
//          SelectionChanged).
//   S491 — Menu + MenuItem + ContextMenu (Header/Command/CommandParameter +
//          open/select + Click routing + Checkable).
//   S492 — ToolTip + Popup (deferred show + placement + the routed
//          Opened/Closed surface).
//   S493 — GridSplitter (drag-resizes adjacent Grid tracks — the S174
//          constant-time dest-rect mapping).
//   S494 — DataGrid (native VAML analog: ItemsSource + the typed DataGrid*
//          Column set + the editing/selection surface; classify -> the
//          list/grid fill).
//
// The full WPF-own-surface + base-bundle DP/event fidelity is the 11k-8 audit
// contract (FIDELITY-CLOSURE); each control here registers its REAL DP surface.
// No ContentControl/ItemsControl/Selector abstract base exists yet (that is
// the S649/11k-11 continuation, recorded NO-GAP not silent). ADR-018 ZERO
// new locks. Presenter/classifier arms Solid in UiSceneEncoder + VamlRenderer.
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // the controls + register_builtin_elements
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/GridLength.h>
#include <vaml/core/RoutedEvent.h>
#include <vaml/core/UIElement.h>
#include <vaml/core/Value.h>
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element (vertical slice)
#include <vaml/vulkan/ui/UiScene.h>         // MaterialKind / ElementVisual

using vaml::ContextMenu;
using vaml::DataGrid;
using vaml::DataGridColumn;
using vaml::GridSplitter;
using vaml::Menu;
using vaml::MenuItem;
using vaml::Popup;
using vaml::TabControl;
using vaml::TabItem;
using vaml::ToolTip;
using vaml::UIElement;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

// ---------------------------------------------------------------------------
// (A) REAL UIElement re-base + registered built-ins (S490–S494)
// ---------------------------------------------------------------------------
TEST_CASE("11e: every container/menu/tooltip/popup/splitter/grid control is a REAL UIElement + registered built-in")
{
    register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    const char* names[] = {"TabControl", "TabItem", "Menu", "MenuItem", "ContextMenu",
                           "ToolTip", "Popup", "GridSplitter", "DataGrid"};
    for (const char* n : names)
    {
        SHOULD(ShouldlyIntegral(g.is_registered(n)).ShouldBe(true));
        auto e = g.create(n);
        // Each resolves to a concrete re-based UIElement (not a generic blob).
        SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(1));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr).ShouldBe(1));
    }
    // Concrete type discrimination.
    SHOULD(ShouldlyIntegral(dynamic_cast<TabControl*>(g.create("TabControl").get()) != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(dynamic_cast<MenuItem*>(g.create("MenuItem").get()) != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(dynamic_cast<DataGrid*>(g.create("DataGrid").get()) != nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (B) S490 — TabControl + TabItem
// ---------------------------------------------------------------------------
TEST_CASE("11e S490: TabControl selects a TabItem + swaps content + raises SelectionChanged")
{
    register_builtin_elements();
    TabControl tc;
    auto t1 = std::make_unique<TabItem>();
    (void)t1->set_header("One");
    auto t2 = std::make_unique<TabItem>();
    (void)t2->set_header("Two");
    tc.add_tab(std::move(t1));
    tc.add_tab(std::move(t2));

    SHOULD(ShouldlyIntegral(tc.tab_count() == 2u).ShouldBe(true));
    // The first tab is auto-selected (the auto-select rule).
    SHOULD(ShouldlyIntegral(tc.selected_index() == 0).ShouldBe(true));
    SHOULD(ShouldlyString(tc.selected_item()).ShouldBe(std::string("One")));

    // SelectionChanged raises on a real change.
    int fired = 0;
    [[maybe_unused]] auto token =
        tc.add_routed_handler(vaml::RoutedEvent::registered("SelectionChanged", vaml::RouteEventDirection::Bubble),
                              [&](vaml::RoutedEventArgs&) { ++fired; });
    (void)tc.select_index(1);
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));
    SHOULD(ShouldlyIntegral(tc.selected_index() == 1).ShouldBe(true));
    SHOULD(ShouldlyString(tc.selected_item()).ShouldBe(std::string("Two")));
    // The selected TabItem's IsSelected reflects the choice.
    SHOULD(ShouldlyIntegral(tc.tab_at(1)->is_selected()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tc.tab_at(0)->is_selected()).ShouldBe(false));

    // TabItem DP round-trip.
    (void)tc.tab_at(0)->set_tab_strip_placement(1);
    SHOULD(ShouldlyIntegral(tc.tab_at(0)->tab_strip_placement() == 1).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (C) S491 — Menu + MenuItem + ContextMenu
// ---------------------------------------------------------------------------
TEST_CASE("11e S491: MenuItem raises Click + Checkable toggles + ContextMenu opens")
{
    register_builtin_elements();
    Menu menu;
    auto file = std::make_unique<MenuItem>();
    (void)file->set_header("File");
    auto save = std::make_unique<MenuItem>();
    (void)save->set_header("Save");
    (void)save->set_command("SaveCommand");
    (void)save->set_command_parameter("doc1");
    file->add_item(std::move(save));
    menu.add_item(std::move(file));
    SHOULD(ShouldlyIntegral(menu.item_count() == 1u).ShouldBe(true));

    MenuItem* fileItem = dynamic_cast<MenuItem*>(menu.child_at(0));
    SHOULD(ShouldlyIntegral(fileItem != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(fileItem->item_count() == 1u).ShouldBe(true));
    MenuItem* saveItem = dynamic_cast<MenuItem*>(fileItem->child_at(0));
    SHOULD(ShouldlyIntegral(saveItem != nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(saveItem->command()).ShouldBe(std::string("SaveCommand")));
    SHOULD(ShouldlyString(saveItem->command_parameter()).ShouldBe(std::string("doc1")));

    // A Click raises the BUBBLING Click event routed to the MenuItem (source).
    int clicks = 0;
    [[maybe_unused]] auto tok =
        saveItem->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++clicks; });
    (void)saveItem->raise_click();
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));

    // Checkable toggles IsChecked on a click.
    auto chk = std::make_unique<MenuItem>();
    (void)chk->set_header("Bold");
    (void)chk->set_is_checkable(true);
    MenuItem* chkRaw = chk.get();
    (void)chkRaw->raise_click();
    SHOULD(ShouldlyIntegral(chkRaw->is_checked()).ShouldBe(true));
    (void)chkRaw->raise_click();
    SHOULD(ShouldlyIntegral(chkRaw->is_checked()).ShouldBe(false));

    // ContextMenu: IsOpen + Placement + PlacementTarget + Opened/Closed.
    ContextMenu cm;
    auto item = std::make_unique<MenuItem>();
    (void)item->set_header("Delete");
    cm.add_item(std::move(item));
    SHOULD(ShouldlyIntegral(cm.item_count() == 1u).ShouldBe(true));
    int opened = 0, closed = 0;
    [[maybe_unused]] auto to = cm.opened().add([&] { ++opened; });
    [[maybe_unused]] auto tC = cm.closed().add([&] { ++closed; });
    SHOULD(ShouldlyIntegral(cm.is_open()).ShouldBe(false));
    (void)cm.open(true);
    SHOULD(ShouldlyIntegral(cm.is_open()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(opened).ShouldBe(1));
    (void)cm.set_placement(0);
    SHOULD(ShouldlyIntegral(cm.placement() == 0).ShouldBe(true));
    (void)cm.open(false);
    SHOULD(ShouldlyIntegral(closed).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (D) S492 — ToolTip + Popup (deferred show + placement)
// ---------------------------------------------------------------------------
TEST_CASE("11e S492: ToolTip/Popup deferred show + Opened/Closed + StaysOpen")
{
    register_builtin_elements();
    ToolTip tip;
    (void)tip.set_placement(1);
    SHOULD(ShouldlyIntegral(tip.placement() == 1).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tip.visible()).ShouldBe(false));
    SHOULD(ShouldlyIntegral(tip.is_open()).ShouldBe(false));
    int opened = 0;
    [[maybe_unused]] auto to = tip.opened().add([&] { ++opened; });
    (void)tip.open(true);
    SHOULD(ShouldlyIntegral(tip.is_open()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tip.visible()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(opened).ShouldBe(1));
    (void)tip.open(false);
    SHOULD(ShouldlyIntegral(tip.visible()).ShouldBe(false));

    Popup pop;
    (void)pop.set_stays_open(true);
    SHOULD(ShouldlyIntegral(pop.stays_open()).ShouldBe(true));
    (void)pop.open(true);
    SHOULD(ShouldlyIntegral(pop.visible()).ShouldBe(true));
    (void)pop.set_horizontal_offset(5.0);
    SHOULD(ShouldlyString(pop.horizontal_offset()).ShouldBe(5.0));
    (void)pop.set_vertical_offset(6.0);
    SHOULD(ShouldlyString(pop.vertical_offset()).ShouldBe(6.0));
}

// ---------------------------------------------------------------------------
// (E) S493 — GridSplitter (the S174 constant-time dest-rect mapping)
// ---------------------------------------------------------------------------
TEST_CASE("11e S493: GridSplitter drag-resizes an adjacent track (constant-time)")
{
    register_builtin_elements();
    GridSplitter gs;
    (void)gs.set_drag_increment(2.0);
    SHOULD(ShouldlyString(gs.drag_increment()).ShouldBe(2.0));
    (void)gs.set_keyboard_increment(4.0);
    SHOULD(ShouldlyString(gs.keyboard_increment()).ShouldBe(4.0));
    (void)gs.set_resize_direction(1);
    SHOULD(ShouldlyIntegral(gs.resize_direction() == 1).ShouldBe(true));

    // An absolute Pixel track grows by the snapped delta (constant-time).
    vaml::GridLength px = vaml::GridLength::Pixel(100.0f);
    vaml::GridLength out = gs.drag_resize_track(px, 10.0);
    SHOULD(ShouldlyIntegral(out.unitType == vaml::GridUnitType::Pixel).ShouldBe(true));
    SHOULD(ShouldlyString(static_cast<double>(out.value)).ShouldBe(110.0));

    // A Star track folds the delta into a Pixel length (the resolved dest).
    vaml::GridLength star = vaml::GridLength::Star(1.0f);
    vaml::GridLength so = gs.drag_resize_track(star, -6.0); // snapped to -6 (step 2)
    SHOULD(ShouldlyIntegral(so.unitType == vaml::GridUnitType::Pixel).ShouldBe(true));
    SHOULD(ShouldlyString(static_cast<double>(so.value)).ShouldBe(0.0)); // clamped >= 0

    // Drag increments SNAP to the DragIncrement (constant-time, never a re-alloc).
    // delta 5 / step 2 -> round(2.5)=3 -> 3*2=6 -> 100+6=106.
    vaml::GridLength snapped = gs.drag_resize_track(px, 5.0);
    SHOULD(ShouldlyString(static_cast<double>(snapped.value)).ShouldBe(106.0));
}

// ---------------------------------------------------------------------------
// (F) S494 — DataGrid (native list/grid fill + columns + selection)
// ---------------------------------------------------------------------------
TEST_CASE("11e S494: DataGrid items + columns + selection + editing surface")
{
    register_builtin_elements();
    DataGrid dg;
    (void)dg.set_items("Alice, Bob, Carol");
    SHOULD(ShouldlyString(dg.items()).ShouldBe(std::string("Alice, Bob, Carol")));

    (void)dg.add_column(DataGridColumn{DataGridColumn::Kind::Text, "Name", 100.0f});
    (void)dg.add_column(DataGridColumn{DataGridColumn::Kind::Text, "Email", 160.0f});
    SHOULD(ShouldlyIntegral(dg.columns().size() == 2u).ShouldBe(true));
    SHOULD(ShouldlyString(dg.columns()[0].header()).ShouldBe(std::string("Name")));

    // Selection (SelectionChanged + SelectedItem).
    int fired = 0;
    [[maybe_unused]] auto tok =
        dg.add_routed_handler(vaml::RoutedEvent::registered("SelectionChanged", vaml::RouteEventDirection::Bubble),
                              [&](vaml::RoutedEventArgs&) { ++fired; });
    (void)dg.set_selected_index(1);
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));
    SHOULD(ShouldlyString(dg.selected_item()).ShouldBe(std::string("Bob")));

    // Editing/selection surface + DP round-trips.
    (void)dg.set_selection_unit(0);
    SHOULD(ShouldlyIntegral(dg.selection_unit() == 0).ShouldBe(true));
    (void)dg.set_is_read_only(true);
    SHOULD(ShouldlyIntegral(dg.is_read_only()).ShouldBe(true));
    (void)dg.set_headers_visibility(1);
    SHOULD(ShouldlyIntegral(dg.headers_visibility() == 1).ShouldBe(true));
    (void)dg.set_current_cell(2, 1);
    SHOULD(ShouldlyIntegral(dg.current_row() == 2).ShouldBe(true));
    (void)dg.set_auto_generate_columns(false);
    SHOULD(ShouldlyIntegral(dg.auto_generate_columns()).ShouldBe(false));
}

// ---------------------------------------------------------------------------
// (G) classifier: classify_element -> Solid (the 11e fills)
// ---------------------------------------------------------------------------
TEST_CASE("11e: classify_element emits Solid for the container/menu/tooltip/popup/splitter/grid controls")
{
    register_builtin_elements();
    TabControl tc;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(tc).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    TabItem ti;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(ti).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    Menu menu;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(menu).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    MenuItem mi;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(mi).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    ContextMenu cm;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(cm).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    ToolTip tip;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(tip).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    Popup pop;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(pop).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    GridSplitter gs;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(gs).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
    DataGrid dg;
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_merge_element(dg).kind) ==
                            static_cast<int>(MaterialKind::Solid)).ShouldBe(true));
}
