#pragma once
// VAML PropertyRegistry — maps the (NAME, OWNER) COMPOSITE key -> PropertyKey
// and enforces pair uniqueness (dedupe detection). S032 (dep:S031). Foundation
// for the DP property system. S033 added the constexpr-safe static id bridge
// (id_of / try_id) on top of the S032 instance-based shape. S800 introduced the
// composite (name, owner) shape at the PropertyId level; **S801 re-keys the
// STORE itself onto the (name, owner) pair** — the central edit that resolves
// the S033/S039 name-only contract drift at its source (WPF ground truth:
// DependencyProperty.cs `FromNameKey(name, ownerType)` — strict pair identity;
// a same-name/different-owner pair is a DISTINCT property, never a collision).
//
// The registry is *instance-based*: construct one per owner-setup scope. A
// global static registry instance (the S032-recorded deferred decision) exists
// as PropertyRegistry::global() for DependencyProperty::Register (S034/S039).
//
// Uniqueness scope (S801 re-recorded design decision): keys are globally unique
// by the (name, ownerTypeId) PAIR (WPF-style). register_key() throws
// PropertyAlreadyRegistered on a SAME-PAIR collision — and ONLY on the same
// pair. A same-name/different-owner pair registers TWICE as DISTINCT
// properties (coexistence, the S801 contract). The store is unchanged by a
// rejected insert. type + flags are NOT part of the key identity (they follow
// the owner-defined key, matching WPF where `(name, ownerType)` is the identity
// and type/flags are authored against that key).
//
// Static id bridge: id_of(name, owner) yields the composite PropertyId for a
// registered pair (throws PropertyNotFound if absent); try_id(name, owner) is
// the noexcept variant returning an invalid (empty) id. This ties the runtime
// composite-keyed store to the compile-time PropertyId (S800, S801).
//
// S146 (refactor gate) — the pimpl is COLLAPSED. The registry now holds its
// vaml::detail::NameTable<PropertyKey, CompositeKey> content-keyed store BY
// VALUE (see S146 note in NameTable.h). Lookup takes (std::string_view name,
// std::string_view owner) and materializes NO transient std::string. The
// registry OWNS its key storage (the NameTable's stable composite nodes,
// process lifetime for the global registry) and keys by CONTENT — never
// pointer. ADR-018: no lock added.

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/detail/NameTable.h" // S146: promoted to public detail tree so the pimpl can collapse

#include <cstddef>
#include <stdexcept>
#include <string>

namespace vaml {

// register_key() saw a (name, owner) pair that is already registered (dedupe).
class PropertyAlreadyRegistered : public std::logic_error
{
public:
    explicit PropertyAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// find() did not locate a key for the requested (name, owner) pair.
class PropertyNotFound : public std::out_of_range
{
public:
    explicit PropertyNotFound(const char* what) : std::out_of_range(what) {}
};

class VAML_CORE_EXPORT PropertyRegistry
{
public:
    PropertyRegistry();
    ~PropertyRegistry();

    PropertyRegistry(const PropertyRegistry&) = delete;
    PropertyRegistry& operator=(const PropertyRegistry&) = delete;
    PropertyRegistry(PropertyRegistry&&) noexcept;
    PropertyRegistry& operator=(PropertyRegistry&&) noexcept;

    // Registers `key`, uniques by the (key.name(), key.ownerTypeId()) PAIR.
    // Throws PropertyAlreadyRegistered if a key with the SAME pair is already
    // registered (the store is left unchanged). A same-name/different-owner key
    // registers as a DISTINCT entry (coexistence). Returns a const reference to
    // the stored key (valid for the registry's lifetime).
    [[nodiscard]] const PropertyKey& register_key(PropertyKey key);

    // Lookup by composite (name, owner): returns a pointer to the stored key,
    // or nullptr if none. Never throws. Accepts std::string (implicit
    // conversion) or std::string_view.
    [[nodiscard]] const PropertyKey* try_find(std::string_view name,
                                              std::string_view owner) const noexcept;

    // Lookup by composite (name, owner): returns the stored key, or throws
    // PropertyNotFound.
    [[nodiscard]] const PropertyKey& find(std::string_view name,
                                          std::string_view owner) const;

    // Static id bridge (S800/S801): returns the composite PropertyId::of(name,
    // owner) for a registered pair. Throws PropertyNotFound if the pair is not
    // registered (never returns an id for an unregistered pair).
    [[nodiscard]] PropertyId id_of(std::string_view name, std::string_view owner) const;

    // Static id bridge (S800/S801), noexcept: returns the registered pair's
    // composite PropertyId, or an invalid (empty) id if the pair is not
    // registered. Never throws — the noexcept counterpart to id_of.
    [[nodiscard]] PropertyId try_id(std::string_view name, std::string_view owner) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // S039: the process-wide static (name, owner) -> key dedupe master. This is
    // the landing site for DependencyProperty::Register (S039). Thread-safe
    // construction (Meyers function-local static).
    [[nodiscard]] static PropertyRegistry& global() noexcept;

private:
    // S146: pimpl collapsed — the content-keyed store is now a BY-VALUE member,
    // re-keyed S801 onto the (name, owner) composite. NameTable is move-only
    // (the registry is move-only; copy stays deleted).
    vaml::detail::NameTable<PropertyKey, vaml::detail::CompositeKey> table_;
};

} // namespace vaml
