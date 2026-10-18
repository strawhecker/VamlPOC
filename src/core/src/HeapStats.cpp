// S146 — public heap-churn counter definitions (see HeapStats.h + HeapStats.h
// internal). Backed by the internal vaml::detail::HeapStats singleton.
#include "vaml/core/HeapStats.h"

#include "HeapStats.h"

namespace vaml {

std::size_t heap_allocation_count() noexcept
{
    return vaml::detail::HeapStats::instance().count();
}

std::size_t heap_allocation_bytes() noexcept
{
    return vaml::detail::HeapStats::instance().bytes();
}

} // namespace vaml
