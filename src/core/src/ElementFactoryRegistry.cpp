// S087 — ElementFactoryRegistry out-of-line definitions (pimpl). Maps element
// type NAME -> ElementFactory (production of unique_ptr<DependencyObject>),
// enforcing name uniqueness (dedupe) + deterministic insertion-order listing.

#include "vaml/core/ElementFactoryRegistry.h"

#include <unordered_map>
#include <utility>

namespace vaml {

class ElementFactoryRegistry::Impl
{
public:
    // Returns true on a successful insert, false on a name collision (the
    // existing entry is left unchanged — the moving factory is consumed by
    // emplace only on success, per unordered_map guarantees).
    bool insert(std::string_view typeName, ElementFactory factory)
    {
        auto [prev, inserted] =
            map_.try_emplace(std::string(typeName), std::move(factory));
        (void)prev;
        return inserted;
    }

    const ElementFactory* find(std::string_view typeName) const noexcept
    {
        // MSVC lacks transparent heterogeneous lookup for string_view on an
        // unordered_map<std::string,...>; build a key string for the lookup.
        // The reader path is not registration hot-path, so one short-string
        // construction per resolve is acceptable (SSO).
        auto it = map_.find(std::string(typeName));
        return it == map_.end() ? nullptr : &it->second;
    }

    std::size_t size() const noexcept { return map_.size(); }

    // Insertion order: the order-list is appended only on a successful insert,
    // so it exactly mirrors registration order and never duplicates a name.
    std::vector<std::string> names() const { return order_; }

    // Public-api shape helpers.
    void record_order(std::string_view typeName) { order_.emplace_back(typeName); }

private:
    std::unordered_map<std::string, ElementFactory> map_;
    std::vector<std::string> order_;
};

VAML_CORE_EXPORT
ElementFactoryRegistry::ElementFactoryRegistry()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
ElementFactoryRegistry::~ElementFactoryRegistry() = default;

VAML_CORE_EXPORT
ElementFactoryRegistry::ElementFactoryRegistry(ElementFactoryRegistry&&) noexcept = default;

VAML_CORE_EXPORT
ElementFactoryRegistry& ElementFactoryRegistry::operator=(ElementFactoryRegistry&&) noexcept = default;

VAML_CORE_EXPORT
void ElementFactoryRegistry::register_element(std::string_view typeName, ElementFactory factory)
{
    // Dedupe first; only record insertion order on a successful insert (so the
    // order-list never diverges from the map's names).
    if (!impl_->insert(typeName, std::move(factory)))
    {
        std::string msg = "element already registered: '" + std::string(typeName) + "'";
        throw ElementAlreadyRegistered(msg.c_str());
    }
    impl_->record_order(typeName);
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> ElementFactoryRegistry::try_create(std::string_view typeName) const noexcept
{
    const ElementFactory* factory = impl_->find(typeName);
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
    const ElementFactory* factory = impl_->find(typeName);
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
    return impl_->find(typeName) != nullptr;
}

VAML_CORE_EXPORT
std::size_t ElementFactoryRegistry::size() const noexcept
{
    return impl_->size();
}

VAML_CORE_EXPORT
std::vector<std::string> ElementFactoryRegistry::registered_names() const
{
    return impl_->names();
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
