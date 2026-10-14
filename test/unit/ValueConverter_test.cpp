// S089 (dep:S088) — Value converter registry + the resolution of the S030
// Color/Thickness MINIMAL REAL markers.
//
// Proves the converter-domain step the S084 attribute-value grammar handed off
// to (S084 DETECTS scalar numeric; S089 CONVERTS + owns the comma-list
// thickness form), and that S089 is the resolution point for the S030
// Color/Thickness markers (their full string-parsing semantics now live on the
// types via parse_color/parse_thickness; the sentinel tags are removed).
//
// S089 scope: the REAL registry + the four built-in conversions + focused
// happy-path coverage so the step is green. The exhaustive error/edge suite +
// the formal unknown-literal error taxonomy are S090's named step (recorded
// NO-GAP, not silent). This file still covers the S089-relevant error cases
// (unknown type -> UnknownType; malformed literal -> MalformedValue) because
// those are part of the registry's real behavior now.
//
// Test plan (Shouldly vocabulary):
//   1. parse_thickness count semantics:
//        "10,20"   -> Thickness{10,20,10,20} (title case, right/bottom MIRRORED)
//        "10"      -> uniform Thickness{10,10,10,10}
//        "1,2,3,4" -> explicit Thickness{1,2,3,4}
//   2. parse_color semantics:
//        "#FF0000"      -> Color{255,0,0,255} (opaque)
//        "#80FF0000"    -> alpha honored -> Color{255,0,0,128}
//        case-insensitive hex; leading '#' optional
//   3. Registry mechanics (custom registry): register+convert returns the right
//      Value; is_registered/size/empty; registered_names insertion order;
//      dedupe throws ValueAlreadyRegistered + registry unchanged.
//   4. Error paths (S089-relevant): unknown type -> ValueConversionException
//      kind()==UnknownType + type_name(); malformed literal -> MalformedValue
//      naming type + literal; Thickness bad count/non-numeric/empty -> throw.
//   5. Marker-resolution proof: register_builtin_converters() then the four
//      headline conversions resolve through the standard global registry.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h" // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>   // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/Color.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h" // ValueConversionError / ValueConversionException
#include "vaml/core/ValueConverter.h"

#include <string>
#include <vector>

using vaml::Color;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueAlreadyRegistered;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::ValueConverterRegistry;
using vaml::parse_color;
using vaml::parse_thickness;
using vaml::register_builtin_converters;
using vaml::test::ShouldThrow;

// ---------------------------------------------------------------------------
// 1. Thickness count semantics (parse_thickness — the real parsing semantics
//    that S089 authors on the type, resolving the S030 marker).
// ---------------------------------------------------------------------------

TEST_CASE("S089: Thickness '10,20' -> 10,20,10,20 (right/bottom mirrored)")
{
    const Thickness t = parse_thickness("10,20");
    SHOULD(ShouldlyString(t).ShouldBe(Thickness{10, 20, 10, 20}));
    SHOULD(ShouldlyString(t.left).ShouldBe(10.0f));
    SHOULD(ShouldlyString(t.top).ShouldBe(20.0f));
    SHOULD(ShouldlyString(t.right).ShouldBe(10.0f));
    SHOULD(ShouldlyString(t.bottom).ShouldBe(20.0f));
}

TEST_CASE("S089: Thickness '10' -> uniform on all four edges")
{
    const Thickness t = parse_thickness("10");
    SHOULD(ShouldlyString(t).ShouldBe(Thickness{10, 10, 10, 10}));
}

TEST_CASE("S089: Thickness '1,2,3,4' -> explicit left,top,right,bottom")
{
    const Thickness t = parse_thickness("1,2,3,4");
    SHOULD(ShouldlyString(t).ShouldBe(Thickness{1, 2, 3, 4}));
}

TEST_CASE("S089: Thickness malformed counts/literals throw MalformedValue")
{
    // 3 values is not a valid WPF thickness count.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_thickness("1,2,3"); }));
    // A non-numeric token rejects the whole literal.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_thickness("10,abc"); }));
    // Empty is malformed.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_thickness(""); }));
}

// ---------------------------------------------------------------------------
// 2. Color (parse_color — the real parsing semantics S089 authors on the type).
// ---------------------------------------------------------------------------

TEST_CASE("S089: Color '#FF0000' -> opaque red Color{255,0,0,255}")
{
    const Color c = parse_color("#FF0000");
    SHOULD(ShouldlyString(c).ShouldBe(Color{255, 0, 0, 255}));
}

TEST_CASE("S089: Color '#80FF0000' honors alpha -> Color{255,0,0,128}")
{
    // 0x80 alpha, 0xFF red, 0x00 green, 0x00 blue -> A,R,G,B byte order.
    const Color c = parse_color("#80FF0000");
    SHOULD(ShouldlyString(c).ShouldBe(Color{255, 0, 0, 128}));
}

