// S360 (dep:S329) — ResourceDictionary out-of-line definitions. See the
// companion header for the design.

#include "vaml/core/ResourceDictionary.h"

#include "vaml/core/Color.h"     // parse_color (system resource seeds)
#include "vaml/core/Intern.h"    // intern()

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace vaml {

// ---------------------------------------------------------------------------
// ELEMENT RESOURCE SIDE-TABLE (S360).
//
// A UIElement's OWN ResourceDictionary is stored OUT-OF-LINE in this process-
// wide side-table keyed by the element's address — NOT as an inline unique_ptr
// member — so an element/grid that never declares resources does not grow and
// the S145 object-pool slot-size invariant is preserved. An element with
// resources has exactly ONE owner (the dictionary) and one lifetime (destroyed
// when its entry is removed with remove_element_resources, or when the whole
// table is reset on master reset). ADR-018: this is single-threaded UI use; no
// lock (mirrors the registries' single-threaded startup discipline).
//
// NOTE on lifetime: the side-table holds a non-owning UIElement* key and owns
// the ResourceDictionary. A removed/destroyed element must be released via
// remove_element_resources(dynamic_cast<UIElement*>) so no dangling key is left.
// The view/test teardown calls the master reset; readers never see a dangling
// key because the S039 parent-chain walk only follows live ancestors.
namespace detail {

using ElementResourcesTable = std::unordered_map<const UIElement*, std::unique_ptr<ResourceDictionary>>;

VAML_CORE_EXPORT
ElementResourcesTable& element_resources_table() noexcept
{
    static ElementResourcesTable table;
    return table;
}

VAML_CORE_EXPORT
void set_element_resources(UIElement* element, std::unique_ptr<ResourceDictionary> resources)
{
    if (element == nullptr)
    {
        return;
    }
    if (resources == nullptr)
    {
        element_resources_table().erase(element);
        return;
    }
    element_resources_table()[element] = std::move(resources);
}

VAML_CORE_EXPORT
const ResourceDictionary* get_element_resources(const UIElement* element) noexcept
{
    if (element == nullptr)
    {
        return nullptr;
    }
    const auto& table = element_resources_table();
    const auto it = table.find(element);
    return it == table.end() ? nullptr : it->second.get();
}

VAML_CORE_EXPORT
void remove_element_resources(UIElement* element) noexcept
{
    if (element != nullptr)
    {
        element_resources_table().erase(element);
    }
}

VAML_CORE_EXPORT
void element_resources_master_reset() noexcept
{
    element_resources_table().clear();
}

} // namespace detail

// ------------------------------------------------------------------ ResourceLookupException
VAML_CORE_EXPORT
ResourceLookupException::ResourceLookupException(ResourceLookupError kind,
                                                 std::string key,
                                                 std::string what)
    : std::runtime_error(std::move(what))
    , kind_(kind)
    , key_(std::move(key))
{
}

// ------------------------------------------------------------------ ResourceValue
VAML_CORE_EXPORT
ResourceValue::ResourceValue(std::unique_ptr<DependencyObject> obj)
    : kind_(Kind::Object)
    , object_(std::move(obj))
{
}

VAML_CORE_EXPORT
ResourceValue::ResourceValue(Value value)
    : kind_(Kind::Plain)
    , value_(std::move(value))
{
}

VAML_CORE_EXPORT
ResourceValue ResourceValue::DeferredRef(RefKind refKind, std::string_view key)
{
    ResourceValue rv;
    rv.kind_ = Kind::Deferred;
    rv.refKind_ = refKind;
    rv.refKey_ = std::string(key);
    return rv;
}

VAML_CORE_EXPORT
ResourceValue::ResourceValue(ResourceValue&& other) noexcept
    : kind_(other.kind_)
    , object_(std::move(other.object_))
    , value_(std::move(other.value_))
    , refKind_(other.refKind_)
    , refKey_(std::move(other.refKey_))
{
    other.kind_ = Kind::Plain;
}

VAML_CORE_EXPORT
ResourceValue& ResourceValue::operator=(ResourceValue&& other) noexcept
{
    if (this != &other)
    {
        kind_ = other.kind_;
        object_ = std::move(other.object_);
        value_ = std::move(other.value_);
        refKind_ = other.refKind_;
        refKey_ = std::move(other.refKey_);
        other.kind_ = Kind::Plain;
    }
    return *this;
}

VAML_CORE_EXPORT
ResourceValue::~ResourceValue() = default;

VAML_CORE_EXPORT
DependencyObject* ResourceValue::object() noexcept
{
    return object_.get();
}

VAML_CORE_EXPORT
const DependencyObject* ResourceValue::object() const noexcept
{
    return object_.get();
}

VAML_CORE_EXPORT
const Value& ResourceValue::value() const noexcept
{
    return value_;
}

VAML_CORE_EXPORT
ResourceValue::RefKind ResourceValue::ref_kind() const noexcept
{
    return refKind_;
}

VAML_CORE_EXPORT
std::string_view ResourceValue::ref_key() const noexcept
{
    return refKey_;
}

// ------------------------------------------------------------------ ResourceDictionary

VAML_CORE_EXPORT
ResourceDictionary::ResourceDictionary() = default;

VAML_CORE_EXPORT
ResourceDictionary::~ResourceDictionary() = default;

VAML_CORE_EXPORT
ResourceDictionary::ResourceDictionary(ResourceDictionary&& other) noexcept
    : entries_(std::move(other.entries_))
    , merged_(std::move(other.merged_))
{
}

VAML_CORE_EXPORT
ResourceDictionary& ResourceDictionary::operator=(ResourceDictionary&& other) noexcept
{
    if (this != &other)
    {
        entries_ = std::move(other.entries_);
        merged_ = std::move(other.merged_);
    }
    return *this;
}

VAML_CORE_EXPORT
void ResourceDictionary::set(std::string_view key, ResourceValue value)
{
    if (find_self(key) != nullptr)
    {
        throw ResourceAlreadyRegistered("resource key already registered");
    }
    entries_.push_back(Entry{ intern(key), std::move(value) });
}

VAML_CORE_EXPORT
void ResourceDictionary::set(std::string_view key, std::unique_ptr<DependencyObject> obj)
{
    set(key, ResourceValue(std::move(obj)));
}

VAML_CORE_EXPORT
bool ResourceDictionary::remove(std::string_view key) noexcept
{
    for (auto it = entries_.begin(); it != entries_.end(); ++it)
    {
        if (it->key == key)
        {
            entries_.erase(it); // owns the ResourceValue -> releases the object
            return true;
        }
    }
    return false;
}

VAML_CORE_EXPORT
bool ResourceDictionary::has(std::string_view key) const noexcept
{
    return try_get(key) != nullptr;
}

VAML_CORE_EXPORT
const ResourceValue* ResourceDictionary::try_get(std::string_view key) const noexcept
{
    // SELF-WINS: a merged child only contributes when THIS dictionary has no
    // entry for the key. Among merged children, the LAST merged wins (reverse
    // lookup — WPF-faithful sibling merge precedence).
    if (const Entry* e = find_self(key); e != nullptr)
    {
        return &e->value;
    }
    for (auto it = merged_.rbegin(); it != merged_.rend(); ++it)
    {
        if (const ResourceValue* v = (*it)->try_get(key); v != nullptr)
        {
            return v;
        }
    }
    return nullptr;
}

VAML_CORE_EXPORT
const ResourceValue& ResourceDictionary::get(std::string_view key) const
{
    if (const ResourceValue* v = try_get(key); v != nullptr)
    {
        return *v;
    }
    throw ResourceLookupException(
        ResourceLookupError::MissingKey, std::string(key),
        "resource '" + std::string(key) + "' not found");
}

VAML_CORE_EXPORT
void ResourceDictionary::merge_with(ResourceDictionary& merged)
{
    if (std::find(merged_.begin(), merged_.end(), &merged) == merged_.end())
    {
        merged_.push_back(&merged);
    }
}

VAML_CORE_EXPORT
const std::vector<ResourceDictionary*>& ResourceDictionary::merged() const noexcept
{
    return merged_;
}

VAML_CORE_EXPORT
std::size_t ResourceDictionary::size() const noexcept
{
    return entries_.size();
}

VAML_CORE_EXPORT
std::vector<std::string_view> ResourceDictionary::keys() const
{
    std::vector<std::string_view> out;
    out.reserve(entries_.size());
    for (const Entry& e : entries_)
    {
        out.push_back(e.key);
    }
    return out;
}

VAML_CORE_EXPORT
const ResourceDictionary::Entry* ResourceDictionary::find_self(std::string_view key) const noexcept
{
    for (const Entry& e : entries_)
    {
        if (e.key == key)
        {
            return &e;
        }
    }
    return nullptr;
}

VAML_CORE_EXPORT
ResourceDictionary::Entry* ResourceDictionary::find_self(std::string_view key) noexcept
{
    for (Entry& e : entries_)
    {
        if (e.key == key)
        {
            return &e;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------------ ReferenceDictionary

VAML_CORE_EXPORT
ResourceDictionary& ReferenceDictionary::app_resources() noexcept
{
    static ResourceDictionary app; // Meyer's singleton (mirrors the global registries)
    return app;
}

VAML_CORE_EXPORT
const ResourceDictionary& ReferenceDictionary::system_resources() noexcept
{
    // A small documented immutable SYSTEM key set for S360. The full
    // SystemColors/SystemBrushes feed is S12/renderer/culture — recorded
    // NO-GAP, not silent. Seeds a couple of standard system keys so the
    // system scope is real and testable today.
    static const ResourceDictionary* system = [] {
        auto* d = new ResourceDictionary();
        // "#FFF0F0F0" — the standard WPF "Control" light background.
        d->set("SystemWindowBackgroundColor", ResourceValue(Value::Color(parse_color("#FFF0F0F0"))));
        // "#FF000000" — the standard "ControlText" near-black foreground.
        d->set("SystemControlTextColor", ResourceValue(Value::Color(parse_color("#FF000000"))));
        return d;
    }();
    return *system;
}

// ------------------------------------------------------------------ lookup

VAML_CORE_EXPORT
const ResourceValue* find_resource(const ResourceDictionary& dict, std::string_view key) noexcept
{
    return dict.try_get(key);
}

VAML_CORE_EXPORT
const ResourceValue* find_for_element(const UIElement& element, std::string_view key) noexcept
{
    // 1) Tree scope: the element's own Resources, then each UIElement ancestor's
    //    Resources (walk the parent chain). Note: `parent()` is the S039 loose
    //    parent (DependencyObject*); only UIElement ancestors carry Resources.
    {
        const ResourceDictionary* own = element.resources();
        if (own != nullptr)
        {
            if (const ResourceValue* v = own->try_get(key); v != nullptr)
            {
                return v;
            }
        }
        for (DependencyObject* p = element.parent(); p != nullptr; p = p->parent())
        {
            auto* ui = dynamic_cast<UIElement*>(p);
            if (ui == nullptr)
            {
                continue;
            }
            const ResourceDictionary* r = ui->resources();
            if (r != nullptr)
            {
                if (const ResourceValue* v = r->try_get(key); v != nullptr)
                {
                    return v;
                }
            }
        }
    }

    // 2) App scope.
    if (const ResourceValue* v = ReferenceDictionary::app_resources().try_get(key); v != nullptr)
    {
        return v;
    }

    // 3) System scope (last).
    return ReferenceDictionary::system_resources().try_get(key);
}

} // namespace vaml
