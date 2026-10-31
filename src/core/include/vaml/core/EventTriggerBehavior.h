#pragma once
// VAML EventTrigger — the Microsoft.Xaml.Behaviors interactivity EventTrigger
// (S502, Sprint 11f). WPF/GROUND TRUTH (Microsoft.Xaml.Behaviors
// EventTrigger.cs): the interactivity `EventTrigger : TriggerBase` holds an
// `Actions` collection (`TriggerAction` list) + an `EventName`, and when the
// named routed event raises on the associated element it invokes each action in
// order. This is the S11k-5 "Actions payload + TriggerAction::Invoke evaluation"
// resolved at 11f (the `i:` interactivity surface — NOT the System.Windows
// Style.Triggers EventTrigger, which stays the S559 NO-GAP token).
//
// THIS EventTrigger is a BEHAVIOR (a TriggerBase is an interaction unit living
// in a BehaviorCollection, so it derives Behavior — WPF-faithful: the
// interactivity TriggerBase attaches to an element via a BehaviorCollection).
// On attach it subscribes the element's routed event named by `EventName`; when
// the event raises it fires each owned action's `invoke(parameter)` in order.
//
// Surface:
//   - `EventName` (String) — the event to subscribe (e.g. "Click"). Resolved
//     against the S320 RoutedEvent registry (Click/KeyDown/KeyUp/Loaded/...);
//     an unknown name reports via the binding-diagnostics channel (never silent)
//     and does NOT crash attach.
//   - `add_action(owned TriggerAction)` — the ordered action list (WPF `Actions`).
//   - on_attach subscribes + on_detach unsubscribes the routed handler.
//
// Real + permanent (NOT MINIMAL REAL). ADR-018: zero locks (single-threaded on
// the element's affinity thread, exactly like the routed-event engine).

#include "vaml/core/Behavior.h"
#include "vaml/core/RoutedEvent.h"    // RoutedEvent / RoutedEventArgs (S320/S321)
#include "vaml/core/TriggerAction.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <vector>

namespace vaml {

class VAML_CORE_EXPORT InteractivityEventTrigger final : public Behavior
{
public:
    InteractivityEventTrigger();
    ~InteractivityEventTrigger() override;

    InteractivityEventTrigger(const InteractivityEventTrigger&) = delete;
    InteractivityEventTrigger& operator=(const InteractivityEventTrigger&) = delete;

    // The event to subscribe (e.g. "Click"). Set BEFORE attach. An unknown
    // event name is reported (not silent) and does NOT throw on attach.
    void set_event_name(std::string name);
    [[nodiscard]] const std::string& event_name() const noexcept { return eventName_; }

    // Adds an owned action to the ordered list (WPF `Actions`).
    [[nodiscard]] TriggerAction* add_action(std::unique_ptr<TriggerAction> action);

    // The owned actions in add order.
    [[nodiscard]] std::size_t action_count() const noexcept { return actions_.size(); }
    [[nodiscard]] std::vector<TriggerAction*> actions() const noexcept;

    // Tests: the number of times the subscribed routed event has been observed
    // (i.e. how many invocations drove the actions). Observable after a raise.
    [[nodiscard]] std::size_t fired_count() const noexcept { return fired_; }

protected:
    void on_attach() override;
    void on_detach() noexcept override;

private:
    std::string                       eventName_;
    std::vector<std::unique_ptr<TriggerAction>> actions_;
    UIElement::RoutedHandlerToken     handlerToken_ = 0;
    bool                              subscribed_  = false;
    std::size_t                       fired_       = 0;

    void on_event(RoutedEventArgs& args);
};

} // namespace vaml