TEST_CASE("S089: Color hex parsing is case-insensitive")
{
    SHOULD(ShouldlyString(parse_color("#AaBbCc")).ShouldBe(parse_color("#aabbcc")));
    SHOULD(ShouldlyString(parse_color("#AABBCC")).ShouldBe(Color{0xAA, 0xBB, 0xCC, 255}));
}

TEST_CASE("S089: Color leading '#' is optional")
{
    SHOULD(ShouldlyString(parse_color("FF0000")).ShouldBe(parse_color("#FF0000")));
}

TEST_CASE("S089: Color malformed literals throw MalformedValue")
{
    // Wrong length (5 digits) / non-hex digit / empty.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#FFFFF"); }));
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#GG0000"); }));
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color(""); }));
}

// ---------------------------------------------------------------------------
// 3. Registry mechanics (custom registry)
// ---------------------------------------------------------------------------

TEST_CASE("S089: register + convert returns the right Value; order is insertion")
{
    ValueConverterRegistry r;
    r.register_converter("First", [](std::string_view) -> Value { return Value::Int32(1); });
    r.register_converter("Second", [](std::string_view) -> Value { return Value::Int32(2); });

    SHOULD(ShouldlyString(r.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(r.empty()).ShouldBe(false));
    SHOULD(ShouldlyString(r.is_registered("First")).ShouldBe(true));
    SHOULD(ShouldlyString(r.is_registered("Second")).ShouldBe(true));
    SHOULD(ShouldlyString(r.is_registered("Nope")).ShouldBe(false));

    SHOULD(ShouldlyString(r.convert("First", "ignored")).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(r.convert("Second", "ignored")).ShouldBe(Value::Int32(2)));

    const std::vector<std::string> names = r.registered_names();
    SHOULD(ShouldlyString(names.size()).ShouldBe(2u));
    if (names.size() == 2)
    {
        SHOULD(ShouldlyString(names[0]).ShouldBe(std::string("First")));
        SHOULD(ShouldlyString(names[1]).ShouldBe(std::string("Second")));
    }
}

TEST_CASE("S089: dedupe throws ValueAlreadyRegistered and leaves the registry unchanged")
{
    ValueConverterRegistry r;
    r.register_converter("Color", [](std::string_view) -> Value { return Value::Int32(1); });

    bool threw = false;
    try
    {
        r.register_converter("Color", [](std::string_view) -> Value { return Value::Int32(2); });
    }
    catch (const ValueAlreadyRegistered&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // Registry unchanged: the ORIGINAL converter still runs.
    SHOULD(ShouldlyString(r.convert("Color", "ignored")).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(r.size()).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// 4. Error paths (S089-relevant)
// ---------------------------------------------------------------------------

TEST_CASE("S089: unknown type -> ValueConversionException(UnknownType) naming the type")
{
    ValueConverterRegistry r;

    bool threw = false;
    try
    {
        (void)r.convert("Frobnicator", "anything");
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::UnknownType)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Frobnicator")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)r.convert("Nope", "1"); }));
}

TEST_CASE("S089: malformed literal -> ValueConversionException(MalformedValue) naming type+literal")
{
    ValueConverterRegistry r;
    r.register_converter("Color", [](std::string_view s) -> Value {
        return Value::Color(parse_color(s));
    });

    bool threw = false;
    try
    {
        (void)r.convert("Color", "#GG0000"); // 'G' is not a hex digit
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::MalformedValue)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Color")));
        SHOULD(ShouldlyString(e.literal()).ShouldBe(std::string("#GG0000")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // Missing converter path also throws UnknownType (not MalformedValue).
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)r.convert("Nope", "1"); }));
}

// ---------------------------------------------------------------------------
// 5. Marker-resolution proof (the four headline conversions through the
//    STANDARD global registry path that S091 will consume)
// ---------------------------------------------------------------------------

TEST_CASE("S089: register_builtin_converters then global-path conversions resolve the four types")
{
    register_builtin_converters();
    ValueConverterRegistry& registry = ValueConverterRegistry::global();

    SHOULD(ShouldlyString(registry.is_registered("Int32")).ShouldBe(true));
    SHOULD(ShouldlyString(registry.is_registered("Thickness")).ShouldBe(true));
    SHOULD(ShouldlyString(registry.is_registered("Color")).ShouldBe(true));
    SHOULD(ShouldlyString(registry.is_registered("Bool")).ShouldBe(true));

    // The four headline conversions through the global registry:
    SHOULD(ShouldlyString(registry.convert("Int32", "42")).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(registry.convert("Thickness", "10,20"))
               .ShouldBe(Value::Thickness(Thickness{10, 20, 10, 20})));
    SHOULD(ShouldlyString(registry.convert("Color", "#FF0000"))
               .ShouldBe(Value::Color(Color{255, 0, 0, 255})));
    SHOULD(ShouldlyString(registry.convert("Bool", "true")).ShouldBe(Value::Bool(true)));

    // Idempotent: a second registration is a no-op guard, not a double-throw.
    register_builtin_converters();
    SHOULD(ShouldlyString(registry.convert("Int32", "7")).ShouldBe(Value::Int32(7)));
}
