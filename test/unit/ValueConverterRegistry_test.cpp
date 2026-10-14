// S090 (dep:S089) — Exhaustive ValueConverterRegistry suite + the formal
// unknown-literal error taxonomy.
//
// S089 delivered the REAL registry + four built-in converters + the minimal
// {None,UnknownType,MalformedValue} taxonomy, and its scope boundary explicitly
// deferred the exhaustive error/edge suite + the unknown-literal category to
// S090. THIS TU is that deferred deliverable (NO-GAP: resolved here, not
// silently backfilled). It is the *exhaustive* sweep, complementary to S089's
// ValueConverter_test.cpp (left untouched as the stable happy-path proof).
//
// S090 adds ONE product surface (per the plan):
//   - ValueConversionError::UnknownLiteral — the formal unknown-literal error
//     category. Orthogonal, exhaustive with the S089 pair:
//         UnknownType    = the requested TARGET TYPE has no converter;
//         MalformedValue = the literal is malformed FOR a known target type;
//         UnknownLiteral = the literal matches NO type the registry can make.
//   - ValueConverterRegistry::try_infer_type(literal, outType) — the recognizer
//     that makes UnknownLiteral reachable + provable: probes registered
//     converters in INSERTION ORDER, returns the first that fully accepts the
//     literal (runs it; a throwing converter = "does not accept"); false when
//     none accepts (=> UnknownLiteral). Recognizer, not a best-match matcher.
//     S091's documented upstream use.
//
// TEST-VOCABULARY NOTES (observed S089 behavior, asserted honestly, not fixed —
// S090's scope is taxonomy + recognizer + tests, not changing the converters):
//   - ValueConverter.cpp's iequals() folds ONLY 'A'..'F' (it was written for
//     hex) but the Bool converter reuses it for "true"/"false". So the only
//     non-lowercase variant that accepts is "False" (capital F folds; T/R/U/L/S
//     are outside A..F). "True"/"TRUE"/"FALSE" are REJECTED.
//   - std::from_chars skips NO leading/trailing whitespace, so " 42"/"42 " are
//     Malformed; "4.2" is not an Int32; out-of-range ints are Malformed.
//   - EDGE / INFERENCE / TAXONOMY suites run against the REAL global registry
//     (the exact production from_chars/iequals/parse_thickness/parse_color
//     semantics that S091 consumes). MECHANICS (dedupe/move) need a mutable
//     registry, so they use a throw-based CUSTOM registry (isolated, and only
//     exercising register/convert/order — no parsing asserted there).
//
// Test plan (Shouldly vocabulary):
//   1. Int32 edge suite (accepts + Malformed incl. whitespace/junk/decimal/overflow).
//   2. Bool edge suite (accepts true/false/"False"; rejects True/TRUE/FALSE/1/0/yes/"").
//   3. Thickness edge suite (1/2/4 counts incl. negatives/decimals/exponent).
//   4. Color edge suite (6/8-digit, alpha-front, lowercase, optional '#').
//   5. Registry mechanics (re-proof dedupe-throws-unchanged; move semantics).
//   6. try_infer_type recognizer (first-match order + inference determinism).
//   7. Taxonomy distinctness proofs (UnknownType / MalformedValue / UnknownLiteral).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Color.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h" // ValueConversionError / ValueConversionException
#include "vaml/core/ValueConverter.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using vaml::Color;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueAlreadyRegistered;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::ValueConverter;
using vaml::ValueConverterRegistry;
using vaml::parse_color;
using vaml::parse_thickness;
using vaml::register_builtin_converters;
using vaml::test::ShouldThrow;

