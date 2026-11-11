// S032 + S143 (B3, dep:S140/S141) + S146 + S801 — PropertyRegistry out-of-line
// definitions. Maps the (NAME, OWNER) COMPOSITE key -> PropertyKey and enforces
// PAIR uniqueness (dedupe) — the S801 central re-key that resolves the S033/S039
// name-only contract drift at its source (WPF `FromNameKey(name, ownerType)`:
// strict pair identity; same-name/different-owner is DISTINCT, same-pair throws).
//
// S143 re-keyed the internal index OFF std::unordered_map and ONTO the
// S141-pattern content-keyed probe store (vaml::detail::NameTable). S801 re-keys
// that store's key from a single string to the (name, owner) composite. Lookup
// takes (std::string_view name, std::string_view owner) and materializes NO
// transient std::string. The registry OWNS its key storage (the NameTable's
// stable composite nodes, process lifetime for the global registry) and keys by
// CONTENT — never pointer. This is the S140 DECISION 1 resolution for the global
// dedupe master: a global / cross-thread table cannot hold thread-local intern
// views, so it stays content-keyed with self-owned stable storage. ADR-018: no
// lock added.
//
// S146 (refactor gate): the pimpl (class Impl + impl_) is COLLAPSED — the
// NameTable<PropertyKey, CompositeKey> member is now held BY VALUE in the
// registry (see PropertyRegistry.h).

#include "vaml/core/PropertyRegistry.h"

#include <string>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
PropertyRegistry::PropertyRegistry() = default;

VAML_CORE_EXPORT
PropertyRegistry::~PropertyRegistry() = default;

VAML_CORE_EXPORT
PropertyRegistry::PropertyRegistry(PropertyRegistry&&) noexcept = default;

VAML_CORE_EXPORT
PropertyRegistry& PropertyRegistry::operator=(PropertyRegistry&&) noexcept = default;

VAML_CORE_EXPORT
const PropertyKey& PropertyRegistry::register_key(PropertyKey key)
{
    // Capture name + owner up front for a clear error message AND because the
    // NameTable insert() would otherwise re-copy them from the key's live
    // buffers while `key` is simultaneously being MOVED as the value (the order
    // of argument evaluation is unspecified — the copy could see a moved-from
    // empty key). Copying first keeps the NameTable's owned composite key
    // correct. (S146: the NameTable is the BY-VALUE member; S801: composite.)
    std::string name     = key.name();
    std::string owner    = key.ownerTypeId();
    std::string original = name;
    std::string origOwner = owner;

    const PropertyKey* stored = table_.insert(std::move(name), std::move(owner), std::move(key));
    if (stored == nullptr)
    {
        std::string msg = "property already registered: '" + original + "' owned by '" + origOwner + "'";
        throw PropertyAlreadyRegistered(msg.c_str());
    }
    return *stored;
}

VAML_CORE_EXPORT
const PropertyKey* PropertyRegistry::try_find(std::string_view name,
                                              std::string_view owner) const noexcept
{
    return table_.find(name, owner);
}

VAML_CORE_EXPORT
const PropertyKey& PropertyRegistry::find(std::string_view name,
                                          std::string_view owner) const
{
    const PropertyKey* stored = table_.find(name, owner);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + std::string(name) + "' owned by '" + std::string(owner) + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return *stored;
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::id_of(std::string_view name, std::string_view owner) const
{
    const PropertyKey* stored = table_.find(name, owner);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + std::string(name) + "' owned by '" + std::string(owner) + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return PropertyId::of(stored->name(), stored->ownerTypeId());
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::try_id(std::string_view name, std::string_view owner) const noexcept
{
    const PropertyKey* stored = table_.find(name, owner);
    // Unregistered pair -> invalid (empty) id; caller distinguishes via
    // PropertyId::valid().
    return stored == nullptr ? PropertyId{}
                             : PropertyId::of(stored->name(), stored->ownerTypeId());
}

VAML_CORE_EXPORT
std::size_t PropertyRegistry::size() const noexcept
{
    return table_.size();
}

VAML_CORE_EXPORT
PropertyRegistry& PropertyRegistry::global() noexcept
{
    // Meyers singleton: function-local static is constructed on first use and
    // destroyed in reverse construction order at program exit. This is the
    // global (name, owner) -> key dedupe master consumed by
    // DependencyProperty::Register (S039). No static-init-order fiasco.
    static PropertyRegistry instance;
    return instance;
}

} // namespace vaml
