// S325 (dep:S324) — keyboard focus + routed key events (bubble). WPF-faithful
// focus over a visual tree + deterministic pre-order tab-order + routed
// KeyDown/KeyUp on the focused element.
//
// Validates, with the Shouldly vocabulary:
//   1. Focusable DP: default true; set_focusable(false) marks non-focusable.
//   2. Focus: KeyboardFocus sets/clears; FocusedElement() reflects it.
//   3. Tab-order = pre-order over focusable leaves; MoveFocusNext/Previous
//      wrap; non-Focusable (containers) skipped.
//   4. RaiseKeyRouted raises a bubbling KeyDown/KeyUp on the focused element;
//      a handled key stops the climb.
//   5. Clear focus (KeyboardFocus(nullptr)) -> FocusedElement() null; a key
//      raise with no focus is a no-op.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral

#include "vaml/core/FocusManager.h"       // FocusManager + KeyCode
#include "vaml/core/RoutedEvent.h"        // RoutedEventArgs / KeyRoutedEventArgs
#include "vaml/core/UIElement.h"

#include <memory>

using vaml::FocusManager;
using vaml::KeyCode;
using vaml::KeyRoutedEventArgs;
using vaml::UIElement;

namespace {

// Builds a pre-order focusable tree:
//   root (focusable)
//     childA (focusable)
//       leafA1 (focusable)   <-- overridden: set focusable(false) for skip tests
//       leafA2 (focusable)
//     childB (focusable)
//       leafB1 (focusable)
//     leafC (focusable)
// Pre-order order: root, childA, leafA1, leafA2, childB, leafB1, leafC
std::unique_ptr<UIElement> build_tree()
{
    auto root = std::make_unique<UIElement>();
    UIElement* a = root->create_child<UIElement>();
    UIElement* a1 = a->create_child<UIElement>();
    UIElement* a2 = a->create_child<UIElement>();
    UIElement* b = root->create_child<UIElement>();
    UIElement* b1 = b->create_child<UIElement>();
    UIElement* c = root->create_child<UIElement>();
    (void)a1; (void)a2; (void)b1; (void)c;
    return root;
}

} // namespace

TEST_CASE("S325: Focusable DP defaults true and set_focusable flips it")
{
    UIElement el;
    SHOULD(ShouldlyIntegral(el.focusable() ? 1 : 0).ShouldBe(1));
    (void)el.set_focusable(false);
    SHOULD(ShouldlyIntegral(el.focusable() ? 1 : 0).ShouldBe(0));
    (void)el.set_focusable(true);
    SHOULD(ShouldlyIntegral(el.focusable() ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S325: KeyboardFocus sets/clears; FocusedElement reflects it")
{
    auto root = build_tree();
    UIElement* a = root->child_at(0);
    FocusManager fm(root.get());
    SHOULD(ShouldlyIntegral(fm.FocusedElement() == nullptr).ShouldBe(1));

    fm.KeyboardFocus(a);
    SHOULD(ShouldlyString(fm.FocusedElement()).ShouldBe(a));

    fm.KeyboardFocus(nullptr);
    SHOULD(ShouldlyIntegral(fm.FocusedElement() == nullptr).ShouldBe(1));
}

TEST_CASE("S325: tab-order is pre-order over focusable leaves; Next/Previous wrap")
{
    auto root = build_tree();
    UIElement* a = root->child_at(0);
    UIElement* a1 = a->child_at(0);
    UIElement* a2 = a->child_at(1);
    UIElement* b = root->child_at(1);
    UIElement* b1 = b->child_at(0);
    UIElement* c = root->child_at(2);

    FocusManager fm(root.get());
    // All six elements are focusable by default: pre-order root,a,a1,a2,b,b1,c.
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(root.get()));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a1));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a2));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(b));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(b1));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(c));
    // Wrap: after the last, Next returns the first again.
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(root.get()));
    // Previous wraps to the last.
    SHOULD(ShouldlyString(fm.MoveFocusPrevious()).ShouldBe(c));
}

TEST_CASE("S325: non-Focusable (container) elements are skipped in tab-order")
{
    auto root = build_tree();
    UIElement* a = root->child_at(0);
    UIElement* a1 = a->child_at(0);
    UIElement* a2 = a->child_at(1);
    UIElement* b = root->child_at(1);
    UIElement* c = root->child_at(2);

    // Make the three PANEL-ish elements (root, a, b) non-focusable (containers).
    (void)root->set_focusable(false);
    (void)a->set_focusable(false);
    (void)b->set_focusable(false);

    FocusManager fm(root.get());
    // Remaining order: a1, a2, b1, c (b and its siblings a/root skipped).
    UIElement* b1 = b->child_at(0);
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a1));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a2));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(b1));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(c));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(a1)); // wraps
}

TEST_CASE("S325: RaiseKeyRouted raises a bubbling KeyDown/KeyUp on the focused element")
{
    auto root = build_tree();
    UIElement* a = root->child_at(0);
    FocusManager fm(root.get());
    fm.KeyboardFocus(a);

    int downs = 0;
    int ups = 0;
    KeyCode seenKey = KeyCode::None;
    bool seenDown = false;
    (void)a->add_routed_handler(vaml::KeyDownEvent(), [&](vaml::RoutedEventArgs& e) {
        ++downs;
        auto* k = dynamic_cast<KeyRoutedEventArgs*>(&e);
        if (k) { seenKey = k->key; seenDown = k->isDown; }
    });
    (void)a->add_routed_handler(vaml::KeyUpEvent(), [&](vaml::RoutedEventArgs& e) {
        ++ups;
        auto* k = dynamic_cast<KeyRoutedEventArgs*>(&e);
        if (k) { seenDown = k->isDown; }
    });

    (void)fm.RaiseKeyRouted(KeyCode::Enter, true);
    (void)fm.RaiseKeyRouted(KeyCode::Enter, false);
    SHOULD(ShouldlyIntegral(downs).ShouldBe(1));
    SHOULD(ShouldlyIntegral(ups).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(seenKey)).ShouldBe(static_cast<int>(KeyCode::Enter)));
}

TEST_CASE("S325: a handled key on the focused element stops the climb to an ancestor")
{
    auto root = build_tree();
    UIElement* a = root->child_at(0);
    FocusManager fm(root.get());
    fm.KeyboardFocus(a);

    int rootKey = 0;
    (void)root->add_routed_handler(vaml::KeyDownEvent(), [&](vaml::RoutedEventArgs&) { ++rootKey; });
    (void)a->add_routed_handler(vaml::KeyDownEvent(),
                                [](vaml::RoutedEventArgs& e) { e.handled = true; });

    (void)fm.RaiseKeyRouted(KeyCode::Escape, true);
    SHOULD(ShouldlyIntegral(rootKey).ShouldBe(0)); // the focused element handled it
}

TEST_CASE("S325: no focus => RaiseKeyRouted is a no-op")
{
    auto root = build_tree();
    FocusManager fm(root.get());
    SHOULD(ShouldlyIntegral(fm.RaiseKeyRouted(KeyCode::Enter, true) ? 1 : 0).ShouldBe(0));
}