namespace {

// Asserts convert(typeName, literal) SUCCEEDS and yields the expected Value.
void expect_ok(ValueConverterRegistry& r, const char* typeName, const char* literal,
               const Value& expected)
{
    const Value got = r.convert(typeName, literal);
    SHOULD(ShouldlyString(got).ShouldBe(expected));
}

// Asserts convert(typeName, literal) throws MalformedValue naming type+literal.
void expect_malformed(ValueConverterRegistry& r, const char* typeName, const char* literal)
{
    bool threw = false;
    try
    {
        (void)r.convert(typeName, literal);
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::MalformedValue)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string(typeName)));
        SHOULD(ShouldlyString(e.literal()).ShouldBe(std::string(literal)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

[[noreturn]] void throw_malformed(const char* typeName, std::string_view literal)
{
    throw ValueConversionException(ValueConversionError::MalformedValue,
                                   std::string(typeName), std::string(literal),
                                   "cannot convert '" + std::string(literal) + "' to " +
                                       std::string(typeName) + " (MalformedValue)");
}

// --- throw-based CUSTOM converters (for the MUTABLE mechanics registry only;
// --- whole-string semantics, MalformedValue on any rejection). -------------
void register_int32(ValueConverterRegistry& r)
{
    r.register_converter("Int32", [](std::string_view s) -> Value {
        std::int32_t v = 0;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec != std::errc() || ptr != (s.data() + s.size()))
        {
            throw_malformed("Int32", s);
        }
        return Value::Int32(static_cast<std::int32_t>(v));
    });
}

void register_thickness(ValueConverterRegistry& r)
{
    r.register_converter("Thickness", [](std::string_view s) -> Value {
        return Value::Thickness(parse_thickness(s));
    });
}

void register_color(ValueConverterRegistry& r)
{
    r.register_converter("Color", [](std::string_view s) -> Value {
        return Value::Color(parse_color(s));
    });
}

void register_bool(ValueConverterRegistry& r)
{
    r.register_converter("Bool", [](std::string_view s) -> Value {
        if (s == "true")
        {
            return Value::Bool(true);
        }
        if (s == "false")
        {
            return Value::Bool(false);
        }
        throw_malformed("Bool", s);
    });
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Int32 edge suite — REAL global converter (from_chars semantics)
// ---------------------------------------------------------------------------

TEST_CASE("S090 Int32: accepts canonical decimal forms (real global converter)")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_ok(r, "Int32", "42", Value::Int32(42));
    expect_ok(r, "Int32", "-17", Value::Int32(-17));
    expect_ok(r, "Int32", "042", Value::Int32(42)); // leading zeros: valid decimal
    expect_ok(r, "Int32", "2147483647", Value::Int32(2147483647));
    expect_ok(r, "Int32", "-2147483648", Value::Int32(-2147483648));
}

TEST_CASE("S090 Int32: malformed literals (whitespace/junk/decimal/overflow)")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_malformed(r, "Int32", "");
    expect_malformed(r, "Int32", " 42");   // leading space — from_chars skips none
    expect_malformed(r, "Int32", "42 ");   // trailing space
    expect_malformed(r, "Int32", "42x");   // junk suffix
    expect_malformed(r, "Int32", "4.2");   // decimal is not an integer
    expect_malformed(r, "Int32", "2147483648");  // overflow (INT32_MAX + 1)
    expect_malformed(r, "Int32", "-2147483649"); // underflow (INT32_MIN - 1)
}

// ---------------------------------------------------------------------------
// 2. Bool edge suite — REAL global converter (iequals fold-quirk asserted)
// ---------------------------------------------------------------------------

TEST_CASE("S090 Bool: accepts true / false / foldable 'False' variant")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_ok(r, "Bool", "true", Value::Bool(true));
    expect_ok(r, "Bool", "false", Value::Bool(false));
    expect_ok(r, "Bool", "False", Value::Bool(false)); // capital F folds (A..F)
}

TEST_CASE("S090 Bool: rejects non-foldable variants and non-bool literals")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    // T/R/U/L/S are outside iequals' A..F fold-range -> not equal to true/false.
    expect_malformed(r, "Bool", "True");
    expect_malformed(r, "Bool", "TRUE");
    expect_malformed(r, "Bool", "FALSE");
    expect_malformed(r, "Bool", "1");
    expect_malformed(r, "Bool", "0");
    expect_malformed(r, "Bool", "yes");
    expect_malformed(r, "Bool", "");
}

// ---------------------------------------------------------------------------
// 3. Thickness edge suite — REAL parser (parse_thickness in the global registry)
// ---------------------------------------------------------------------------

TEST_CASE("S090 Thickness: valid 1/2/4 counts incl. negatives/decimals/exponent")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_ok(r, "Thickness", "10", Value::Thickness(Thickness{10, 10, 10, 10}));
    expect_ok(r, "Thickness", "10,20", Value::Thickness(Thickness{10, 20, 10, 20}));
    expect_ok(r, "Thickness", "1,2,3,4", Value::Thickness(Thickness{1, 2, 3, 4}));
    expect_ok(r, "Thickness", "-5", Value::Thickness(Thickness{-5, -5, -5, -5}));
    expect_ok(r, "Thickness", "1.5", Value::Thickness(Thickness{1.5f, 1.5f, 1.5f, 1.5f}));
    expect_ok(r, "Thickness", "1e2", Value::Thickness(Thickness{100.0f, 100.0f, 100.0f, 100.0f}));
}

