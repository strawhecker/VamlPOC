// VAML notification corpus + ObservableCollectionBase (11n-4). ADR-018 /
// DECISION-2: ZERO new locks — the mutation raises both WPF signals, run on
// the single affinity thread; cross-thread changes are the DECISION-2
// change-log (11n-4 infra), never a collection lock.

#include "vaml/core/ObservableCollection.h"

#include <utility>

namespace vaml {

// --- NotifyCollectionChangedEventArgs --------------------------------------
VAML_CORE_EXPORT
NotifyCollectionChangedEventArgs::NotifyCollectionChangedEventArgs(
    CollectionChangedAction action, std::size_t newIndex, Value newItem,
    std::size_t oldIndex, Value oldItem)
    : action_(action)
    , newIndex_(newIndex)
    , newItem_(std::move(newItem))
    , oldIndex_(oldIndex)
    , oldItem_(std::move(oldItem))
{}

VAML_CORE_EXPORT
NotifyCollectionChangedEventArgs::NotifyCollectionChangedEventArgs(
    CollectionChangedAction action)
    : action_(action)
    , newIndex_(0)
    , newItem_(Value::Null())
    , oldIndex_(0)
    , oldItem_(Value::Null())
{}

// --- ObservableCollectionBase ----------------------------------------------
VAML_CORE_EXPORT
CollectionChangedToken ObservableCollectionBase::add_collection_changed(CollectionChangedHandler handler)
{
    const CollectionChangedToken t = next_cc_++;
    collectionChanged_.emplace_back(t, std::move(handler));
    return t;
}

VAML_CORE_EXPORT
bool ObservableCollectionBase::remove_collection_changed(CollectionChangedToken token)
{
    for (auto it = collectionChanged_.begin(); it != collectionChanged_.end(); ++it)
    {
        if (it->first == token)
        {
            collectionChanged_.erase(it);
            return true;
        }
    }
    return false;
}

VAML_CORE_EXPORT
ObservableCollectionBase::PropertyChangedToken
ObservableCollectionBase::add_property_changed(PropertyId id, PropertyChangedHandler handler)
{
    return prop_axis_.add_property_changed(id, std::move(handler));
}

VAML_CORE_EXPORT
void ObservableCollectionBase::remove_property_changed(PropertyChangedToken token)
{
    prop_axis_.remove_property_changed(token);
}

VAML_CORE_EXPORT
void ObservableCollectionBase::fire_property_changed(PropertyId id, const Value& oldV, const Value& newV)
{
    (void)oldV;    // the S241 signal carries (id, old, new); the store Set path is (id, new)
    (void)prop_axis_.Set(id, newV);
}

VAML_CORE_EXPORT
void ObservableCollectionBase::raise_change(const NotifyCollectionChangedEventArgs& e)
{
    // WPF order: PropertyChanged(Count) + PropertyChanged(Item[]), THEN
    // CollectionChanged. The prop axis stores the new Count via Set.
    (void)prop_axis_.Set(PropertyId::of("Count"), Value::Int32(static_cast<std::int32_t>(items_.size())));
    (void)prop_axis_.Set(PropertyId::of("Item[]"), Value::Int32(static_cast<std::int32_t>(items_.size())));
    // copy-before-invoke (registration-order discipline)
    const auto copy = collectionChanged_;
    for (const auto& [t, h] : copy)
    {
        if (h)
            h(e);
    }
}

VAML_CORE_EXPORT
void ObservableCollectionBase::Add(Value item)
{
    items_.push_back(item);
    const std::size_t idx = items_.size() - 1;
    raise_change(NotifyCollectionChangedEventArgs(
        CollectionChangedAction::Add, idx, item, 0, Value::Null()));
}

VAML_CORE_EXPORT
void ObservableCollectionBase::Insert(std::size_t index, Value item)
{
    if (index > items_.size())
        index = items_.size();
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index), item);
    raise_change(NotifyCollectionChangedEventArgs(
        CollectionChangedAction::Add, index, item, 0, Value::Null()));
}

VAML_CORE_EXPORT
bool ObservableCollectionBase::Remove(const Value& item)
{
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        if (items_[i] == item)
        {
            const Value old = items_[i];
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
            raise_change(NotifyCollectionChangedEventArgs(
                CollectionChangedAction::Remove, 0, Value::Null(), i, old));
            return true;
        }
    }
    return false;
}

VAML_CORE_EXPORT
void ObservableCollectionBase::RemoveAt(std::size_t index)
{
    if (index >= items_.size())
        return;
    const Value old = items_[index];
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
    raise_change(NotifyCollectionChangedEventArgs(
        CollectionChangedAction::Remove, 0, Value::Null(), index, old));
}

VAML_CORE_EXPORT
void ObservableCollectionBase::Clear()
{
    if (items_.empty())
        return;
    items_.clear();
    raise_change(NotifyCollectionChangedEventArgs(CollectionChangedAction::Reset));
}

VAML_CORE_EXPORT
void ObservableCollectionBase::SetAt(std::size_t index, Value item)
{
    if (index >= items_.size())
        return;
    const Value old = items_[index];
    if (old == item)
        return; // real-change discipline: no Replace signal on equal
    items_[index] = item;
    raise_change(NotifyCollectionChangedEventArgs(
        CollectionChangedAction::Replace, index, item, index, old));
}

VAML_CORE_EXPORT
void ObservableCollectionBase::Move(std::size_t oldIndex, std::size_t newIndex)
{
    if (oldIndex >= items_.size() || newIndex >= items_.size())
        return;
    if (oldIndex == newIndex)
        return;
    const Value item = items_[oldIndex];
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(oldIndex));
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(newIndex), item);
    raise_change(NotifyCollectionChangedEventArgs(
        CollectionChangedAction::Move, newIndex, item, oldIndex, item));
}

} // namespace vaml
