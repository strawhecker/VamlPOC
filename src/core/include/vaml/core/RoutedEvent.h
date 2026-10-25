#pragma once
// VAML RoutedEvent — the routed-event identity + strategy (S320, dep:S292),
// plus the ROUTED-EVENT ENGINE (S321, dep:S320): bubbling + tunneling +
// `handled` over the REAL parent chain.
//
// ============================================================================
// S320 — the ROUTED-EVENT PRIMITIVE (this header)
// ============================================================================
// `vaml::RoutedEvent` is the ADR-005 "XAML-faithful routed events
// (bubble/tunnel/handled)" identity object: a NAME + a ROUTE STRATEGY. It is
// the WPF `RegisterRoutedEvent` analogue — a real, permanent registry (the
// S320 title: "RoutedEvent primitive: Signal<T> + token + route-policy hook;
// reentrancy-safe add/remove"). The ADR-005 Signal<T> primitive (Signal.h,
// S320) is the handler-list mechanism; the strategy (Bubble vs Tunnel) is the
// route-policy hook this header declares (S321 realizes it over the parent
// chain).
//
// `RoutedEvent::registered(name, direction)` looks up (or registers) the
// event type carrying that name + strategy — the identity is content-keyed by
// the interned name (S140 discipline), and a repeated registration of the SAME
// name + strategy is idempotent (returns the existing); a repeated registration
// with a CONFLICTING strategy throws (a name may not be both Bubbling and
// Tunneling — that would be ambiguous).
//
// `RoutedEventArgs` is the LIVE args object a raise flows through: it carries
// `source` (the element that raised / is being routed around) + the living
// `handled` flag the route honors. S321 consumes it (a handler setting
// `handled = true` stops the route).
//
// READ-ONLY EVENT IDENTITY SURFACE — the events the markup author writes
// (S322) resolve through the registry: `vaml::ClickEvent` (Bubbling) and
// `vaml::KeyDownEvent`/`vaml::KeyUpEvent` (Bubbling, S325) are registered
// statics. See RoutedEvent.cpp.

// ============================================================================
// S321 — the ROUTED-EVENT ENGINE (bubbling + tunneling + handled)
// ============================================================================
// THIS step (S321) realizes the S320 route-policy hook over the REAL parent
// chain (`DependencyObject::parent()` — the S039/S041 parent link, and the
// S121 visual tree). `UIElement` gains the per-event routed-handler surface
// (`add_routed_handler` / `remove_routed_handler`, backed by the Signal<T>
// primitive per event) + `UIElement::RaiseEvent` — the breathing engine:
//
//   BUBBLING:   the route walks from `source` UP the parent() chain to the
//               root; each element's handlers for that event fire in
//               registration order; a handler setting `args.handled = true`
//               STOPS the bubble (later ancestors AND later handlers of the
//               same element do not fire — WPF-faithful).
//   TUNNELING:  the route walks from the ROOT DOWN to `source` (the preview
//               pass); `handled` set during the tunnel STOPS the tunnel (the
//               source / bubble never sees it).
//
// `args.source` = the element the route is currently visiting (the element
// whose handlers are firing); `args.originalSource` = the element the raise
// started from (never changes). Registration order per element is preserved
// (Signal<T> ordering).
//
// LIFETIME + DESTRUCTION: a routed handler holds a raw `UIElement*` (or a
// std::function capturing whatever the caller wires). The UIElement that OWNS
// the handlers is the one being routed; its `destroyed_` notification (S245)
// is NOT auto-unsubscribed here — the caller (S322 markup wiring, S329_1)
// manages handler lifetime, which matches the app's element lifetime
// (the markup-wired handlers are per-Load reader-owned; the reader outlives
// the tree). This is the honest scope for a routed engine where the tree is
// owned by the reader/panel that routes.

#include "vaml/core/Signal.h"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vaml {

