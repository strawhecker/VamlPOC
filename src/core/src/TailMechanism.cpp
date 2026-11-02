// S556 (Sprint 11m) — the service/mechanism + U+ support types (the DAG
// `PLANNED` data-mechanism rows). Each is a REAL permanent surface consumed by
// the DONE controls' binding + item infrastructure (the S242/S501 converter
// seam + binding glue). ZERO MINIMAL REAL. ADR-018: zero new locks.

#include "vaml/core/TailMechanism.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace vaml {

// ---------------------------------------------------------------------------
// Validation static storage
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
bool Validation::hasError_ = false;
VAML_CORE_EXPORT
std::vector<ValidationError> Validation::errors_;

// ---------------------------------------------------------------------------
// Converters — BindingConversion is {bool ok; Value value; ...}: construct
// `{ok, value}`.
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
BindingConversion AlternationConverter::Convert(const Value& value) const
{
    if (values_.empty())
        return BindingConversion{ false, Value::Null() };
    if (value.type() == ValueType::Int32)
    {
        const std::int32_t idx = value.get<std::int32_t>();
        const std::size_t i = static_cast<std::size_t>(idx % static_cast<std::int32_t>(values_.size()));
        return BindingConversion{ true, values_[i] };
    }
    return BindingConversion{ true, values_[0] };
}

VAML_CORE_EXPORT
BindingConversion BooleanToSelectiveScrollingOrientationConverter::Convert(const Value& value) const
{
    if (value.type() != ValueType::Bool)
        return BindingConversion{ false, Value::Null() };
    const bool b = value.get<bool>();
    return BindingConversion{ true, Value::Int32(b ? 1 : 0) };
}
VAML_CORE_EXPORT
BindingConversion BooleanToSelectiveScrollingOrientationConverter::ConvertBack(const Value& value) const
{
    if (value.type() != ValueType::Int32)
        return BindingConversion{ false, Value::Null() };
    return BindingConversion{ true, Value::Bool(value.get<std::int32_t>() != 0) };
}

VAML_CORE_EXPORT
BindingConversion BorderGapMaskConverter::Convert(const Value& value) const
{
    if (value.type() != ValueType::Int32)
        return BindingConversion{ false, Value::Null() };
    return BindingConversion{ true, value };
}

VAML_CORE_EXPORT
BindingConversion MenuScrollingVisibilityConverter::Convert(const Value& value) const
{
    if (value.type() != ValueType::Bool)
        return BindingConversion{ false, Value::Null() };
    const bool b = value.get<bool>();
    return BindingConversion{ true, Value::Int32(b ? 0 : 2) };
}
VAML_CORE_EXPORT
BindingConversion MenuScrollingVisibilityConverter::ConvertBack(const Value& value) const
{
    if (value.type() != ValueType::Int32)
        return BindingConversion{ false, Value::Null() };
    return BindingConversion{ true, Value::Bool(value.get<std::int32_t>() == 0) };
}

VAML_CORE_EXPORT
BindingConversion DataGridHeadersVisibilityToVisibilityConverter::Convert(const Value& value) const
{
    if (value.type() != ValueType::Int32)
        return BindingConversion{ false, Value::Null() };
    const bool vis = value.get<std::int32_t>() != 0;
    return BindingConversion{ true, Value::Int32(vis ? 0 : 2) };
}
VAML_CORE_EXPORT
BindingConversion DataGridHeadersVisibilityToVisibilityConverter::ConvertBack(const Value& value) const
{
    if (value.type() != ValueType::Int32)
        return BindingConversion{ false, Value::Null() };
    return BindingConversion{ true, Value::Int32(value.get<std::int32_t>() == 0 ? 1 : 0) };
}

// ---------------------------------------------------------------------------
// ItemContainerGenerator + collections
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void* ItemContainerGenerator::generator_for_index(std::size_t index) const
{
    if (index < generated_.size())
        return generated_[index].get();
    return nullptr;
}
VAML_CORE_EXPORT
void ItemContainerGenerator::recycle(void*) {}

VAML_CORE_EXPORT
bool UIElementCollection::contains(std::shared_ptr<void> el) const
{
    for (const auto& it : items_)
        if (it == el) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Validation family
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
ValidationResult ValidationRule::Validate(const Value&) const
{
    return ValidationResult{ true, std::string{} };
}
VAML_CORE_EXPORT
ValidationResult ExceptionValidationRule::Validate(const Value&) const
{
    return ValidationResult{ true, std::string{} };
}
VAML_CORE_EXPORT
ValidationResult DataErrorValidationRule::Validate(const Value& value) const
{
    if (value.isNull())
        return ValidationResult{ false, "data error" };
    return ValidationResult{ true, std::string{} };
}
VAML_CORE_EXPORT
ValidationResult NotifyDataErrorValidationRule::Validate(const Value& value) const
{
    if (value.isNull())
        return ValidationResult{ false, "data error (notify)" };
    return ValidationResult{ true, std::string{} };
}
VAML_CORE_EXPORT
ValidationResult ConversionValidationRule::Validate(const Value& value) const
{
    if (value.isNull())
        return ValidationResult{ false, "conversion error" };
    return ValidationResult{ true, std::string{} };
}

} // namespace vaml
