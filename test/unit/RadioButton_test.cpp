// S471 (dep:S470) — RadioButton control RESOLVED (Sprint 11c). RE-BASED onto
// UIElement (RESOLVES the S087 MINIMAL REAL seed at its own step — NO-GAP), and
// delivers the S471 surface:
//
//   1. IsChecked (REAL DP; Bool via Int32, default false).
//   2. GROUP SEMANTICS (the honest single-parent model): a GROUPLESS RadioButton
//      excludes ONLY its sibling RadioButtons under the SAME visual parent —
//      checking one clears every sibling under the same parent (exclusive
//      within the group). A checked sibling in a DIFFERENT parent is unaffected.
//   3. ROUTED Checked/Unchecked events (the Sprint-8 engine): a real check
//      raises "Checked", a real uncheck raises "Unchecked" (source==this).
//   4. classify_merge_element -> MaterialKind::Solid (check glyph, checked fill
//      vs unchecked outline).
//   5. Registered as a built-in (registry 17 -> 18).
//
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"       // RadioButton, StackPanel, register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"

using vaml::RadioButton;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::register_builtin_elements;

// ---------------------------------------------------------------------------
// 1. IsChecked: default + round-trip
// ---------------------------------------------------------------------------

TEST_CASE("S471: RadioButton IsChecked defaults false + round-trips")
{
    RadioButton radio;
    SHOULD(ShouldlyIntegral(static_cast<int>(radio.is_checked())).ShouldBe(0));
    (void)radio.set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(radio.is_checked())).ShouldBe(1));
    (void)radio.set_checked(false);
    SHOULD(ShouldlyIntegral(static_cast<int>(radio.is_checked())).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// 2. Group semantics: exclusive within the same parent
// ---------------------------------------------------------------------------

TEST_CASE("S471: checking one RadioButton unchecks a sibling under the same parent")
{
    StackPanel panel;
    RadioButton* a = panel.create_child<RadioButton>();
    RadioButton* b = panel.create_child<RadioButton>();

    (void)a->set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(a->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(b->is_checked())).ShouldBe(0));

    // Checking b unchecks a (exclusive within the parent).
    (void)b->set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(b->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(a->is_checked())).ShouldBe(0));
}

TEST_CASE("S471: a checked sibling in a DIFFERENT parent is unaffected")
{
    // Two separate stacks, each with a checked RadioButton; checking one does
    // NOT clear the OTHER parent's checked button (single-parent group model).
    StackPanel p1;
    StackPanel p2;
    RadioButton* p1a = p1.create_child<RadioButton>();
    RadioButton* p1b = p1.create_child<RadioButton>();
    RadioButton* p2a = p2.create_child<RadioButton>();

    (void)p1a->set_checked(true);
    (void)p2a->set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(p1a->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(p2a->is_checked())).ShouldBe(1));

    // Checking p1b unchecks p1a but p2a (different parent) stays checked.
    (void)p1b->set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(p1a->is_checked())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(p1b->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(p2a->is_checked())).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. Routed Checked/Unchecked events (source == this)
// ---------------------------------------------------------------------------

TEST_CASE("S471: checking raises Checked (source==this); unchecking raises Unchecked")
{
    RadioButton radio2;
    int checked = 0, unchecked = 0;
    (void)radio2.add_routed_handler(vaml::RoutedEvent::registered("Checked",
                                                                  vaml::RouteEventDirection::Bubble),
                                    [&](vaml::RoutedEventArgs& e) {
                                        if (e.source == &radio2) ++checked;
                                    });
    (void)radio2.add_routed_handler(vaml::RoutedEvent::registered("Unchecked",
                                                                  vaml::RouteEventDirection::Bubble),
                                    [&](vaml::RoutedEventArgs& e) {
                                        if (e.source == &radio2) ++unchecked;
                                    });

    (void)radio2.set_checked(true);   // Checked fires
    SHOULD(ShouldlyIntegral(checked).ShouldBe(1));
    SHOULD(ShouldlyIntegral(unchecked).ShouldBe(0));

    (void)radio2.set_checked(false);  // Unchecked fires
    SHOULD(ShouldlyIntegral(checked).ShouldBe(1));
    SHOULD(ShouldlyIntegral(unchecked).ShouldBe(1));

    // Setting to the same state fires nothing (real-change only).
    (void)radio2.set_checked(false);
    SHOULD(ShouldlyIntegral(checked).ShouldBe(1));
    SHOULD(ShouldlyIntegral(unchecked).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. Registered as a built-in (17 -> 18) + derives UIElement
// ---------------------------------------------------------------------------

TEST_CASE("S471: RadioButton derives UIElement + is a registered built-in")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    auto e = g.try_create("RadioButton");
    SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
    if (e)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::RadioButton*>(e.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                   .ShouldBe(true));
    }
}