// The route a routed event travels. Bubbling = source UP to the root (the
// `Click`/`KeyDown` default); Tunneling = the PREVIEW pass, ROOT DOWN to the
// source. S321 honors this per event.
enum class RouteEventDirection : std::uint8_t
{
    Bubble  = 0, // source -> ancestors -> root
    Tunnel  = 1, // root -> ... -> source (the preview pass)
};

// The LIVE args a raise flows through. `source` is the element currently being
// routed around (its handlers fire); `originalSource` NEVER changes + is the
// element the raise started from. `handled` is the living WPF-faithful flag:
// when a handler sets it true, the route stops (S321). Set by the raiser /
// dispatcher; a handler MUTATES `handled` to stop the climb.
struct RoutedEventArgs
{
    // Polymorphic (virtual dtor) so a ROUTED-handler can dynamic_cast back to a
    // derived args (e.g. KeyRoutedEventArgs, S325) to read event-specific
    // fields while the S321 engine passes it as RoutedEventArgs&.
    virtual ~RoutedEventArgs() = default;

    // The element the route is currently visiting (its handlers are firing).
    void* source = nullptr;
    // The element the raise started from — never changes.
    void* originalSource = nullptr;
    // The living stop-flag. A handler sets this true to stop the route (S321).
    bool handled = false;
};

// The routed-event handled callback (S321). A handler receives the LIVE args
// and MAY set `args.handled = true` to stop the route (WPF-faithful). This is
// the S322 markup `Click="OnClick"` handler type. `UIElement::RoutedHandler` is
// this same type (the UIElement surface name); the S322 reader reference uses
// this alias.
using RoutedEventHandler = std::function<void(RoutedEventArgs&)>;

// The ROUTED-EVENT IDENTITY + STRATEGY (S320). WPF `RegisterRoutedEvent`
// analogue via a real content-keyed registry (see the header comment).
class VAML_CORE_EXPORT RoutedEvent
{
public:
    RoutedEvent() = default;

    // The S320 registry entry point — the WPF RegisterRoutedEvent analogue.
    // Returns the (single) RoutedEvent registered under `name` with `dir`:
    //   - a fresh name: registers + returns a new event,
    //   - an existing name + the SAME direction: idempotent, returns it,
    //   - an existing name + a CONFLICTING direction: throws (ambiguous).
    [[nodiscard]] static const RoutedEvent& registered(std::string_view name,
                                                       RouteEventDirection dir);

    // The event's identity name (interned; content-equal view).
    [[nodiscard]] std::string_view name() const noexcept
    {
        return name_ != nullptr ? std::string_view(name_) : std::string_view();
    }

    // The event's route strategy.
    [[nodiscard]] RouteEventDirection direction() const noexcept { return dir_; }

    // A bare identity constructor (name + strategy). Consumers SHOULD use the
    // `registered()` registry / statics (`ClickEvent`, `KeyDownEvent`,
    // `KeyUpEvent`) so the identity is shared; a directly-constructed event is
    // only usable for its own raise (name/direction), never for registry
    // identity. Declared public so the internal registry can construct the
    // stored instances.
    constexpr RoutedEvent(const char* name, RouteEventDirection dir)
        : name_(name), dir_(dir)
    {
    }

private:
    const char*          name_ = nullptr;
    RouteEventDirection  dir_  = RouteEventDirection::Bubble;
};

// The registered BUBBLING `Click` routed event (S320; S322 wires markup
// `Click="..."` to handlers on this event; S324 raises it on a click).
[[nodiscard]] const RoutedEvent& ClickEvent() noexcept;

// The registered BUBBLING keyboard routed events (S325). `KeyDown`/`KeyUp` are
// raised on the focused element and bubble (a handled key stops the climb).
[[nodiscard]] const RoutedEvent& KeyDownEvent() noexcept;
[[nodiscard]] const RoutedEvent& KeyUpEvent() noexcept;

} // namespace vaml
