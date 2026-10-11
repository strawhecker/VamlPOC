// S030 — Value out-of-line definitions: std::string lifetime management,
// equality, and the type-mismatch throw path. The per-type hot paths are
// inline in the header; these are the parts that must be real + linkable.

#include "vaml/core/Value.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace vaml {

namespace {

// Stable names for diagnostics (mirrors the enum order).
const char* type_name(ValueType t) noexcept
{
    switch (t)
    {
        case ValueType::Null:       return "Null";
        case ValueType::Bool:       return "Bool";
        case ValueType::Int32:      return "Int32";
        case ValueType::Int64:      return "Int64";
        case ValueType::Float:      return "Float";
        case ValueType::Double:     return "Double";
        case ValueType::String:     return "String";
        case ValueType::Color:      return "Color";
        case ValueType::Thickness:  return "Thickness";
        case ValueType::BrushId:    return "BrushId";
        case ValueType::UntypedPtr: return "UntypedPtr";
    }
    return "<invalid>";
}

} // namespace

// Reconstructs a copy of other into *this (which must be freshly
// constructed — only the union member is initialized here).
VAML_CORE_EXPORT
void Value::copy_union(Value::Storage& dst, const Value::Storage& src, ValueType t)
{
    switch (t)
    {
        case ValueType::Bool:       new (&dst) bool(src.asBool); break;
        case ValueType::Int32:      new (&dst) std::int32_t(src.asInt32); break;
        case ValueType::Int64:      new (&dst) std::int64_t(src.asInt64); break;
        case ValueType::Float:      new (&dst) float(src.asFloat); break;
        case ValueType::Double:     new (&dst) double(src.asDouble); break;
        case ValueType::String:     new (&dst) std::string(src.asString); break;
        case ValueType::Color:      new (&dst) vaml::Color(src.asColor); break;
        case ValueType::Thickness:  new (&dst) vaml::Thickness(src.asThickness); break;
        case ValueType::BrushId:    new (&dst) std::int32_t(src.asInt32); break;
        case ValueType::UntypedPtr: new (&dst) void*(src.asPtr); break;
        case ValueType::Null:       break; // no union member
    }
}

VAML_CORE_EXPORT
Value::~Value()
{
    if (type_ == ValueType::String)
    {
        storage_.asString.~basic_string();
    }
}

VAML_CORE_EXPORT
Value::Value(const Value& other)
    : type_(other.type_)
{
    copy_union(storage_, other.storage_, type_);
}

VAML_CORE_EXPORT
Value& Value::operator=(const Value& other)
{
    if (this == &other)
    {
        return *this;
    }

    // Destroy current string payload, then rebuild from other.
    if (type_ == ValueType::String)
    {
        storage_.asString.~basic_string();
    }
    type_ = other.type_;
    copy_union(storage_, other.storage_, type_);
    return *this;
}

VAML_CORE_EXPORT
Value::Value(Value&& other) noexcept
    : type_(other.type_)
{
    if (type_ == ValueType::String)
    {
        new (&storage_) std::string(std::move(other.storage_.asString));
    }
    else
    {
        copy_union(storage_, other.storage_, type_);
    }
    other.type_ = ValueType::Null;
}

VAML_CORE_EXPORT
Value& Value::operator=(Value&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (type_ == ValueType::String)
    {
        storage_.asString.~basic_string();
    }

    type_ = other.type_;
    if (type_ == ValueType::String)
    {
        new (&storage_) std::string(std::move(other.storage_.asString));
    }
    else
    {
        copy_union(storage_, other.storage_, type_);
    }
    other.type_ = ValueType::Null;
    return *this;
}

VAML_CORE_EXPORT
bool Value::operator==(const Value& other) const noexcept
{
    if (type_ != other.type_)
    {
        return false;
    }

    switch (type_)
    {
        case ValueType::Null:       return true;
        case ValueType::Bool:       return storage_.asBool == other.storage_.asBool;
        case ValueType::Int32:      return storage_.asInt32 == other.storage_.asInt32;
        case ValueType::Int64:      return storage_.asInt64 == other.storage_.asInt64;
        case ValueType::Float:      return storage_.asFloat == other.storage_.asFloat;
        case ValueType::Double:     return storage_.asDouble == other.storage_.asDouble;
        case ValueType::String:     return storage_.asString == other.storage_.asString;
        case ValueType::Color:      return storage_.asColor == other.storage_.asColor;
        case ValueType::Thickness:  return storage_.asThickness == other.storage_.asThickness;
        case ValueType::BrushId:    return storage_.asInt32 == other.storage_.asInt32;
        case ValueType::UntypedPtr: return storage_.asPtr == other.storage_.asPtr;
    }
    return false;
}

VAML_CORE_EXPORT
[[noreturn]] void Value::throw_mismatch(ValueType actual) const
{
    // Build a bounded diagnostic: current tag vs the requested payload type is
    // reported through logic_error what(). (The requested type name is not
    // passed here; callers get the actual stored tag, which is the actionable
    // half.)
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Value type mismatch: stored tag is '%s'",
                  type_name(actual));
    throw ValueTypeMismatch(buf);
}

} // namespace vaml
