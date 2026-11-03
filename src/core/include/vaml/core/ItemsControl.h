#pragma once
// VAML ItemsControl — the reactive item host (WPF System.Windows.Controls.ItemsControl).
// 11n-4 — DAG anchor `ItemsControl`, the live incremental container model.
//
// WPF-faithful: an ItemsControl holds an item collection and renders ONE
// per-item container (a ContentPresenter child) for each item. The DAG anchor
// is delivered HERE as a REAL UIElement that INCREMENTALLY adds/removes its
// per-item children on the source collection's CollectionChanged — the "mutate
// the ObservableCollection -> the bound ItemsControl reflects the change live"
// reactivity the roadmap's test closes with. This is event-subscription
// (CollectionChanged), NOT polled — WPF-faithful Option A.
//
// Lifetime contract (matches the established S242/S243 BindingExpression raw-
// pointer contract, documented): an ItemsControl must not outlive its bound
// ObservableCollection. It subscribes to the collection's CollectionChanged;
// on its own destruction it unsubscribes (the S245 detach-on-destroy
// discipline) so a destroyed ItemsControl never reacts to a later collection
// mutation. Single-threaded affinity — no locks (ADR-018 / DECISION-2).

#include "vaml/core/ObservableCollection.h" // the observable source (+ INotifyCollectionChanged)
#include "vaml/core/UIElement.h"
#include "vaml/core/export.h"

#include <cstddef>

namespace vaml {

class ContentPresenter;

class VAML_CORE_EXPORT ItemsControl : public UIElement
{
public:
    ItemsControl();
    ~ItemsControl() override;

    ItemsControl(const ItemsControl&) = delete;
    ItemsControl& operator=(const ItemsControl&) = delete;

    // Binds the ItemsControl to a live observable collection. Subscribes to its
    // CollectionChanged and INCREMENTALLY adds/removes one per-item container
    // per change (no full rebuild). The control takes a NON-OWNING reference
    // (the caller owns the collection; the control must not outlive it). Passing
    // nullptr unbinds (removes all containers + unsubscribes).
    void set_items_observable(ObservableCollectionBase& collection);

    // Reactive container count — the observable proof: equals the bound
    // collection's size and tracks it live (Add->+1, Remove->-1, Reset->rebuild).
    [[nodiscard]] std::size_t item_container_count() const noexcept { return child_count(); }

    // True when currently bound to a collection (live subscription armed).
    [[nodiscard]] bool bound() const noexcept { return bound_; }

private:
    // The CollectionChanged handler (fires on the affinity thread, single
    // thread). Applies the delta incrementally.
    void on_collection_changed(const NotifyCollectionChangedEventArgs& e);

    // Rebuilds the per-item containers to match the collection's current size
    // (used by Reset and by initial bind).
    void rebuild_containers();

    // Appends one per-item ContentPresenter container.
    void add_container();

    // Removes the per-item container at `index`.
    void remove_container(std::size_t index);

    ObservableCollectionBase* collection_ = nullptr;
    CollectionChangedToken changeToken_ = 0;
    bool bound_ = false;
};

} // namespace vaml
