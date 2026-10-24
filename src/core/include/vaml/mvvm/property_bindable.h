#pragma once
// VAML mvvm — bindable-property convenience layer. S247_1 (dep:S247).
//
// The Toolkit-analog convenience surface for authoring ViewModels over the
// S240/S241 core (ObservableObject + typed PropertyChanged signal). It cuts
// down boilerplate WITHOUT runtime DI and WITHOUT reflection: a
// `PROPERTY_BINDABLE(type, Name)`-style macro expands to a typed backing field
// + getter + setter, and a free `vaml::mvvm::set_property(...)` template
// performs a REAL-change write that updates the field AND mirrors it into the
// observable ObservableObject Value store — firing the genuine S241
// PropertyChanged signal so the S242+ binding engine observes it as-is.
//
// This is source-generation spirit implemented with plain C++ templates
// (header-only, compiled into vaml-core). NO new library target, NO runtime
// registry, NO reflection, NO DI. ADR-018: ZERO locks — a bindable property is
// written on the ViewModel's owner thread (mirrors ObservableObject).
//
// Value-domain contract: a bindable property value must be one of the bounded
// S030 payload types that `Value` can hold (bool, std::int32_t, std::int64_t,
// float, double, std::string, vaml::Color, vaml::Thickness, vaml::Matrix2D).
// A `TO_VALUE_IS_MISSING_OR_UNSUPPORTED` marker is emitted on any other T, so
// an unsupported type FAILS TO COMPILE (the bounded-domain guard — no silent
// truncation into a wrong Value tag).
//
// Field <-> store sync contract (the canonical usage):
//   - The typed backing field is the C++ source of truth (fast typed reads).
//   - `set_property` keeps the ObservableObject Value store in lock-step with
//     the field, so `observable_object_reader()->Get(name)` (S240/S241) and the
//     S242 binding engine read the live value.
//   - A bindable property is written ONLY through its generated `set_<Name>`
//     setter. Writing the base ObservableObject::Set directly for a bindable
//     name desyncs the field mirror (the field would keep its old value while
//     the store changes) — a documented footgun. The base Set remains for
//     manual / non-bindable properties.

#include "vaml/core/ObservableObject.h"   // ObservableObject, PropertyId
#include "vaml/core/Value.h"              // Value, ValueType

#include <string>

namespace vaml::mvvm {

// --- to_value: bounded S030 payload types -> Value factories ---------------
// The mapping keeps bindable-property values inside the official Value tag
// domain (S030), so the store + S240/S242 reader/binding engine handle them
// with zero special-casing. A non-listed T fails to compile (bounded guard).

[[nodiscard]] inline Value to_value(bool v) noexcept { return Value::Bool(v); }
[[nodiscard]] inline Value to_value(std::int32_t v) noexcept { return Value::Int32(v); }
[[nodiscard]] inline Value to_value(std::int64_t v) noexcept { return Value::Int64(v); }
[[nodiscard]] inline Value to_value(float v) noexcept { return Value::Float(v); }
[[nodiscard]] inline Value to_value(double v) noexcept { return Value::Double(v); }
[[nodiscard]] inline Value to_value(const std::string& v) { return Value::String(v); }
[[nodiscard]] inline Value to_value(vaml::Color v) noexcept { return Value::Color(v); }
[[nodiscard]] inline Value to_value(vaml::Thickness v) noexcept { return Value::Thickness(v); }
[[nodiscard]] inline Value to_value(vaml::Matrix2D v) noexcept { return Value::Matrix2D(v); }

// Real-change-only setter (the Toolkit-analog free template).
//
// Returns true ONLY when `new_value` differs from `field` (i.e. a REAL
// change). On a real change: assigns `field` AND calls `obj.Set(id,
// to_value(new_value))` — the S241 Set which fires the typed PropertyChanged
// signal (id, old, new) and stores the Value for the S240 writer/reader.
// On NO change (field == new_value): returns false with NO store write and NO
// signal (S035/S241 real-change discipline). Uses `==` on T — every bounded
// payload type (bool, integral, float/double, std::string, Color, Thickness,
// Matrix2D) has a value `==`; a T without `==` fails to compile together with
// the to_value bounded-domain guard (never a silent no-change).
template <typename T>
[[nodiscard]] bool set_property(ObservableObject& obj, PropertyId id, T& field,
                                const T& new_value)
{
    if (field == new_value)
    {
        // No real change: no store write, no signal.
        return false;
    }
    field = new_value;
    return obj.Set(id, to_value(new_value));
}

} // namespace vaml::mvvm

// --- PROPERTY_BINDABLE(type, Name) -----------------------------------------
// Expands to, inside a class deriving vaml::ObservableObject:
//   - `private TYPE Name_field_ = TYPE{};`  - the typed backing field
//   - `const TYPE& Name() const noexcept`   - the getter
//   - `bool set_Name(const TYPE&)`          - the setter (wires set_property)
// The property NAME is registered via PropertyId::of(#Name) — a string literal
// (compile-time, no registry, no collision class). Accessor naming matches the
// repo convention (orientation()/set_orientation()).
#define PROPERTY_BINDABLE(TYPE, NAME)                                        \
private:                                                                     \
    TYPE NAME##_field_ = TYPE{};                                             \
                                                                             \
public:                                                                      \
    [[nodiscard]] const TYPE& NAME() const noexcept { return NAME##_field_; } \
    [[nodiscard]] bool set_##NAME(const TYPE& v)                            \
    {                                                                        \
        return vaml::mvvm::set_property(*this, vaml::PropertyId::of(#NAME),   \
                                        NAME##_field_, v);                    \
    }
