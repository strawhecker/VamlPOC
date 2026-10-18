// S140_1 — vaml::intern() definition. The thread-local binding of the S141
// SIMD probe store (ADR-018: thread-local + lockless — zero locks).
#include "vaml/core/Intern.h"

#include "InternStore.h"

namespace vaml {

namespace {

// The per-thread intern store. Plain class (not itself thread_local) so the
// S141 InternStore tests may instantiate isolated stores; only this binding
// is thread_local. Retained for the life of the interning thread.
thread_local detail::InternStore g_internStore;

} // namespace

std::string_view intern(std::string_view sv) noexcept
{
    return g_internStore.intern(sv);
}

} // namespace vaml
