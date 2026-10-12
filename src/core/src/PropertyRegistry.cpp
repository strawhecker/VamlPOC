// S032 — PropertyRegistry out-of-line definitions (pimpl). Maps property
// NAME -> PropertyKey and enforces name uniqueness (dedupe detection).

#include "vaml/core/PropertyRegistry.h"

#include <unordered_map>
#include <utility>

namespace vaml {

class PropertyRegistry::Impl
{
public:
    // Returns the pointer to the stored key, or nullptr on a name collision
    // (in which case the existing entry is left unchanged).
    const PropertyKey* insert(PropertyKey key)
    {
        auto [it, inserted] = map_.emplace(key.name(), std::move(key));
        return inserted ? &it->second : nullptr;
    }

    const PropertyKey* find(const std::string& name) const noexcept
    {
        auto it = map_.find(name);
        return it == map_.end() ? nullptr : &it->second;
    }

    std::size_t size() const noexcept { return map_.size(); }

private:
    std::unordered_map<std::string, PropertyKey> map_;
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
    // Capture the name up front: on a successful emplace it is moved-from; on
    // a collision match it is untouched (unordered_map::emplace does not
    // consume arguments when the key already exists).
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
const PropertyKey* PropertyRegistry::try_find(const std::string& name) const noexcept
{
    return impl_->find(name);
}

VAML_CORE_EXPORT
const PropertyKey& PropertyRegistry::find(const std::string& name) const
{
    const PropertyKey* stored = impl_->find(name);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + name + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return *stored;
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::id_of(const std::string& name) const
{
    const PropertyKey* stored = impl_->find(name);
    if (stored == nullptr)
    {
        std::string msg = "property not found: '" + name + "'";
        throw PropertyNotFound(msg.c_str());
    }
    return PropertyId::of(stored->name());
}

VAML_CORE_EXPORT
PropertyId PropertyRegistry::try_id(const std::string& name) const noexcept
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

} // namespace vaml
