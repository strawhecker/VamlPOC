// S031 — Value round-trip + type-mismatch-throws tests.
// S031 dep:S030. Scope per roadmap: prove EVERY payload round-trips through
// get<T>() exactly, and prove get<T>() throws vaml::ValueTypeMismatch for any
// non-valid payload type on a given tag — including the discriminators that
// share a payload (Int32/BrushId both read via int32_t; anything else throws).
// This is the complement to S030's Value_test.cpp (factory→tag/equality/copy).

// Main/impl is owned by smoke_test.cpp (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN).
// This TU only declares tests — it must NOT re-define the doctest main.
#include "doctest/doctest.h"

#include "vaml/core/Value.h"

#include <cstdint>
#include <string>

using vaml::Color;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;

// ---------------------------------------------------------------------------
// Step 1 — Round-trip: store-then-read returns the exact payload for every non-
// Null tag, and the re-factored value equals the original (operator== round-trip).
// ---------------------------------------------------------------------------

TEST_CASE("S031 Value: every scalar payload round-trips through get<T> exactly")
{
    // Bool
    {
        const Value v = Value::Bool(true);
        CHECK(v.get<bool>() == true);
        CHECK(Value::Bool(v.get<bool>()) == v); // re-factory == original
    }
    // Int32
    {
        const Value v = Value::Int32(-2147483647L - 1L); // INT32_MIN
        CHECK(v.get<std::int32_t>() == -2147483647L - 1L);
        CHECK(Value::Int32(v.get<std::int32_t>()) == v);
    }
    // Int64
    {
        const Value v = Value::Int64(-9007199254740993LL);
        CHECK(v.get<std::int64_t>() == -9007199254740993LL);
        CHECK(Value::Int64(v.get<std::int64_t>()) == v);
    }
    // Float
    {
        const Value v = Value::Float(3.25f);
        CHECK(v.get<float>() == 3.25f);
        CHECK(Value::Float(v.get<float>()) == v);
    }
    // Double
    {
        const Value v = Value::Double(3.25);
        CHECK(v.get<double>() == 3.25);
        CHECK(Value::Double(v.get<double>()) == v);
    }
}

TEST_CASE("S031 Value: String round-trips by value (copy), not by pointer")
{
    const Value v = Value::String("round trip string");
    const std::string got = v.get<std::string>();
    CHECK(got == "round trip string");
    // Mutating the read-back copy must not affect the stored value.
    std::string mutated = v.get<std::string>();
    mutated.push_back('!');
    CHECK(v.get<std::string>() == "round trip string"); // stored copy unchanged
    CHECK(Value::String(v.get<std::string>()) == v);
}

TEST_CASE("S031 Value: Color/Thickness/BrushId/UntypedPtr round-trip through get<T>")
{
    // Color
    {
        const Color c{1, 2, 3, 4};
        const Value v = Value::Color(c);
        CHECK(v.get<Color>() == c);
        CHECK(Value::Color(v.get<Color>()) == v);
    }
    // Thickness
    {
        const Thickness t{1.0f, 2.0f, 3.0f, 4.0f};
        const Value v = Value::Thickness(t);
        CHECK(v.get<Thickness>() == t);
        CHECK(Value::Thickness(v.get<Thickness>()) == v);
    }
    // BrushId — int32 payload under a dedicated tag.
    {
        const Value v = Value::BrushId(99);
        CHECK(v.get<std::int32_t>() == 99);
        CHECK(Value::BrushId(v.get<std::int32_t>()) == v);
    }
    // UntypedPtr — pointer round-trips by value.
    {
        int x = 0;
        const Value v = Value::UntypedPtr(&x);
        CHECK(v.get<void*>() == &x);
        CHECK(Value::UntypedPtr(v.get<void*>()) == v);
    }
}

// ---------------------------------------------------------------------------
// Step 2 — Type-mismatch: get<T> throws ValueTypeMismatch when T is not a valid
// payload type for the stored tag. Null has NO payload, so ANY get throws.
// ---------------------------------------------------------------------------

TEST_CASE("S031 Value: Null has no payload — every get<T> throws")
{
    const Value null = Value::Null();
    CHECK_THROWS_AS((void)null.get<bool>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<std::int32_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<std::int64_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<float>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<double>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<std::string>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<Color>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<Thickness>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)null.get<void*>(), ValueTypeMismatch);
    // Gesture of miss must not corrupt — Null stays Null.
    CHECK(null.isNull());
}

