#pragma once
// VAML PropertyKey flags — the declarative bit-set carried by a dependency
// property key. S032 (dep:S031). Foundation for the DP property system.
//
// The three bits here are exactly the roadmap's S032/S034 flag set
// (inherits | readonly | bindsTwoWayByDefault). They are pure *declaration*
// flags with no runtime behavior. Anything with behavior (default value,
// CoerceValue, ValidateValue callbacks) is metadata delivered by S034, not a
// flag — so no such bits exist here, by design.
//
// Flags combine under OR (`operator|`) and are tested with `has_flag`, which
// treats bits as independent (each maps to exactly one semantic). There is no
// "at most one of" constraint among these bits.

#include "vaml/core/export.h"

#include <cstdint>

namespace vaml {

enum class PropertyFlags : std::uint32_t
{
    None                   = 0,
    Inherits               = 1 << 0, // participates in the inheritance cascade (S039/S040)
    ReadOnly               = 1 << 1, // Set() throws (S045)
    BindsTwoWayByDefault   = 1 << 2, // default binding mode is two-way (S242-S244)
    InheritOnly            = 1 << 3, // S039: value comes ONLY from the ancestor chain;
                                     // the object's own local override does NOT contribute.
                                     // Meaningful only when Inherits is ALSO set (S039).
};

// Bitwise OR — combines independent flags into a single PropertyFlags value.
VAML_CORE_EXPORT PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs) noexcept;

// True iff every bit in `flag` is set in `value`. Independent-bit semantics:
// the result does not require any *other* bit that may also be set in `value`
// to be absent.
VAML_CORE_EXPORT bool has_flag(PropertyFlags value, PropertyFlags flag) noexcept;

} // namespace vaml
