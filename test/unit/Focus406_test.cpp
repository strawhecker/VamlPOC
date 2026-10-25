// S406 (dep:S405) — Focus + tab order + Enter-activates-default + routed key
// events (vaml::core). Headless proof of the S406 headline:
//
//   (A) TAB ORDER: the S325 FocusManager's deterministic pre-order tab walk
//       reaches every FOCUSABLE control in the S10 set (the controls are
//       focusable UIElements), and MoveFocusNext/Previous wrap.
//   (B) ROUTED KEY EVENTS: RaiseKeyRouted raises the bubbling KeyDown/KeyUp on
//       the focused element (the S321 engine); a handler may handle/stop it.
//   (C) ENTER-ACTIVATES-DEFAULT (S406 the new surface): an Enter KEY-DOWN that
//       NO focused control/handler consumed raises the CLICK on the configured
//       default button (WPF-faithful); a handled Enter (or a non-Enter key) does
//       NOT re-activate.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>          // Button, register_builtin_elements
#include <vaml/core/FocusManager.h>     // FocusManager / KeyCode / KeyRoutedEventArgs
#include <vaml/core/RoutedEvent.h>      // ClickEvent / KeyDownEvent
#include <vaml/core/UIElement.h>
#include <vaml/core/Geometry.h>

using vaml::Button;
using vaml::KeyCode;
using vaml::FocusManager;

// A small helper root with a few focusable Buttons (the S10 focusable controls).
struct Focus450Fixture
{
    vaml::UIElement root;
    Button* b1 = nullptr;
    Button* b2 = nullptr;
    Button* b3 = nullptr;
    Focus450Fixture()
    {
        b1 = root.create_child<Button>();
        b2 = root.create_child<Button>();
        b3 = root.create_child<Button>();
        // The S10 controls are focusable leaves (a leaf defaults focusable true);
        // make it explicit so the tab-order assertion is robust regardless of the
        // control default.
        if (b1) (void)b1->set_focusable(true);
        if (b2) (void)b2->set_focusable(true);
        if (b3) (void)b3->set_focusable(true);
    }
    [[nodiscard]] std::size_t focusable_count() const
    {
        std::size_t n = 0;
        if (b1 && b1->focusable()) ++n;
        if (b2 && b2->focusable()) ++n;
        if (b3 && b3->focusable()) ++n;
        return n;
    }
};

TEST_CASE("S406: tab order walks the focusable controls (pre-order) + wraps")
{
    vaml::register_builtin_elements();
    Focus450Fixture fx;
    FocusManager fm(&fx.root);

    // No focus initially; MoveFocusNext starts at the FIRST focusable element.
    // The root itself is focusable by default (per S325: a fresh UIElement is a
    // focusable leaf), so the pre-order is root, b1, b2, b3.
    SHOULD(ShouldlyString(fm.FocusedElement() == nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(&fx.root));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(fx.b1));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(fx.b2));
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(fx.b3));
    // Wrap: next after b3 -> back to root.
    SHOULD(ShouldlyString(fm.MoveFocusNext()).ShouldBe(&fx.root));
    // MoveFocusPrevious walks back to the last (b3).
    SHOULD(ShouldlyString(fm.MoveFocusPrevious()).ShouldBe(fx.b3));
}

TEST_CASE("S406: routed key events raise KeyDown/KeyUp on the focused element")
{
    vaml::register_builtin_elements();
    Focus450Fixture fx;
    FocusManager fm(&fx.root);
    fm.KeyboardFocus(fx.b2);

    int downs = 0, ups = 0;
    (void)fx.b2->add_routed_handler(vaml::KeyDownEvent(), [&](vaml::RoutedEventArgs& e) {
        static_cast<void>(e);
        ++downs;
    });
    (void)fx.b2->add_routed_handler(vaml::KeyUpEvent(), [&](vaml::RoutedEventArgs&) { ++ups; });

    const bool d = fm.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/true);
    const bool u = fm.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/false);
    SHOULD(ShouldlyString(d).ShouldBe(true));
    SHOULD(ShouldlyString(u).ShouldBe(true));
    SHOULD(ShouldlyIntegral(downs).ShouldBe(1));
    SHOULD(ShouldlyIntegral(ups).ShouldBe(1));
}

TEST_CASE("S406: ENTER activates the default button when no focus consumes it (WPF-faithful)")
{
    vaml::register_builtin_elements();
    Focus450Fixture fx;
    // The default button is b2; it need not be focused.
    FocusManager fm(&fx.root);
    fm.set_default_button(fx.b2);
    // A click handler on the default button counts activations.
    int defaultClicks = 0;
    (void)fx.b2->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++defaultClicks; });

    // Focus sits on b1 (nothing consumes Enter yet). An unconsumed Enter KEY-
    // DOWN activates the default button -> +1 click.
    fm.KeyboardFocus(fx.b1);
    (void)fm.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/true);
    SHOULD(ShouldlyIntegral(defaultClicks).ShouldBe(1));

    // A non-Enter key does NOT activate the default button.
    (void)fm.RaiseKeyRouted(KeyCode::Tab, /*isDown=*/true);
    SHOULD(ShouldlyIntegral(defaultClicks).ShouldBe(1));

    // An Enter KEY-UP does NOT activate (only the DOWN is the activation).
    (void)fm.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/false);
    SHOULD(ShouldlyIntegral(defaultClicks).ShouldBe(1));

    // A HANDLED Enter on the focused element stops the default activation.
    (void)fx.b1->add_routed_handler(vaml::KeyDownEvent(), [&](vaml::RoutedEventArgs& e) {
        e.handled = true; // the TextBox single-line accept consumed Enter
    });
    (void)fm.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/true);
    SHOULD(ShouldlyIntegral(defaultClicks).ShouldBe(1)); // no new click (handled)

    // No default button configured -> Enter does nothing.
    FocusManager fm2(&fx.root);
    fm2.KeyboardFocus(fx.b1);
    (void)fm2.RaiseKeyRouted(KeyCode::Enter, /*isDown=*/true);
    SHOULD(ShouldlyIntegral(defaultClicks).ShouldBe(1)); // unchanged
}
