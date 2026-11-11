#pragma once
// VAML DependencyProperty — the global definitional authority for a dependency
// property: the (PropertyKey, PropertyMetadata) pair plus its registering
// landing site. S039 (dep:S038).
//
// S801 — the global identity is the COMPOSITE (name, owner) pair (WPF ground
// truth: DependencyProperty.cs `FromNameKey(name, ownerType)`). The companion
// GlobalTable is re-keyed from name-only onto the (name, ownerTypeId) pair, and
// Register/TryGet/Get take the owner explicitly. A same-name/different-owner
// pair registers as DISTINCT DependencyProperties (coexistence); a same-pair
// re-register throws PropertyAlreadyRegistered and stores nothing. NO owner-less
// (bare-name) DP lookup survives — the owner is always carried, exactly as WPF
// never resolves a bare name to a DP.
//
// Roles:
//   - DependencyProperty::Register(name, type, ownerTypeId, metadata) is the
//     landing site. It inserts a PropertyKey into PropertyRegistry::global()
//     keyed on (name, ownerTypeId); a same-pair re-register throws
//     PropertyAlreadyRegistered and stores nothing — the existing registration
//     is left unchanged. Keeps the FULL DependencyProperty (key + metadata,
//     i.e. flags + default) in a companion (name, owner)-keyed table. Returns a
//     const reference valid for the process lifetime.
//   - TryGet(name, owner) / Get(name, owner) look the DependencyProperty up by
//     its composite pair. They are the read path both the public API and
//     (internally) effective-value resolution use to obtain the canonical flags
//     + default.
//   - The inheritance descriptor (registering flags + default) comes from THIS
//     type; the local value comes from the object's per-object store. S039's
//     resolution composes the two (see DependencyObject::GetEffectiveValue).
//
// S146 (refactor gate) — the pimpl is COLLAPSED: the definitional data (owned
// name_, move-only PropertyMetadata metadata_, ownerTypeId_) is now plain
// by-value members. The companion GlobalTable (name+owner -> unique_ptr<
// DependencyProperty>, in DependencyProperty.cpp) keeps its S143
// vaml::detail::NameTable content-keyed probe, now with the composite pair key
// (S801). ADR-018: no lock added.

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h" // PropertyKey — the (name,type,owner,flags) tuple
#include "vaml/core/PropertyMetadata.h" // PropertyMetadata, ValueTypeMismatch
#include "vaml/core/detail/NameTable.h" // S146: promoted to public detail tree so the pimpl can collapse

#include <string>
#include <string_view>
#include <utility>

namespace vaml {

class VAML_CORE_EXPORT DependencyProperty
{
public:
    // --- Registration landing site (S039) -------------------------------
    // Registers `name` globally (pair-unique via PropertyRegistry::global(); a
    // duplicate (name, ownerTypeId) PAIR throws PropertyAlreadyRegistered and
    // stores nothing — the existing registration is left unchanged, WPF-style;
    // a same-name/different-owner pair is a DISTINCT property). Type coherence
    // enforced: `type` must equal metadata.boundType() (else ValueTypeMismatch,
    // NO-LAZY guard). Stores the full (key, metadata) definition including
    // flags (Inherits / InheritOnly) and the defaultValue. Returns a const
    // reference valid for the process lifetime.
    [[nodiscard]] static const DependencyProperty& Register(
        std::string_view name, ValueType type, std::string_view ownerTypeId,
        PropertyMetadata metadata);

    // --- Read path ------------------------------------------------------
    // Returns the global DependencyProperty for the (name, owner) PAIR, or
    // nullptr if not registered. Never throws.
    [[nodiscard]] static const DependencyProperty* TryGet(std::string_view name,
                                                          std::string_view owner) noexcept;

    // Returns the global DependencyProperty for the (name, owner) PAIR, or
    // throws PropertyNotFound.
    [[nodiscard]] static const DependencyProperty& Get(std::string_view name,
                                                       std::string_view owner);

    // S803 — the COMPOSITE owner-chain resolver (the permanent replacement for
    // the S803-removed unique-name bridge). Resolves a property attribute key
    // against an element TYPE-NAME:
    //   * a BARE name walks that element's DECLARED OWNER-CHAIN (WPF's XAML
    //     type-inheritance DP resolution: a DP's owner is its declaring type, so
    //     `Padding` on a Button hits owner "Control", `Text` on a TextBlock hits
    //     owner "TextBlock"), calling TryGet(name, owner) at each level
    //     (most-derived first);
    //   * a DOTTED `Owner.Property` key is split on the last dot and resolved as
    //     TryGet(tail, owner) directly (the owner is stated — NO chain walk),
    //     exactly as the reader's authored `<Owner.Property>` form.
    // `elementTypeName` is the authored element's type-name (the concrete owner
    // root) and is unused for the dotted form. Shared by the S2 reader, the S7
    // compiler/validators, and the release VBamlReader (single authority for the
    // owner-chain table; see DependencyProperty.cpp). Returns nullptr when no
    // owner resolves the key. No bare-name global lookup survives; never
    // silently picks among ambiguous names.
    [[nodiscard]] static const DependencyProperty* ResolveForElement(
        std::string_view name, std::string_view elementTypeName) noexcept;

    // Returns the global DependencyProperty for the (name, owner) PAIR, or
    // throws PropertyNotFound.

    // --- Immutable accessors -------------------------------------------
    [[nodiscard]] PropertyId property_id() const noexcept;            // id derived from (name, owner)
    [[nodiscard]] const PropertyMetadata& metadata() const noexcept;  // flags + default + callbacks
    [[nodiscard]] PropertyFlags flags() const noexcept;               // shorthand for metadata().flags()

    // S043: the DECLARING type's identity (the "owner" of the attached
    // property). For an attached dependency property the owner is the class
    // that defines the key (A); the value may be stored on any other object
    // (B) — owner != class. Exposes the ownerTypeId the key was registered
    // under. Backed by the stored key's ownerTypeId (a stable std::string,
    // valid for the process lifetime).
    [[nodiscard]] const std::string& owner_type_id() const noexcept;

    // Public destructor: the companion GlobalTable deletes its
    // unique_ptr<DependencyProperty> members at process exit, so the dtor must
    // be reachable from std::default_delete (S146).
    ~DependencyProperty();

private:
    // S146: pimpl collapsed — the definitional data is plain by-value members.
    // `name_` is the OWNED name; `metadata_` is the move-only PropertyMetadata
    // definition; `ownerTypeId_` is the DECLARING class identity.
    std::string        name_;
    PropertyMetadata   metadata_;
    std::string        ownerTypeId_;

    // Built ONLY through the COLLAPSED ctor below (internal; Register()'s
    // GlobalTable stores DependencyProperty via unique_ptr). move-only (copy
    // deleted) because metadata_ is move-only.
    DependencyProperty(std::string name, PropertyMetadata metadata,
                       std::string ownerTypeId);
    DependencyProperty(DependencyProperty&&) noexcept;
    DependencyProperty& operator=(DependencyProperty&&) noexcept;
    DependencyProperty(const DependencyProperty&) = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;
};

} // namespace vaml
