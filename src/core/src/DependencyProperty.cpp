// S039 — DependencyProperty out-of-line definitions: the Register landing
// site + global companion table + the read path. This is the authored,
// resolved S036-re-scope global static registry surface (recorded NOT silent).
// S801 re-keys the companion GlobalTable + read path onto the COMPOSITE
// (name, owner) pair (WPF `FromNameKey(name, ownerType)` ground truth; S033/S039
// name-only drift resolved at its source).
//
// S146 (refactor gate): the pimpl (class Impl) is COLLAPSED — DependencyProperty
// now holds its definitional data (name_, metadata_, ownerTypeId_) as plain
// by-value members. The companion GlobalTable below keeps the S143 content-keyed
// vaml::detail::NameTable, re-keyed S801 onto the (name, owner) composite.

#include "vaml/core/DependencyProperty.h"

#include "vaml/core/PropertyRegistry.h"
#include "vaml/core/detail/NameTable.h" // S143 (B4): content-keyed probe store for GlobalTable

#include <string>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
DependencyProperty::DependencyProperty(std::string name,
                                       PropertyMetadata metadata, std::string ownerTypeId)
    : name_(std::move(name))
    , metadata_(std::move(metadata))
    , ownerTypeId_(std::move(ownerTypeId))
{
}

VAML_CORE_EXPORT
DependencyProperty::DependencyProperty(DependencyProperty&&) noexcept = default;

VAML_CORE_EXPORT
DependencyProperty& DependencyProperty::operator=(DependencyProperty&&) noexcept = default;

VAML_CORE_EXPORT
DependencyProperty::~DependencyProperty() = default;

// The companion static table: (name, owner) -> the full DependencyProperty
// definition. Lives for the process lifetime alongside PropertyRegistry::global().
// Function-local static => no static-init-order fiasco.
//
// S143 (B4) re-keys GlobalTable OFF std::unordered_map and ONTO the S141-pattern
// content-keyed probe store (NameTable), which OWNS its stable key nodes (process
// lifetime) and matches BY CONTENT with NO transient std::string materialized on
// lookup. S801 re-keys that store's key onto the (name, owner) COMPOSITE (the
// S140 DECISION 1 resolution for a GLOBAL table — self-owned stable composite
// storage, content-keyed, ADR-018: no lock added).
namespace {

struct GlobalTable
{
    vaml::detail::NameTable<std::unique_ptr<DependencyProperty>,
                            vaml::detail::CompositeKey> byName;

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
    const std::string ownedOwner(ownerTypeId);

    // Dedupe on the (name, ownerTypeId) PAIR globally (WPF-style). register_key
    // throws PropertyAlreadyRegistered only on a SAME-PAIR collision and leaves
    // the registry (and thus our companion table) unchanged. A same-name/
    // different-owner pair is DISTINCT.
    PropertyRegistry& registry = PropertyRegistry::global();
    PropertyKey key(ownedName, type, ownedOwner, metadata.flags());
    (void)registry.register_key(std::move(key));

    // Insert the full (name+metadata) definition into the companion table under
    // the (name, owner) pair. If the pair is somehow already present here
    // (cannot happen given registry dedupe above), keep the EXISTING
    // registration (store unchanged).
    GlobalTable& table = GlobalTable::instance();
    auto owner = std::unique_ptr<DependencyProperty>(
        new DependencyProperty(ownedName, std::move(metadata), ownedOwner));
    std::unique_ptr<DependencyProperty>* stored =
        table.byName.insert(ownedName, ownedOwner, std::move(owner));
    if (stored == nullptr)
    {
        // Already present (defensive, cannot happen given the registry dedupe
        // above): keep the EXISTING registration (store unchanged).
        const std::unique_ptr<DependencyProperty>* existing =
            table.byName.find(ownedName, ownedOwner);
        return *existing->get();
    }
    return **stored;
}

VAML_CORE_EXPORT
const std::string& DependencyProperty::owner_type_id() const noexcept
{
    return ownerTypeId_;
}

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::TryGet(std::string_view name,
                                                     std::string_view owner) noexcept
{
    // S143 B4 + S801 composite: content lookup with NO transient std::string
    // materialized. NEVER an owner-less lookup — the owner is always carried.
    GlobalTable& table = GlobalTable::instance();
    const std::unique_ptr<DependencyProperty>* found = table.byName.find(name, owner);
    if (found == nullptr || *found == nullptr)
    {
        return nullptr;
    }
    return found->get();
}

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::TryGetByName(std::string_view name) noexcept
{
    // [MINIMAL REAL] S803-resolved. Guarded unique-name read: only a name held
    // by EXACTLY ONE distinct (owner) registration resolves; ambiguity returns
    // nullptr (the caller must not silently pick). The store stays
    // composite-keyed; this never materializes an owner-less key.
    GlobalTable& table = GlobalTable::instance();
    std::size_t matches = 0;
    const std::unique_ptr<DependencyProperty>* found =
        table.byName.find_unique_name(name, &matches);
    if (found == nullptr || *found == nullptr || matches != 1)
    {
        return nullptr;
    }
    return found->get();
}

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::GetByName(std::string_view name)
{
    const DependencyProperty* dp = TryGetByName(name);
    if (dp == nullptr)
    {
        throw PropertyNotFound("DependencyProperty::GetByName: no property registered with exactly this name (absent or ambiguous)");
    }
    return *dp;
}

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::Get(std::string_view name,
                                                  std::string_view owner)
{
    const DependencyProperty* dp = TryGet(name, owner);
    if (dp == nullptr)
    {
        throw PropertyNotFound("DependencyProperty::Get: no property registered for this (name, owner) pair");
    }
    return *dp;
}

VAML_CORE_EXPORT
PropertyId DependencyProperty::property_id() const noexcept
{
    // The id wraps the OWNED name + owner as string_views. Both back the
    // object's OWNED name_ / ownerTypeId_ permanent std::strings, so the views
    // are valid for the process lifetime.
    return PropertyId::of(name_, ownerTypeId_);
}

VAML_CORE_EXPORT
const PropertyMetadata& DependencyProperty::metadata() const noexcept
{
    return metadata_;
}

VAML_CORE_EXPORT
PropertyFlags DependencyProperty::flags() const noexcept
{
    return metadata_.flags();
}

} // namespace vaml