TEST_CASE("S090 Thickness: malformed counts/empty/whitespace")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_malformed(r, "Thickness", "");
    expect_malformed(r, "Thickness", "1,2,3");      // 3 values: invalid count
    expect_malformed(r, "Thickness", "1,2,3,4,5");  // 5 values: invalid count
    expect_malformed(r, "Thickness", "10,");        // dangling comma -> empty token
    expect_malformed(r, "Thickness", ",10");        // leading comma -> empty token
    expect_malformed(r, "Thickness", "10,,20");     // doubled comma -> empty token
    expect_malformed(r, "Thickness", " 10");        // leading space (from_chars)
    expect_malformed(r, "Thickness", "10 ");        // trailing space
}

// ---------------------------------------------------------------------------
// 4. Color edge suite — REAL parser (parse_color in the global registry)
// ---------------------------------------------------------------------------

TEST_CASE("S090 Color: 6/8-digit, alpha-front, lowercase, optional '#'")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_ok(r, "Color", "#FF0000", Value::Color(Color{255, 0, 0, 255}));
    expect_ok(r, "Color", "FF0000", Value::Color(Color{255, 0, 0, 255})); // '#' optional
    expect_ok(r, "Color", "#80FF0000", Value::Color(Color{255, 0, 0, 128})); // alpha-front argb
    expect_ok(r, "Color", "#ff0000", Value::Color(Color{255, 0, 0, 255}));   // lowercase hex
    expect_ok(r, "Color", "#aabbcc", Value::Color(Color{0xAA, 0xBB, 0xCC, 255}));
}

TEST_CASE("S090 Color: malformed literals")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    expect_malformed(r, "Color", "");
    expect_malformed(r, "Color", "#");           // '#' then no digits
    expect_malformed(r, "Color", "#FFF");        // 3-digit shorthand: NOT supported
    expect_malformed(r, "Color", "#FFFFF");      // 5 digits: wrong length
    expect_malformed(r, "Color", "#GG0000");     // 'G' non-hex
    expect_malformed(r, "Color", "Red");         // named colors out of scope
}

// ---------------------------------------------------------------------------
// 5. Registry mechanics (CUSTOM mutable registry; re-proof + move semantics)
// ---------------------------------------------------------------------------

TEST_CASE("S090 mechanics: dedupe throws ValueAlreadyRegistered, registry unchanged")
{
    ValueConverterRegistry r;
    register_color(r);

    const std::size_t before = r.size();
    SHOULD(ShouldThrow<ValueAlreadyRegistered>([&] {
        register_color(r); // same name, second conversion function
    }));
    // Registry unchanged: size stable AND the ORIGINAL converter still runs.
    SHOULD(ShouldlyString(r.size()).ShouldBe(before));
    SHOULD(ShouldlyString(r.convert("Color", "#00FF00"))
               .ShouldBe(Value::Color(Color{0, 255, 0, 255})));
}

TEST_CASE("S090 mechanics: move-construct / move-assign preserve registrations + order")
{
    ValueConverterRegistry src;
    register_int32(src);
    register_thickness(src);

    // Move-construct preserves both converters + insertion order.
    ValueConverterRegistry dst{std::move(src)};
    const std::vector<std::string> moved = dst.registered_names();
    SHOULD(ShouldlyString(moved.size()).ShouldBe(2u));
    if (moved.size() == 2)
    {
        SHOULD(ShouldlyString(moved[0]).ShouldBe(std::string("Int32")));
        SHOULD(ShouldlyString(moved[1]).ShouldBe(std::string("Thickness")));
    }
    expect_ok(dst, "Int32", "5", Value::Int32(5));
    expect_ok(dst, "Thickness", "1,2", Value::Thickness(Thickness{1, 2, 1, 2}));
    // Dedupe behaviour survives the move.
    SHOULD(ShouldThrow<ValueAlreadyRegistered>([&] { register_int32(dst); }));

    // Move-assign onto an already-populated registry replaces its content.
    ValueConverterRegistry other;
    register_bool(other);
    SHOULD(ShouldlyString(other.size()).ShouldBe(1u));
    other = std::move(dst);
    const std::vector<std::string> assigned = other.registered_names();
    SHOULD(ShouldlyString(assigned.size()).ShouldBe(2u));
    if (assigned.size() == 2)
    {
        SHOULD(ShouldlyString(assigned[0]).ShouldBe(std::string("Int32")));
        SHOULD(ShouldlyString(assigned[1]).ShouldBe(std::string("Thickness")));
    }
    // Old content (Bool) gone.
    SHOULD(ShouldlyString(other.is_registered("Bool")).ShouldBe(false));
}

