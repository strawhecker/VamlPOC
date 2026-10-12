#pragma once
// VAML PropertyKey — immutable identifier for a registered dependency
// property. S032 (dep:S031). Foundation for the DP property system.
//
// A PropertyKey is the canonical (name, type, owner-type id, flags) tuple from
// the roadmap. It is immutable once constructed; the registry (S032's
// PropertyRegistry, S033's full register/lookup suite) owns the mapping from
// property NAME to a key and enforces name uniqueness.
//
// Design choices (recorded for forward steps):
//   - owner-type id is a std::string type-name (e.g. "Button"). There is no
//     cheap portable runtime type_of in C++ that survives library boundaries;
//     a stable name is debuggable and satisfies S043 (attached properties,
//     where owner != the class the value is stored on). S033's constexpr-safe
//     static id layers on top of this without changing the fields.
//   - Keys are distinct (operator==) on all four fields, but the *registry*
//     uniques on NAME alone (WPF-style global uniqueness); ownerTypeId stays as
//     identity metadata inside the key, not a second registry dimension. This
//     gives S033 a crisp duplicate->throw contract.

#include "vaml/core/PropertyFlags.h"
#include "vaml/core/Value.h" // ValueType (property value type)

#include <string>

namespace vaml {

class VAML_CORE_EXPORT PropertyKey
{
public:
    PropertyKey(std::string name, ValueType type, std::string ownerTypeId, PropertyFlags flags);

    // --- Getters (immutable API) ---------------------------------------
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ValueType type() const noexcept { return type_; }
    [[nodiscard]] const std::string& ownerTypeId() const noexcept { return ownerTypeId_; }
    [[nodiscard]] PropertyFlags flags() const noexcept { return flags_; }

    // --- Equality (all four fields) -------------------------------------
    [[nodiscard]] bool operator==(const PropertyKey& other) const noexcept;
    [[nodiscard]] bool operator!=(const PropertyKey& other) const noexcept
    {
        return !(*this == other);
    }

    // Human-readable "Name (Type, owner 'Owner') [+flags]" line for error
    // messages and diagnostics. Defined out-of-line in PropertyKey.cpp.
    [[nodiscard]] std::string describe() const;

private:
    std::string      name_;
    ValueType        type_;
    std::string      ownerTypeId_; // owner-type identity (declaring class) — S043 uses it
    PropertyFlags    flags_;
};

} // namespace vaml
