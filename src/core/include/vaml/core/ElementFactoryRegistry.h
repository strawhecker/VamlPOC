#pragma once
// VAML ElementFactoryRegistry — maps element TYPE NAME -> a factory producing
// std::unique_ptr<DependencyObject>. S087 (dep:S086). Consumed by S088
// (Resolve "Button" -> instance; unknown type -> structured error naming
// type+line) and S091 (Runtime VamlReader builds children from type names).
//
// Design (mirrors the S032/S039 property-registry + DependencyProperty:
//   - NAME-UNIQUE registration, enforcing dedupe. register_element() throws
//     ElementAlreadyRegistered on a name collision; the registry is unchanged
//     by a rejected insert (the S032 discipline).
//   - The registry stores an OWNED copy of each registered name (same
//     SSO/dangle discipline as the property system — callers may pass a
//     transient std::string_view; the map keeps a stable std::string).
//   - try_create() is the noexcept probe (nullptr if unregistered);
//     create() throws ElementNotFound. S088 maps that throw into the
//     line-carrying reader error, so ElementFactoryRegistry itself carries no
//     source-location concept (recorded NO-GAP, not silent).
//   - registered_names() returns INSERTION ORDER (deterministic — never
//     unordered_map iteration).
//
// A GLOBAL singleton (Meyers) mirrors PropertyRegistry::global()/
// DependencyProperty::global and is populated once at startup by
// register_builtin_elements() (Element.h). Population is documented
// single-threaded startup; the singleton is not a cross-thread mutation point.
//
// S146 (refactor gate) — the pimpl is COLLAPSED: the registry holds its
// vaml::detail::NameTable<ElementFactory> content-keyed store BY VALUE (no Impl
// heap node / impl_-> trampoline). ElementFactory (std::function) is the
// public API alias the register signature requires.

// S144 (dep:S143) — the registry's internal index is re-keyed OFF
// std::unordered_map + owned std::string keys and ONTO vaml::detail::NameTable
// (content-keyed S141-pattern probe store, self-owned stable keys, content
// match, no transient std::string on find) — mirroring PropertyRegistry /
// DependencyProperty::GlobalTable at S143. registered_names() returns
// string_views into the registry's OWNED stable name storage (no by-value
// vector<std::string>).
//
// S146 (refactor gate) — the pimpl is COLLAPSED. The registry now holds its
// vaml::detail::NameTable<ElementFactory> content-keyed store BY VALUE (the
// dead pre-retrofit surface purge: no Impl heap node, no impl_-> trampoline,
// no per-object heap hop). NameTable.h was promoted to the public detail tree
// (vaml/core/detail/NameTable.h) solely to make this collapse legal; it remains
// an internal implementation detail the public API does not promise.

#include "vaml/core/DependencyObject.h" // unique_ptr<DependencyObject> factory target
#include "vaml/core/detail/NameTable.h"  // S146: promoted to public detail tree so the pimpl can collapse

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// register_element() saw a type name that is already registered (dedupe).
class ElementAlreadyRegistered : public std::logic_error
{
public:
    explicit ElementAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// create() did not locate a factory for the requested type name.
class ElementNotFound : public std::out_of_range
{
public:
    explicit ElementNotFound(const char* what) : std::out_of_range(what) {}
};

// A factory produces a new DependencyObject for a registered element type name.
using ElementFactory = std::function<std::unique_ptr<DependencyObject>()>;

class VAML_CORE_EXPORT ElementFactoryRegistry
{
public:
    ElementFactoryRegistry();
    ~ElementFactoryRegistry();

    ElementFactoryRegistry(const ElementFactoryRegistry&) = delete;
    ElementFactoryRegistry& operator=(const ElementFactoryRegistry&) = delete;
    ElementFactoryRegistry(ElementFactoryRegistry&&) noexcept;
    ElementFactoryRegistry& operator=(ElementFactoryRegistry&&) noexcept;

    // Registers `typeName` -> `factory`. Type names are unique: registering an
    // already-present name throws ElementAlreadyRegistered and leaves the
    // registry unchanged (the factory is NOT replaced). The registry stores an
    // OWNED copy of `typeName`.
    void register_element(std::string_view typeName, ElementFactory factory);

    // Creates an instance for `typeName`, or nullptr if no factory is
    // registered for it. Never throws.
    [[nodiscard]] std::unique_ptr<DependencyObject> try_create(std::string_view typeName) const noexcept;

    // Creates an instance for `typeName`, or throws ElementNotFound if no
    // factory is registered for it.
    [[nodiscard]] std::unique_ptr<DependencyObject> create(std::string_view typeName) const;

    // True if a factory is registered under `typeName`. Never throws.
    [[nodiscard]] bool is_registered(std::string_view typeName) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // All registered type names in INSERTION ORDER (deterministic — not
    // unordered_map iteration). Returns string_views into the registry's OWNED
    // stable name storage (S144) — NO by-value vector<std::string> copy, no
    // owned-name gotcha. The views are valid for the registry's lifetime.
    [[nodiscard]] std::vector<std::string_view> registered_names() const;

    // The process-wide startup singleton populated once by
    // register_builtin_elements(). Thread-safe construction (Meyers function-
    // local static). Documented single-threaded startup population; S092/S286
    // consume it from the reader/compiled path.
    [[nodiscard]] static ElementFactoryRegistry& global() noexcept;

private:
    // S146: pimpl collapsed — the content-keyed store is now a BY-VALUE member.
    // NameTable is move-only (the registry is move-only; copy stays deleted).
    // insert/find/size/names delegate directly (no impl_-> trampoline, no heap
    // hop). /* A fresh registry has an empty table (no implicit elements). */
    vaml::detail::NameTable<ElementFactory> table_;
};

} // namespace vaml
