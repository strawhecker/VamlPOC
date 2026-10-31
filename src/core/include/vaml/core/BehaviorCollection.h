#pragma once
// VAML BehaviorCollection — the ordered owned behavior list on an element
// (S502, Sprint 11f). WPF/GROUND TRUTH (Microsoft.Xaml.Behaviors
// Interaction/BehaviorCollection.cs): `BehaviorCollection` is the
// `IList<Behavior>` attached to an element via `i:Interaction.Behaviors`; the
// element OWNS the collection, and the collection OWNS its behaviors. THIS
// header is the VAML mapping — REAL + permanent (the S502 behaviors model, an
// 11f deliverable).
//
// ATTACH/DETACH contract (WPF-faithful — BehaviorCollection.Attach):
//   - `set_owner(element)` / the element's `i:Interaction.Behaviors` attached
//     property attach the collection to its owner;
//   - `add(behavior)` takes OWNERSHIP of a behavior AND attaches it to the
//     collection's owner (if already owned);
//   - `detach_all()` / the owner destructor detach every behavior (each is
//     detached from its associated element).
//
// The S11k-5 EventTrigger forward token resolves here: an EventTrigger behavior
// (a TriggerBase) lives in a BehaviorCollection and subscribes the owner's
// routed event. The reader authors `<i:Interaction.Behaviors>` onto an element
// (see VamlReader): each authored behavior child is built and added to the
// element's collection.
//
// ADR-018: ZERO new locks (single-threaded on the element's affinity thread).

#include "vaml/core/Behavior.h"
#include "vaml/core/UIElement.h"

#include <memory>
#include <vector>

namespace vaml {

class VAML_CORE_EXPORT BehaviorCollection
{
public:
    BehaviorCollection() = default;
    ~BehaviorCollection();

    BehaviorCollection(const BehaviorCollection&) = delete;
    BehaviorCollection& operator=(const BehaviorCollection&) = delete;
    BehaviorCollection(BehaviorCollection&&) noexcept;
    BehaviorCollection& operator=(BehaviorCollection&&) noexcept;

    // The owner element this collection is attached to. The reader sets it via
    // `i:Interaction.Behaviors`; a null owner means the collection is not yet
    // attached (behaviors are held but not attached until an owner lands).
    void set_owner(UIElement* owner) noexcept;
    [[nodiscard]] UIElement* owner() const noexcept { return owner_; }

    // Adds a behavior, taking OWNERSHIP. If the collection has an owner, the
    // behavior is attached to it immediately. Returns the raw pointer.
    [[nodiscard]] Behavior* add(std::unique_ptr<Behavior> behavior);

    // The owned behaviors in ADD order.
    [[nodiscard]] std::size_t count() const noexcept { return behaviors_.size(); }
    [[nodiscard]] std::vector<Behavior*> as_pointers() const noexcept;

    // Detaches every behavior from its associated element. Idempotent.
    void detach_all() noexcept;

private:
    UIElement*                       owner_     = nullptr;
    std::vector<std::unique_ptr<Behavior>> behaviors_;
};

// --- The `i:Interaction.Behaviors` ATTACHED surface --------------------------
// WPF-faithful: `i:Interaction.GetBehaviors(element)` / `SetBehaviors`. The
// reader routes an authored `<i:Interaction.Behaviors>` property-element onto
// the element's BehaviorCollection. The element owns ONE collection (non-owning
// owner pointer). ADR-018 zero locks.

// Gets (or lazily creates) the element's owned BehaviorCollection. The
// collection is held out-of-line (side-table keyed by element address) so a
// UIElement with no behaviors does not grow (the S145 object-pool budget —
// mirrors the S360 ResourceDictionary + S500 DataContext side-table). The entry
// is released by ~UIElement via remove_element_behaviors (mirrors the S360/S500
// side-table teardown), so its detach_all runs while the element is alive.
[[nodiscard]] VAML_CORE_EXPORT BehaviorCollection* get_element_behaviors(UIElement& element);
// True when the element carries a behavior collection (no lazy creation).
[[nodiscard]] VAML_CORE_EXPORT bool element_has_behaviors(const UIElement& element) noexcept;
// Releases the element's owned behavior collection (detaching each behavior).
// Called from ~UIElement so no dangling key survives into static teardown, and
// so the detach's remove_routed_handler runs while the element is alive.
VAML_CORE_EXPORT void remove_element_behaviors(UIElement* element) noexcept;

} // namespace vaml
