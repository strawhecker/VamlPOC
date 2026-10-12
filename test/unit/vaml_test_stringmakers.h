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
#include "vaml/core/PropertyKey.h"      // PropertyKey
#include "vaml/core/Value.h"            // Value, ValueType

#include <ostream>
#include <sstream>
#include <string>

// -------------------------------------------------------------------------------------------
// S036_4 — operator<< overloads for VAML core types (test-side only).
//
// Shouldly's stream-path formatter (ShouldlyStr -> ThrowAwesomelyStr) renders a failed
// assertion via `std::stringstream << actual`, so the domain types migrated to the Shouldly
// vocabulary need a human-readable operator<<. These reuse the EXACT readable-text intent of
// the StringMaker specializations below ("<Int32 42>", "SetResult::APPLIED", "PropertyId(Width)")
// so the same message shape appears whether doctest stringifies (StringMaker) or Shouldly does
// (operator<<). Defined in `namespace vaml` so ADL finds them for `vaml::Value` etc.
//
// They live HERE (test-side), NOT in core: Value.h/Color.h/Thickness.h must stay lean, and
// Color/Thickness are S089 MINIMAL REAL ("do NOT extend silently") — so the formatting surface
// is deliberately kept out of the production headers. Only test TUs include this header.
// -------------------------------------------------------------------------------------------
namespace vaml {

inline std::ostream& operator<<(std::ostream& os, PropertyKey const& k)
{
    return os << k.describe(); // human-readable (name/type/owner/flags) via the core describe()
}

inline std::ostream& operator<<(std::ostream& os, Color const& c)
{
    return os << "RGBA(" << unsigned(c.r) << "," << unsigned(c.g) << ","
              << unsigned(c.b) << "," << unsigned(c.a) << ")";
}

inline std::ostream& operator<<(std::ostream& os, Thickness const& t)
{
    return os << "Insets(" << t.left << "," << t.top << "," << t.right << "," << t.bottom << ")";
}

inline std::ostream& operator<<(std::ostream& os, ValueType t)
{
    switch (t)
    {
        case ValueType::Null:      return os << "Null";
        case ValueType::Bool:      return os << "Bool";
        case ValueType::Int32:     return os << "Int32";
        case ValueType::Int64:     return os << "Int64";
        case ValueType::Float:     return os << "Float";
        case ValueType::Double:    return os << "Double";
        case ValueType::String:    return os << "String";
        case ValueType::Color:     return os << "Color";
        case ValueType::Thickness: return os << "Thickness";
        case ValueType::BrushId:   return os << "BrushId";
        case ValueType::UntypedPtr:return os << "UntypedPtr";
    }
    return os << "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, Value const& v)
{
    os << "<" << v.type() << " ";
    switch (v.type())
    {
        case ValueType::Null:       os << "null";                            break;
        case ValueType::Bool:       os << (v.get<bool>() ? "true" : "false"); break;
        case ValueType::Int32:      os << v.get<std::int32_t>();              break;
        case ValueType::Int64:      os << v.get<std::int64_t>();              break;
        case ValueType::Float:      os << v.get<float>();                     break;
        case ValueType::Double:     os << v.get<double>();                    break;
        case ValueType::String:     os << "\"" << v.get<std::string>() << "\""; break;
        case ValueType::Color:      os << v.get<Color>();                     break;
        case ValueType::Thickness:  os << v.get<Thickness>();                 break;
        case ValueType::BrushId:    os << v.get<std::int32_t>();              break;
        case ValueType::UntypedPtr: os << v.get<void*>();                     break;
    }
    return os << ">";
}

inline std::ostream& operator<<(std::ostream& os, SetResult const& s)
{
    switch (s)
    {
        case SetResult::APPLIED:   return os << "SetResult::APPLIED";
        case SetResult::REJECTED:  return os << "SetResult::REJECTED";
        case SetResult::NO_CHANGE: return os << "SetResult::NO_CHANGE";
        case SetResult::QUEUED:    return os << "SetResult::QUEUED"; // S037
    }
    return os << "SetResult::?";
}

inline std::ostream& operator<<(std::ostream& os, PropertyId const& id)
{
    os << "PropertyId(";
    if (id.valid())
    {
        os << id.name().data();
    }
    else
    {
        os << "empty";
    }
    return os << ")";
}

} // namespace vaml

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
            case vaml::SetResult::QUEUED:    return "SetResult::QUEUED"; // S037
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
