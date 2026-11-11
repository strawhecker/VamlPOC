#pragma once
// S143 — vaml::detail::NameTable<T,K>: a content-keyed associative store that
// re-keys the ChangeRecorder / PropertyRegistry / DependencyProperty::GlobalTable
// internal indexes OFF std::unordered_map / owned-std::string keys and onto the
// S141 SIMD-probe pattern (ADR-018 spirit: no hash, equality by CONTENT, never
// pointer identity).
//
// S146 (refactor gate) — this header is PROMOTED from src/core/src/NameTable.h
// into the PUBLIC include tree (vaml/core/detail/NameTable.h) so the retained
// public registries can COLLAPSE their pimpl and hold a NameTable member
// by-value (the pimpl is gone — dead pre-retrofit surface). It stays in the
// vaml::detail namespace to signal it is an internal implementation detail that
// the public API does not promise (included-only for the registries' storage).
//
// S801 — the table is now TEMPLATED on its key type `K` (default `std::string`),
// so the SAME content-keyed probe store serves both:
//   - the single-string-keyed stores, K = std::string (default): ChangeRecorder,
//     ChangeQueue, and any string-keyed consumer — UNCHANGED, byte-identical
//     (the roadmap's "NOT blanket-rewritten" promise, honored via the default
//     argument, not a fork); and
//   - the composite-keyed stores, K = vaml::detail::CompositeKey: PropertyRegistry
//     + DependencyProperty::GlobalTable. For K = CompositeKey the insert/find
//     overloads take `(name, owner)` and match by STRICT PAIR equality (the
//     S801 coexistence contract), keyed by the PAIR's content fold.
// The arity of the keyed methods (1 arg for K=std::string, 2 args for K=
// CompositeKey) disambiguates cleanly; the composite overloads are guarded by
// `std::is_same_v<K, CompositeKey>` so a string-keyed table never exposes them.
//
//
// WHY SELF-OWNED keys and NOT thread-local intern views (S140 DECISION 1, whose
// body is S143): these three stores are either GLOBAL (PropertyRegistry::global,
// DependencyProperty::GlobalTable) or cross-thread. They CANNOT hold a
// thread-local intern view — a foreign thread interning into the owner's pool
// would dangle. So the table OWNS a copy of the key (for K=CompositeKey it owns
// BOTH the name and owner strings) and matches BY CONTENT (byte compare), never
// by pointer. This is exactly the S141-shape: a probe slot = rotating-XOR fold
// % 255 (0xFF sentinel unreachable), a chain walk disambiguated by full content
// compare, append-only nodes.
//
// CONTRACT:
//   - insert(key): appends an entry keyed on an OWNED copy of `key`. Returns a
//     pointer to the STABLE stored value, or nullptr when the key is already
//     present (the existing entry is left unchanged — callers use this for
//     dedupe detection, e.g. PropertyAlreadyRegistered).
//       * K=std::string : insert(std::string name, T value)
//       * K=CompositeKey : insert(std::string name, std::string owner, T value)
//         (the pair key is constructed from the two args; BOTH are copied into
//         owned storage).
//   - find(key): content lookup with NO transient owned key materialized.
//     Returns nullptr if absent.
//       * K=std::string : find(std::string_view name)
//       * K=CompositeKey : find(std::string_view name, std::string_view owner)
//   - find_key(key): returns the OWNED canonical key (or nullptr).
//   - size()/empty(): entry count.
//   - The stored value's ADDRESS is stable for the table's lifetime (append-only
//     heap nodes; relocating the node-ownership vector never moves the value) —
//     so a returned `const PropertyKey&` / `const DependencyProperty&` survives.
//   - swap(other): exchanges rows + probe slots (used by ChangeRecorder's
//     snapshot-then-emit flush semantics).
//
// NOT thread-safe — the single-threaded registration / object-affinity contract
// it replaces is UNCHANGED. No lock is added (ADR-018: zero new locks). The
// sole relocated lock remains the S037/S041 ChangeQueue mutex.

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "vaml/core/detail/CompositeKey.h"

namespace vaml::detail {

template <class T, class K = std::string>
class NameTable
{
public:
    NameTable()
    {
        for (std::uint32_t& s : slots_)
        {
            s = kEnd;
        }
    }
    ~NameTable() = default;
    NameTable(const NameTable&) = delete;
    NameTable& operator=(const NameTable&) = delete;
    NameTable(NameTable&&) noexcept = default;
    NameTable& operator=(NameTable&&) noexcept = default;

    void swap(NameTable& other) noexcept
    {
        rows_.swap(other.rows_);
        for (std::uint32_t& s : slots_)
        {
            const std::size_t i = static_cast<std::size_t>(&s - slots_);
            std::swap(s, other.slots_[i]);
        }
    }

