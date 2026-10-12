// S030 — Value tests (S030 scope: factory→tag mapping, type()/isNull(),
// equality for all 11 payloads). The type-mismatch-throws suite is S031
// (roadmap makes rounding + mismatch its own step).

// S036_4: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString overloads).

// NOTE: doctest's implementation (main) is defined by smoke_test.cpp via
// DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN. This TU only declares tests — it must
// NOT re-define it (would duplicate main/doctest statics).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString (SHOULD seam, S036_1)

#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <utility>

using vaml::Color;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueType;

TEST_CASE("S030 Value: Null factory maps to Null tag and isNull")
{
    const Value v = Value::Null();
    SHOULD(ShouldlyString(v.type()).ShouldBe(ValueType::Null));
    SHOULD(ShouldlyString(v.isNull()).ShouldBe(true));
}

TEST_CASE("S030 Value: scalar factories map to their tags")
{
    SHOULD(ShouldlyString(Value::Bool(true).type()).ShouldBe(ValueType::Bool));
    SHOULD(ShouldlyString(Value::Int32(7).type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(Value::Int64(7).type()).ShouldBe(ValueType::Int64));
    SHOULD(ShouldlyString(Value::Float(1.5f).type()).ShouldBe(ValueType::Float));
    SHOULD(ShouldlyString(Value::Double(1.5).type()).ShouldBe(ValueType::Double));
    SHOULD(ShouldlyString(Value::BrushId(3).type()).ShouldBe(ValueType::BrushId));
}

TEST_CASE("S030 Value: String/Color/Thickness/UntypedPtr map to their tags")
{
    SHOULD(ShouldlyString(Value::String("hi").type()).ShouldBe(ValueType::String));
    SHOULD(ShouldlyString(Value::Color(Color{255, 0, 0, 255}).type()).ShouldBe(ValueType::Color));
    SHOULD(ShouldlyString(Value::Thickness(Thickness{1.0f, 2.0f, 3.0f, 4.0f}).type()).ShouldBe(ValueType::Thickness));
    int x = 0;
    SHOULD(ShouldlyString(Value::UntypedPtr(&x).type()).ShouldBe(ValueType::UntypedPtr));
}

TEST_CASE("S030 Value: only Null isNull, non-Null payloads are not null")
{
    SHOULD(ShouldlyString(Value::Null().isNull()).ShouldBe(true));
    SHOULD(ShouldlyString(Value::Bool(false).isNull()).ShouldNotBe(true));
    SHOULD(ShouldlyString(Value::Int32(0).isNull()).ShouldNotBe(true));
    SHOULD(ShouldlyString(Value::String("").isNull()).ShouldNotBe(true));
}

TEST_CASE("S030 Value: get<T> round-trips every payload type")
{
    SHOULD(ShouldlyString(Value::Bool(true).get<bool>()).ShouldBe(true));
    SHOULD(ShouldlyString(Value::Int32(-42).get<std::int32_t>()).ShouldBe(-42));
    SHOULD(ShouldlyString(Value::Int64(-42).get<std::int64_t>()).ShouldBe(-42));
    SHOULD(ShouldlyString(Value::Float(3.25f).get<float>()).ShouldBe(3.25f));
    SHOULD(ShouldlyString(Value::Double(3.25).get<double>()).ShouldBe(3.25));
    SHOULD(ShouldlyString(Value::String("abc").get<std::string>()).ShouldBe(std::string("abc")));
    SHOULD(ShouldlyString(Value::Color(Color{10, 20, 30, 40}).get<Color>()).ShouldBe(Color{10, 20, 30, 40}));
    SHOULD(ShouldlyString(Value::Thickness(Thickness{1, 2, 3, 4}).get<Thickness>()).ShouldBe(Thickness{1, 2, 3, 4}));
    void* p = nullptr;
    SHOULD(ShouldlyString(Value::UntypedPtr(&p).get<void*>()).ShouldBe(&p));
}

TEST_CASE("S030 Value: BrushId is an int32 payload readable via get<int32_t>")
{
    // BrushId carries a tagged scalar id; retrieval accepts the Int32 payload
    // type. The distinct tag is preserved (S031 owns the mismatch suite).
    const Value v = Value::BrushId(99);
    SHOULD(ShouldlyString(v.type()).ShouldBe(ValueType::BrushId));
    SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(99));
}

