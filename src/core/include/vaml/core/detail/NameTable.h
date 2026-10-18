#pragma once
// S143 — vaml::detail::NameTable<T>: a content-keyed associative store that
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
//
// WHY SELF-OWNED keys and NOT thread-local intern views (S140 DECISION 1, whose
// body is S143): these three stores are either GLOBAL (PropertyRegistry::global,
// DependencyProperty::GlobalTable) or cross-thread. They CANNOT hold a
// thread-local intern view — a foreign thread interning into the owner's pool
// would dangle. So the table OWNS a copy of the key and matches BY CONTENT
// (byte compare), never by pointer. This is exactly the S141-shape: a probe
// slot = rotating-XOR fold % 255 (0xFF sentinel unreachable), a chain walk
// disambiguated by full string_view content compare, append-only nodes.
//
// CONTRACT:
//   - insert(name, value): appends an entry keyed on an OWNED copy of `name`.
//     Returns a pointer to the STABLE stored value, or nullptr when the name is
//     already present (the existing entry is left unchanged — callers use this
//     for dedupe detection, e.g. PropertyAlreadyRegistered).
//   - find(name): content lookup by std::string_view with NO transient
//     std::string materialized. Returns nullptr if absent.
//   - find_key(name): returns the OWNED canonical key string (or nullptr).
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
#include <utility>
#include <vector>

namespace vaml::detail {

template <class T>
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

    // Appends `name` -> `value`. Returns a pointer to the STABLE stored value,
    // or nullptr if `name` is already present (existing entry unchanged).
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

    T* find(std::string_view name) noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->name == name)
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    const T* find(std::string_view name) const noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->name == name)
            {
                return &rows_[idx]->value;
            }
        }
        return nullptr;
    }

    const std::string* find_key(std::string_view name) const noexcept
    {
        for (std::uint32_t idx = slots_[fold(name)]; idx != kEnd; idx = rows_[idx]->next)
        {
            if (rows_[idx]->name == name)
            {
                return &rows_[idx]->name;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }
    [[nodiscard]] bool empty() const noexcept { return rows_.empty(); }

    // S144: insertion-order name listing. rows_ is append-only, so its order
    // exactly mirrors insert() order and never duplicates a name. Returns
    // string_views into the OWNED stable key storage (valid for the table's
    // lifetime) — NO owned-name copy (S144 accessor migration).
    [[nodiscard]] std::vector<std::string_view> names() const
    {
        std::vector<std::string_view> out;
        out.reserve(rows_.size());
        for (const auto& node : rows_)
        {
            out.push_back(node->name);
        }
        return out;
    }

private:
    struct Node
    {
        std::string   name;
        T             value;
        std::uint32_t next;

        Node(std::string n, T v, std::uint32_t nx)
            : name(std::move(n)), value(std::move(v)), next(nx) {}
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

    std::uint32_t slots_[255];
    std::vector<std::unique_ptr<Node>> rows_;
};

} // namespace vaml::detail
