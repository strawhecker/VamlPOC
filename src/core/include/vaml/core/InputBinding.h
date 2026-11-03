#pragma once
// VAML InputBinding — the gesture↔command binding base (WPF
// System.Windows.Input.InputBinding). 11n-2.
//
// WPF-faithful shape (verified from
// E:\GitRepos\wpf\src\...\Input\Command\InputBinding.cs):
//   - InputBinding : Freezable (the immutable base) + exposes the
//     ICommandSource shape (Command/CommandParameter/CommandTarget);
//   - KeyBinding  : InputBinding — binds a KeyGesture to a command;
//   - MouseBinding: InputBinding — binds a MouseGesture to a command.
//
// An InputBinding associates an InputGesture with a command so that when the
// gesture is detected (the key/mouse input layer, S324/S325), the command is
// executed. It is a Freezable: once frozen it is immutable (the WPF
// Freezable invariant — you cannot change a frozen InputBinding).

#include "vaml/core/Freezable.h"        // Freezable (the immutable base)
#include "vaml/core/ICommand.h"         // the ONE S246 command interface
#include "vaml/core/ICommandSource.h"   // the command-invoking interface
#include "vaml/core/InputGesture.h"     // the gesture family
#include "vaml/core/Value.h"            // the parameter domain
#include "vaml/core/export.h"

#include <memory>

namespace vaml {

// The abstract gesture→command binding base.
class VAML_CORE_EXPORT InputBinding : public Freezable, public ICommandSource
{
public:
    // --- ICommandSource (anchor 11n-2) -------------------------------------
    [[nodiscard]] vaml::ICommand* source_command() const noexcept override;
    [[nodiscard]] vaml::Value source_command_parameter() const override;
    [[nodiscard]] DependencyObject* source_command_target() const noexcept override;

    // WPF-faithful: the gesture this binding maps (read-only once set);
    // KeyBinding carries a KeyGesture, MouseBinding a MouseGesture.
    [[nodiscard]] virtual const InputGesture& gesture() const = 0;

protected:
    // The commanded gesture. Set by the concrete KeyBinding/MouseBinding ctor
    // (an InputBinding's command/parameter/target are authored after).
    InputBinding() = default;

    // protect against direct copies (polymorphic Freezable)
    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    vaml::ICommand* command_ = nullptr;
    vaml::Value     parameter_ = Value::Null();
    DependencyObject* target_ = nullptr;
};

// KeyBinding — a key gesture bound to a command (WPF System.Windows.Input
// KeyBinding).
class VAML_CORE_EXPORT KeyBinding final : public InputBinding
{
public:
    explicit KeyBinding(KeyGesture gesture);

    [[nodiscard]] const InputGesture& gesture() const override;
    [[nodiscard]] const KeyGesture& key_gesture() const noexcept;

    // WPF-faithful convenience: set up the binding from key+modifiers+command
    // in one call (the {InputBinding} markup path).
    KeyBinding& with_command(ICommand* command);
    KeyBinding& with_parameter(Value parameter);

private:
    KeyGesture gesture_;
};

// MouseBinding — a mouse gesture bound to a command (WPF
// System.Windows.Input MouseBinding).
class VAML_CORE_EXPORT MouseBinding final : public InputBinding
{
public:
    explicit MouseBinding(MouseGesture gesture);

    [[nodiscard]] const InputGesture& gesture() const override;
    [[nodiscard]] const MouseGesture& mouse_gesture() const noexcept;

    MouseBinding& with_command(ICommand* command);
    MouseBinding& with_parameter(Value parameter);

private:
    MouseGesture gesture_;
};

} // namespace vaml
