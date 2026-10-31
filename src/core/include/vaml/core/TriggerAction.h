#pragma once
// VAML TriggerAction — the Microsoft.Xaml.Behaviors action model (S502, Sprint
// 11f). WPF/GROUND TRUTH (Microsoft.Xaml.Behaviors TriggerBase/TriggerAction):
// an action is the payload a trigger FIRES when its condition matches. The
// WPF-faithful split:
//   - `TriggerBase` (System.Windows) — the `Style.Triggers` EventTrigger's base
//     (`TriggerBase : DependencyObject`; the S362 model + the 11k-5 container).
//   - `TriggerAction` (System.Windows, Style.Triggers action family) — the
//     action base (`BeginStoryboard`'s base, the S362 SetterValue.TemplateBinding
//     family).
//   - `TriggerAction` (Microsoft.Xaml.Behaviors, interactivity family) — the
//     `InvokeCommandAction` base THIS header models (the `i:`/`behaviors:`/`dd:`
//     interaction surface S502 owns).
//
// THIS header is the Microsoft.Xaml.Behaviors interactivity `TriggerAction`:
// an owner-derived action object with `invoke(parameter)` — called when its
// owning trigger fires. Type-safe concrete actions (InvokeCommandAction)
// override `invoke`. Real + permanent (NOT MINIMAL REAL, NOT a stub).
//
// ADR-018: ZERO new locks (single-threaded on the element's affinity thread).

#include "vaml/core/DependencyObject.h" // TriggerAction IS-a DependencyObject (WPF-faithful)
#include "vaml/core/ICommand.h"         // ICommand (the invoke target of InvokeCommandAction)
#include "vaml/core/Value.h"            // Value (the invoke parameter)

namespace vaml {

// The interactivity `TriggerAction` base. An action is fired by a trigger
// (EventTrigger) when the trigger's condition matches — the action's `invoke`
// runs with the trigger's parameter.
class VAML_CORE_EXPORT TriggerAction : public DependencyObject
{
public:
    ~TriggerAction() override = default;

    TriggerAction(const TriggerAction&) = delete;
    TriggerAction& operator=(const TriggerAction&) = delete;

    // Runs the action with `parameter` (WPF `TriggerAction.Invoke(param)`).
    // Concrete actions override invoke; the base is a no-op (an action with no
    // concrete behavior). Pure; no-throw contract is the concrete action's.
    virtual void invoke(const Value& parameter) { (void)parameter; }

protected:
    TriggerAction() = default;
};

// Trivial predicates reused by the discrimination seams + tests.
[[nodiscard]] VAML_CORE_EXPORT const char* trigger_action_kind_name();

} // namespace vaml
