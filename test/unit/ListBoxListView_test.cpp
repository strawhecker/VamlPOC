// S473 (dep:S472) — ListBox + ListView controls RESOLVED (Sprint 11c). RE-BASED
// onto UIElement (RESOLVE the S087 MINIMAL REAL seeds at their own steps —
// NO-GAP), and deliver the S473 surface:
//
//   1. Items / ItemsSource + SelectedIndex/SelectedItem + a real SelectionChanged
//      routed event (shared item-selection base via ListBox; ListView derives
//      ListBox).
//   2. `ListBox.ItemTemplate` property-element authoring: the authored template
//      (S452 grammar) routes into the S364 engine so items render authored
//      per-item content — `render_items()` instantiates one UIElement per item.
//   3. classify_merge_element -> MaterialKind::Solid (list fill).
//   4. Registered as built-ins (registry 19 -> 21).
//
// SCOPE (recorded NOT silent): multi-selection + full virtualization are the
// S649-list continuation (NO-GAP). The single-selection + ItemTemplate render
// are the S473 real algorithm (headless-tested).
//
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"        // ListBox, ListView, Text, register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"

using vaml::ListBox;
using vaml::ListView;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::register_builtin_elements;

// ---------------------------------------------------------------------------
// 1. Items / ItemsSource + SelectedIndex / SelectedItem + SelectionChanged
// ---------------------------------------------------------------------------

TEST_CASE("S473: ListBox Items/ItemsSource round-trip + SelectedIndex default -1 + SelectionChanged")
{
    ListBox lb;
    (void)lb.set_items("One, Two, Three");
    SHOULD(ShouldlyString(lb.items()).ShouldBe(std::string("One, Two, Three")));
    (void)lb.set_items_source("A,B");
    SHOULD(ShouldlyString(lb.items_source()).ShouldBe(std::string("A,B")));

    // Default: nothing selected.
    SHOULD(ShouldlyIntegral(lb.selected_index()).ShouldBe(-1));

    int fires = 0;
    (void)lb.selection_changed().add([&]() { ++fires; });
    (void)lb.set_selected_index(2);
    SHOULD(ShouldlyIntegral(lb.selected_index()).ShouldBe(2));
    SHOULD(ShouldlyString(lb.selected_item()).ShouldBe(std::string("Three")));
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));

    // Out-of-range clamps (readable, never a throw).
    (void)lb.set_selected_index(99);
    SHOULD(ShouldlyIntegral(lb.selected_index()).ShouldBe(2));
}

TEST_CASE("S473: ListView derives ListBox + shares the selection surface")
{
    ListView lv;
    (void)lv.set_items("X, Y");
    (void)lv.set_selected_index(1);
    SHOULD(ShouldlyIntegral(lv.selected_index()).ShouldBe(1));
    SHOULD(ShouldlyString(lv.selected_item()).ShouldBe(std::string("Y")));
    // ListView IS-A ListBox (the shared item-selection base).
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ListBox*>(&lv) != nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 2. ItemTemplate property-element authoring -> items render authored content
// ---------------------------------------------------------------------------

TEST_CASE("S473: ListBox.ItemTemplate round-trip + render_items produces item elements")
{
    ListBox lb;
    // The authored ItemTemplate text (the S452 property-element grammar routes
    // the authored template here).
    (void)lb.set_item_template("<TextBlock/>");
    SHOULD(ShouldlyString(lb.item_template()).ShouldBe(std::string("<TextBlock/>")));

    (void)lb.set_items("Red, Green, Blue");
    // render_items instantiates one UIElement per item (the S364 instantiate
    // seam — the "items render authored content" headline).
    const std::size_t built = lb.render_items();
    SHOULD(ShouldlyIntegral(built).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(lb.child_count()).ShouldBe(3u));
}

TEST_CASE("S473: ListView.ItemTemplate + render_items produce item elements")
{
    ListView lv;
    (void)lv.set_item_template("<TextBlock/>");
    (void)lv.set_items("A, B");
    const std::size_t built = lv.render_items();
    SHOULD(ShouldlyIntegral(built).ShouldBe(2u));
}

// ---------------------------------------------------------------------------
// 3. Registered as built-ins (19 -> 21) + derive UIElement
// ---------------------------------------------------------------------------

TEST_CASE("S473: ListBox + ListView derive UIElement + are registered built-ins")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    auto e = g.try_create("ListBox");
    SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
    if (e)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ListBox*>(e.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                   .ShouldBe(true));
    }

    auto v = g.try_create("ListView");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ListView*>(v.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ListBox*>(v.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(v.get()) != nullptr)
                   .ShouldBe(true));
    }
}
