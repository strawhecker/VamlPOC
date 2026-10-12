// S032 — PropertyFlags free functions: operator| and has_flag (defined
// out-of-line so the exported symbols resolve from a single TU).

#include "vaml/core/PropertyFlags.h"

namespace vaml {

VAML_CORE_EXPORT
PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs) noexcept
{
    using U = std::uint32_t;
    return static_cast<PropertyFlags>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

VAML_CORE_EXPORT
bool has_flag(PropertyFlags value, PropertyFlags flag) noexcept
{
    using U = std::uint32_t;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace vaml
