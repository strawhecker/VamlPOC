#pragma once
// S146 — internal heap-stats singleton backing the public
// vaml::heap_allocation_count()/heap_allocation_bytes() (see HeapStats.h).
//
// A relaxed-atomic counter funnel; incremented ONLY on VAML-owned monotonic
// growth paths (InternStore retain-arena/entry growth, ObjectPool block spill).
// Steady state (recycle + content-dedup hits) never touches it — so a flat
// counter across repeated identical passes proves zero steady-state heap churn.

#include <atomic>
#include <cstddef>

namespace vaml::detail {

class HeapStats
{
public:
    HeapStats() = default;
    HeapStats(const HeapStats&) = delete;
    HeapStats& operator=(const HeapStats&) = delete;

    // Records one heap-allocation event of `bytes` bytes (monotonic).
    void record(std::size_t bytes) noexcept
    {
        count_.fetch_add(1, std::memory_order_relaxed);
        bytes_.fetch_add(bytes, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t count() const noexcept
    {
        return count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t bytes() const noexcept
    {
        return bytes_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] static HeapStats& instance() noexcept
    {
        static HeapStats g;
        return g;
    }

private:
    std::atomic<std::size_t> count_{0};
    std::atomic<std::size_t> bytes_{0};
};

} // namespace vaml::detail
