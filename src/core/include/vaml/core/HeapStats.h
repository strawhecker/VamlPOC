#pragma once
// S146 — PUBLIC heap-churn counter (feeds S628).
//
// A monotonic, process/thread-wide counter of heap-ALLOCATION EVENTS produced
// by the VAML allocation funnels, exposed so a test/benchmark can assert that
// steady-state operation performs ZERO heap churn.
//
// CONTRACT (stated, not hidden):
//   - Counts ONLY the VAML-owned allocation funnels — the S141 InternStore
//     retain-arena + entry growth and the S145 object-pool block spill —
//     NOT every ::operator new in the process. These are the ONLY heap-churn
//     sources in steady state now (the block pools relax + recycle in steady
//     state; InternStore dedups by content). Global new/delete overrides were
//     deliberately NOT used: they would also trap third-party (doctest) and
//     process noise, and they would collide with the host allocator.
//   - Nondeterminism note: because the counter is incremented only on the
//     MONOTONIC growth paths (never on recycle/dedup-hit), a correct steady
//     state is a FLAT counter — snapshot after a warmup pass, then assert
//     it does not grow across further identical passes.
//   - Thread-safety: relaxed atomics — safe to read/write from any thread. The
//     funnels themselves are thread-local (ADR-018), so contention ~none.

#include "vaml/core/export.h"

#include <cstddef>

namespace vaml {

// Monotonic count of VAML heap-allocation events since process start.
[[nodiscard]] VAML_CORE_EXPORT std::size_t heap_allocation_count() noexcept;

// Monotonic total bytes allocated by the VAML funnels since process start.
[[nodiscard]] VAML_CORE_EXPORT std::size_t heap_allocation_bytes() noexcept;

} // namespace vaml
