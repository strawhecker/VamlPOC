// S031 — Value round-trip + type-mismatch-throws tests.
// S031 dep:S030. Scope per roadmap: prove EVERY payload round-trips through
// get<T>() exactly, and prove get<T>() throws vaml::ValueTypeMismatch for any
// non-valid payload type on a given tag — including the discriminators that
// share a payload (Int32/BrushId both read via int32_t; anything else throws).
// This is the complement to S030's Value_test.cpp (factory→tag/equality/copy).

// S036_4: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString + ShouldThrow).

// Main/impl is owned by smoke_test.cpp (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN).
// This TU only declares tests — it must NOT re-define the doctest main.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/Value.h"
#include "vaml/core/Transform.h"  // Matrix2D (S208 additive Value tag)

#include <cstdint>
#include <string>

using vaml::Color;
using vaml::Matrix2D;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::test::ShouldThrow;

// ---------------------------------------------------------------------------
// Step 1 — Round-trip: store-then-read returns the exact payload for every non-
// Null tag, and the re-factored value equals the original (operator== round-trip).
// ---------------------------------------------------------------------------

TEST_CASE("S031 Value: every scalar payload round-trips through get<T> exactly")
{
    // Bool
    {
        const Value v = Value::Bool(true);
        SHOULD(ShouldlyString(v.get<bool>()).ShouldBe(true));
        SHOULD(ShouldlyString(Value::Bool(v.get<bool>())).ShouldBe(v)); // re-factory == original
    }
    // Int32
    {
        const Value v = Value::Int32(-2147483647L - 1L); // INT32_MIN
        SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(-2147483647L - 1L));
        SHOULD(ShouldlyString(Value::Int32(v.get<std::int32_t>())).ShouldBe(v));
    }
    // Int64
    {
        const Value v = Value::Int64(-9007199254740993LL);
        SHOULD(ShouldlyString(v.get<std::int64_t>()).ShouldBe(-9007199254740993LL));
        SHOULD(ShouldlyString(Value::Int64(v.get<std::int64_t>())).ShouldBe(v));
    }
    // Float
    {
        const Value v = Value::Float(3.25f);
        SHOULD(ShouldlyString(v.get<float>()).ShouldBe(3.25f));
        SHOULD(ShouldlyString(Value::Float(v.get<float>())).ShouldBe(v));
    }
    // Double
    {
        const Value v = Value::Double(3.25);
        SHOULD(ShouldlyString(v.get<double>()).ShouldBe(3.25));
        SHOULD(ShouldlyString(Value::Double(v.get<double>())).ShouldBe(v));
    }
}

TEST_CASE("S031 Value: String round-trips by value (copy), not by pointer")
{
    const Value v = Value::String("round trip string");
    const std::string got = v.get<std::string>();
    SHOULD(ShouldlyString(got).ShouldBe(std::string("round trip string")));
    // Mutating the read-back copy must not affect the stored value.
    std::string mutated = v.get<std::string>();
    mutated.push_back('!');
    SHOULD(ShouldlyString(v.get<std::string>()).ShouldBe(std::string("round trip string"))); // stored copy unchanged
    SHOULD(ShouldlyString(Value::String(v.get<std::string>())).ShouldBe(v));
}

TEST_CASE("S031 Value: Color/Thickness/BrushId/UntypedPtr round-trip through get<T>")
{
    // Color
    {
        const Color c{1, 2, 3, 4};
        const Value v = Value::Color(c);
        SHOULD(ShouldlyString(v.get<Color>()).ShouldBe(c));
        SHOULD(ShouldlyString(Value::Color(v.get<Color>())).ShouldBe(v));
    }
    // Thickness
    {
        const Thickness t{1.0f, 2.0f, 3.0f, 4.0f};
        const Value v = Value::Thickness(t);
        SHOULD(ShouldlyString(v.get<Thickness>()).ShouldBe(t));
        SHOULD(ShouldlyString(Value::Thickness(v.get<Thickness>())).ShouldBe(v));
    }
    // Matrix2D — the S208 per-element transform (additive tag).
    {
        const Matrix2D m = Matrix2D::translation(3.0f, -4.0f);
        const Value v = Value::Matrix2D(m);
        SHOULD(ShouldlyString(v.get<Matrix2D>()).ShouldBe(m));
        SHOULD(ShouldlyString(Value::Matrix2D(v.get<Matrix2D>())).ShouldBe(v));
        SHOULD(ShouldlyString(v.type()).ShouldBe(ValueType::Matrix2D));
    }
    // BrushId — int32 payload under a dedicated tag.
    {
        const Value v = Value::BrushId(99);
        SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(99));
        SHOULD(ShouldlyString(Value::BrushId(v.get<std::int32_t>())).ShouldBe(v));
    }
    // UntypedPtr — pointer round-trips by value.
    {
        int x = 0;
        const Value v = Value::UntypedPtr(&x);
        SHOULD(ShouldlyString(v.get<void*>()).ShouldBe(&x));
        SHOULD(ShouldlyString(Value::UntypedPtr(v.get<void*>())).ShouldBe(v));
    }
}

// ---------------------------------------------------------------------------
// Step 2 — Type-mismatch: get<T> throws ValueTypeMismatch when T is not a valid
// payload type for the stored tag. Null has NO payload, so ANY get throws.
// ---------------------------------------------------------------------------