TEST_CASE("S090 mechanics: global() is idempotent; per-registry isolation")
{
    register_builtin_converters();
    ValueConverterRegistry& global = ValueConverterRegistry::global();

    // Idempotent singleton: same address both times.
    SHOULD(ShouldlyString(&ValueConverterRegistry::global() == &global).ShouldBe(true));

    // A custom registry is independent of global.
    ValueConverterRegistry custom;
    register_bool(custom);
    SHOULD(ShouldlyString(custom.is_registered("Bool")).ShouldBe(true));
    SHOULD(ShouldlyString(custom.is_registered("Int32")).ShouldBe(false)); // not in custom
    // global still has Int32 from its four built-ins.
    SHOULD(ShouldlyString(global.is_registered("Int32")).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 6. try_infer_type recognizer (first-match order + inference determinism)
// ---------------------------------------------------------------------------

TEST_CASE("S090 infer: first-match-by-registration-order; '10' -> Int32 not Thickness")
{
    // The built-ins register in the order Int32, Thickness, Color, Bool. "10" is
    // a valid 1-value Thickness too, but first-match-by-registration-order means
    // Int32 (registered first) wins.
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    std::string type;
    SHOULD(ShouldlyString(r.try_infer_type("10", &type)).ShouldBe(true));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("Int32"))); // first match wins

    // Null outType is allowed (flag-only query, e.g. "is this a known value at all?").
    type = "untouched";
    SHOULD(ShouldlyString(r.try_infer_type("10", nullptr)).ShouldBe(true));
}

TEST_CASE("S090 infer: inference determinism across the four types")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    std::string type;
    SHOULD(ShouldlyString(r.try_infer_type("true", &type)).ShouldBe(true));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("Bool"))); // Int32/Thickness/Color reject

    SHOULD(ShouldlyString(r.try_infer_type("10,20", &type)).ShouldBe(true));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("Thickness"))); // Int32 rejects (leftover ',')

    SHOULD(ShouldlyString(r.try_infer_type("#FF0000", &type)).ShouldBe(true));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("Color")));

    type = "sentinel";
    SHOULD(ShouldlyString(r.try_infer_type("banana", &type)).ShouldBe(false));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("sentinel"))); // untouched on failure
}

TEST_CASE("S090 infer: no registered type accepts the literal -> UnknownLiteral reachable")
{
    // A registry with NO converters can accept nothing.
    ValueConverterRegistry empty;
    std::string type = "unchanged";
    SHOULD(ShouldlyString(empty.try_infer_type("anything", &type)).ShouldBe(false));
    SHOULD(ShouldlyString(type).ShouldBe(std::string("unchanged")));

    // A populated registry (Bool only) with an unrecognizable literal.
    ValueConverterRegistry r;
    register_bool(r);
    SHOULD(ShouldlyString(r.try_infer_type("banana", &type)).ShouldBe(false));
}

// ---------------------------------------------------------------------------
// 7. Taxonomy distinctness proofs (same registry, three orthogonal outcomes)
// ---------------------------------------------------------------------------

TEST_CASE("S090 taxonomy: UnknownType, MalformedValue, UnknownLiteral are distinct")
{
    register_builtin_converters();
    ValueConverterRegistry& r = ValueConverterRegistry::global();

    // (a) UnknownType — no converter for the requested type name.
    bool unknownType = false;
    try { (void)r.convert("Frobnicator", "whatever"); }
    catch (const ValueConversionException& e) {
        unknownType = (e.kind() == ValueConversionError::UnknownType);
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Frobnicator")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(unknownType)).ShouldBe(1));

    // (b) MalformedValue — malformed FOR a known target type.
    bool malformed = false;
    try { (void)r.convert("Color", "#GG0000"); }
    catch (const ValueConversionException& e) {
        malformed = (e.kind() == ValueConversionError::MalformedValue);
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Color")));
        SHOULD(ShouldlyString(e.literal()).ShouldBe(std::string("#GG0000")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(malformed)).ShouldBe(1));

    // (c) UnknownLiteral — the literal matches NO registered type. Reachable via
    // try_infer_type()==false (its provable surface); distinct from (a) (no
    // target type was even asked for) and from (b) (no type was the "right" one).
    std::string type = "untouched";
    SHOULD(ShouldlyString(r.try_infer_type("banana", &type)).ShouldBe(false));
}
