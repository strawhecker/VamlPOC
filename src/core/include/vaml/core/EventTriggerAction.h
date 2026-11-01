#pragma once
// VAML EventTriggerAction — the S512a evaluation seam (dep:S502/S510/S512a,
// Sprint 11g). resolves the 11k-5 recorded NO-GAP: the `EventTrigger`'s ACTIONS
// payload becomes LIVE.
//
// WPF-FAITHFUL: an `EventTrigger` (from `<Owner.Triggers>` or `<Style.Triggers>`)
// is a `TriggerActionCollection`-holding description that subscribes its
// RoutedEvent on a source (default = the owning element, or `SourceName`) via
// `UIElement::add_routed_handler`; when the event fires, the trigger invokes its
// owned `Actions` in collection order. A `BeginStoryboard` action begins its
// Storyboard (which then ticks per the S510 clock).
//
// This seam REPLACES the S363 StyleApplier's dormant `default:fires=false` for
// EventTriggers: Property/Data triggers keep the property-applier path
// (untouched); EventTriggers now evaluate through `attach_event_triggers`.
//
// Lifetime: the returned handle owns the routed-subscription so it can be
// removed; the caller (the reader/app) keeps it for the subtree's lifetime.
// ADR-018: ZERO new locks (single-threaded UI-thread subscription + fire).
//
// The ControllableStoryboardAction family (BeginStoryboard.StoryboardName +
// stop/pause cross-trigger control) is recorded NO-GAP (forward token) — not
// silent.

#include "vaml/core/RoutedEvent.h"  // the routed-event registry + ReturedEvent
#include "vaml/core/Style.h"        // Trigger / EventTrigger / TriggerKind
#include "vaml/core/Storyboard.h"   // Storyboard::NameResolver
#include "vaml/core/TriggerAction.h" // TriggerAction::invoke (the fire path)
#include "vaml/core/UIElement.h"     // UIElement::add_routed_handler
#include "vaml/core/export.h"

#include <memory>
#include <string>
#include <vector>

namespace vaml {

// Owns one live EventTrigger subscription: the routed handler on the source +
// the owning trigger's actions (shared ownership of the action set).
struct VAML_CORE_EXPORT EventTriggerHandle
{
    // One live routed subscription.
    UIElement*            element  = nullptr; // the source element (non-owning)
    UIElement::RoutedHandlerToken token{};     // the removal token (S321)
    // The event this handle subscribed (so detach removes the right one). When
    // null (no valid RoutedEvent resolved), the handle is empty and detach is a
    // no-op.
    const vaml::RoutedEvent* event = nullptr;
    std::vector<std::shared_ptr<TriggerAction>> actions; // the owned action set
    // Removes the routed subscription (detach). Called on destruction/teardown.
    void detach() noexcept;
    ~EventTriggerHandle();
    EventTriggerHandle() = default;
    EventTriggerHandle(EventTriggerHandle&&) noexcept;
    EventTriggerHandle& operator=(EventTriggerHandle&&) noexcept;
    EventTriggerHandle(const EventTriggerHandle&) = delete;
    EventTriggerHandle& operator=(const EventTriggerHandle&) = delete;
};

// Attaches every EventTrigger in `triggers` that applies to `element`, wiring
// the live routed subscription + the BeginStoryboard-driven action firing.
// `triggers` is the per-element trigger list (reader's element_triggers() for
// element-level, or a Style's triggers() for style-level). `resolveName` maps a
// target name (for BeginStoryboard's Storyboard targets + SourceName) to a live
// element; the reader's `named()` registry is the canonical provider.
//
// Returns the LIVE handles (each owns a routed subscription that stays armed
// until destroyed/detached). A Property/Data trigger is ignored here (the S363
// property-applier owns those). An EventTrigger with NO actions contributes a
// handle that still subscribes the routed event (observability, per the S321
// surface) — but BeginStoryboard actions are what fire.
[[nodiscard]] VAML_CORE_EXPORT std::vector<EventTriggerHandle>
attach_event_triggers(UIElement* element,
                      const std::vector<Trigger>& triggers,
                      const Storyboard::NameResolver& resolveName);

// Convenience: attach a single EventTrigger record to `element`.
[[nodiscard]] VAML_CORE_EXPORT std::vector<EventTriggerHandle>
attach_event_trigger(UIElement* element, const EventTrigger& et,
                     const Storyboard::NameResolver& resolveName);

} // namespace vaml
