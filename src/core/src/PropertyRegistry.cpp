// S032 + S143 (B3, dep:S140/S141) — PropertyRegistry out-of-line definitions.
// Maps property NAME -> PropertyKey and enforces name uniqueness (dedupe).
//
// S143 re-keys the internal index OFF std::unordered_map<std::string,...> and
// ONTO the S141-pattern content-keyed probe store (vaml::detail::NameTable).
// Lookup takes std::string_view and materializes NO transient std::string.
// The registry OWNS its key storage (the NameTable's stable key nodes, process
// lifetime for the global registry) and keys by CONTENT — never pointer. This
// is the S140 DECISION 1 resolution for the global dedupe master: a global /
// cross-thread table cannot hold thread-local intern views, so it stays
// content-keyed with self-owned stable storage. ADR-018: no lock added.

#include "vaml/core/PropertyRegistry.h"

#include "NameTable.h" // S143: content-keyed probe store (re-keys off unordered_map)

#include <string>
#include <utility>

namespace vaml {

class PropertyRegistry::Impl
{
public:
    // Returns the pointer to the stored key, or nullptr on a name collision
    // (in which case the existing entry is left unchanged). S143: content
    // keyed — the PropertyKey owns its name, and the NameTable owns a second
    // stable key node used for probing; equality is by content.
    const PropertyKey* insert(PropertyKey key)
    {
        const std::string name = key.name();
        return table_.insert(name, std::move(key));
    }

    const PropertyKey* find(std::string_view name) const noexcept
    {
        return table_.find(name);
    }

    std::size_t size() const noexcept { return table_.size(); }

private:
    vaml::detail::NameTable<PropertyKey> table_;
};

VAML_CORE_EXPORT
PropertyRegistry::PropertyRegistry()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
PropertyRegistry::~PropertyRegistry() = default;

VAML_CORE_EXPORT
PropertyRegistry::PropertyRegistry(PropertyRegistry&&) noexcept = default;

VAML_CORE_EXPORT
PropertyRegistry& PropertyRegistry::operator=(PropertyRegistry&&) noexcept = default;

VAML_CORE_EXPORT
const PropertyKey& PropertyRegistry::register_key(PropertyKey key)
{
    // Capture the name up front for a clear error message (the NameTable's
    // content-keyed insert returns nullptr on a duplicate and leaves the
    // existing entry unchanged).
    std::string name = key.name();

    const PropertyKey* stored = impl_->insert(std::move(key));
    if (stored == nullptr)
    {
        std::string msg = "property already registered: '" + name + "'";
        throw PropertyAlreadyRegistered(msg.c_str());
    }
    return *stored;
}

VAML_CORE_EXPORT
const PropertyKey* PropertyRegistry::try_find(std::string_view name) const noexcept
{
    return impl_->find(name);
}

VAML_CORE_EXPORT
const PropertyKey& PropertyRegistry::find(std::string_view name) const
{
    const PropertyKey* stored = impl_->find(name);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + std::string(name) + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return *stored;
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::id_of(std::string_view name) const
{
    const PropertyKey* stored = impl_->find(name);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + std::string(name) + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return PropertyId::of(stored->name());
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::try_id(std::string_view name) const noexcept
{
    const PropertyKey* stored = impl_->find(name);
    // Unregistered name -> invalid (empty) id; caller distinguishes via
    // PropertyId::valid().
    return stored == nullptr ? PropertyId{} : PropertyId::of(stored->name());
}

VAML_CORE_EXPORT
std::size_t PropertyRegistry::size() const noexcept
{
    return impl_->size();
}

VAML_CORE_EXPORT
PropertyRegistry& PropertyRegistry::global() noexcept
{
    // Meyers singleton: function-local static is constructed on first use and
    // destroyed in reverse construction order at program exit. This is the
    // global name -> key dedupe master consumed by DependencyProperty::Register
    // (S039). No static-init-order fiasco (function-local static init is
    // thread-safe in C++11+).
    static PropertyRegistry instance;
    return instance;
}

} // namespace vaml
