#pragma once
// VAML PropertyId — a constexpr-safe, collision-free static id for a dependency
// property. S033 (dep:S032) established the name-token; S800 supersedes the
// bare-name form with the COMPOSITE `(name_view, owner_view)` name-token.
// The compile-time counterpart to the registry: S036's local-value store keys
// on this id.
//
// S800 (foundation, dep:S814 — pure type change, NO live semantic change yet).
// The id is now a pair:
//   - name :  the property name, e.g. "Orientation";
//   - owner : the fully-qualified DECLARING-TYPE name, e.g. "vaml::StackPanel"
//             (or "::Foo" for a global-namespace app type). This is VAML's
//             faithful equivalent of WPF's owner `Type` (DependencyProperty.cs
//             `FromNameKey(name, ownerType)`) expressed as a C++ qualified
//             type-name string.
//
// Pair-keyed identity: two ids are equal IFF both name AND owner are equal;
// std::hash is pair-keyed so equal ids hash equal. A same-name/different-owner
// pair is a DISTINCT id (the disambiguation WPF's `(name, Type)` key provides).
//
//   - constexpr-safe: `constexpr PropertyId id = PropertyId::of("X","::Foo");`
//     is fully evaluated at compile time (proven by static_assert in tests);
//   - collision-free by construction: it carries the name+owner views, not a
//     hash, so there is no collision class, no cross-TU nondeterminism, and no
//     static-init-order fiasco;
//   - stable and interconvertible with the registry's store
//     (PropertyRegistry::id_of / try_id).
//
// NAMESPACE POLICY (addresses the C++ vs C# divergence; ratified at the S800
// sprint head):
//   * C++ has a genuine global namespace; C# does not. VAML MUST host apps
//     that live outside any namespace, so the global-namespace owner form
//     `::Foo` is a real, valid, resolvable ownership scope. `::StackPanel` is
//     a FOREIGN/global type name, distinct from VAML's `vaml::StackPanel`, and
//     does not resolve to VAML.
//   * VAML DELIBERATELY DOES NOT SUPPORT owner types declared in an ANONYMOUS
//     C++ namespace (`namespace { ... }`): an anonymous namespace has no name,
//     so no token string can name it, and same-name DPs from two TUs' anonymous
//     namespaces would be unresolvable. This is a documented boundary, NOT a
//     gap. (C# has no anonymous namespace either — full parity.)
//
// LEGACY BRIDGE (this step ONLY, deprecated, contractually-eliminated at
// S801–S803): `PropertyId::of(name)` constructs an id with owner `""`. Because
// the registry's store is still name-keyed this step (the re-key is S801), all
// legacy ids carry owner `""`, so strict pair equality reduces to today's
// name-only behavior among them — NO live semantic change. The empty-owner form
// is NOT name-only semantics in the contract (the type has no owner-less
// variant and no equality ignores the owner field); it is a documented compile
// artifact scheduled for removal. S801 removes the empty-owner path when no id
// is owner-less.
//
// NOT a STUB / NOT MINIMAL REAL — the composite token is real, permanent
// surface, kept and extended (S801+ re-keys the store onto this pair).
//
// Lifetime note: name and owner MUST be string literals (or other storage that
// outlives the id). Wrapping a temporary std::string is a lifetime bug. This
// mirrors the registry contract, where keys are named by permanent literals.

#include <cstddef>
#include <functional>
#include <string_view>

namespace vaml {

class PropertyId
{
public:
    // Default id: empty name + empty owner (no property). "not found" token.
    constexpr PropertyId() noexcept = default;

    // [S800 LEGACY BRIDGE — deprecated, eliminated at S801–S803] Adopt an
    // existing name with an EMPTY owner. `name` must refer to storage that
    // outlives the id (a string literal is the canonical source). Prefer
    // of(name, owner) so every id carries its declaring type.
    constexpr explicit PropertyId(std::string_view name) noexcept : name_(name) {}

    // S800 COMPOSITE: adopt a property name + its fully-qualified declaring
    // type-name. Both must outlive the id (string literals are canonical).
    constexpr PropertyId(std::string_view name, std::string_view owner) noexcept
        : name_(name), owner_(owner)
    {
    }

    // [S800 LEGACY BRIDGE — deprecated, eliminated at S801–S803] Bind a bare
    // property name to an id with empty owner. Kept ONLY so existing runtime
    // consumers compile + behave unchanged this step. See LEGACY BRIDGE note.
    [[nodiscard]] static constexpr PropertyId of(std::string_view name) noexcept
    {
        return PropertyId(name);
    }

    // S800 COMPOSITE FACTORY: bind a property name + fully-qualified declaring
    // type-name to its id. Because both args are std::string_view and only the
    // views are copied, `constexpr` works and the result is a compile-time
    // constant when the arguments are constant expressions.
    [[nodiscard]] static constexpr PropertyId of(std::string_view name,
                                                 std::string_view owner) noexcept
    {
        return PropertyId(name, owner);
    }

    // The property name this id stands for.
    [[nodiscard]] constexpr std::string_view name() const noexcept { return name_; }

    // The fully-qualified declaring-type name this id is owned by
    // (e.g. "vaml::StackPanel" or "::Foo"). Empty ONLY on a legacy-bridge or
    // default-constructed id (removed as a live path at S801).
    [[nodiscard]] constexpr std::string_view owner() const noexcept { return owner_; }

    // An id is "valid" when it names a real property (non-empty name).
    [[nodiscard]] constexpr bool valid() const noexcept { return !name_.empty(); }

    // S800 STRICT PAIR equality: equal IFF name AND owner are equal.
    // Same-name/different-owner pairs are DISTINCT ids. Among legacy ids
    // (owner ""), this reduces to today's name-only behavior — no semantic
    // change this step.
    constexpr bool operator==(const PropertyId& other) const noexcept
    {
        return name_ == other.name_ && owner_ == other.owner_;
    }
    constexpr bool operator!=(const PropertyId& other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::string_view name_;
    std::string_view owner_;
};

} // namespace vaml

// std::hash specialization — lets PropertyId be used as a map/set key (S036's
// local-value store keys on it). Two ids that compare equal hash equal.
// S800: PAIR-KEYED — hashes both name and owner.
namespace std {
template <>
struct hash<vaml::PropertyId>
{
    std::size_t operator()(const vaml::PropertyId& id) const noexcept
    {
        // Pair-keyed, per strict pair equality: equal ids (same name AND same
        // owner) hash equal; same-name/different-owner ids hash differently.
        std::size_t h = std::hash<std::string_view>{}(id.name());
        // Combine without introducing a collision class beyond the views'
        // (name, owner) content (the honest constexpr-safe pair token).
        h ^= std::hash<std::string_view>{}(id.owner()) + 0x9e3779b9U + (h << 6U) + (h >> 2U);
        return h;
    }
};
} // namespace std
