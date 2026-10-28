// S472 (dep:S471) — ComboBox control RESOLVED (Sprint 11c). RE-BASED onto
// UIElement (RESOLVES the S087 MINIMAL REAL seed at its own step — NO-GAP), and
// delivers the S472 surface:
//
//   1. Items / ItemsSource (REAL DPs; the owned item container).
//   2. SelectedIndex / SelectedItem (REAL DPs; SelectedIndex defaults −1; a
//      malformed selection is READABLE, never a throw).
//   3. SELECTIONCHANGED routed event on a real selection change.
//   4. DROPDOWN INTERACTION: is_open + open(dropdown) + select_item(display).
//   5. classify_merge_element -> MaterialKind::Solid (EditField-style box).
//   6. Registered as a built-in (registry 18 -> 19).
//
// SCOPE (recorded NOT silent): the editable/fully-styled chrome is the S649-list
// continuation (NO-GAP). The editable dropdown / styled chrome is NOT fabricated
// here; the ComboBox DP surface + selection + change are REAL.
//
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"        // ComboBox, register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"

using vaml::ComboBox;
using vaml::UIElement;
using vaml::register_builtin_elements;

// ---------------------------------------------------------------------------
// 1. Items / ItemsSource round-trip
// ---------------------------------------------------------------------------

TEST_CASE("S472: Items / ItemsSource round-trip")
{
    ComboBox cb;
    (void)cb.set_items("Apple, Banana, Cherry");
    SHOULD(ShouldlyString(cb.items()).ShouldBe(std::string("Apple, Banana, Cherry")));
    (void)cb.set_items_source("One,TWO");
    SHOULD(ShouldlyString(cb.items_source()).ShouldBe(std::string("One,TWO")));
}

// ---------------------------------------------------------------------------
// 2. SelectedIndex defaults −1 + set + round-trip; malformed is readable
// ---------------------------------------------------------------------------

TEST_CASE("S472: SelectedIndex defaults -1 + set + round-trip + malformed is readable")
{
    ComboBox cb;
    (void)cb.set_items("A, B, C");
    // Default: nothing selected.
    SHOULD(ShouldlyIntegral(cb.selected_index()).ShouldBe(-1));
    SHOULD(ShouldlyString(cb.selected_item()).ShouldBe(std::string("")));

    // Select index 1 -> the item "B".
    (void)cb.set_selected_index(1);
    SHOULD(ShouldlyIntegral(cb.selected_index()).ShouldBe(1));
    SHOULD(ShouldlyString(cb.selected_item()).ShouldBe(std::string("B")));

    // An out-of-range index CLAMPS and stays readable (never a throw).
    (void)cb.set_selected_index(99);
    SHOULD(ShouldlyIntegral(cb.selected_index()).ShouldBe(2)); // clamped to last

    // An empty combo's selection is -1 (readable, never a throw).
    ComboBox empty;
    (void)empty.set_selected_index(5);
    SHOULD(ShouldlyIntegral(empty.selected_index()).ShouldBe(-1));
}

// ---------------------------------------------------------------------------
// 3. SelectionChanged routed event on a real selection change
// ---------------------------------------------------------------------------

TEST_CASE("S472: SelectionChanged fires on a real selection change")
{
    ComboBox cb;
    (void)cb.set_items("A, B, C");
    int fires = 0;
    (void)cb.add_routed_handler(vaml::RoutedEvent::registered("SelectionChanged",
                                                              vaml::RouteEventDirection::Bubble),
                                [&](vaml::RoutedEventArgs& e) {
                                    if (e.source == &cb) ++fires;
                                });
    (void)cb.selection_changed().add([&]() { ++fires; }); // the Signal

    (void)cb.set_selected_index(2);
    SHOULD(ShouldlyIntegral(fires).ShouldBe(2));
    // Selecting the SAME index again is a real-change no-op (no re-fire).
    (void)cb.set_selected_index(2);
    SHOULD(ShouldlyIntegral(fires).ShouldBe(2));
}

// ---------------------------------------------------------------------------
// 4. Dropdown interaction: is_open + open() + select_item()
// ---------------------------------------------------------------------------

TEST_CASE("S472: dropdown interaction — is_open + open + select_item")
{
    ComboBox cb;
    (void)cb.set_items("Red, Green, Blue");
    SHOULD(ShouldlyIntegral(static_cast<int>(cb.is_open())).ShouldBe(0));
    (void)cb.open(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(cb.is_open())).ShouldBe(1));
    (void)cb.open(false);
    SHOULD(ShouldlyIntegral(static_cast<int>(cb.is_open())).ShouldBe(0));

    // select_item by display picks Green (index 1) + raises SelectionChanged.
    int fires = 0;
    (void)cb.selection_changed().add([&]() { ++fires; });
    SHOULD(ShouldlyIntegral(static_cast<int>(cb.select_item("Green"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(cb.selected_index()).ShouldBe(1));
    SHOULD(ShouldlyString(cb.selected_item()).ShouldBe(std::string("Green")));
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));

    // A non-matching display returns false (no selection change).
    SHOULD(ShouldlyIntegral(static_cast<int>(cb.select_item("Nope"))).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// 5. Registered as a built-in (18 -> 19) + derives UIElement
// ---------------------------------------------------------------------------

TEST_CASE("S472: ComboBox derives UIElement + is a registered built-in")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    auto e = g.try_create("ComboBox");
    SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
    if (e)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ComboBox*>(e.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                   .ShouldBe(true));
    }
}
