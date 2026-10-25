// S323 — InputMarshaller out-of-line definitions. See InputMarshaller.h for the
// contract. A headless, GLFW-free input -> routed-event state machine over the
// S321 engine + S323 hit-testing.

#include "vaml/core/InputMarshaller.h"

#include <cmath>

namespace vaml {
namespace input {

namespace {

// The key events' routed underlying framers (shared with the viewer + tests).
const RoutedEvent& key_event(bool isDown)
{
    return isDown ? KeyDownEvent() : KeyUpEvent();
}

} // namespace

UIElement* InputMarshaller::hit(float x, float y) const
{
    return (root_ != nullptr) ? root_->HitTestAt(x, y) : nullptr;
}

bool InputMarshaller::Push(const InputEvent& ev)
{
    lastClick_ = false;

    switch (ev.kind)
    {
        case InputEventKind::MouseDown:
        {
            if (!ev.isLeftDown)
            {
                break; // only left-press arms a click
            }
            // Record the press on the hit element (if any).
            pressed_ = hit(ev.x, ev.y);
            pressedX_ = ev.x;
            pressedY_ = ev.y;
            pressedActive_ = true;
            movedDuringPress_ = false;
            break;
        }

        case InputEventKind::MouseUp:
        {
            if (!pressedActive_)
            {
                break; // a release with no armed press is a no-op
            }
            const bool wasPress = pressedActive_;
            const bool movedAway = movedDuringPress_;
            const bool sameElement = (pressed_ != nullptr) && (pressed_ == hit(ev.x, ev.y));
            const bool withinTolerance = std::fabs(ev.x - pressedX_) <= moveTolerance_ &&
                                         std::fabs(ev.y - pressedY_) <= moveTolerance_;

            pressedActive_ = false;
            movedDuringPress_ = false;

            // A CLICK: pressed + released on the SAME element, not dragged
            // beyond tolerance. Raise a bubbling ClickEvent with the correct
            // source (the S321 engine + S324's correctness contract).
            if (wasPress && sameElement && !movedAway && withinTolerance && pressed_ != nullptr)
            {
                RoutedEventArgs args;
                args.source = pressed_;
                pressed_->RaiseEvent(ClickEvent(), args);
                lastClick_ = true;
            }
            break;
        }

        case InputEventKind::MouseMove:
        {
            if (ev.isLeftDown && pressedActive_)
            {
                // Track a drag: moving beyond tolerance marks it a drag, not a
                // click (the pan/zoom discrimination in the viewer).
                if (std::fabs(ev.x - pressedX_) > moveTolerance_ ||
                    std::fabs(ev.y - pressedY_) > moveTolerance_)
                {
                    movedDuringPress_ = true;
                }
            }
            hovered_ = hit(ev.x, ev.y);
            break;
        }

        case InputEventKind::KeyDown:
        case InputEventKind::KeyUp:
        {
            // Raise a bubbling routed key event on the focused element (S325
            // wires focus; the marshaller's focused() defaults to the root).
            UIElement* target = focused();
            if (target == nullptr)
            {
                break;
            }
            RoutedEventArgs args;
            args.source = target;
            target->RaiseEvent(key_event(ev.isDown), args);
            break;
        }
    }

    return lastClick_;
}

} // namespace input
} // namespace vaml
