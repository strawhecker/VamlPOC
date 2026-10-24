#pragma once
// VAML ObservableObject — the INotifyPropertyChanged-analogue base for
// ViewModels. S241 (dep:S240). Sprint 6 second token.
//
// A standalone PLAIN base class for ViewModels — it deliberately does NOT
// derive from `DependencyObject` (or `UIElement`). Rationale (S241 decision,
// WPF-faithful):
//   - A ViewModel is an `INotifyPropertyChanged` SOURCE, not a framework
//     element. It carries no dependency properties, no visual tree, no
//     visual-parent/thread-affinity marker (S036/S039), no inheritance
//     cascade, no coerce/validate pipeline (S034/S045) — those belong to the
//     control/DP side, not to a data source.
//   - The DP store (S142 block-resident, affinity) has zero value for a VM
//     that just needs to own named typed values and tell listeners when they
//     change.
//   - So ObservableObject is LIGHTWEIGHT, MOVABLE, and has ONE job: own named
//     typed `Value`s and fire a typed PropertyChanged signal on a REAL change.
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — the
// type is complete on delivery) and is headless-testable on the `vaml::Value`
// + Shouldly conventions established by S030..S240.
//
// The typed PropertyChanged signal (the INotifyPropertyChanged-analogue):
//   - `PropertyChangedHandler` = std::function<void(PropertyId, const Value&,
//     const Value&)> — fires (id, old, new);
//   - `add_property_changed(handler)` registers a BROADCAST handler that
//     receives EVERY property change (WPF-style single event; the subscriber
//     filters by id) — registered with an empty (invalid) PropertyId sentinel;
//   - `add_property_changed(PropertyId, handler)` registers a PER-PROPERTY
//     convenience that auto-filters to that one property — the exact shape the
//     S242+ binding engine subscribes with;
//   - both back ONE registration-ordered entry list fired in registration
//     order, mirroring the S041 token/registration-order subscription
//     discipline exactly; `remove_property_changed(token)` removes a single
//     handler, remaining handlers keep their order, a re-add appends.
//
// SET FIRES ONLY ON A REAL CHANGE (old != new by `Value` equality — the
// S035/S036/S037 "real-change-only" discipline the S243 invalidation
// coalescing already relies on). `Set` returns `true` when a change fired.
//
// REBASE ONTO S240 — `observable_object_reader()`: a REAL `PropertyReader`
// (S240) so the S242+ binding engine reads ObservableObject properties through
// the SAME `evaluate_property_path` it uses everywhere:
//   - a PRESENT property  -> reads Get (final null = ok=true; the evaluator
//     interprets a NON-final null as NullIntermediate);
//   - an ABSENT name      -> MissingProperty (Class B, via has(), never a raw
//     throw);
//   - an Index segment / non-`Value::UntypedPtr` ObservableObject root ->
//     NotReadable.
// This makes the S240_1 Class B resolve points (MissingProperty / Null /
// NotReadable) reachable on the ObservableObject axis; the binding ENGINE
// emits the S240_1 `BindingDiagnostic` from them at S242+ (already
// contractually tagged — NO-GAP, not a backfill).
//
// ADR-018: ZERO new locks. The subscriber list + value store are
// single-threaded (a ViewModel is mutated on its owner thread and read
// through the same thread's binding engine); cross-thread marshalling is
// S326/S211 territory, not this type's.

#include "vaml/core/PropertyId.h"       // PropertyId
#include "vaml/core/PropertyPath.h"     // PropertyRead, PropertyReader, PropertySegment
#include "vaml/core/Value.h"            // Value
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

// The INotifyPropertyChanged-analogue: a lightweight observable value holder.
class VAML_CORE_EXPORT ObservableObject
{
public:
    ObservableObject() = default;
    virtual ~ObservableObject() = default;

    ObservableObject(const ObservableObject&) = delete;
    ObservableObject& operator=(const ObservableObject&) = delete;
    ObservableObject(ObservableObject&&) noexcept = default;
    ObservableObject& operator=(ObservableObject&&) noexcept = default;

    // --- Typed PropertyChanged signal (INotifyPropertyChanged-analogue) ----
    using PropertyChangedHandler = std::function<void(PropertyId, const Value&, const Value&)>;
    using PropertyChangedToken   = std::size_t;

    // Registers a BROADCAST handler: fires (id, old, new) for EVERY property
    // change, in registration order relative to all other handlers. Returns a
    // removal token. The WPF-style single-event analogue (subscriber filters
    // by id).
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyChangedHandler handler);

    // Registers a PER-PROPERTY handler: fires ONLY for the given property
    // (auto-filtered), in registration order with all other handlers. The
    // shape the S242+ binding engine subscribes with.
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyId id, PropertyChangedHandler handler);

    // Removes the handler identified by `token` (broadcast or per-property —
    // the same list). No-op for an unknown/expired token. O(n) removal
    // preserves the registration order of what remains; a re-add appends.
    void remove_property_changed(PropertyChangedToken token);

    // --- Lightweight typed store (name -> Value) ---------------------------
    // Set fires the PropertyChanged signal ONLY on a REAL change (old != new
    // by Value equality). Returns true when a change fired and was stored;
    // false when the new value equals the current one (no storage change, no
    // signal). A property is stored the first time it is Set.
    [[nodiscard]] bool Set(PropertyId id, const Value& value);

    // Reads the property's current value (Value::Null() when the property has
    // NEVER been set). Combined with has() to distinguish "absent" from
    // "present with a null value".
    [[nodiscard]] Value Get(PropertyId id) const;

    // True iff the property has ever been Set (regardless of its value — a
    // Set-to-Value::Null property IS present; a never-set name is absent).
    [[nodiscard]] bool has(PropertyId id) const;

private:
    // Ordered subscription list (registration order = fire order). A broadcast
    // handler has a non-valid (empty) PropertyId. Value store keyed by OWNED
    // name copy (never the non-owning PropertyId view — the S036/S037/S140
    // owned-name discipline; a PropertyId's name can point at transient text).
    struct Sub
    {
        PropertyId            id;      // valid => per-property; invalid => broadcast
        PropertyChangedToken  token;
        PropertyChangedHandler handler;
    };

    void notify(PropertyId id, const Value& old, const Value& newValue) const;

    std::map<std::string, Value> store_;
    std::vector<Sub>             subs_;
    PropertyChangedToken         next_token_ = 1;
};

// The S240 PropertyReader over an ObservableObject transported as
// Value::UntypedPtr. See the header comment for the Class B mapping.
[[nodiscard]] VAML_CORE_EXPORT PropertyReader observable_object_reader();

} // namespace vaml
