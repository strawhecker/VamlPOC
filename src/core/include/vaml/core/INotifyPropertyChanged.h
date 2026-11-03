#pragma once
// VAML INotifyPropertyChanged — the formal change-notification interface (WPF
// System.ComponentModel.INotifyPropertyChanged). 11n-4 — DAG anchor
// `INotifyPropertyChanged`, the notification-interfaces layer of the reactive
// spine. ADR-018: ZERO new locks (single-threaded affinity; the subscription
// list + value store are processed on the affinity thread).

#include "vaml/core/PropertyId.h"       // PropertyId
#include "vaml/core/Value.h"            // Value
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>

namespace vaml {

// The formal INotifyPropertyChanged contract: an observable owner that lets a
// subscriber register interest in property changes via a typed (id, old, new)
// handler and receive them WPF-faithfully. `ObservableObject` (S241) is the
// concrete real implementation of this interface — its existing shape IS the
// interface's impl (this header formalizes the dependency-root contract a
// CollectionView / binding subscribes to).
//
// WPF-faithful (the BCL INotifyPropertyChanged raises PropertyChanged(name,
// args) for each changed property); VAML types the signal as (id, old, new) so
// a subscriber can diff/re-run without re-reading. A broadcast subscriber
// (add_property_changed(handler)) fires for EVERY change; a per-property
// subscriber (add_property_changed(id, handler)) fires only for that id. Both
// back ONE registration-ordered list firing in registration order. A removal
// token unsubscribes a single handler.
class VAML_CORE_EXPORT INotifyPropertyChanged
{
public:
    virtual ~INotifyPropertyChanged() = default;

    // The typed PropertyChanged signal shape: (property id, old value, new
    // value).
    using PropertyChangedHandler = std::function<void(PropertyId, const Value&, const Value&)>;
    using PropertyChangedToken   = std::size_t;

    // Registers a BROADCAST handler: fires (id, old, new) for EVERY property
    // change, in registration order. Returns a removal token.
    [[nodiscard]] virtual PropertyChangedToken add_property_changed(PropertyChangedHandler handler) = 0;

    // Registers a PER-PROPERTY handler: fires ONLY for the given property,
    // in registration order with all other handlers. The shape the binding
    // engine / CollectionView subscribes with.
    [[nodiscard]] virtual PropertyChangedToken add_property_changed(PropertyId id,
                                                                    PropertyChangedHandler handler) = 0;

    // Removes the handler identified by `token`. No-op for an unknown/expired
    // token.
    virtual void remove_property_changed(PropertyChangedToken token) = 0;
};

} // namespace vaml
