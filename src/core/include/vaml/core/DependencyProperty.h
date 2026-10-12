#pragma once
// VAML DependencyProperty — the global definitional authority for a dependency
// property: the (PropertyKey, PropertyMetadata) pair plus its registering
// landing site. S039 (dep:S038).
//
// S039 is the AUTHORING STEP for the re-scoped S036 global static registry +
// DependencyProperty::Register landing site (S034/S036 both recorded the
// deferral, NOT silent). S036 delivered OBJECT-SCOPED registration only
// (DependencyObject::register_property) because a global static registry in
// S036 would have been speculative surface with no consumer. S039's
// effective-value resolution is the FIRST real consumer of a global property
// table — so the global table is authored here, fully implemented + tested,
// NOT as MINIMAL REAL.
//
// Roles:
//   - DependencyProperty::Register(name, type, ownerTypeId, metadata) is the
//     landing site. It inserts a PropertyKey into PropertyRegistry::global()
//     (name-unique; a name already registered throws PropertyAlreadyRegistered
//     and stores nothing) and keeps the FULL DependencyProperty (key +
//     metadata, i.e. flags + default) in a companion static name-keyed table.
//     Returns a const reference valid for the process lifetime (both the
//     global registry and the companion table outlive every consumer).
//   - TryGet(name) / Get(name) look the DependencyProperty up by name. They
//     are the read path both the public API and (internally) effective-value
//     resolution use to obtain the canonical flags + default.
//   - The inheritance descriptor (registering flags + default) comes from THIS
//     type; the local value comes from the object's per-object store. S039's
//     resolution composes the two (see DependencyObject::GetEffectiveValue).
//
// Storage is pimpl'd (class Impl + unique_ptr) so the public header exposes no
// std::unordered_map and no move-only PropertyMetadata value (matches the
// S034 PropertyMetadata / S036 DependencyObject pimpl conventions).

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h" // PropertyKey — the (name,type,owner,flags) tuple
#include "vaml/core/PropertyMetadata.h" // PropertyMetadata, ValueTypeMismatch

#include <memory>
#include <string_view>

namespace vaml {

class VAML_CORE_EXPORT DependencyProperty
{
public:
    // --- Registration landing site (S039) -------------------------------
    // Registers `name` globally (name-unique via PropertyRegistry::global();
    // a duplicate NAME throws PropertyAlreadyRegistered and stores nothing —
    // the existing registration is left unchanged, WPF-style). Type coherence
    // enforced: `type` must equal metadata.boundType() (else ValueTypeMismatch,
    // NO-LAZY guard). Stores the full (key, metadata) definition including
    // flags (Inherits / InheritOnly) and the defaultValue. Returns a const
    // reference valid for the process lifetime.
    [[nodiscard]] static const DependencyProperty& Register(
        std::string_view name, ValueType type, std::string_view ownerTypeId,
        PropertyMetadata metadata);

    // --- Read path ------------------------------------------------------
    // Returns the global DependencyProperty for `name`, or nullptr if not
    // registered. Never throws.
    [[nodiscard]] static const DependencyProperty* TryGet(std::string_view name) noexcept;

    // Returns the global DependencyProperty for `name`, or throws
    // PropertyNotFound.
    [[nodiscard]] static const DependencyProperty& Get(std::string_view name);

    // --- Immutable accessors -------------------------------------------
    [[nodiscard]] PropertyId property_id() const noexcept;            // id derived from name
    [[nodiscard]] const PropertyMetadata& metadata() const noexcept;  // flags + default + callbacks
    [[nodiscard]] PropertyFlags flags() const noexcept;               // shorthand for metadata().flags()

private:
    class Impl; // pimpl: hides std::unordered_map + PropertyMetadata
    std::unique_ptr<Impl> impl_;

    explicit DependencyProperty(std::unique_ptr<Impl> impl);
};

} // namespace vaml
