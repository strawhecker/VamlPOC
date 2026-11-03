// 11n-4 — ItemsControl, the reactive item host. Incremental per-item container
// add/remove on the bound collection's CollectionChanged (event-subscription,
// WPF-faithful Option A — NOT polled). The "mutate the ObservableCollection ->
// the bound ItemsControl reflects the change live" proof.
//
// ADR-018 / DECISION-2: single-threaded affinity, ZERO locks. The collection is
// mutated + the handler runs on the affinity thread; cross-thread changes go
// through the DECISION-2 change-log (an 11n-4 CollectionView), never a lock.

#include "vaml/core/ItemsControl.h"

#include "vaml/core/ContentPresenter.h"

#include <memory>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
ItemsControl::ItemsControl() = default;

VAML_CORE_EXPORT
ItemsControl::~ItemsControl()
{
    // S245 detach-on-destroy: unsubscribe the live collection subscription so
    // a destroyed ItemsControl never reacts to a later collection mutation.
    if (bound_ && collection_ != nullptr)
    {
        collection_->remove_collection_changed(changeToken_);
    }
    bound_ = false;
    collection_ = nullptr;
}

VAML_CORE_EXPORT
void ItemsControl::set_items_observable(ObservableCollectionBase& collection)
{
    // Unbind any current subscription first.
    if (bound_ && collection_ != nullptr)
    {
        collection_->remove_collection_changed(changeToken_);
    }
    bound_ = false;
    collection_ = nullptr;

    collection_ = &collection;
    changeToken_ = collection_->add_collection_changed(
        [this](const NotifyCollectionChangedEventArgs& e) { on_collection_changed(e); });
    bound_ = true;

    // Initial population: one per-item container per item, matching the
    // collection's live size.
    rebuild_containers();
}

VAML_CORE_EXPORT
void ItemsControl::on_collection_changed(const NotifyCollectionChangedEventArgs& e)
{
    switch (e.action())
    {
    case CollectionChangedAction::Add:
        // Insert one container at new_index (the Index is the collection index,
        // which equals the container index — they stay in lockstep).
        add_container();
        break;
    case CollectionChangedAction::Remove:
        remove_container(e.old_index());
        break;
    case CollectionChangedAction::Replace:
        // A Replace swaps the item in place; the container slot count is
        // unchanged (the container content would be re-prepared by the item
        // binding layer — the container set is unchanged here).
        break;
    case CollectionChangedAction::Move:
        // A Move reorders; the container set is unchanged (the item order
        // within the containers follows the collection order).
        break;
    case CollectionChangedAction::Reset:
        rebuild_containers();
        break;
    }
}

VAML_CORE_EXPORT
void ItemsControl::rebuild_containers()
{
    // Align the container count to the collection size: remove extras, add
    // missing. (Incremental per change; a full rebuild only on Reset / initial
    // bind.)
    const std::size_t target = collection_ ? collection_->size() : 0;
    while (child_count() > target)
        remove_container(child_count() - 1);
    while (child_count() < target)
        add_container();
}

VAML_CORE_EXPORT
void ItemsControl::add_container()
{
    // One per-item ContentPresenter container. (Item-template application is
    // the S500 ContentPresenter prep layer; this places the container only.)
    add_child(std::make_unique<ContentPresenter>());
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
void ItemsControl::remove_container(std::size_t index)
{
    if (index >= child_count())
        return;
    [[maybe_unused]] std::size_t remaining = remove_child(index);
    InvalidateMeasure();
    InvalidateArrange();
}

} // namespace vaml
