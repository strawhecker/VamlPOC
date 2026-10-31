// S502 — InteractivityEventTrigger (the Microsoft.Xaml.Behaviors interactivity
// EventTrigger behavior): subscribes the element's routed event + fires each
// owned TriggerAction on the raise. Named `InteractivityEventTrigger` to
// DISTINGUISH it from the System.Windows Style.Triggers `EventTrigger` (S362/11k-5).

#include "vaml/core/EventTriggerBehavior.h"

#include "vaml/core/BindingDiagnostics.h" // binding_diagnostics channel (unknown event, never silent)

namespace vaml {

InteractivityEventTrigger::InteractivityEventTrigger() = default;

InteractivityEventTrigger::~InteractivityEventTrigger()
{
    // If still attached, detach (unsubscribes the routed handler) so no dangling
    // subscription survives before the owned actions are freed.
    detach();
}

std::vector<TriggerAction*> InteractivityEventTrigger::actions() const noexcept
{
    std::vector<TriggerAction*> out;
    out.reserve(actions_.size());
    for (const auto& a : actions_)
    {
        out.push_back(a.get());
    }
    return out;
}

TriggerAction* InteractivityEventTrigger::add_action(std::unique_ptr<TriggerAction> action)
{
    TriggerAction* raw = action.get();
    actions_.push_back(std::move(action));
    return raw;
}

void InteractivityEventTrigger::set_event_name(std::string name)
{
    eventName_ = std::move(name);
}

namespace {

// Resolves an authored event name to the registered RoutedEvent (the S322/
// BindEvents table reused — the S320 registry). Returns nullptr for an unknown
// name (reported by the caller, never silent).
const RoutedEvent* resolve_named_event(std::string_view name)
{
    if (name == "Click")       { return &ClickEvent(); }
    if (name == "KeyDown")     { return &KeyDownEvent(); }
    if (name == "KeyUp")       { return &KeyUpEvent(); }
    if (name == "Loaded")      { return &LoadedEvent(); }
    if (name == "Closing")     { return &ClosingEvent(); }
    if (name == "TextChanged") { return &TextChangedEvent(); }
    return nullptr;
}

} // namespace

void InteractivityEventTrigger::on_attach()
{
    if (subscribed_)
    {
        return;
    }
    UIElement* element = associated_object();
    if (element == nullptr)
    {
        return;
    }
    const RoutedEvent* event = resolve_named_event(eventName_);
    if (event == nullptr)
    {
        // Unknown event name — report via the S250_1 binding-diagnostics
        // channel (one channel, two consumers), never silent. Not a throw (a
        // future event may not yet be registered).
        BindingDiagnostic diag;
        diag.targetProp  = eventName_;
        diag.reasonCode  = BindingReasonCode::SetTargetFailed;
        diag.sourcePath  = eventName_;
        diag.direction   = BindingDirection::OneWay;
        diag.detail      = "event_trigger: unknown event name '" + eventName_ + "'";
        vaml::binding_diagnostics().emit(diag);
        return;
    }
    handlerToken_ = element->add_routed_handler(
        *event, [this](RoutedEventArgs& args) { on_event(args); });
    subscribed_ = true;
}

void InteractivityEventTrigger::on_detach() noexcept
{
    if (!subscribed_)
    {
        return;
    }
    UIElement* element = associated_object();
    if (element != nullptr)
    {
        const RoutedEvent* event = resolve_named_event(eventName_);
        if (event != nullptr)
        {
            (void)element->remove_routed_handler(*event, handlerToken_);
        }
    }
    subscribed_ = false;
}

void InteractivityEventTrigger::on_event(RoutedEventArgs& /*args*/)
{
    // WPF-faithful: each action invokes with the trigger's parameter (here the
    // Null default — the concrete InvokeCommandAction carries its own
    // CommandParameter override). Fire in add order.
    ++fired_;
    const Value parameter = Value::Null();
    for (auto& action : actions_)
    {
        action->invoke(parameter);
    }
}

} // namespace vaml
