// S141 — SIMD probe store implementation (see InternStore.h for the contract).
#include "InternStore.h"

#include <cstring>
#include <limits>
#include <utility>

namespace vaml::detail {

namespace {

constexpr std::uint32_t kEnd = std::numeric_limits<std::uint32_t>::max();

constexpr std::size_t kInitialArenaBytes = 4096;
constexpr std::size_t kInitialEntryCapacity = 32;

} // namespace

InternStore::~InternStore()
{
    master_destroy_all();
}

std::uint8_t InternStore::fold_bytes(std::string_view sv) noexcept
{
    // Rotating-XOR over the bytes, % 255 -> 0..254 (never 255, so the 0xFF
    // empty-marker is unreachable as a probe index).
    std::uint8_t acc = 0;
    for (std::uint8_t b : sv)
    {
        acc = static_cast<std::uint8_t>((acc >> 1) | (acc << 7)); // rotate left 1
        acc ^= b;
    }
    if (acc == 0xFF)
    {
        // A defensive fold to keep the sentinel out of probe space. In
        // practice acc is already < 255 here; keep it provably so.
        acc = 0xFE;
    }
    return static_cast<std::uint8_t>(acc % 255);
}

void InternStore::master_destroy_all() noexcept
{
    // Free the arena blocks (retain-forever arena, freed only on explicit
    // teardown). Entry array and slot state reset.
    Block* b = arena_;
    while (b != nullptr)
    {
        Block* next = b->next;
        delete[] b->data;
        delete b;
        b = next;
    }
    arena_ = nullptr;

    delete[] entries_;
    entries_ = nullptr;
    entry_capacity_ = 0;
    count_ = 0;
    slots_init_ = false;
}

std::string_view InternStore::intern(std::string_view sv) noexcept
{
    // Lazily initialize the probe-slot table to "empty" (each head = kEnd).
    if (!slots_init_)
    {
        for (std::uint32_t& s : slots_)
        {
            s = kEnd;
        }
        slots_init_ = true;
    }

    const std::uint8_t probe = fold_bytes(sv);
    const std::size_t len = sv.size();
    const char* raw = sv.data();

    // Probe the append-only chain for fold byte `probe` (single-shot, no
    // re-probe / reorder). Content compare: equal length + equal bytes.
    for (std::uint32_t idx = slots_[probe]; idx != kEnd; idx = entries_[idx].next)
    {
        const Entry& e = entries_[idx];
        if (e.length == len && (len == 0 || std::memcmp(e.data, raw, len) == 0))
        {
            // HIT — return the existing permanent view (content-dedup).
            return std::string_view(e.data, e.length);
        }
    }

    // MISS — retain the bytes into the pool, insert, return the new view.
    if (arena_ == nullptr || arena_->used + len + 1 > arena_->cap)
    {
        std::size_t nextCap = (arena_ == nullptr) ? kInitialArenaBytes : arena_->cap * 2;
        while (nextCap < len + 1)
        {
            nextCap *= 2;
        }
        auto* blk = new Block{new char[nextCap], nextCap, 0, nullptr};
        blk->next = arena_;
        arena_ = blk;
    }
    char* dst = arena_->data + arena_->used;
    if (len != 0)
    {
        std::memcpy(dst, raw, len);
    }
    dst[len] = '\0'; // small convenience; not part of the view
    arena_->used += len + 1;

    // Grow the entry array (append; indices stay 0..count_-1 => stable).
    if (count_ == entry_capacity_)
    {
        std::size_t nextCap = (entry_capacity_ == 0) ? kInitialEntryCapacity
                                                     : entry_capacity_ + (entry_capacity_ / 2);
        while (nextCap < 32)
        {
            nextCap += 32;
        }
        nextCap = ((nextCap + 31) / 32) * 32; // round up to a 32-entry group
        auto* newEntries = new Entry[nextCap];
        if (count_ != 0)
        {
            std::memmove(newEntries, entries_, count_ * sizeof(Entry));
        }
        delete[] entries_;
        entries_ = newEntries;
        entry_capacity_ = nextCap;
    }

    const std::uint32_t newIndex = static_cast<std::uint32_t>(count_);
    entries_[newIndex] = Entry{dst, static_cast<std::uint32_t>(len), slots_[probe]};
    slots_[probe] = newIndex;
    ++count_;
    return std::string_view(dst, len);
}

} // namespace vaml::detail
