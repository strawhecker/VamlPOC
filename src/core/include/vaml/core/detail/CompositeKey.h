#pragma once
// S801 — vaml::detail::CompositeKey: the `(name, owner)` composite content key
// for the S143 content-keyed probe store (NameTable). This is the WPF ground-
// truth equivalence of DependencyProperty.cs `FromNameKey(name, ownerType)`:
// equality is STRICT PAIR equality (name AND owner), never name-only. A
// same-name/different-owner pair is a DISTINCT key (coexistence), and a
// same-pair re-insert is a conflict (the registry throws PropertyAlreadyRegistered).
//
// DESIGN:
//   - SELF-OWNED key storage, exactly like NameTable's string keys: the
//     composite owns TWO stable std::string members (name + owner). This is the
//     S140 DECISION 1 resolution for a GLOBAL / cross-thread table — it cannot
//     hold thread-local intern views, so it stays content-keyed with self-owned
//     stable storage. ADR-018: no lock added.
//   - fold() rotates-XOR folds name + owner bytes (S141 pattern, 0xFF sentinel
//     unreachable) so a probe slot is a function of the WHOLE pair — same-name/
//     different-owner pairs probe independently and never collide into the same
//     slot unless they are content-equal.
//   - find(K) matches by CONTENT (full byte compare of BOTH members), never by
//     pointer — a lookup key derives its name/owner views without owning them.
//
// COMPOSITE KEY INVARIANT (the S801 contract): a CompositeKey must NEVER be
// empty-owner in a live path — the store's lookup/insert always carries a real
// (name, owner) pair. "name == \"\"" denotes "no key" and is the only empty form
// (mirrors the empty-name sentinel in PropertyId / the store's absent entries).
//
// This is real, permanent surface — NOT MINIMAL REAL, NOT a STUB. It is the
// store-level identity that S801 re-keys the registry + GlobalTable onto, and
// it is kept as the forward-resolvable seed for S802 (per-object store re-key)
// and S803 (reader dotted-key composite authoring form). Resolves as the
// composite identity across S801–S803. Do NOT revert to name-only at any step
// NOR backfill an owner-less path.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace vaml::detail {

class CompositeKey
{
public:
    CompositeKey() = default;

    // Construct an owned composite (name, owner) content key. Both strings are
    // COPIED into self-owned stable storage (the S140 DECISION 1 content-keyed
    // contract — the table owns its keys, foreign thread-local views never
    // dangle here).
    CompositeKey(std::string name, std::string owner)
        : name_(std::move(name)), owner_(std::move(owner))
    {
    }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }

    // STRICT PAIR equality: equal IFF name AND owner are equal. Same-name/
    // different-owner pairs are DISTINCT — coexistence is the S801 contract.
    [[nodiscard]] bool operator==(const CompositeKey& other) const noexcept
    {
        return name_ == other.name_ && owner_ == other.owner_;
    }
    [[nodiscard]] bool operator!=(const CompositeKey& other) const noexcept
    {
        return !(*this == other);
    }

    // Equality against a derived (view) pair — lets a lookup key that does not
    // own its strings compare by content to an owned composite without
    // materializing a transient owned string (the store's inner node compare).
    [[nodiscard]] bool equals(std::string_view name, std::string_view owner) const noexcept
    {
        return name_ == name && owner_ == owner;
    }

    // S141-pattern content fold over name + owner bytes -> 0..254 (0xFF sentinel
    // unreachable). A probe slot is a function of the WHOLE pair.
    [[nodiscard]] std::uint8_t fold() const noexcept
    {
        return fold_of(name_, owner_);
    }

    // Static content fold of an arbitrary (name, owner) pair — used by the
    // store's lookup path to derive a probe slot WITHOUT materializing an
    // owned composite (the lookup key carries views, not owned strings).
    [[nodiscard]] static std::uint8_t fold_of(std::string_view name, std::string_view owner) noexcept
    {
        std::uint8_t acc = fold_bytes(name);
        acc = static_cast<std::uint8_t>((acc >> 1) | (acc << 7));
        acc ^= fold_bytes(owner);
        if (acc == 0xFF)
        {
            acc = 0xFE;
        }
        return static_cast<std::uint8_t>(acc % 255);
    }

private:
    static std::uint8_t fold_bytes(std::string_view sv) noexcept
    {
        std::uint8_t acc = 0;
        for (std::uint8_t b : sv)
        {
            acc = static_cast<std::uint8_t>((acc >> 1) | (acc << 7)); // rotate left 1
            acc ^= b;
        }
        return acc;
    }

    std::string name_;
    std::string owner_;
};

} // namespace vaml::detail
