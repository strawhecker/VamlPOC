#pragma once
// VAML ICollectionView — the live collection read-model contract (WPF
// System.ComponentModel.ICollectionView / System.Windows.Data.CollectionView).
// 11n-4 — DAG anchor `ICollectionView`, the live read model an ItemsControl
// consumes.
//
// WPF-faithful shape: the collection view is a live, readable projection of
// an underlying collection — it exposes Count / Contains / GetItemAt / IndexOf
// (the read API), the current-item navigation (MoveCurrentTo* / CurrentItem /
// CurrentPosition), Refresh (re-read the source), and the two notification
// surfaces a consumer binds live reactivity from: the collection-change signal
// (`CollectionChanged`) and the property-change signal (`PropertyChanged`, for
// Count/CurrentItem). A concrete CollectionView implements this over an
// INotifyCollectionChanged source.
//
// ADR-018 / DECISION-2: ZERO new locks on the single-threaded affinity path;
// the concrete view's change-log (DECISION-2) governs cross-thread updates
// (opt-in), never a lock.

#include "vaml/core/INotifyCollectionChanged.h" // the notification corpus
#include "vaml/core/INotifyPropertyChanged.h"   // the PropertyChanged axis
#include "vaml/core/Value.h"                    // Value
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>

namespace vaml {

// The live, readable collection read contract. A concrete CollectionView
// implements this; an ItemsControl reads items through it and subscribes to
// its CollectionChanged to incrementally add/remove containers.
class VAML_CORE_EXPORT ICollectionView
{
public:
    virtual ~ICollectionView() = default;

    // --- Read API (the live projection of the underlying collection) -------
    [[nodiscard]] virtual std::size_t Count() const = 0;
    [[nodiscard]] virtual bool Contains(const Value& item) const = 0;
    [[nodiscard]] virtual Value GetItemAt(std::size_t index) const = 0;
    [[nodiscard]] virtual std::size_t IndexOf(const Value& item) const = 0;

    // --- Current-item navigation (WPF CollectionView) ----------------------
    [[nodiscard]] virtual Value CurrentItem() const = 0;
    [[nodiscard]] virtual std::size_t CurrentPosition() const = 0;
    virtual bool MoveCurrentTo(const Value& item) = 0;
    virtual bool MoveCurrentToPosition(std::size_t position) = 0;
    virtual bool MoveCurrentToFirst() = 0;
    virtual bool MoveCurrentToLast() = 0;
    virtual bool MoveCurrentToNext() = 0;
    virtual bool MoveCurrentToPrevious() = 0;

    // --- Refresh (re-read the underlying source) --------------------------
    virtual void Refresh() = 0;

    // --- Live notification surfaces (an ItemsControl subscribes here) ------
    // CollectionChanged: fired when the view's live items change (a delta an
    // ItemsControl applies incrementally).
    [[nodiscard]] virtual CollectionChangedToken add_collection_changed(CollectionChangedHandler handler) = 0;
    virtual bool remove_collection_changed(CollectionChangedToken token) = 0;
};

} // namespace vaml
