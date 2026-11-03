#pragma once
// VAML InputGesture — the abstract gesture base (WPF
// System.Windows.Input.InputGesture). 11n-2.
//
// WPF-faithful: an InputGesture maps an input (mouse / keyboard) to a command;
// an InputBinding associates a gesture with an ICommandSource. The abstract
// InputGesture is the base for KeyGesture (keyboard) + MouseGesture (mouse).
//
// ZERO invented names — KeyGesture/MouseGesture are the WPF-real types. One
// combined header keeps the gesture family small and self-contained.

#include "vaml/core/export.h"

#include <cstdint>
#include <string>

namespace vaml {

// The match result of a gesture against an input action.
enum class InputGestureMatch : std::uint8_t
{
    NotMatched,
    Matched,
};

// The abstract gesture base.
class VAML_CORE_EXPORT InputGesture
{
public:
    virtual ~InputGesture() = default;

    // The human-readable gesture string (WPF: the gesture's display text,
    // e.g. "Ctrl+S"); used by UI affordances (menu accelerators).
    [[nodiscard]] virtual std::string gesture_text() const = 0;
};

// KeyGesture — a keyboard gesture (WPF System.Windows.Input.KeyGesture):
// a key + optional modifiers -> a command. Modifiers reuse the WPF ModifierKeys
// bit values as plain ints (Alt=1, Control=2, Shift=4, Windows=8) — ZERO
// invented enum names.
class VAML_CORE_EXPORT KeyGesture : public InputGesture
{
public:
    // key: a KeyCode is used by the input layer; here we carry the canonical
    // WPF Keys via a plain int (the S324/S325 key domain). modifiers: the
    // ModifierKeys bitmask (Alt=1/Control=2/Shift=4/Windows=8).
    KeyGesture(int key, int modifiers = 0);

    [[nodiscard]] std::string gesture_text() const override;
    [[nodiscard]] int key() const noexcept { return key_; }
    [[nodiscard]] int modifiers() const noexcept { return modifiers_; }

    // The match test against a concrete keyboard action (key + modifiers).
    [[nodiscard]] InputGestureMatch matches(int key, int modifiers) const noexcept;

private:
    int key_;
    int modifiers_;
};

// MouseGesture — a mouse gesture (WPF System.Windows.Input.MouseGesture):
// a mouse action + optional modifiers -> a command.
class VAML_CORE_EXPORT MouseGesture : public InputGesture
{
public:
    // action: the WPF MouseActions as plain ints — LeftClick=1/RightClick=2/
    // MiddleClick=3/WheelClick=4/LeftDoubleClick=5/RightDoubleClick=6/
    // MiddleDoubleClick=7. modifiers: ModifierKeys bitmask.
    MouseGesture(int action, int modifiers = 0);

    [[nodiscard]] std::string gesture_text() const override;
    [[nodiscard]] int action() const noexcept { return action_; }
    [[nodiscard]] int modifiers() const noexcept { return modifiers_; }

    [[nodiscard]] InputGestureMatch matches(int action, int modifiers) const noexcept;

private:
    int action_;
    int modifiers_;
};

} // namespace vaml