// The valid type for each non-Null tag, and the set of all types that must
// throw against it. Exercised exhaustively so the mismatch path is proven for
// every (tag, type) pair except the valid one(s).
TEST_CASE("S031 Value: every tag throws on every non-valid payload type")
{
    // Bool: only bool reads; everything else throws.
    {
        const Value v = Value::Bool(true);
        CHECK(v.get<bool>() == true);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<bool>() == true); // valid read still succeeds after throws
    }
    // Int32: int32 only (NOT int64/float/double — exact storage, no promotion).
    {
        const Value v = Value::Int32(-42);
        CHECK(v.get<std::int32_t>() == -42);
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<std::int32_t>() == -42);
    }
    // Int64.
    {
        const Value v = Value::Int64(-42);
        CHECK(v.get<std::int64_t>() == -42);
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<std::int64_t>() == -42);
    }
    // Float.
    {
        const Value v = Value::Float(1.5f);
        CHECK(v.get<float>() == 1.5f);
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<float>() == 1.5f);
    }
    // Double.
    {
        const Value v = Value::Double(1.5);
        CHECK(v.get<double>() == 1.5);
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<double>() == 1.5);
    }
    // String: mismatches every scalar, and is mismatched-by every scalar.
    {
        const Value v = Value::String("abc");
        CHECK(v.get<std::string>() == "abc");
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<std::string>() == "abc"); // stored string intact after throws
    }
    // Color.
    {
        const Value v = Value::Color(Color{1, 2, 3, 4});
        CHECK(v.get<Color>() == Color{1, 2, 3, 4});
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<Color>() == Color{1, 2, 3, 4});
    }
    // Thickness.
    {
        const Value v = Value::Thickness(Thickness{1, 2, 3, 4});
        CHECK(v.get<Thickness>() == Thickness{1, 2, 3, 4});
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
        CHECK(v.get<Thickness>() == Thickness{1, 2, 3, 4});
    }
    // UntypedPtr: only void* reads.
    {
        int x = 0;
        const Value v = Value::UntypedPtr(&x);
        CHECK(v.get<void*>() == &x);
        CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int32_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
        CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
        CHECK(v.get<void*>() == &x);
    }
}

TEST_CASE("S031 Value: BrushId reads via int32_t only — shared payload, distinct tag")
{
    const Value v = Value::BrushId(7);
    // Valid: the shared int32 payload type.
    CHECK(v.get<std::int32_t>() == 7);
    CHECK(v.type() == ValueType::BrushId); // tag is NOT Int32
    // Invalid: every other type throws (incl. int64 — no promotion).
    CHECK_THROWS_AS((void)v.get<bool>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<std::int64_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<float>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<std::string>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<Color>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<Thickness>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<void*>(), ValueTypeMismatch);
    // And Int32 must NOT be readable as BrushId (there is no BrushId C++ type;
    // int32 is the shared payload, verified above — the tag stays distinct).
    const Value i32 = Value::Int32(7);
    CHECK(i32.type() == ValueType::Int32);
    CHECK(i32.get<std::int32_t>() == 7);
    // Distinct tags with equal int32 payloads are NOT equal (S030 contract).
    CHECK_FALSE(v == i32);
}

TEST_CASE("S031 Value: scalar-vs-scalar cross-type reads always throw (no coercion)")
{
    // A Bool value can never be read as any integer, and integer values can
    // never be read across widths — S030 stores exact payloads, no promotion.
    CHECK_THROWS_AS((void)Value::Bool(true).get<std::int32_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)Value::Int32(1).get<bool>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)Value::Int32(1).get<std::int64_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)Value::Int64(1).get<std::int32_t>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)Value::Float(1.0f).get<double>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)Value::Double(1.0).get<float>(), ValueTypeMismatch);
}

TEST_CASE("S031 Value: mismatch leaves the value intact and re-readable")
{
    // Throwing get<T> must be a read-only no-op on state: a value that has a
    // wrong-type read attempted still round-trips its true payload and still
    // equals its original. Proves the throw path never corrupts the union.
    const Value original = Value::String("keep me");
    Value v = original;
    CHECK_THROWS_AS((void)v.get<int>(), ValueTypeMismatch);
    CHECK_THROWS_AS((void)v.get<double>(), ValueTypeMismatch);
    CHECK(v == original);
    CHECK(v.get<std::string>() == "keep me");

    CHECK_THROWS_AS((void)Value::Int32(5).get<std::string>(), ValueTypeMismatch);
    CHECK(Value::Int32(5).get<std::int32_t>() == 5);
}
