// S602 (dep:S601) — COLOR FIDELITY root A3. parse_color now accepts the WPF
// `#RGB`/`#ARGB` shorthand (3/4-digit), which WPF ColorConverter accepts but
// vaml's parse_color (S089/11j-3) previously rejected (6/8-digit only). The
// earlier `#AAA` audit case (MalformedValue) now resolves.
//
// WPF-faithful (verified against the WPF source — Parsers.ParseHexColor,
// Parsers.cs:37):
//   - `#RGB`: each of R,G,B is a nibble expanded to a byte (`v*16+v`), alpha=255
//     (`#AAA` -> `#AAAAAA`; `#F80` -> `#FF8800`).
//   - `#ARGB` (4-digit): **alpha FIRST** — a=char1, r=char2, g=char3, b=char4,
//     each nibble `v*16+v` (NOT #RGBA; the earlier draft's #RGBA assumption was
//     corrected against ParseHexColor). `#1a2b` -> A=0x11,R=0x22,G=0x33,B=0x44.
//
// Test plan (Shouldly vocabulary):
//   1. #RGB (3-digit) expands each nibble, alpha=255: #AAA->#AAAAAA, #F80->#FF8800,
//      #000->#000000, #fff->#FFFFFF.
//   2. #ARGB (4-digit) alpha-first expansion: #1a2b->A=11,R=22,G=33,B=44.
//   3. 6/8-digit + named forms UNCHANGED (regression re-proof).
//   4. Unknown "#"-prefixed / malformed still throws MalformedValue.

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

// ---------------------------------------------------------------------------
// 1. #RGB (3-digit) — each nibble expands to a byte; alpha defaults opaque.
// ---------------------------------------------------------------------------
TEST_CASE("S602: #RGB shorthand expands each nibble, alpha opaque")
{
    // #AAA -> R=0xAA,G=0xAA,B=0xAA (the audit's named failure case now resolves).
    SHOULD(ShouldlyString(parse_color("#AAA")).ShouldBe(Color{0xAA, 0xAA, 0xAA, 0xFF}));
    // #F80 -> R=0xFF,G=0x88,B=0x00.
    SHOULD(ShouldlyString(parse_color("#F80")).ShouldBe(Color{0xFF, 0x88, 0x00, 0xFF}));
    // #000 / #fff.
    SHOULD(ShouldlyString(parse_color("#000")).ShouldBe(Color{0x00, 0x00, 0x00, 0xFF}));
    SHOULD(ShouldlyString(parse_color("#fff")).ShouldBe(Color{0xFF, 0xFF, 0xFF, 0xFF}));
    // Case-insensitive nibbles.
    SHOULD(ShouldlyString(parse_color("#abc")).ShouldBe(Color{0xAA, 0xBB, 0xCC, 0xFF}));
    SHOULD(ShouldlyString(parse_color("#ABC")).ShouldBe(Color{0xAA, 0xBB, 0xCC, 0xFF}));
}

// ---------------------------------------------------------------------------
// 2. #ARGB (4-digit) — ALPHA FIRST, each nibble expands.
// ---------------------------------------------------------------------------
TEST_CASE("S602: #ARGB 4-digit shorthand is alpha-FIRST (WPF ParseHexColor)")
{
    // #1a2b -> A=0x11, R=0x22, G=0x33, B=0x44 (each nibble v*16+v), in #ARGB order.
    SHOULD(ShouldlyString(parse_color("#1234")).ShouldBe(Color{0x22, 0x33, 0x44, 0x11}));
    // #F800 -> A=0xFF, R=0x88, G=0x00, B=0x00.
    SHOULD(ShouldlyString(parse_color("#F800")).ShouldBe(Color{0x88, 0x00, 0x00, 0xFF}));
    // #00FF -> A=0x00, R=0x00, G=0xFF, B=0xFF.
    SHOULD(ShouldlyString(parse_color("#00FF")).ShouldBe(Color{0x00, 0xFF, 0xFF, 0x00}));
}

// ---------------------------------------------------------------------------
// 3. 6/8-digit + named forms UNCHANGED (regression re-proof).
// ---------------------------------------------------------------------------
TEST_CASE("S602: 6/8-digit and named colors unchanged (regression re-proof)")
{
    SHOULD(ShouldlyString(parse_color("#FF0000")).ShouldBe(Color{255, 0, 0, 255}));
    SHOULD(ShouldlyString(parse_color("#12345678")).ShouldBe(Color{0x34, 0x56, 0x78, 0x12}));
    SHOULD(ShouldlyString(parse_color("Red")).ShouldBe(Color{255, 0, 0, 255}));
    SHOULD(ShouldlyString(parse_color("Transparent")).ShouldBe(Color{0xFF, 0xFF, 0xFF, 0x00}));
}

// ---------------------------------------------------------------------------
// 4. Malformed still throws MalformedValue.
// ---------------------------------------------------------------------------
TEST_CASE("S602: malformed color literals still throw MalformedValue")
{
    // A "#"-prefixed literal is never a name.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#Red"); }));
    // 2-digit / 5-digit / 7-digit are not valid hex forms.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#12"); }));
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#12345"); }));
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("#1234567"); }));
    // An unknown bare identifier.
    SHOULD(ShouldThrow<ValueConversionException>([&] { (void)parse_color("NotAColor"); }));
}
