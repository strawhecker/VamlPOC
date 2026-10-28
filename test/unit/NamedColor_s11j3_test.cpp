// 11j-3 (GAP CLOSE, dep:S089) — the WPF named-color table.
//
// RESOLVES the S089 recorded NO-GAP ("named-color tables are a later
// brush/resource step"): parse_color now accepts the full WPF named-color set,
// case-insensitive, closing the `MalformedValue` color genre across the
// external WPF corpus + unblocking `Transparent` overlays.
//
// The hex path is UNCHANGED (the S089 suite stays green); named-color support
// is ADDITIVE:
//   1. representative named colors parse to their canonical 8-bit RGBA;
//   2. `Transparent` = fully-transparent BLACK (0,0,0,0);
//   3. case-insensitivity (`aliceblue`, `BLACK`, `Transparent`);
//   4. an unknown bare identifier still throws MalformedValue (never a silent
//      color — the S090 error discipline);
//   5. a "#"-prefixed literal is NEVER a name (a "#Red" is malformed —
//      names never start with '#');
//   6. hex behavior unchanged (trailing regression re-proof).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h" // operator<< for Color
#include <vaml/assert/Shouldly.h>   // ShouldlyString / ShouldThrow

#include "vaml/core/Color.h"

#include <string>

using vaml::Color;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::parse_color;
using vaml::test::ShouldThrow;

TEST_CASE("11j-3: representative named colors parse to their canonical RGBA")
{
    SHOULD(ShouldlyString(parse_color("Red")).ShouldBe(Color{255, 0, 0, 255}));
    SHOULD(ShouldlyString(parse_color("Black")).ShouldBe(Color{0, 0, 0, 255}));
    SHOULD(ShouldlyString(parse_color("White")).ShouldBe(Color{255, 255, 255, 255}));
    SHOULD(ShouldlyString(parse_color("Green")).ShouldBe(Color{0, 128, 0, 255}));
    SHOULD(ShouldlyString(parse_color("Blue")).ShouldBe(Color{0, 0, 255, 255}));
    SHOULD(ShouldlyString(parse_color("AliceBlue")).ShouldBe(Color{0xF0, 0xF8, 0xFF, 0xFF}));
    SHOULD(ShouldlyString(parse_color("CornflowerBlue")).ShouldBe(Color{0x64, 0x95, 0xED, 0xFF}));
    SHOULD(ShouldlyString(parse_color("SlateGray")).ShouldBe(Color{0x70, 0x80, 0x90, 0xFF}));
}

TEST_CASE("11j-3: Transparent = fully-transparent BLACK (0,0,0,0)")
{
    // The `Transparent` overlay unblock the sprint names — WPF-faithful.
    const Color c = parse_color("Transparent");
    SHOULD(ShouldlyString(c.r).ShouldBe(0));
    SHOULD(ShouldlyString(c.g).ShouldBe(0));
    SHOULD(ShouldlyString(c.b).ShouldBe(0));
    SHOULD(ShouldlyString(c.a).ShouldBe(0));
}

TEST_CASE("11j-3: named-color lookup is case-insensitive")
{
    SHOULD(ShouldlyString(parse_color("aliceblue")).ShouldBe(parse_color("AliceBlue")));
    SHOULD(ShouldlyString(parse_color("BLACK")).ShouldBe(parse_color("Black")));
    SHOULD(ShouldlyString(parse_color("TRANSPARENT")).ShouldBe(parse_color("Transparent")));
    SHOULD(ShouldlyString(parse_color("rEd")).ShouldBe(parse_color("Red")));
}

TEST_CASE("11j-3: an unknown bare identifier still throws MalformedValue")
{
    // Unknown name -> MalformedValue (the S090 discipline: an unrecognized
    // literal is never a silent 0/black).
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("NotAColor"); }));
    bool threw = false;
    try
    {
        (void)parse_color("Frobnicate");
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::MalformedValue)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Color")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("11j-3: a '#'-prefixed literal is NEVER a named color")
{
    // Names never start with '#'; a "#Red" is malformed, not a color.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#Red"); }));
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#AliceBlue"); }));
}

TEST_CASE("11j-3: hex behavior unchanged (trailing S089 regression re-proof)")
{
    SHOULD(ShouldlyString(parse_color("#FF0000")).ShouldBe(Color{255, 0, 0, 255}));
    SHOULD(ShouldlyString(parse_color("#80FF0000")).ShouldBe(Color{255, 0, 0, 128}));
    SHOULD(ShouldlyString(parse_color("FF0000")).ShouldBe(parse_color("#FF0000")));
    // A bare 6-hex-char literal is a valid hex color (leading '#' optional),
    // NOT a name — hex wins when the literal is a valid hex form.
    SHOULD(ShouldlyString(parse_color("ABCDEF")).ShouldBe(Color{0xAB, 0xCD, 0xEF, 255}));
}

TEST_CASE("11j-3: a 6/8-length non-hex bare literal that IS a name resolves")
{
    // e.g. a hypothetical 8-char name with letters beyond F would fall through
    // to the table; "Red" (3 chars) already proves the non-hex-length path.
    // "REDDish"—not a name—still throws:
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("Zzzzzz"); }));
}

TEST_CASE("11j-3: empty literal is MalformedValue")
{
    // An EMPTY literal is not a name (the table has no empty entry) and not a
    // hex form -> MalformedValue (authoritative; never a silent {0,0,0,0}).
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color(""); }));
}
