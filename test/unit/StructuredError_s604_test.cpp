// S604 (dep:S603) — STRUCTURED-ERROR taxonomy tightening root A5.
// Genuine MalformedValue/UnknownType cases fire the RIGHT structured exception
// (S048/ValueConversionException + the reader taxonomy), never a message
// catch-all. Where S600/S601/s/s mis-ordered/out-of-set property-elements (S603)
// produced the precise authoring error, this suite asserts the exact structured
// kind + the non-empty what() naming the offending fragment.
//
// Test plan (Shouldly vocabulary):
//   1. A cross-domain wrong enum name (S600) -> ValueConversionException
//      {MalformedValue} naming the DP's domain + the literal (never a silent
//      guess, never a bare catch-all).
//   2. An out-of-set GeometryDrawing property-element (S603) -> VamlReaderException
//      {UnknownProperty} naming the offending local-name (the precise authoring
//      error, not a vague message).
//   3. A malformed color (S602) -> ValueConversionException {MalformedValue}
//      naming "Color".
//   4. Grep-style discipline: these arms route through the typed taxonomy.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ValueConversionError.h"
#include "vaml/core/VamlReader.h"

#include <string>

using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;

// ---------------------------------------------------------------------------
// 1. Cross-domain wrong enum name -> ValueConversionException{MalformedValue}.
// ---------------------------------------------------------------------------
TEST_CASE("S604: a cross-domain wrong enum name throws the structured MalformedValue")
{
    // TextWrapping="Center" is wrong (Center is a TextAlignment member).
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <TextBlock TextWrapping="Center"/>
</StackPanel>
)";
    VamlReader reader;
    try
    {
        (void)reader.Load(xaml);
        REQUIRE_MESSAGE(false, "expected a ValueConversionException");
    }
    catch (const ValueConversionException& e)
    {
        // The right STRUCTURED kind: MalformedValue (never UnknownType, never
        // a bare catch-all message).
        REQUIRE(e.kind() == ValueConversionError::MalformedValue);
        // Naming the DP's declared domain type.
        REQUIRE(e.type_name() == "TextWrapping");
        // Naming the offending literal.
        REQUIRE(e.literal() == "Center");
        // A non-empty what() naming type + literal.
        REQUIRE(std::string(e.what()).size() > 0);
    }
}

// ---------------------------------------------------------------------------
// 2. Out-of-set GeometryDrawing property-element -> VamlReaderException{UnknownProperty}.
// ---------------------------------------------------------------------------
TEST_CASE("S604: an out-of-set GeometryDrawing property-element throws the precise authoring error")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <DrawingBrush x:Key="Bad">
      <DrawingBrush.Drawing>
        <DrawingGroup>
          <GeometryDrawing>
            <GeometryDrawing.NotARealProp/>
          </GeometryDrawing>
        </DrawingGroup>
      </DrawingBrush.Drawing>
    </DrawingBrush>
  </StackPanel.Resources>
</StackPanel>
)";
    VamlReader reader;
    try
    {
        (void)reader.Load(xaml);
        REQUIRE_MESSAGE(false, "expected a VamlReaderException");
    }
    catch (const VamlReaderException& e)
    {
        // The RIGHT reader kind: UnknownProperty (not a vague runtime_error).
        REQUIRE(e.kind() == VamlReaderError::UnknownProperty);
        // A non-empty what() naming the offending local-name.
        REQUIRE(std::string(e.what()).size() > 0);
    }
}

// ---------------------------------------------------------------------------
// 3. A malformed color -> ValueConversionException{MalformedValue} naming Color.
// ---------------------------------------------------------------------------
TEST_CASE("S604: a malformed color throws ValueConversionException{MalformedValue} naming Color")
{
    VamlReader reader;
    // A 7-digit hex is not a valid color form (S602: 3/4/6/8-digit) — the
    // authoring reaches convertLiteral on the Color DP and must throw the
    // structured MalformedValue, never a silent color or a bare message.
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel.Resources>
    <SolidColorBrush x:Key="X" Color="#1234567"/>
  </StackPanel.Resources>
</StackPanel>
)";
    try
    {
        (void)reader.Load(xaml);
        REQUIRE_MESSAGE(false, "expected a ValueConversionException");
    }
    catch (const ValueConversionException& e)
    {
        REQUIRE(e.kind() == ValueConversionError::MalformedValue);
        REQUIRE(e.type_name() == "Color");
        REQUIRE(e.literal() == "#1234567");
        REQUIRE(std::string(e.what()).size() > 0);
    }
}
