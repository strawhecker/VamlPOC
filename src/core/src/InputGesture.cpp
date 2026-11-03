// VAML InputGesture family — KeyGesture/MouseGesture (WPF
// System.Windows.Input). 11n-2. ZERO invented names; modifier/action ints are
// the WPF ModifierKeys/MouseActions values. ADR-018: ZERO new locks.

#include "vaml/core/InputGesture.h"

namespace vaml {

// --- KeyGesture ------------------------------------------------------------
VAML_CORE_EXPORT
KeyGesture::KeyGesture(int key, int modifiers)
    : key_(key), modifiers_(modifiers)
{}

VAML_CORE_EXPORT
std::string KeyGesture::gesture_text() const
{
    // WPF-faithful display form: modifiers + key, e.g. "Ctrl+S". We render the
    // canonical modifier tokens (Ctrl/Alt/Shift/Win) in WPF order + the key as
    // its decimal Keys value (the codebase's key domain is int — the WPF key
    // NAME lookup is a presentation nicety, not a value-plane guarantee).
    std::string text;
    if (modifiers_ & 1) text += "Alt+";     // ModifierKeys.Alt = 1
    if (modifiers_ & 2) text += "Ctrl+";    // ModifierKeys.Control = 2
    if (modifiers_ & 4) text += "Shift+";   // ModifierKeys.Shift = 4
    if (modifiers_ & 8) text += "Win+";     // ModifierKeys.Windows = 8
    text += std::to_string(key_);
    return text;
}

VAML_CORE_EXPORT
InputGestureMatch KeyGesture::matches(int key, int modifiers) const noexcept
{
    return (key == key_ && modifiers == modifiers_) ? InputGestureMatch::Matched
                                                    : InputGestureMatch::NotMatched;
}

// --- MouseGesture ----------------------------------------------------------
VAML_CORE_EXPORT
MouseGesture::MouseGesture(int action, int modifiers)
    : action_(action), modifiers_(modifiers)
{}

VAML_CORE_EXPORT
std::string MouseGesture::gesture_text() const
{
    std::string text;
    if (modifiers_ & 1) text += "Alt+";
    if (modifiers_ & 2) text += "Ctrl+";
    if (modifiers_ & 4) text += "Shift+";
    if (modifiers_ & 8) text += "Win+";
    switch (action_)
    {
        case 1:  text += "LeftClick"; break;
        case 2:  text += "RightClick"; break;
        case 3:  text += "MiddleClick"; break;
        case 4:  text += "WheelClick"; break;
        case 5:  text += "LeftDoubleClick"; break;
        case 6:  text += "RightDoubleClick"; break;
        case 7:  text += "MiddleDoubleClick"; break;
        default: text += "MouseAction" + std::to_string(action_);
    }
    return text;
}

VAML_CORE_EXPORT
InputGestureMatch MouseGesture::matches(int action, int modifiers) const noexcept
{
    return (action == action_ && modifiers == modifiers_) ? InputGestureMatch::Matched
                                                          : InputGestureMatch::NotMatched;
}

} // namespace vaml
