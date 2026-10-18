// S039 — DependencyProperty out-of-line definitions: the Register landing
// site + global companion table + the read path. This is the authored,
// resolved S036-re-scope global static registry surface (recorded NOT silent).

#include "vaml/core/DependencyProperty.h"

#include "vaml/core/PropertyRegistry.h"
#include "NameTable.h" // S143 (B4): content-keyed probe store for GlobalTable

#include <string>
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

// The companion static table: name -> the full DependencyProperty definition.
// Lives for the process lifetime alongside PropertyRegistry::global().
// Function-local static => no static-init-order fiasco; constructed on first
// Register/TryGet/Get.
//
// S143 (B4) re-keys GlobalTable OFF std::unordered_map<std::string,...> and ONTO
// the S141-pattern content-keyed probe store (NameTable), which OWNS its stable
// key nodes (process lifetime) and matches BY CONTENT with NO transient
// std::string materialized on lookup. This is the S140 DECISION 1 resolution for
// a GLOBAL table — it cannot hold thread-local intern views, so it stays
// content-keyed with self-owned stable storage. ADR-018: no lock added (the
// single-threaded-registration-then-read contract is unchanged).
namespace {

struct GlobalTable
{
    vaml::detail::NameTable<std::unique_ptr<DependencyProperty>> byName;

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
    auto owner = std::unique_ptr<DependencyProperty>(
        new DependencyProperty(std::make_unique<Impl>(ownedName, type, std::move(metadata), std::string(ownerTypeId))));
    std::unique_ptr<DependencyProperty>* stored =
        table.byName.insert(ownedName, std::move(owner));
    if (stored == nullptr)
    {
        // Already present (defensive, cannot happen given the registry dedupe
        // above): keep the EXISTING registration (store unchanged).
        const std::unique_ptr<DependencyProperty>* existing = table.byName.find(ownedName);
        return *existing->get();
    }
    return **stored;
}

VAML_CORE_EXPORT
const std::string& DependencyProperty::owner_type_id() const noexcept
{
    return impl_->ownerTypeId;
}

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::TryGet(std::string_view name) noexcept
{
    // S143 B4: content lookup with NO transient std::string materialized.
    GlobalTable& table = GlobalTable::instance();
    const std::unique_ptr<DependencyProperty>* found = table.byName.find(name);
    if (found == nullptr || *found == nullptr)
    {
        return nullptr;
    }
    return found->get();
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
