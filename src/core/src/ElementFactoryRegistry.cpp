// S087 — ElementFactoryRegistry out-of-line definitions (pimpl). Maps element
// type NAME -> ElementFactory (production of unique_ptr<DependencyObject>),
// enforcing name uniqueness (dedupe) + deterministic insertion-order listing.
//
// S144 (dep:S143): the internal index is re-keyed OFF std::unordered_map<std::string,...>
// + an owned-name order vector and ONTO vaml::detail::NameTable<ElementFactory>
// (the S141-pattern content-keyed probe store, self-owned stable keys, content
// match, NO transient std::string on find). registered_names() now returns
// string_views into the NameTable's OWNED stable name storage (no by-value
// vector<std::string> copy — removes the owned-name gotcha). ADR-018: no lock.
//
// S146 (refactor gate): the pimpl (class Impl + impl_) is COLLAPSED — the
// NameTable<ElementFactory> member is held BY VALUE in the registry (see
// ElementFactoryRegistry.h). The delegating methods call table_ directly (no
// impl_-> trampoline, no per-object heap hop, no Impl heap node).

#include "vaml/core/ElementFactoryRegistry.h"

#include <utility>

namespace vaml {

VAML_CORE_EXPORT
ElementFactoryRegistry::ElementFactoryRegistry() = default;

VAML_CORE_EXPORT
ElementFactoryRegistry::~ElementFactoryRegistry() = default;

VAML_CORE_EXPORT
ElementFactoryRegistry::ElementFactoryRegistry(ElementFactoryRegistry&&) noexcept = default;

VAML_CORE_EXPORT
ElementFactoryRegistry& ElementFactoryRegistry::operator=(ElementFactoryRegistry&&) noexcept = default;

VAML_CORE_EXPORT
void ElementFactoryRegistry::register_element(std::string_view typeName, ElementFactory factory)
{
    // The NameTable's content insert returns nullptr on a duplicate and leaves
    // the existing entry unchanged (dedupe, S032 discipline).
    if (table_.insert(std::string(typeName), std::move(factory)) == nullptr)
    {
        std::string msg = "element already registered: '" + std::string(typeName) + "'";
        throw ElementAlreadyRegistered(msg.c_str());
    }
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> ElementFactoryRegistry::try_create(std::string_view typeName) const noexcept
{
    const ElementFactory* factory = table_.find(typeName);
    if (factory == nullptr)
    {
        return nullptr;
    }
    try
    {
        return (*factory)();
    }
    catch (...)
    {
        // A factory that throws is a bug in the element under construction;
        // try_create's noexcept contract converts it to a null result. NO-LAZY:
        // this is the S089/control-sprint resolution point — a throwing factory
        // is never silently swallowed here beyond converting it to "no element".
        return nullptr;
    }
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> ElementFactoryRegistry::create(std::string_view typeName) const
{
    const ElementFactory* factory = table_.find(typeName);
    if (factory == nullptr)
    {
        std::string msg = "element not found: '" + std::string(typeName) + "'";
        throw ElementNotFound(msg.c_str());
    }
    // The factory may throw e.g. bad_alloc; an element-specific
    // construction failure propagates to the caller normally (S088 maps it).
    return (*factory)();
}

VAML_CORE_EXPORT
bool ElementFactoryRegistry::is_registered(std::string_view typeName) const noexcept
{
    return table_.find(typeName) != nullptr;
}

VAML_CORE_EXPORT
std::size_t ElementFactoryRegistry::size() const noexcept
{
    return table_.size();
}

VAML_CORE_EXPORT
std::vector<std::string_view> ElementFactoryRegistry::registered_names() const
{
    return table_.names();
}

VAML_CORE_EXPORT
ElementFactoryRegistry& ElementFactoryRegistry::global() noexcept
{
    // Meyers singleton: function-local static constructed on first use,
    // destroyed in reverse construction order at exit. No static-init-order
    // fiasco (C++11 thread-safe function-local static init). Matches
    // PropertyRegistry::global()/DependencyProperty::global().
    static ElementFactoryRegistry instance;
    return instance;
}

} // namespace vaml
