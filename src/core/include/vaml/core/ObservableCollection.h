#pragma once
// VAML ObservableCollection<T> — the WPF-faithful observable collection
// (WPF System.Collections.ObjectModel.ObservableCollection<T>), VAML-native.
// 11n-4 — the reactive spine's collection core.
//
// WPF-faithful behavior: every mutation raises BOTH signals —
//   * PropertyChanged("Count") + PropertyChanged("Item[]"), and
//   * CollectionChanged (a NotifyCollectionChangedEventArgs),
// in WPF order (PropertyChanged first, then CollectionChanged — see
// .NET ObservableCollection.OnPropertyChanged + OnCollectionChanged).
//
// VAML-native storage: a heterogeneous `std::vector<Value>` (the VAML value
// domain boxes a concrete T into a Value via to_value) — faithful to the
// roadmap "over a heterogeneous Value::Collection".
//
// ADR-018 / DECISION-2: ZERO new locks on the single-threaded UI path. The
// collection is mutated + read on the affinity thread; cross-thread changes
// are the DECISION-2 change-log territory (11n-4 infra), never a collection
// lock. Re-entrancy (a handler mutates mid-dispatch) is the budgeted-drain
// concern, not a per-collection mutex.

#include "vaml/core/INotifyCollectionChanged.h"
#include "vaml/core/ObservableObject.h"  // the S241 PropertyChanged axis (Count/Item[])
#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace vaml {

// Boxes a concrete item type into the VAML Value domain (the heterogeneous
// storage). Specialized for the common literal types.
template <typename T, typename = void>
struct ValueBoxer
{
    // Default: for non-literal T, store the item as an UntypedPtr to a
    // caller-owned item (the app owns the pointed-to data). Falls back to an
    // emptry-value box for unboxable types (the binding consumes the pointer).
    static Value box(T&& v)
    {
        return Value::UntypedPtr(new std::decay_t<T>(std::forward<T>(v)));
    }
};

template <> struct ValueBoxer<std::string>
{ static Value box(std::string v) { return Value::String(std::move(v)); } };
template <> struct ValueBoxer<Value>
{ static Value box(Value v) { return v; } };
template <> struct ValueBoxer<std::int32_t>
{ static Value box(std::int32_t v) { return Value::Int32(v); } };
template <> struct ValueBoxer<std::int64_t>
{ static Value box(std::int64_t v) { return Value::Int64(v); } };
template <> struct ValueBoxer<double>
{ static Value box(double v) { return Value::Double(v); } };
template <> struct ValueBoxer<bool>
{ static Value box(bool v) { return Value::Bool(v); } };

// The heterogeneous Value store + the notification/PropertyChanged axis.
class VAML_CORE_EXPORT ObservableCollectionBase : public INotifyCollectionChanged
{
public:
    ObservableCollectionBase() = default;
    ~ObservableCollectionBase() override = default;

    ObservableCollectionBase(const ObservableCollectionBase&) = delete;
    ObservableCollectionBase& operator=(const ObservableCollectionBase&) = delete;

    // --- INotifyCollectionChanged -----------------------------------------
    [[nodiscard]] CollectionChangedToken add_collection_changed(CollectionChangedHandler handler) override;
    bool remove_collection_changed(CollectionChangedToken token) override;

    // --- PropertyChanged axis (Count/Item[]), the S241 signal -------------
    // Subscribes to the WPF PropertyChanged contract (Count + Item[]).
    using PropertyChangedHandler = ObservableObject::PropertyChangedHandler;
    using PropertyChangedToken   = ObservableObject::PropertyChangedToken;
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyId id, PropertyChangedHandler handler);
    void remove_property_changed(PropertyChangedToken token);
    void fire_property_changed(PropertyId id, const Value& oldV, const Value& newV);

    // --- Collection access -------------------------------------------------
    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] Value at(std::size_t i) const { return items_.at(i); }
    [[nodiscard]] const std::vector<Value>& items() const noexcept { return items_; }
    [[nodiscard]] PropertyId count_property() const { return PropertyId::of("Count"); }
    [[nodiscard]] PropertyId item_property() const { return PropertyId::of("Item[]"); }

    // --- Mutations (each raises PropertyChanged + CollectionChanged) -------
    void Add(Value item);
    void Insert(std::size_t index, Value item);
    bool Remove(const Value& item);
    void RemoveAt(std::size_t index);
    void Clear();
    void SetAt(std::size_t index, Value item);
    void Move(std::size_t oldIndex, std::size_t newIndex);

protected:
    // Raise the two WPF signals in WPF order: PropertyChanged("Count") +
    // PropertyChanged("Item[]"), THEN CollectionChanged(e).
    void raise_change(const NotifyCollectionChangedEventArgs& e);

private:
    std::vector<Value> items_;
    ObservableObject prop_axis_;  // the S241 PropertyChanged broadcaster
    std::vector<std::pair<CollectionChangedToken, CollectionChangedHandler>> collectionChanged_;
    CollectionChangedToken next_cc_ = 1;
};

// The T-form: boxes T into Value via ValueBoxer<T>; storage is the shared
// heterogeneous Value store.
template <typename T>
class ObservableCollection
{
public:
    ObservableCollection() = default;

    void Add(T item) { base_.Add(ValueBoxer<T>::box(std::move(item))); }
    void Insert(std::size_t i, T item) { base_.Insert(i, ValueBoxer<T>::box(std::move(item))); }
    void RemoveAt(std::size_t i) { base_.RemoveAt(i); }
    bool Remove(const T& item) { return base_.Remove(ValueBoxer<T>::box(item)); }
    void Clear() { base_.Clear(); }
    void SetAt(std::size_t i, T item) { base_.SetAt(i, ValueBoxer<T>::box(std::move(item))); }
    void Move(std::size_t oldIndex, std::size_t newIndex) { base_.Move(oldIndex, newIndex); }

    [[nodiscard]] std::size_t size() const noexcept { return base_.size(); }
    [[nodiscard]] bool empty() const noexcept { return base_.empty(); }
    [[nodiscard]] Value at(std::size_t i) const { return base_.at(i); }

    // The raw notification surface (CollectionChanged + Count/Item[]).
    [[nodiscard]] CollectionChangedToken add_collection_changed(CollectionChangedHandler h)
    {
        return base_.add_collection_changed(std::move(h));
    }
    bool remove_collection_changed(CollectionChangedToken t) { return base_.remove_collection_changed(t); }
    [[nodiscard]] auto add_property_changed(PropertyId id, ObservableCollectionBase::PropertyChangedHandler h)
    {
        return base_.add_property_changed(id, std::move(h));
    }
    void remove_property_changed(ObservableCollectionBase::PropertyChangedToken t)
    {
        base_.remove_property_changed(t);
    }

    ObservableCollectionBase& base() noexcept { return base_; }

private:
    ObservableCollectionBase base_;
};

} // namespace vaml
