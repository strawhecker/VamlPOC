#pragma once
// VAML Behavior — the Microsoft.Xaml.Behaviors behavior model (S502, Sprint 11f).
//
// WPF/GROUND TRUTH (verified against the Microsoft.Xaml.Behaviors
// Interaction/Behavior.cs + Behavior<T>.cs): a `Behavior` is the interactivity
// unit that ATTACHES to an element and adds behavior via the element's routed/
// event surface. Behaviors are held in a `BehaviorCollection` on the element's
// `i:Interaction.Behaviors` attached property. This header is the VAML mapping
// of that model, made REAL + permanent (NOT MINIMAL REAL, NOT a stub):
//
//   - `Behavior`            — the abstract base: attach_to(UIElement)/detach +
//                             the associated object + attached flag.
//   - `TypedBehavior<T>`    — the typed behavior base (the VAML mapping of
//                             WPF-faithful `Behavior<T> : Behavior` — the C++
//                             name `Behavior<T>` cannot coinhabit the namespace
//                             of the non-template `Behavior`, so the typed form
//                             is `TypedBehavior<T>`; the DAG_controls row
//                             `Behavior<T>` maps to it): `associated_object()`
//                             returns the TYPED element (`Behavior<T>.AssociatedObject`).
//   - `BehaviorCollection`  (in BehaviorCollection.h) — the ordered owned
//                             behavior list on an element (the `i:Interaction.Behaviors`
//                             value); `attach_to` attaches every behavior to the element.
//
// The S11k-5 EventTrigger forward token (10e/11f/11g) resolves the concrete
// action payload HERE at 11f: an `EventTrigger` (a TriggerBase-derived behavior)
// subscribes the element's routed event and fires its ordered `TriggerAction`
// list when the event raises (see TriggerAction.h / InvokeCommandAction.h).
//
// ADR-018: ZERO new locks (single-threaded on the element's affinity thread,
// exactly like the routed-event engine S321).

#include "vaml/core/DependencyObject.h" // Behavior IS a DependencyObject (resource/inspectable)
#include "vaml/core/UIElement.h"        // the associated element (UIElement)

#include <memory>
#include <type_traits>

namespace vaml {

// The abstract behavior base. A behavior ATTACHES to ONE element (`attach_to`)
// and DETACHES (`detach`) — WPF-faithful attach/detach lifecycle. When
// attached, `is_attached()` is true and `associated_object()` is the element.
class VAML_CORE_EXPORT Behavior : public DependencyObject
{
public:
    ~Behavior() override = default;

    Behavior(const Behavior&) = delete;
    Behavior& operator=(const Behavior&) = delete;

    // Attaches this behavior to `element` (the WPF `Behavior.Attach`). The
    // behavior then observes/acts via `element`'s surface. A behavior already
    // attached to ANOTHER element must be detached first (a behavior has one
    // associated object). Concrete behaviors implement on_attach; the base
    // records the associated element + the attached flag.
    void attach_to(UIElement* element);
    // Detaches from the associated element (the WPF `Behavior.Detach`).
    // Idempotent + no-throw on a not-attached behavior. Concrete behaviors
    // implement on_detach; the base clears the associated element + flag.
    void detach() noexcept;

    // True when attached to an element. Never throws.
    [[nodiscard]] bool is_attached() const noexcept { return attached_; }
    // The associated element (the WPF `Behavior.AssociatedObject`).
    [[nodiscard]] UIElement* associated_object() const noexcept { return associated_; }

protected:
    Behavior() = default;

    // The concrete attach hook (called by attach_to AFTER the associated
    // element is recorded, so on_attach can read associated_object()).
    virtual void on_attach() {}
    // The concrete detach hook (called by detach BEFORE the associated element
    // is cleared, so on_detach can read associated_object()).
    virtual void on_detach() noexcept {}

private:
    UIElement* associated_ = nullptr;
    bool       attached_   = false;
};

// The typed behavior base (the VAML mapping of WPF `Behavior<T> : Behavior`, T =
// the associated element type). `associated_object()` returns the TYPED element
// (the WPF `Behavior<T>.AssociatedObject : T`).
template <class T>
class TypedBehavior : public ::vaml::Behavior
{
public:
    static_assert(std::is_base_of<UIElement, T>::value,
                  "TypedBehavior<T>: T must derive from UIElement");

    // The TYPED associated object (the WPF `AssociatedObject : T`). nullptr
    // when not attached; derives from the base UIElement* via dynamic_cast.
    [[nodiscard]] T* associated_object() noexcept
    {
        return dynamic_cast<T*>(::vaml::Behavior::associated_object());
    }
    [[nodiscard]] const T* associated_object() const noexcept
    {
        return dynamic_cast<const T*>(::vaml::Behavior::associated_object());
    }
};

} // namespace vaml
