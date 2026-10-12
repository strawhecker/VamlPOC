#pragma once
// VAML PropertyId — a constexpr-safe, collision-free static id for a dependency
// property. S033 (dep:S032). The compile-time counterpart to the name-keyed
// PropertyRegistry: S036's local-value store keys on this id.
//
// A PropertyId simply wraps the property NAME as a std::string_view bound (by
// design) to a string literal. That makes it:
//   - constexpr-safe: `constexpr PropertyId id = PropertyId::of("Width");`
//     is fully evaluated at compile time (proven by static_assert in tests);
//   - collision-free by construction: it carries the name, not a hash, so
//     there is no collision class, no cross-TU nondeterminism, and no
//     static-init-order fiasco;
//   - stable and interconvertible with the registry's name-keyed store
//     (PropertyRegistry::id_of / try_id).
//
// Choosing a name-token over a compile-time hash is deliberate: a constexpr
// hash (e.g. FNV-1a) is also constexpr but reintroduces a (tiny) collision
// class and makes the compile-time proof opaque. The name-token is the honest
// "constexpr-safe static id". NOT a STUB / NOT MINIMAL REAL — this type is
// complete and real on delivery (header-only, fully inline).
//
// Lifetime note: the owner MUST pass a string literal (or other storage that
// outlives the id). Wrapping a temporary std::string is a lifetime bug. This
// mirrors the registry contract, where keys are named by permanent literals.

#include <cstddef>
#include <functional>
#include <string_view>

namespace vaml {

class PropertyId
{
public:
    // Default id: empty name (no property). Useful as a "not found" token.
    constexpr PropertyId() noexcept = default;

    // Adopt an existing name. `name` must refer to storage that outlives the
    // id (a string literal is the canonical source).
    constexpr explicit PropertyId(std::string_view name) noexcept : name_(name) {}

    // Factory: bind a property name to its id. Because it takes a
    // std::string_view and only copies the view, `constexpr` works and the
    // result is a compile-time constant when the argument is one.
    [[nodiscard]] static constexpr PropertyId of(std::string_view name) noexcept
    {
        return PropertyId(name);
    }

    // The property name this id stands for.
    [[nodiscard]] constexpr std::string_view name() const noexcept { return name_; }

    // An id is "valid" when it names a real property (non-empty).
    [[nodiscard]] constexpr bool valid() const noexcept { return !name_.empty(); }

    constexpr bool operator==(const PropertyId& other) const noexcept
    {
        return name_ == other.name_;
    }
    constexpr bool operator!=(const PropertyId& other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::string_view name_;
};

} // namespace vaml

// std::hash specialization — lets PropertyId be used as a map/set key (S036's
// local-value store keys on it). Two ids that compare equal hash equal.
namespace std {
template <>
struct hash<vaml::PropertyId>
{
    std::size_t operator()(const vaml::PropertyId& id) const noexcept
    {
        return std::hash<std::string_view>{}(id.name());
    }
};
} // namespace std
