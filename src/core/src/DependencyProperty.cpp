// S039 — DependencyProperty out-of-line definitions: the Register landing
// site + global companion table + the read path. This is the authored,
// resolved S036-re-scope global static registry surface (recorded NOT silent).

#include "vaml/core/DependencyProperty.h"

#include "vaml/core/PropertyRegistry.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace vaml {

class DependencyProperty::Impl
{
public:
    std::string        name;     // OWNED name (stable identity + id source)
    ValueType          type;     // bound value type
    PropertyMetadata   metadata; // flags + default + callbacks (the definition)
    std::string        ownerTypeId; // S043: the DECLARING class's identity (attached owner A)

    Impl(std::string n, ValueType t, PropertyMetadata m, std::string owner)
        : name(std::move(n))
        , type(t)
        , metadata(std::move(m))
        , ownerTypeId(std::move(owner))
    {
    }
};

VAML_CORE_EXPORT
DependencyProperty::DependencyProperty(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

// The companion static table: name (owned std::string, stable) -> the full
// DependencyProperty definition. Lives for the process lifetime alongside
// PropertyRegistry::global(). Function-local static => no static-init-order
// fiasco; constructed on first Register/TryGet/Get.
namespace {

struct GlobalTable
{
    std::unordered_map<std::string, std::unique_ptr<DependencyProperty>> byName;

    static GlobalTable& instance()
    {
        static GlobalTable table;
        return table;
    }
};

} // namespace

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::Register(
    std::string_view name, ValueType type, std::string_view ownerTypeId, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard).
    if (type != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyProperty::Register: key type does not match metadata's bound value type");
    }

    const std::string ownedName(name);

    // Dedupe on NAME globally (WPF-style). register_key throws
    // PropertyAlreadyRegistered on a collision and leaves the registry (and
    // thus our companion table) unchanged.
    PropertyRegistry& registry = PropertyRegistry::global();
    PropertyKey key(ownedName, type, std::string(ownerTypeId), metadata.flags());
    (void)registry.register_key(std::move(key));

    // Insert the full (name+metadata) definition into the companion table. If
    // `name` is somehow already present here (cannot happen given registry
    // dedupe above), keep the EXISTING registration (store unchanged).
    GlobalTable& table = GlobalTable::instance();
    auto [it, inserted] = table.byName.emplace(
        ownedName,
        std::unique_ptr<DependencyProperty>(
            new DependencyProperty(std::make_unique<Impl>(ownedName, type, std::move(metadata), std::string(ownerTypeId)))));
    (void)inserted;
    return *it->second;
}

VAML_CORE_EXPORT
const std::string& DependencyProperty::owner_type_id() const noexcept
{
    return impl_->ownerTypeId;
}

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::TryGet(std::string_view name) noexcept
{
    GlobalTable& table = GlobalTable::instance();
    auto it = table.byName.find(std::string(name));
    if (it == table.byName.end() || it->second == nullptr)
    {
        return nullptr;
    }
    return it->second.get();
}

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::Get(std::string_view name)
{
    const DependencyProperty* dp = TryGet(name);
    if (dp == nullptr)
    {
        throw PropertyNotFound("DependencyProperty::Get: no property registered with this name");
    }
    return *dp;
}

VAML_CORE_EXPORT
PropertyId DependencyProperty::property_id() const noexcept
{
    // The id wraps the OWNED name as a string_view. The owner is this Impl's
    // permanent std::string, so the view is valid for the process lifetime.
    return PropertyId::of(impl_->name);
}

VAML_CORE_EXPORT
const PropertyMetadata& DependencyProperty::metadata() const noexcept
{
    return impl_->metadata;
}

VAML_CORE_EXPORT
PropertyFlags DependencyProperty::flags() const noexcept
{
    return impl_->metadata.flags();
}

} // namespace vaml
