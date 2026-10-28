// S320 + S321 — RoutedEvent identity/strategy registry + the boiling routed-
// event engine. See RoutedEvent.h for the contract. This TU owns the S320
// `registered()` registry + the S321 UIElement routed-handler surface +
// `RaiseEvent` (bubbling/tunneling/handled over the real parent chain).

#include "vaml/core/RoutedEvent.h"

#include "vaml/core/UIElement.h"
#include "vaml/core/Intern.h"

#include <deque>
#include <utility>

namespace vaml {

// ---------------------------------------------------------------------------
// S320 — the RoutedEvent registry (content-keyed by the interned name)
// ---------------------------------------------------------------------------
namespace {

// The S320 registry: interned name -> stored RoutedEvent. Content-keyed, so a
// caller's name compares BY CONTENT (never pointer-identity — the S140/S143
// discipline). ADR-018: single-threaded (routed events are registered on the
// UI/affinity thread during startup/load), ZERO new locks.
struct RoutedEventRegistry
{
    const RoutedEvent& get(std::string_view name, RouteEventDirection dir)
    {
        const std::string_view key = vaml::intern(name);
        const auto it = byName_.find(key);
        if (it != byName_.end())
        {
            if (it->second.first != dir)
            {
                throw std::logic_error(
                    "RoutedEvent '" + std::string(name) +
                    "' is already registered with a different routing strategy");
            }
            return *it->second.second;
        }

        // Register a fresh event. The stored RoutedEvent lives in a STABLE,
        // process-lifetime pool (a Meyers-static deque — INSERTION NEVER
        // INVALIDATES references, unlike a vector's reallocation) so the
        // returned const reference + the map's stored pointers are never
        // invalidated by a later registration. We store (direction, ptr).
        static std::deque<RoutedEvent> events;
        events.emplace_back(key.data(), dir);
        RoutedEvent* ev = &events.back();
        byName_.emplace(key, std::make_pair(dir, ev));
        return *ev;
    }

private:
    std::unordered_map<std::string_view, std::pair<RouteEventDirection, RoutedEvent*>> byName_;
};

// A single Meyers-static registry instance (no static-init-order hazard — the
// intern pool + this registry are both function-local).
RoutedEventRegistry& routed_event_registry()
{
    static RoutedEventRegistry reg;
    return reg;
}

} // namespace

VAML_CORE_EXPORT
const RoutedEvent& RoutedEvent::registered(std::string_view name, RouteEventDirection dir)
{
    return routed_event_registry().get(name, dir);
}

// The registered BUBBLING `Click` event (S320). S324 raises it on a click;
// S322 wires markup `Click="..."` to it.
VAML_CORE_EXPORT
const RoutedEvent& ClickEvent() noexcept
{
    return RoutedEvent::registered("Click", RouteEventDirection::Bubble);
}

VAML_CORE_EXPORT
const RoutedEvent& KeyDownEvent() noexcept
{
    return RoutedEvent::registered("KeyDown", RouteEventDirection::Bubble);
}

VAML_CORE_EXPORT
const RoutedEvent& KeyUpEvent() noexcept
{
    return RoutedEvent::registered("KeyUp", RouteEventDirection::Bubble);
}

// S463 — the authored-set events (Window Loaded/Closing + TextBox TextChanged).
VAML_CORE_EXPORT
const RoutedEvent& LoadedEvent() noexcept
{
    return RoutedEvent::registered("Loaded", RouteEventDirection::Bubble);
}

VAML_CORE_EXPORT
const RoutedEvent& ClosingEvent() noexcept
{
    return RoutedEvent::registered("Closing", RouteEventDirection::Bubble);
}

VAML_CORE_EXPORT
const RoutedEvent& TextChangedEvent() noexcept
{
    return RoutedEvent::registered("TextChanged", RouteEventDirection::Bubble);
}

} // namespace vaml
