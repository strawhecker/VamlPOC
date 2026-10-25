#pragma once
// VAML InputMarshaller — the LIBRARY-level input -> routed-event translation
// (S323, Sprint 8, dep:S322). Headless-testable, GPU/GLFW-free (ADR-011 — the
// GLFW window callbacks live in the viewer/vulkan consumer layer and are
// translated to these plain calls).
//
// S323 (the S160-recorded resolution point): raw mouse/keyboard input becomes
// ROUTED EVENTS via this marshaller + the S321 real engine + hit-testing. The
// marshaller is a BY-VALUE state machine over a root element:
//
//   - A LEFT MOUSE PRESS: on MouseDown, hit-test the root at (x, y) and record
//     the DOWN element + position. On MouseUp, if the release hit-test returns
//     the SAME element WITHIN a move tolerance of the down position, raise a
//     bubbling `ClickEvent` with source = that element (the S321 route; a
//     handler may set handled to stop it). A press that moved beyond the
//     tolerance is a DRAG, not a click (no Click raised).
//
//   - KEY EVENTS: KeyDown/KeyUp raise bubbling routed key events (S321) on the
//     FOCUSED element (S325 wires focus; S323 exposes the `focused()` seam the
//     marshaller asks — the caller supplies it, so the marshaller never holds
//     focus state it doesn't own).
//
//   - A RaiseRoutingFn seam lets the marshaller route through the REAL
//     UIElement::RaiseEvent (the S321 engine) headlessly — the default / the
//     only sanctioned router.
//
// Design notes (recorded, not silent):
//   * The marshaller does NOT own the tree; it borrows a non-owning root. The
//     caller owns the element lifetime (a viewer reload replaces the root).
//   * Click = press+release on the same element within `moveTolerance` logical
//     px (default 4). A one-element click (hit at press == hit at release)
//     even if the element moved is still a click if both hits are the same.
//   * A press on ONE element + release on ANOTHER is NOT a click (WPF: no click
//     on a drag to another element).
//   * ADR-018: zero new locks — single-threaded on the UI/affinity thread.

#include "vaml/core/UIElement.h"      // UIElement (hit-test + RaiseEvent)
#include "vaml/core/RoutedEvent.h"    // RoutedEvent / RoutedEventArgs

#include <cstdint>

namespace vaml {
namespace input {

// The raw input events the marshaller consumes (the shapes a GLFW callback
// translates into — see the viewer adapter, S329_1). Keyed by kind + a small
// payload; the marshaller is a pure state machine over these.
enum class InputEventKind : std::uint8_t
{
    MouseDown, // left button pressed at (x, y)
    MouseUp,   // left button released at (x, y)
    MouseMove, // cursor moved to (x, y) with left button possibly down
    KeyDown,   // a key pressed
    KeyUp,     // a key released
};

// A logical keyboard key (a subset of scan/PC keys — the S323 keyboard surface;
// S325 focus + routed key events consume it).
enum class Key : std::uint8_t
{
    None   = 0,
    A      = 1, B = 2, C = 3, D = 4, E = 5, F = 6, G = 7, H = 8, I = 9,
    J      = 10, K = 11, L = 12, M = 13, N = 14, O = 15, P = 16, Q = 17,
    R      = 18, S = 19, T = 20, U = 21, V = 22, W = 23, X = 24, Y = 25, Z = 26,
    Digit0 = 27, Digit1 = 28, Digit2 = 29, Digit3 = 30, Digit4 = 31,
    Digit5 = 32, Digit6 = 33, Digit7 = 34, Digit8 = 35, Digit9 = 36,
    Tab      = 52,
    Up       = 53, Down = 54, Left = 55, Right = 56,
    Enter    = 57, Escape = 58,
};

// One input event: kind + coordinate (mouse) / key (keyboard) + button state.
struct InputEvent
{
    InputEventKind kind = InputEventKind::MouseMove;
    float     x = 0.0f;
    float     y = 0.0f;
    Key       key = Key::None;
    bool      isDown = false; // for keys: pressed (true) vs released (false)
    bool      isLeftDown = false; // for mouse: left button held during this event
};

// The routed key event args raised on the focused element (S325 extends focus;
// the marshaller raises these as bubbled key events).
struct KeyRoutedArgs
{
    Key  key = Key::None;
    bool isDown = false;
};

// The marshaller: a per-root input state machine over the S321 engine.
class VAML_CORE_EXPORT InputMarshaller
{
public:
    // `root` is the NON-OWNING element to hit-test (the visual tree root).
    // `moveTolerance` is the drag-vs-click threshold in logical px.
    explicit InputMarshaller(UIElement* root, float moveTolerance = 4.0f)
        : root_(root)
        , moveTolerance_(moveTolerance)
    {
    }

    // Consumes one raw input event, returning whether a ROUTED event was raised
    // (a Click on press+release-same, or a key event) during this call.
    bool Push(const InputEvent& ev);

    // The focused element (non-owning). S325 wires focus + the routed focus
    // manager; the marshaller ASKS the caller for it so it never owns focus
    // state. Returns nullptr when nothing is focused (a key event then raises
    // on the root — WPF-faithful fallback).
    [[nodiscard]] UIElement* focused() const noexcept { return focused_ ? focused_ : root_; }

    // The element a press-hold is currently over (nullptr when no press is
    // active). Exposed for the drag/pan discrimination in the viewer.
    [[nodiscard]] UIElement* pressed_element() const noexcept { return pressed_; }

    // The current hit element under the cursor (the last MouseMove's hit).
    [[nodiscard]] UIElement* hovered_element() const noexcept { return hovered_; }

    // Sets the element that keyboard focus will route to (S325 consumes).
    void set_focused(UIElement* el) noexcept { focused_ = el; }

    // Raised a routed event flag from the last push (test/pass-through probe).
    [[nodiscard]] bool last_raised_click() const noexcept { return lastClick_; }

private:
    UIElement* root_ = nullptr;
    float      moveTolerance_ = 4.0f;

    // The press state (a left-press that may complete into a click).
    UIElement* pressed_ = nullptr;   // the hit element at press time
    float      pressedX_ = 0.0f;
    float      pressedY_ = 0.0f;
    bool       pressedActive_ = false;
    bool       movedDuringPress_ = false;

    UIElement* hovered_ = nullptr;  // last MouseMove hit
    UIElement* focused_ = nullptr;  // S325 focus target (caller-owned)

    bool       lastClick_ = false;

    [[nodiscard]] UIElement* hit(float x, float y) const;
};

} // namespace input
} // namespace vaml
