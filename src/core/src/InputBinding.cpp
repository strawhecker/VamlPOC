// VAML InputBinding family — KeyBinding/MouseBinding (WPF
// System.Windows.Input). 11n-2. InputBinding : Freezable, ICommandSource —
// the gesture↔command binding.

#include "vaml/core/InputBinding.h"

#include <utility>

namespace vaml {

// --- InputBinding : ICommandSource -----------------------------------------
VAML_CORE_EXPORT
vaml::ICommand* InputBinding::source_command() const noexcept
{
    return command_;
}

VAML_CORE_EXPORT
vaml::Value InputBinding::source_command_parameter() const
{
    return parameter_;
}

VAML_CORE_EXPORT
DependencyObject* InputBinding::source_command_target() const noexcept
{
    return target_;
}

// --- KeyBinding ------------------------------------------------------------
VAML_CORE_EXPORT
KeyBinding::KeyBinding(KeyGesture gesture)
    : gesture_(std::move(gesture))
{}

VAML_CORE_EXPORT
const InputGesture& KeyBinding::gesture() const { return gesture_; }

VAML_CORE_EXPORT
const KeyGesture& KeyBinding::key_gesture() const noexcept { return gesture_; }

VAML_CORE_EXPORT
KeyBinding& KeyBinding::with_command(ICommand* command)
{
    throw_if_frozen("KeyBinding");
    command_ = command;
    fire_changed();
    return *this;
}

VAML_CORE_EXPORT
KeyBinding& KeyBinding::with_parameter(Value parameter)
{
    throw_if_frozen("KeyBinding");
    parameter_ = std::move(parameter);
    fire_changed();
    return *this;
}

// --- MouseBinding ----------------------------------------------------------
VAML_CORE_EXPORT
MouseBinding::MouseBinding(MouseGesture gesture)
    : gesture_(std::move(gesture))
{}

VAML_CORE_EXPORT
const InputGesture& MouseBinding::gesture() const { return gesture_; }

VAML_CORE_EXPORT
const MouseGesture& MouseBinding::mouse_gesture() const noexcept { return gesture_; }

VAML_CORE_EXPORT
MouseBinding& MouseBinding::with_command(ICommand* command)
{
    throw_if_frozen("MouseBinding");
    command_ = command;
    fire_changed();
    return *this;
}

VAML_CORE_EXPORT
MouseBinding& MouseBinding::with_parameter(Value parameter)
{
    throw_if_frozen("MouseBinding");
    parameter_ = std::move(parameter);
    fire_changed();
    return *this;
}

} // namespace vaml
