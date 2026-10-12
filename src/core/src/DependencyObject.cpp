// S036 — DependencyObject out-of-line definitions. S036 (dep:S035).
//
// Implements the S035 apply pipeline verbatim in Set():
//   coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//   real change -> store.
//
// Storage is pimpl'd so the public header exposes no std::unordered_map and no
// move-only PropertyMetadata value member.

#include "vaml/core/DependencyObject.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace vaml {

struct DependencyObject::Impl
{
    // One entry per REGISTERED property. `local` is empty (Value::Null) until
    // a Set() stores an override; an empty `local` means "default applied".
    // The key is stored by value (ref returned to the caller) and the
    // property is addressed by its NAME.
    struct PropEntry
    {
        PropertyKey    key;
        PropertyMetadata metadata;
        Value          local = Value::Null(); // Value::Null == no local override

        PropEntry(PropertyKey k, PropertyMetadata m)
            : key(std::move(k)), metadata(std::move(m)) {}
    };

    // Keyed by the OWNED property NAME (std::string), NOT by PropertyId:
    // a PropertyId is a non-owning std::string_view that would dangle once the
    // registering PropertyKey (its storage source) dies. Keying on an owned
    // std::string makes lookups stable against both SSO and heap-backed names.
    std::unordered_map<std::string, PropEntry> store;
    std::thread::id                            affinity = std::this_thread::get_id();
};

VAML_CORE_EXPORT
DependencyObject::DependencyObject()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
DependencyObject::~DependencyObject() = default;

VAML_CORE_EXPORT
DependencyObject::DependencyObject(DependencyObject&&) noexcept = default;

VAML_CORE_EXPORT
DependencyObject& DependencyObject::operator=(DependencyObject&&) noexcept = default;

VAML_CORE_EXPORT
const PropertyKey& DependencyObject::register_property(PropertyKey key, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard).
    if (key.type() != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyObject::register_property: key.type() does not match metadata's bound value type");
    }

    // Determine the canonical owned name (dedupe + store key) BEFORE moving
    // `key` into the entry.
    const std::string name = key.name();

    const auto [it, inserted] = impl_->store.emplace(
        name, Impl::PropEntry(std::move(key), std::move(metadata)));
    if (!inserted)
    {
        // Name already registered -> throw; emplace with a duplicate key does
        // NOT overwrite the existing entry, so the store is unchanged.
        throw PropertyAlreadyRegistered("DependencyObject::register_property: a property with this name is already registered");
    }

    return it->second.key;
}

VAML_CORE_EXPORT
Value DependencyObject::Get(PropertyId id) const
{
    const auto it = impl_->store.find(std::string(id.name()));
    if (it == impl_->store.end())
    {
        throw PropertyNotFound("DependencyObject::Get: no property registered with this id");
    }

    const Impl::PropEntry& entry = it->second;
    // "Default applied": empty local == no override -> return defaultValue().
    return entry.local.isNull() ? entry.metadata.defaultValue() : entry.local;
}

VAML_CORE_EXPORT
SetResult DependencyObject::Set(PropertyId id, const Value& input)
{
    const auto it = impl_->store.find(std::string(id.name()));
    if (it == impl_->store.end())
    {
        throw PropertyNotFound("DependencyObject::Set: no property registered with this id");
    }

    Impl::PropEntry& entry = it->second;
    PropertyMetadata& md   = entry.metadata;

    // S035 pipeline step 1: coerce.
    const Value coerced = md.coerce(input);

    // S035 pipeline step 2: validate ON THE COERCED value.
    if (!md.validate(coerced))
    {
        return SetResult::REJECTED; // store unchanged, no cb
    }

    // Compute the current EFFECTIVE value (local override else default) for
    // both the change comparison and the changed cb's `old`.
    const Value currentEffective = entry.local.isNull() ? md.defaultValue() : entry.local;

    // S035 pipeline step 3: fire changed cb ONLY on a real change.
    if (coerced == currentEffective)
    {
        return SetResult::NO_CHANGE;
    }
    md.notifyChanged(currentEffective, coerced);

    // S035 pipeline step 4: store.
    entry.local = coerced;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
std::thread::id DependencyObject::thread_affinity_id() const noexcept
{
    return impl_->affinity;
}

} // namespace vaml