    // --- single-string-keyed forms (K = std::string) --------------------
    // Appends `name` -> `value`. Returns a pointer to the STABLE stored value,
    // or nullptr if `name` is already present (existing entry unchanged).
    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, std::string>, int> = 0>
    T* insert(std::string name, T value)
    {
        if (find(name) != nullptr)
        {
            return nullptr; // duplicate by content; caller dedupes
        }

        const std::uint8_t probe = fold(name);
        auto owner = std::make_unique<Node>(std::move(name), std::move(value), slots_[probe]);
        T* stored = &owner->value;
        slots_[probe] = static_cast<std::uint32_t>(rows_.size());
        rows_.push_back(std::move(owner));
        return stored;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, std::string>, int> = 0>
    T* find(std::string_view name) noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key == name)
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, std::string>, int> = 0>
    const T* find(std::string_view name) const noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key == name)
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, std::string>, int> = 0>
    const std::string* find_key(std::string_view name) const noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key == name)
            {
                return &rows_[idx]->key;
            }
        }
        return nullptr;
    }

    // --- composite-keyed forms (K = CompositeKey) ----------------------
    // Appends (name, owner) -> value. Returns a pointer to the STABLE stored
    // value, or nullptr if the (name, owner) PAIR is already present (existing
    // entry unchanged). Same-name/different-owner inserts as DISTINCT (S801
    // coexistence); same-pair re-insert is the caller's dedupe signal.
    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    T* insert(std::string name, std::string owner, T value)
    {
        if (find(name, owner) != nullptr)
        {
            return nullptr; // same (name, owner) pair already present; caller dedupes
        }

        CompositeKey pair(std::move(name), std::move(owner));
        const std::uint8_t probe = pair.fold();
        auto node = std::make_unique<Node>(std::move(pair), std::move(value), slots_[probe]);
        T* stored = &node->value;
        slots_[probe] = static_cast<std::uint32_t>(rows_.size());
        rows_.push_back(std::move(node));
        return stored;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    T* find(std::string_view name, std::string_view owner) noexcept
    {
        const std::uint8_t probe = CompositeKey::fold_of(name, owner);
        for (std::uint32_t idx = slots_[probe]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key.equals(name, owner))
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    const T* find(std::string_view name, std::string_view owner) const noexcept
    {
        const std::uint8_t probe = CompositeKey::fold_of(name, owner);
        for (std::uint32_t idx = slots_[probe]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key.equals(name, owner))
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    const CompositeKey* find_key(std::string_view name, std::string_view owner) const noexcept
    {
        const std::uint8_t probe = CompositeKey::fold_of(name, owner);
        for (std::uint32_t idx = slots_[probe]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->key.equals(name, owner))
            {
                return &rows_[idx]->key;
            }
        }
        return nullptr;
    }

    // --- S801 unique-name bridge (composite only) ----------------------
    // A name-only lookup used ONLY where the caller cannot derive an owner
    // from a bare name AND WPF canonically has no explicit owner in the key
    // (the reader's non-dotted attributes). Returns a pointer to the stored
    // value whose key name == `name` when EXACTLY ONE distinct key has that
    // name; sets *matches to the count of distinct (owner) keys holding the
    // name. Returns nullptr when zero OR more-than-one keys share the name
    // (ambiguity — the caller must not silently pick one; NO owner-less
    // semantics survive). This is the S803-resolved bridge (MINIMAL REAL):
    // element-type->owner inference at S803 retires it. It is NOT a name-only
    // keying — the store remains composite-keyed; this only READS by the name
    // member.
    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    T* find_unique_name(std::string_view name, std::size_t* matches) noexcept
    {
        T* found = nullptr;
        *matches = 0;
        for (const auto& node : rows_)
        {
            if (node->key.name() == name)
            {
                ++*matches;
                found = &node->value;
            }
        }
        return (*matches == 1) ? found : nullptr;
    }

    template <class KT = K,
              std::enable_if_t<std::is_same_v<KT, CompositeKey>, int> = 0>
    const T* find_unique_name(std::string_view name, std::size_t* matches) const noexcept
    {
        const T* found = nullptr;
        *matches = 0;
        for (const auto& node : rows_)
        {
            if (node->key.name() == name)
            {
                ++*matches;
                found = &node->value;
            }
        }
        return (*matches == 1) ? found : nullptr;
    }

    // --- shared ---------------------------------------------------------
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }
    [[nodiscard]] bool empty() const noexcept { return rows_.empty(); }

    // S144: insertion-order key listing. rows_ is append-only, so its order
    // exactly mirrors insert() order and never duplicates a key. Returns
    // string_views into the OWNED stable key storage (valid for the table's
    // lifetime) — NO owned-name copy (S144 accessor migration). For K=
    // CompositeKey, the "name" is the pair's name member; owner is not exposed
    // here (the registry/GlobalTable do not enumerate owners — the store-level
    // names listing is shared by the string-keyed path).
    [[nodiscard]] std::vector<std::string_view> names() const
    {
        std::vector<std::string_view> out;
        out.reserve(rows_.size());
        for (const auto& node : rows_)
        {
            out.push_back(key_name(node->key));
        }
        return out;
    }

private:
    struct Node
    {
        K             key;
        T             value;
        std::uint32_t next;

        Node(K k, T v, std::uint32_t nx)
            : key(std::move(k)), value(std::move(v)), next(nx) {}
    };

    static constexpr std::uint32_t kEnd = 0xFFFFFFFFu;

    // Rotating-XOR fold % 255 -> 0..254 (never 255 == 0xFF, so the empty
    // marker is unreachable as a probe index) — mirrors InternStore (S141).
    static std::uint8_t fold(std::string_view sv) noexcept
    {
        std::uint8_t acc = 0;
        for (std::uint8_t b : sv)
        {
            acc = static_cast<std::uint8_t>((acc >> 1) | (acc << 7)); // rotate left 1
            acc ^= b;
        }
        if (acc == 0xFF)
        {
            acc = 0xFE;
        }
        return static_cast<std::uint8_t>(acc % 255);
    }

    static std::string_view key_name(const std::string& s) noexcept { return s; }
    static std::string_view key_name(const CompositeKey& k) noexcept { return k.name(); }

    std::uint32_t slots_[255];
    std::vector<std::unique_ptr<Node>> rows_;
};

} // namespace vaml::detail
