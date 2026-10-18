#pragma once
// VAML PropertyRegistry — maps property NAME -> PropertyKey and enforces
// name uniqueness (dedupe detection). S032 (dep:S031). Foundation for the DP
// property system. S033 (this step) adds the constexpr-safe static id bridge
// (id_of / try_id) on top of the S032 instance-based shape.
//
// The registry is *instance-based*: construct one per owner-setup scope. A
// global static registry instance (the S032-recorded deferred decision) is a
// S034 concern once a real static DependencyProperty::Register landing site
// exists — deliberately NOT added here (S033 has no consumer for a global
// store; adding one would be speculative surface).
//
// Uniqueness scope (recorded design decision): keys are globally unique by
// NAME (WPF-style). register_key() throws PropertyAlreadyRegistered on a name
// collision; the map is unchanged by a rejected insert. ownerTypeId lives
// inside the key as identity metadata, not as a second registry dimension.
//
// Static id bridge: id_of(name) yields the PropertyId for a registered name
// (throws PropertyNotFound if absent); try_id(name) is the noexcept variant
// returning an invalid (empty) id. This ties the runtime name-keyed store to
// the compile-time PropertyId (S033) — the canonical key S036's local-value
// store uses.
//
// S146 (refactor gate) — the pimpl is COLLAPSED. The registry now holds its
// vaml::detail::NameTable<PropertyKey> content-keyed store BY VALUE (the S146
// dead pre-retrofit surface purge: no Impl heap node, no impl_-> trampoline,
// no per-object heap hop). NameTable.h was promoted to the public detail tree
// (vaml/core/detail/NameTable.h) solely to make this collapse legal; it remains
// an internal implementation detail the public API does not promise. Lookup
// still takes std::string_view and materializes NO transient std::string. The
// registry OWNS its key storage (the NameTable's stable key nodes, process
// lifetime for the global registry) and keys by CONTENT — never pointer. This
// is the S140 DECISION 1 resolution for the global dedupe master: a global /
// cross-thread table cannot hold thread-local intern views, so it stays
// content-keyed with self-owned stable storage. ADR-018: no lock added.
//
// (Pre-S146 note for the record: the "keep the ABI surface stable" rationale
// that once justified this pimpl was RETRACTED under ADR-003 generated + ADR-019
// — this MIT-public lib is statically linked with no runtime ABI boundary to
// defend. The pimpl is now gone entirely.)

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/detail/NameTable.h" // S146: promoted to public detail tree so the pimpl can collapse

#include <cstddef>
#include <stdexcept>
#include <string>

namespace vaml {

// register_key() saw a name that is already registered (dedupe detection).
class PropertyAlreadyRegistered : public std::logic_error
{
public:
    explicit PropertyAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// find() did not locate a key for the requested name.
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

    // Registers `key`, uniques by key.name(). Throws PropertyAlreadyRegistered
    // if a key with the same name is already registered (the map is left
    // unchanged). Returns a const reference to the stored key (valid for the
    // registry's lifetime).
    [[nodiscard]] const PropertyKey& register_key(PropertyKey key);

    // Lookup: returns a pointer to the stored key, or nullptr if none. Never
    // throws. Accepts std::string (implicit conversion) or std::string_view.
    [[nodiscard]] const PropertyKey* try_find(std::string_view name) const noexcept;

    // Lookup: returns the stored key, or throws PropertyNotFound.
    [[nodiscard]] const PropertyKey& find(std::string_view name) const;

    // Static id bridge (S033): returns the constexpr-safe PropertyId for a
    // registered name. Throws PropertyNotFound if the name is not registered
    // (never returns an id for an unregistered name).
    [[nodiscard]] PropertyId id_of(std::string_view name) const;

    // Static id bridge (S033), noexcept: returns the registered name's
    // PropertyId, or an invalid (empty) id if the name is not registered.
    // Never throws — the noexcept counterpart to id_of.
    [[nodiscard]] PropertyId try_id(std::string_view name) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // S039: the process-wide static name -> key dedupe master. This is the
    // landing site for DependencyProperty::Register (S039) — the first real
    // consumer of a GLOBAL property table (the S036 re-scope, recorded NOT
    // silent). Object-scoped instances (default ctor) remain for per-object
    // registration; this accessor returns the shared singleton used by
    // global DependencyProperty registration. Thread-safe construction
    // (Meyers function-local static).
    [[nodiscard]] static PropertyRegistry& global() noexcept;

private:
    // S146: pimpl collapsed — the content-keyed store is now a BY-VALUE member.
    // NameTable is move-only (the registry is move-only; copy stays deleted).
    // insert/find/size delegate directly (no impl_-> trampoline, no heap hop).
    vaml::detail::NameTable<PropertyKey> table_;
};

} // namespace vaml
