// S512a — EventTriggerAction: the LIVE evaluation of the 11k-5 EventTrigger
// Actions payload (the routed event fires -> BeginStoryboard begins its
// Storyboard, which ticks per the S510 clock). See EventTriggerAction.h.

#include "vaml/core/EventTriggerAction.h"

#include "vaml/core/BeginStoryboard.h" // BeginStoryboard (the S512a concrete action)

namespace vaml {

// ---------------------------------------------------------------------------
// Resolving an EventTrigger's eventName ("Button.Click" / "Click" / "Loaded")
// to a registered RoutedEvent. WPF-faithful: a Style/EventTrigger references a
// RoutedEvent by its NAME (which may be a control-qualified or bare name). The
// S320 registry holds the known identities (ClickEvent(), LoadedEvent(), …);
// the trigger's eventName (the LAST "."-segmented token, WPF-style) maps onto
// the matching registered identity. An UNKNOWN event name is a no-op
// (the trigger cannot subscribe a route it does not know — recorded, not
// silent; the author's event must be a registered one).
static const RoutedEvent* resolve_routed_event(std::string_view eventName) noexcept
{
    // Take the last "."-segmented token: "Button.Click" -> "Click".
    const auto dot = eventName.rfind('.');
    if (dot != std::string_view::npos)
        eventName = eventName.substr(dot + 1);
    // The known registered identities (each Bubbling).
    if (eventName == "Click")
        return &ClickEvent();
    if (eventName == "Loaded")
        return &LoadedEvent();
    if (eventName == "KeyDown")
        return &KeyDownEvent();
    if (eventName == "KeyUp")
        return &KeyUpEvent();
    if (eventName == "Closing")
        return &ClosingEvent();
    if (eventName == "TextChanged")
        return &TextChangedEvent();
    return nullptr;
}

// ---------------------------------------------------------------------------
// EventTriggerHandle
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void EventTriggerHandle::detach() noexcept
{
    if (element != nullptr && token && event != nullptr)
    {
        (void)element->remove_routed_handler(*event, token);
        element = nullptr;
        token = {};
        event = nullptr;
    }
}

VAML_CORE_EXPORT
EventTriggerHandle::~EventTriggerHandle()
{
    detach();
}

VAML_CORE_EXPORT
EventTriggerHandle::EventTriggerHandle(EventTriggerHandle&& o) noexcept
    : element(o.element), token(o.token), event(o.event), actions(std::move(o.actions))
{
    o.element = nullptr;
    o.token = {};
    o.event = nullptr;
}

VAML_CORE_EXPORT
EventTriggerHandle& EventTriggerHandle::operator=(EventTriggerHandle&& o) noexcept
{
    if (this != &o)
    {
        detach();
        element = o.element;
        token = o.token;
        event = o.event;
        actions = std::move(o.actions);
        o.element = nullptr;
        o.token = {};
        o.event = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// attach_event_triggers
// ---------------------------------------------------------------------------
namespace {

// Primes + attaches ONE BeginStoryboard action to the subtree, so its invoke()
// can resolve targets via `resolveName` against `root`.
void prepare_action(TriggerAction* action, UIElement* root,
                    const Storyboard::NameResolver& resolveName)
{
    if (action == nullptr)
        return;
    auto* begin = dynamic_cast<BeginStoryboard*>(action);
    if (begin != nullptr)
    {
        begin->attach(root, resolveName);
    }
}

} // namespace

VAML_CORE_EXPORT
std::vector<EventTriggerHandle>
attach_event_trigger(UIElement* element, const EventTrigger& et,
                     const Storyboard::NameResolver& resolveName)
{
    std::vector<EventTriggerHandle> out;
    if (element == nullptr)
    {
        return out;
    }
    const RoutedEvent* ev = resolve_routed_event(et.eventName);
    if (ev == nullptr)
    {
        // Unknown routed event: no subscription (recorded, never silent). The
        // handle is empty; the caller sees an empty handle for this trigger.
        out.emplace_back();
        return out;
    }
    // Determine the SOURCE element: default = the element owning the triggers;
    // SourceName = the named element (resolved via the name resolver).
    UIElement* source = element;
    if (!et.sourceName.empty() && resolveName)
    {
        UIElement* named = resolveName(std::string(et.sourceName));
        if (named != nullptr)
            source = named;
    }

    // The actions fire in collection order on the routed event.
    std::vector<std::shared_ptr<TriggerAction>> owned = et.actions;
    for (auto& a : owned)
    {
        prepare_action(a.get(), element, resolveName);
    }

    EventTriggerHandle h;
    h.element = source;
    h.event = ev;
    h.actions = std::move(owned);
    h.token = source->add_routed_handler(*ev, [hActions = h.actions](RoutedEventArgs& args) {
        (void)args;
        for (const auto& a : hActions)
        {
            if (a)
                a->invoke(Value::Null());
        }
    });
    out.push_back(std::move(h));
    return out;
}

VAML_CORE_EXPORT
std::vector<EventTriggerHandle>
attach_event_triggers(UIElement* element,
                      const std::vector<Trigger>& triggers,
                      const Storyboard::NameResolver& resolveName)
{
    std::vector<EventTriggerHandle> out;
    if (element == nullptr)
    {
        return out;
    }
    for (const Trigger& t : triggers)
    {
        if (trigger_kind(t) != TriggerKind::Event)
        {
            // Property/Data triggers are the S363 property-applier's (untouched).
            continue;
        }
        auto handles = attach_event_trigger(element, t.event, resolveName);
        for (auto& h : handles)
            out.push_back(std::move(h));
    }
    return out;
}

} // namespace vaml