TEST_CASE("S030 Value: equality — same tag + same payload")
{
    SHOULD(ShouldlyString(Value::Null()).ShouldBe(Value::Null()));
    SHOULD(ShouldlyString(Value::Bool(true)).ShouldBe(Value::Bool(true)));
    SHOULD(ShouldlyString(Value::Int32(5)).ShouldBe(Value::Int32(5)));
    SHOULD(ShouldlyString(Value::Int64(5)).ShouldBe(Value::Int64(5)));
    SHOULD(ShouldlyString(Value::Float(1.5f)).ShouldBe(Value::Float(1.5f)));
    SHOULD(ShouldlyString(Value::Double(1.5)).ShouldBe(Value::Double(1.5)));
    SHOULD(ShouldlyString(Value::String("xyz")).ShouldBe(Value::String("xyz")));
    SHOULD(ShouldlyString(Value::Color(Color{1, 2, 3, 4})).ShouldBe(Value::Color(Color{1, 2, 3, 4})));
    SHOULD(ShouldlyString(Value::Thickness(Thickness{1, 2, 3, 4})).ShouldBe(Value::Thickness(Thickness{1, 2, 3, 4})));
    void* p = nullptr;
    SHOULD(ShouldlyString(Value::UntypedPtr(&p)).ShouldBe(Value::UntypedPtr(&p)));
}

TEST_CASE("S030 Value: inequality — differing tags or differing payloads")
{
    SHOULD(ShouldlyString(Value::Int32(5)).ShouldNotBe(Value::Int64(5)));      // tag differs
    SHOULD(ShouldlyString(Value::Bool(true)).ShouldNotBe(Value::Bool(false))); // payload differs
    SHOULD(ShouldlyString(Value::Int32(5)).ShouldNotBe(Value::Int32(6)));
    SHOULD(ShouldlyString(Value::String("a")).ShouldNotBe(Value::String("b")));
    SHOULD(ShouldlyString(Value::Color(Color{1, 2, 3, 4})).ShouldNotBe(Value::Color(Color{1, 2, 3, 5})));
    SHOULD(ShouldlyString(Value::Thickness(Thickness{1, 2, 3, 4})).ShouldNotBe(Value::Thickness(Thickness{1, 2, 3, 5})));
    SHOULD(ShouldlyString(Value::Null()).ShouldNotBe(Value::Bool(false))); // null != non-null
    SHOULD(ShouldlyString(Value::BrushId(1)).ShouldNotBe(Value::Int32(1))); // different tags
}

TEST_CASE("S030 Value: copy and move preserve payload and type")
{
    const Value src = Value::String("copy-me");
    const Value copy = src;
    SHOULD(ShouldlyString(copy.type()).ShouldBe(ValueType::String));
    SHOULD(ShouldlyString(copy.get<std::string>()).ShouldBe(std::string("copy-me")));
    SHOULD(ShouldlyString(src.get<std::string>()).ShouldBe(std::string("copy-me")));

    Value moved = Value::String("move-me");
    const Value target = std::move(moved);
    SHOULD(ShouldlyString(target.type()).ShouldBe(ValueType::String));
    SHOULD(ShouldlyString(target.get<std::string>()).ShouldBe(std::string("move-me")));
}

TEST_CASE("S030 Value: copy-assignment and move-assignment")
{
    Value a = Value::Int32(1);
    Value b = Value::String("hello");
    a = b;
    SHOULD(ShouldlyString(a.type()).ShouldBe(ValueType::String));
    SHOULD(ShouldlyString(a.get<std::string>()).ShouldBe(std::string("hello")));

    Value c = Value::Int32(1);
    Value d = Value::String("world");
    c = std::move(d);
    SHOULD(ShouldlyString(c.type()).ShouldBe(ValueType::String));
    SHOULD(ShouldlyString(c.get<std::string>()).ShouldBe(std::string("world")));
}
