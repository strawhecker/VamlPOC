#pragma once
// VAML notification corpus — the collection-change notification surface
// (WPF System.Collections.Specialized INotifyCollectionChanged +
// NotifyCollectionChangedEventArgs + CollectionChangedAction). 11n-4.
//
// The reactive spine's notification interfaces (DAG anchors:
// INotifyCollectionChanged, plus the S241 ObservableObject INotifyPropertyChanged
// analogue). WPF-faithful shape (verified from the BCL
// System.Collections.Specialized.*):
//   - CollectionChangedAction: Add / Remove / Replace / Move / Reset;
//   - NotifyCollectionChangedEventArgs: action + the index/items delta
//     (new/old starting index + per-item values);
//   - INotifyCollectionChanged: the `CollectionChanged` subscription surface an
//     ObservableCollection implements and a CollectionView / ItemsControl
//     subscribes to.
//
// ADR-018: ZERO new locks — the notification corpus is plain value/event
// plumbing fired on the single affinity thread (the single-threaded UI path;
// cross-thread changes go through the DECISION-2 change-log, 11n-4).

#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace vaml {

// The kind of change that raised a CollectionChanged (WPF
// CollectionChangedAction).
enum class CollectionChangedAction : std::uint8_t
{
    Add,
    Remove,
    Replace,
    Move,
    Reset,
};

// The event-args of a collection change (WPF NotifyCollectionChangedEventArgs).
// The per-item payload is typed `Value` (the VAML value domain — the 
// ObservableCollection stores Values; a concrete T is boxed into a Value).
class VAML_CORE_EXPORT NotifyCollectionChangedEventArgs
{
public:
    // A positional delta (Add/Remove/Replace/Move).
    NotifyCollectionChangedEventArgs(CollectionChangedAction action,
                                     std::size_t newIndex, Value newItem,
                                     std::size_t oldIndex, Value oldItem);

    // A Reset (no positional info).
    explicit NotifyCollectionChangedEventArgs(CollectionChangedAction action);

    [[nodiscard]] CollectionChangedAction action() const noexcept { return action_; }

    // The index + item introduced by Add/Replace(new)/Move(new).
    [[nodiscard]] std::size_t new_index() const noexcept { return newIndex_; }
    [[nodiscard]] Value new_item() const { return newItem_; }

    // The index + item removed by Remove/Replace(old)/Move(old).
    [[nodiscard]] std::size_t old_index() const noexcept { return oldIndex_; }
    [[nodiscard]] Value old_item() const { return oldItem_; }

private:
    CollectionChangedAction action_;
    std::size_t             newIndex_;
    Value                   newItem_;
    std::size_t             oldIndex_;
    Value                   oldItem_;
};

// The INotifyCollectionChanged-analogue interface (WPF
// System.Collections.Specialized.INotifyCollectionChanged). A CollectionView /
// ItemsControl subscribes here to drive incremental container add/remove.
using CollectionChangedHandler = std::function<void(const NotifyCollectionChangedEventArgs&)>;
using CollectionChangedToken   = std::size_t;

class VAML_CORE_EXPORT INotifyCollectionChanged
{
public:
    virtual ~INotifyCollectionChanged() = default;

    // Registers a CollectionChanged handler; returns a removal token.
    [[nodiscard]] virtual CollectionChangedToken add_collection_changed(CollectionChangedHandler handler) = 0;
    virtual bool remove_collection_changed(CollectionChangedToken token) = 0;
};

} // namespace vaml
