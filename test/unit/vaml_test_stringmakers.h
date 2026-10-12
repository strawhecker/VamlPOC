#pragma once
// S036_2 — doctest::StringMaker specializations for VAML core types.
//
// These make failed assertions readable at the SOURCE level (the exact
// "source-tool readability scaffold" the re-formulated Shouldly-for-doctest
// needs): when doctest stringifies a failing expression's operands, a
// specialized StringMaker converts a vaml::Value / SetResult / PropertyId into
// a human-readable string instead of garbage-pointer / default text.
//
// This file is TEST-ONLY: the specializations live in `namespace doctest` and
// therefore cannot exist in core (core must stay doctest-free, per the S036
// layering rule). Each test TU that wants readable failure output includes this
// header. It is framework-native (pure StringMaker additions — no macro
// surgery, no touching doctest itself).
//
// StringMaker contract (doctest): `static String convert(T)` — see
// doctest::StringMaker in third_party/doctest/doctest.h.

#include <doctest/doctest.h>

#include "vaml/core/DependencyObject.h" // SetResult
#include "vaml/core/PropertyId.h"       // PropertyId
#include "vaml/core/Value.h"            // Value, ValueType

#include <sstream>
#include <string>

namespace doctest {

namespace detail {
// Maps a vaml::ValueType tag to its readable name (used by StringMaker<Value>).
inline const char* vaml_value_type_name(vaml::ValueType t)
{
    switch (t)
    {
        case vaml::ValueType::Null:      return "Null";
        case vaml::ValueType::Bool:      return "Bool";
        case vaml::ValueType::Int32:     return "Int32";
        case vaml::ValueType::Int64:     return "Int64";
        case vaml::ValueType::Float:     return "Float";
        case vaml::ValueType::Double:    return "Double";
        case vaml::ValueType::String:    return "String";
        case vaml::ValueType::Color:     return "Color";
        case vaml::ValueType::Thickness: return "Thickness";
        case vaml::ValueType::BrushId:   return "BrushId";
        case vaml::ValueType::UntypedPtr:return "UntypedPtr";
    }
    return "Unknown";
}
} // namespace detail

template <>
struct StringMaker<vaml::Value>
{
    static String convert(vaml::Value const& v)
    {
        // "<Int32 42>", "<String \"hello\">", "<Null>" … — tag-first, so the
        // type is always visible even when the payload is a scalar.
        std::ostringstream os;
        os << "<" << detail::vaml_value_type_name(v.type()) << " ";
        switch (v.type())
        {
            case vaml::ValueType::Null:       os << "null";                        break;
            case vaml::ValueType::Bool:       os << (v.get<bool>() ? "true" : "false"); break;
            case vaml::ValueType::Int32:      os << v.get<std::int32_t>();          break;
            case vaml::ValueType::Int64:      os << v.get<std::int64_t>();          break;
            case vaml::ValueType::Float:      os << v.get<float>();                 break;
            case vaml::ValueType::Double:     os << v.get<double>();                break;
            case vaml::ValueType::String:     os << "\"" << v.get<std::string>() << "\""; break;
            case vaml::ValueType::Color:      os << "Color";                        break;
            case vaml::ValueType::Thickness:  os << "Thickness";                    break;
            case vaml::ValueType::BrushId:    os << v.get<std::int32_t>();          break;
            case vaml::ValueType::UntypedPtr: os << v.get<void*>();                 break;
        }
        os << ">";
        return os.str().c_str(); // doctest::String(const char*)
    }
};

template <>
struct StringMaker<vaml::SetResult>
{
    static String convert(vaml::SetResult const& s)
    {
        switch (s)
        {
            case vaml::SetResult::APPLIED:   return "SetResult::APPLIED";
            case vaml::SetResult::REJECTED:  return "SetResult::REJECTED";
            case vaml::SetResult::NO_CHANGE: return "SetResult::NO_CHANGE";
        }
        return "SetResult::?";
    }
};

template <>
struct StringMaker<vaml::PropertyId>
{
    static String convert(vaml::PropertyId const& id)
    {
        if (id.valid())
        {
            return (std::string("PropertyId(") + id.name().data() + ")").c_str();
        }
        return "PropertyId(empty)";
    }
};

} // namespace doctest
