#pragma once
// S140_1 — PUBLIC retained-string primitive: vaml::intern(string_view).
//
// The public face of the S141 SIMD probe store (thread-local, lockless —
// ADR-018). Usable by internal hot paths AND external programmers.
//
// CONTRACT (stated, not hidden):
//   - SINGLE by-value call. `intern(sv)` probes the CALLING THREAD's store BY
//     VALUE; on HIT returns the existing view (content-dedup); on MISS retains
//     the bytes into the thread-local pool, inserts, and returns the new view.
//   - Equality is by CONTENT (SIMD full compare), NEVER pointer-identity.
//     A caller's address is never a usable pre-check; cross-thread interned
//     values are compared by content only. There is intentionally NO
//     by-reference/pointer variant and NO reverse-index auto-conversion.
//   - LIFETIME (thread-scoped): the caller is NOT required to outlive us —
//     anything that flows through intern() is retained for the LIFE OF THE
//     INTERNING THREAD (once in, always there, never removed, never
//     relocated). Do NOT pass an interned view ACROSS threads — carry
//     literals/owned strings across threads instead.
//   - WARNING: persistent retention is unbounded and reclaimed-never per
//     thread. Do NOT call intern() on unbounded or unpredictable-valued data
//     — only bounded vocabulary + parsed document text.

#include "vaml/core/export.h"

#include <string_view>

namespace vaml {

// Returns a process-stable (per the interning thread) string_view for sv.
// Accepts std::string via implicit string_view conversion.
[[nodiscard]] VAML_CORE_EXPORT std::string_view intern(std::string_view sv) noexcept;

// S146 (ASan-gate enabling hook): frees EVERY thread-local intern store's
// retain-forever arena + entry array, resetting the interning state for this
// thread (a subsequent intern() lazily rebuilds the store). Retained views
// already handed to callers are INVALIDATED — this is an explicit TEARDOWN
// hook, only for tests / ASan leak-checking at process exit, mirroring
// object_pool_master_destroy_all(). It exists so the S141 thread-local pool
// (which otherwise deliberately never frees) can be released cleanly and the
// ASan lane reports ZERO leaks. Do NOT call it in normal application flow.
VAML_CORE_EXPORT void intern_master_destroy_all() noexcept;

} // namespace vaml
