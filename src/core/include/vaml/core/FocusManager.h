#pragma once
// VAML FocusManager — keyboard focus + routed key events (S325, Sprint 8,
// dep:S324). WPF-faithful focus over a visual tree, with routed KeyDown/KeyUp
// delivery on the focused element.
//
// S325 delivers:
//   * a per-tree focus manager: the SINGLE focused UIElement for a root,
//     KeyboardFocus(UIElement*) / FocusedElement().
//   * tab-order: MoveFocusNext() / MoveFocusPrevious() walk the visual tree in
//     PRE-ORDER (depth-first, this-first-then-children-in-order) over the
//     FOCUSABLE leaves (UIElement::focusable(), S325), skipping non-Focusable
//     elements; deterministic, not z-index based. Cycling wraps at the ends.
//   * RaiseKeyRouted(Key, isDown): raises the bubbling KeyDownEvent / KeyUpEvent
//     (S320 statics) ON the focused element; a handler setting handled=true
//     stops the climb (the S321 engine).
//
// Design notes (recorded, not silent):
//   * The manager keeps a NON-OWNING pointer to the focused element + the
//     ROOT it derives tab-order from. Like the InputMarshaller (S323), it does
//     NOT own the tree — the caller owns the element lifetime (a viewer reload
//     replaces the root). On removal/destruction of the focused element, the
//     caller clears it (the S245 destroyed-wire precedent): FocusManager exposes
//     no dangling reference — FocusedElement() returns nullptr when cleared.
//   * Focus is per-TREE (one focused element per root); a manager is
//     constructed over a root. ADR-018: zero new locks — single-threaded on the
//     UI/affinity thread.
//   * KeyboardFocus(nullptr) clears focus. Setting focus on a NON-focusable
//     element: coerced? (recorded) — accepted but the tab-order skips it; a
//     manager may still focus it directly (matches WPF's Focusable=false
//     ambiguity; the tab-order is the deterministic contract).
//
// The S323 InputMarshaller asks the FOCUS MANAGER for the focused element via
// its focused() seam (the caller wires set_focused/focused). The manager is
// the S325 source of that focus.

#include "vaml/core/UIElement.h"     // UIElement (focus + RoutedHandler)
#include "vaml/core/RoutedEvent.h"   // RoutedEventArgs / RoutedEventHandler / ClickEvent

#include <cstdint>
#include <vector>

namespace vaml {

class Button; // forward — S406 default-button (Button is defined in Element.h,
              // which FocusManager.h cannot include; the .cpp owns the wiring)

// A keyboard key routed through the key events (mirrors vaml::input::Key; the
// FocusManager + routed key events share the same key surface).
enum class KeyCode : std::uint8_t
{
    None   = 0,
    A = 1, B = 2, C = 3, D = 4, E = 5, F = 6, G = 7, H = 8, I = 9,
    J = 10, K = 11, L = 12, M = 13, N = 14, O = 15, P = 16, Q = 17,
    R = 18, S = 19, T = 20, U = 21, V = 22, W = 23, X = 24, Y = 25, Z = 26,
    Digit0 = 27, Digit1 = 28, Digit2 = 29, Digit3 = 30, Digit4 = 31,
    Digit5 = 32, Digit6 = 33, Digit7 = 34, Digit8 = 35, Digit9 = 36,
    Tab      = 52,
    Up = 53, Down = 54, Left = 55, Right = 56,
    Enter = 57, Escape = 58,
    Backspace = 59, Home = 60, End = 61, Space = 62, TextBoxAccept = 63,
};

// The ROUTED KEY args: extends RoutedEventArgs with the key + down state, so a
// KeyDown/KeyUp handler on the focused element can read WHICH key + direction.
// The S321 engine delivers it through RoutedEventArgs& (a handler dynamic_casts
// to read the key). `handled` still stops the climb.
struct VAML_CORE_EXPORT KeyRoutedEventArgs : public RoutedEventArgs
{
    KeyCode key    = KeyCode::None;
    bool    isDown = false;
};

class VAML_CORE_EXPORT FocusManager
{
public:
    // Constructs a manager over the (non-owning) visual root; tab-order derives
    // from this root's subtree.
    explicit FocusManager(UIElement* root);

    // The focused element (nullptr when nothing is focused / it was removed).
    [[nodiscard]] UIElement* FocusedElement() const noexcept { return focused_; }

    // Grants keyboard focus to `element` (nullable -> clears). The element need
    // not be Focusable to be directly focused (WPF-compatible); the tab-order
    // still skips non-Focusable elements.
    void KeyboardFocus(UIElement* element);

    // Moves focus to the NEXT focusable element in PRE-ORDER tab order (wrapping
    // at the end); returns the newly-focused element (or nullptr if the tree has
    // no focusable element).
    [[nodiscard]] UIElement* MoveFocusNext();

    // Moves focus to the PREVIOUS focusable element in PRE-ORDER tab order
    // (wrapping); returns the newly-focused element.
    [[nodiscard]] UIElement* MoveFocusPrevious();

    // Raises the bubbling routed KeyDown (isDown=true) or KeyUp (isDown=false)
    // event ON the focused element (the S321 engine). A handler may set
    // handled=true to stop the climb. False when nothing is focused (no-op).
    bool RaiseKeyRouted(KeyCode key, bool isDown);

    // --- S406 default-button (Enter-activates-default) ---------------------
    // Sets the DEFAULT BUTTON for this root: the Button whose Click is raised
    // when Enter is pressed AND the focused element / its handlers did NOT
    // consume it (the WPF-faithful "Enter activates the default button"
    // primitive; the default-command association to IsDefault on a Button is
    // the real wiring, recorded). Nullable (clear). The default button need not
    // be focused; Enter raises its Click regardless of where focus sits (WPF).
    void set_default_button(Button* b);
    [[nodiscard]] Button* default_button() const noexcept { return defaultButton_; }

private:
    UIElement* root_ = nullptr;
    UIElement* focused_ = nullptr;
    Button* defaultButton_ = nullptr; // S406 — the Enter-activated default button

    // Walks the visual tree PRE-ORDER collecting the focusable elements into
    // `out` (the deterministic tab order).
    void collect_focusable(UIElement& node, std::vector<UIElement*>& out) const;
};

} // namespace vaml
