// S030 — Value tests (S030 scope: factory→tag mapping, type()/isNull(),
// equality for all 11 payloads). The type-mismatch-throws suite is S031
// (roadmap makes rounding + mismatch its own step).

// NOTE: doctest's implementation (main) is defined by smoke_test.cpp via
// DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN. This TU only declares tests — it must
// NOT re-define it (would duplicate main/doctest statics).
#include "doctest/doctest.h"

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
    CHECK(v.type() == ValueType::Null);
    CHECK(v.isNull());
}

TEST_CASE("S030 Value: scalar factories map to their tags")
{
    CHECK(Value::Bool(true).type() == ValueType::Bool);
    CHECK(Value::Int32(7).type() == ValueType::Int32);
    CHECK(Value::Int64(7).type() == ValueType::Int64);
    CHECK(Value::Float(1.5f).type() == ValueType::Float);
    CHECK(Value::Double(1.5).type() == ValueType::Double);
    CHECK(Value::BrushId(3).type() == ValueType::BrushId);
}

TEST_CASE("S030 Value: String/Color/Thickness/UntypedPtr map to their tags")
{
    CHECK(Value::String("hi").type() == ValueType::String);
    CHECK(Value::Color(Color{255, 0, 0, 255}).type() == ValueType::Color);
    CHECK(Value::Thickness(Thickness{1.0f, 2.0f, 3.0f, 4.0f}).type() == ValueType::Thickness);
    int x = 0;
    CHECK(Value::UntypedPtr(&x).type() == ValueType::UntypedPtr);
}

TEST_CASE("S030 Value: only Null isNull, non-Null payloads are not null")
{
    CHECK(Value::Null().isNull());
    CHECK_FALSE(Value::Bool(false).isNull());
    CHECK_FALSE(Value::Int32(0).isNull());
    CHECK_FALSE(Value::String("").isNull());
}

TEST_CASE("S030 Value: get<T> round-trips every payload type")
{
    CHECK(Value::Bool(true).get<bool>() == true);
    CHECK(Value::Int32(-42).get<std::int32_t>() == -42);
    CHECK(Value::Int64(-42).get<std::int64_t>() == -42);
    CHECK(Value::Float(3.25f).get<float>() == 3.25f);
    CHECK(Value::Double(3.25).get<double>() == 3.25);
    CHECK(Value::String("abc").get<std::string>() == "abc");
    CHECK(Value::Color(Color{10, 20, 30, 40}).get<Color>() == Color{10, 20, 30, 40});
    CHECK(Value::Thickness(Thickness{1, 2, 3, 4}).get<Thickness>() == Thickness{1, 2, 3, 4});
    void* p = nullptr;
    CHECK(Value::UntypedPtr(&p).get<void*>() == &p);
}

TEST_CASE("S030 Value: BrushId is an int32 payload readable via get<int32_t>")
{
    // BrushId carries a tagged scalar id; retrieval accepts the Int32 payload
    // type. The distinct tag is preserved (S031 owns the mismatch suite).
    const Value v = Value::BrushId(99);
    CHECK(v.type() == ValueType::BrushId);
    CHECK(v.get<std::int32_t>() == 99);
}

TEST_CASE("S030 Value: equality — same tag + same payload")
{
    CHECK(Value::Null() == Value::Null());
    CHECK(Value::Bool(true) == Value::Bool(true));
    CHECK(Value::Int32(5) == Value::Int32(5));
    CHECK(Value::Int64(5) == Value::Int64(5));
    CHECK(Value::Float(1.5f) == Value::Float(1.5f));
    CHECK(Value::Double(1.5) == Value::Double(1.5));
    CHECK(Value::String("xyz") == Value::String("xyz"));
    CHECK(Value::Color(Color{1, 2, 3, 4}) == Value::Color(Color{1, 2, 3, 4}));
    CHECK(Value::Thickness(Thickness{1, 2, 3, 4}) == Value::Thickness(Thickness{1, 2, 3, 4}));
    void* p = nullptr;
    CHECK(Value::UntypedPtr(&p) == Value::UntypedPtr(&p));
}

TEST_CASE("S030 Value: inequality — differing tags or differing payloads")
{
    CHECK_FALSE(Value::Int32(5) == Value::Int64(5));      // tag differs
    CHECK_FALSE(Value::Bool(true) == Value::Bool(false)); // payload differs
    CHECK_FALSE(Value::Int32(5) == Value::Int32(6));
    CHECK_FALSE(Value::String("a") == Value::String("b"));
    CHECK_FALSE(Value::Color(Color{1, 2, 3, 4}) == Value::Color(Color{1, 2, 3, 5}));
    CHECK_FALSE(Value::Thickness(Thickness{1, 2, 3, 4}) == Value::Thickness(Thickness{1, 2, 3, 5}));
    CHECK_FALSE(Value::Null() == Value::Bool(false)); // null != non-null
    CHECK_FALSE(Value::BrushId(1) == Value::Int32(1)); // different tags
}

TEST_CASE("S030 Value: copy and move preserve payload and type")
{
    const Value src = Value::String("copy-me");
    const Value copy = src;
    CHECK(copy.type() == ValueType::String);
    CHECK(copy.get<std::string>() == "copy-me");
    CHECK(src.get<std::string>() == "copy-me");

    Value moved = Value::String("move-me");
    const Value target = std::move(moved);
    CHECK(target.type() == ValueType::String);
    CHECK(target.get<std::string>() == "move-me");
}

TEST_CASE("S030 Value: copy-assignment and move-assignment")
{
    Value a = Value::Int32(1);
    Value b = Value::String("hello");
    a = b;
    CHECK(a.type() == ValueType::String);
    CHECK(a.get<std::string>() == "hello");

    Value c = Value::Int32(1);
    Value d = Value::String("world");
    c = std::move(d);
    CHECK(c.type() == ValueType::String);
    CHECK(c.get<std::string>() == "world");
}