TEST_CASE("S031 Value: Null has no payload — every get<T> throws")
{
    const Value null = Value::Null();
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<bool>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<std::int32_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<std::int64_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<float>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<double>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<std::string>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<Color>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<Thickness>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)null.get<void*>(); }));
    // Gesture of miss must not corrupt — Null stays Null.
    SHOULD(ShouldlyString(null.isNull()).ShouldBe(true));
}

// The valid type for each non-Null tag, and the set of all types that must
// throw against it. Exercised exhaustively so the mismatch path is proven for
// every (tag, type) pair except the valid one(s).
TEST_CASE("S031 Value: every tag throws on every non-valid payload type")
{
    // Bool: only bool reads; everything else throws.
    {
        const Value v = Value::Bool(true);
        SHOULD(ShouldlyString(v.get<bool>()).ShouldBe(true));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<bool>()).ShouldBe(true)); // valid read still succeeds after throws
    }
    // Int32: int32 only (NOT int64/float/double — exact storage, no promotion).
    {
        const Value v = Value::Int32(-42);
        SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(-42));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(-42));
    }
    // Int64.
    {
        const Value v = Value::Int64(-42);
        SHOULD(ShouldlyString(v.get<std::int64_t>()).ShouldBe(-42));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<std::int64_t>()).ShouldBe(-42));
    }
    // Float.
    {
        const Value v = Value::Float(1.5f);
        SHOULD(ShouldlyString(v.get<float>()).ShouldBe(1.5f));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<float>()).ShouldBe(1.5f));
    }
    // Double.
    {
        const Value v = Value::Double(1.5);
        SHOULD(ShouldlyString(v.get<double>()).ShouldBe(1.5));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<double>()).ShouldBe(1.5));
    }
    // String: mismatches every scalar, and is mismatched-by every scalar.
    {
        const Value v = Value::String("abc");
        SHOULD(ShouldlyString(v.get<std::string>()).ShouldBe(std::string("abc")));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<std::string>()).ShouldBe(std::string("abc"))); // stored string intact after throws
    }
    // Color.
    {
        const Value v = Value::Color(Color{1, 2, 3, 4});
        SHOULD(ShouldlyString(v.get<Color>()).ShouldBe(Color{1, 2, 3, 4}));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<Color>()).ShouldBe(Color{1, 2, 3, 4}));
    }
    // Thickness.
    {
        const Value v = Value::Thickness(Thickness{1, 2, 3, 4});
        SHOULD(ShouldlyString(v.get<Thickness>()).ShouldBe(Thickness{1, 2, 3, 4}));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
        SHOULD(ShouldlyString(v.get<Thickness>()).ShouldBe(Thickness{1, 2, 3, 4}));
    }
    // UntypedPtr: only void* reads.
    {
        int x = 0;
        const Value v = Value::UntypedPtr(&x);
        SHOULD(ShouldlyString(v.get<void*>()).ShouldBe(&x));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int32_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
        SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
        SHOULD(ShouldlyString(v.get<void*>()).ShouldBe(&x));
    }
}

TEST_CASE("S031 Value: BrushId reads via int32_t only — shared payload, distinct tag")
{
    const Value v = Value::BrushId(7);
    // Valid: the shared int32 payload type.
    SHOULD(ShouldlyString(v.get<std::int32_t>()).ShouldBe(7));
    SHOULD(ShouldlyString(v.type()).ShouldBe(ValueType::BrushId)); // tag is NOT Int32
    // Invalid: every other type throws (incl. int64 — no promotion).
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<bool>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::int64_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<float>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<std::string>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Color>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<Thickness>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<void*>(); }));
    // And Int32 must NOT be readable as BrushId (there is no BrushId C++ type;
    // int32 is the shared payload, verified above — the tag stays distinct).
    const Value i32 = Value::Int32(7);
    SHOULD(ShouldlyString(i32.type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(i32.get<std::int32_t>()).ShouldBe(7));
    // Distinct tags with equal int32 payloads are NOT equal (S030 contract).
    SHOULD(ShouldlyString(v).ShouldNotBe(i32));
}

TEST_CASE("S031 Value: scalar-vs-scalar cross-type reads always throw (no coercion)")
{
    // A Bool value can never be read as any integer, and integer values can
    // never be read across widths — S030 stores exact payloads, no promotion.
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Bool(true).get<std::int32_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Int32(1).get<bool>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Int32(1).get<std::int64_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Int64(1).get<std::int32_t>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Float(1.0f).get<double>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Double(1.0).get<float>(); }));
}

TEST_CASE("S031 Value: mismatch leaves the value intact and re-readable")
{
    // Throwing get<T> must be a read-only no-op on state: a value that has a
    // wrong-type read attempted still round-trips its true payload and still
    // equals its original. Proves the throw path never corrupts the union.
    const Value original = Value::String("keep me");
    Value v = original;
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<int>(); }));
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)v.get<double>(); }));
    SHOULD(ShouldlyString(v).ShouldBe(original));
    SHOULD(ShouldlyString(v.get<std::string>()).ShouldBe(std::string("keep me")));

    SHOULD(ShouldThrow<ValueTypeMismatch>([&] { (void)Value::Int32(5).get<std::string>(); }));
    SHOULD(ShouldlyString(Value::Int32(5).get<std::int32_t>()).ShouldBe(5));
}
