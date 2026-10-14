// S094 (dep:S093) — VamlReader error-taxonomy CONSOLIDATION GATE.
//
// The four title error cases are already structured + reachable through one
// realistic reader path; S094 proves ALL of them on that path + resolves the
// S092-recorded x:Class-on-non-root hand-off with a new reader error:
//
//   A. UNKNOWN ATTR     -> VamlReaderException   (VamlReaderError::UnknownProperty)
//   B. UNKNOWN ELEMENT  -> ElementResolutionException (ElementResolutionError::UnknownType)
//   C. BAD TYPE NAME    -> ElementResolutionException (ElementResolutionError::UnknownType)
//   D. MALFORMED VALUE  -> ValueConversionException   (ValueConversionError::MalformedValue)
//   E. (S094-new) x:Class on a NON-ROOT -> VamlReaderException (ClassOnNonRoot)
//   F. taxonomy DISTINCTNESS — the three exception families stay structured +
//      distinct (kind() is inspected, never a message catch-all), so a real
//      error is never misreported as another genre.
//   G. control — a well-formed doc with all four HAPPY paths present loads
//      clean (the taxonomy gate is not an all-errors gate).
//
// Product change authored at THIS step (not a silent backfill): build() now
// carries a `depth` parameter and throws ClassOnNonRoot for an x:Class
// directive at depth != 0 (root-only, WPF-faithful — the S092-recorded
// last-wins hole is closed). Everything else consumes existing surfaces as-is.
// S094-unique global DPs (S039 discipline): S094Width(Int32) / S094Text(String).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Value/ValueType etc.
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // Button/StackPanel/Text + register_builtin_elements
#include "vaml/core/ElementResolver.h"    // ElementResolutionException
#include "vaml/core/ParsedElement.h"      // ParsedElement + parse_vaml
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using vaml::Button;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::ElementResolutionError;
using vaml::ElementResolutionException;
using vaml::ParsedElement;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Value;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::parse_vaml;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique process-wide (S039). Each
// S094-unique name carries the "S094" prefix; the set guards against accidental
// reuse within this TU.
std::unordered_set<std::string>& seen_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerGlobal(const std::string& name, ValueType type,
                                         PropertyFlags flags, Value def)
{
    if (seen_names().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S094_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// S094 fixture: registers the S094-unique global DPs the S094 docs consume.
void registerS094Properties()
{
    (void)registerGlobal("S094Width", ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S094Text",  ValueType::String, PropertyFlags::None, Value::String(""));
}

// The S094 xaml namespace for x:Class / x:Name directives.
inline const char* kXamlNs = "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"";

} // namespace

// ---------------------------------------------------------------------------
// 1. Unknown attribute -> VamlReaderException(UnknownProperty)
// ---------------------------------------------------------------------------

TEST_CASE("S094: an unknown attribute throws VamlReaderException(UnknownProperty)")
{
    registerS094Properties();
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load("<Button S094_NoSuchProp=\"1\"/>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::UnknownProperty)));
        // The exception exposes the exact offending attribute key.
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("S094_NoSuchProp")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S094: unknown attribute — ShouldThrow form")
{
    VamlReader reader;
    SHOULD(ShouldThrow<VamlReaderException>([&] {
        (void)reader.Load("<Button S094_Bogus=\"1\"/>");
    }));
}

// ---------------------------------------------------------------------------
// 2. Unknown element -> ElementResolutionException(UnknownType), naming type+line
// ---------------------------------------------------------------------------

TEST_CASE("S094: an unknown element throws ElementResolutionException(UnknownType)")
{
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel>\n"
            "  <Button S094Text=\"ok\"/>\n"
            "  <S094Zorp/>\n"
            "</StackPanel>");
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ElementResolutionError::UnknownType)));
        // The offending type name AND its 1-based authored line are structured.
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("S094Zorp")));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(3u));
        // The rendered message also names the real source position.
        SHOULD(ShouldlyIntegral(
                   std::string(e.what()).find("line 3") != std::string::npos).ShouldBe(1));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. Bad type name (empty -> no type) -> ElementResolutionException(UnknownType)
// ---------------------------------------------------------------------------

TEST_CASE("S094: a bad/empty type name throws ElementResolutionException(UnknownType)")
{
    VamlReader reader;

    // The scanner bounds authored type names, so the empty-name path is
    // reached only through a hand-built ParsedElement (S088's documented
    // reachable corner: an empty resolution key => no type at all).
    ParsedElement bad;
    bad.tagName = "";
    bad.local_name = "";
    bad.line = 7;
    bad.col = 3;

    bool threw = false;
    try
    {
        (void)reader.Load(bad);
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ElementResolutionError::UnknownType)));
        // Empty name is reported as UnknownType (not silently ignored), at the
        // hand-built element's position.
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("")));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(7u));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBe(3u));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. Malformed value -> ValueConversionException(MalformedValue), naming literal
// ---------------------------------------------------------------------------

TEST_CASE("S094: a malformed value throws ValueConversionException(MalformedValue)")
{
    registerS094Properties();
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load("<Button S094Width=\"abc\"/>"); // Int32, not a number
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::MalformedValue)));
        // The offending literal is exposed structured, not only in the message.
        SHOULD(ShouldlyString(e.literal()).ShouldBe(std::string("abc")));
        // Rendered message names the literal AND the target type.
        SHOULD(ShouldlyIntegral(
                   std::string(e.what()).find("abc") != std::string::npos).ShouldBe(1));
        SHOULD(ShouldlyIntegral(
                   std::string(e.what()).find("Int32") != std::string::npos).ShouldBe(1));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 5. x:Class on a NON-root -> VamlReaderException(ClassOnNonRoot) (S094-new arm)
// ---------------------------------------------------------------------------

TEST_CASE("S094: x:Class on a non-root element throws VamlReaderException(ClassOnNonRoot)")
{
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load(
            std::string("<StackPanel ") + kXamlNs + " x:Class=\"RootClass\">"
            "  <Button x:Class=\"ChildClass\"/>"
            "</StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::ClassOnNonRoot)));
        // The offending x:Class VALUE is exposed, not just the message.
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("ChildClass")));
        SHOULD(ShouldlyIntegral(
                   std::string(e.what()).find("ChildClass") != std::string::npos).ShouldBe(1));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S094: x:Class on the ROOT is still accepted (root-only contract)")
{
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        std::string("<StackPanel ") + kXamlNs + " x:Class=\"GoodRoot\">"
        "  <Button S094Width=\"1\"/>"
        "</StackPanel>");

    // No throw; the authored root class name is recorded.
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("GoodRoot")));
    SHOULD(ShouldlyIntegral(reader.retained_objects().size()).ShouldBe(1u));
    if (!reader.retained_objects().empty())
    {
        SHOULD(ShouldlyIntegral(
                   dynamic_cast<Button*>(reader.retained_objects()[0].get()) != nullptr)
                   .ShouldBe(1));
    }
}

// ---------------------------------------------------------------------------
// 6. Taxonomy DISTINCTNESS — each error family stays structured + distinct
// ---------------------------------------------------------------------------

TEST_CASE("S094: the three error families stay distinct by kind, not by message")
{
    registerS094Properties();
    VamlReader reader;

    // Unknown element resolves first (build resolves the type before any attr):
    // ElementResolutionException, never a reader/converter error.
    bool resolver = false;
    try
    {
        (void)reader.Load("<NotAThing S094Width=\"1\"/>");
    }
    catch (const ElementResolutionException&)
    {
        resolver = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(resolver)).ShouldBe(1));

    // Malformed value on a KNOWN element+KNOWN property:
    // ValueConversionException, never a reader error.
    bool converter = false;
    try
    {
        (void)reader.Load("<Button S094Width=\"zz\"/>");
    }
    catch (const ValueConversionException& e)
    {
        converter = (e.kind() == ValueConversionError::MalformedValue);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(converter)).ShouldBe(1));

    // Unknown property on a KNOWN element:
    // VamlReaderException(UnknownProperty), never a converter/resolver error.
    bool unknownProp = false;
    try
    {
        (void)reader.Load("<Button S094_SomethingElse=\"1\"/>");
    }
    catch (const VamlReaderException& e)
    {
        unknownProp = (e.kind() == VamlReaderError::UnknownProperty);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(unknownProp)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 7. Control — all four HAPPY paths present in one doc load clean
// ---------------------------------------------------------------------------

TEST_CASE("S094: a doc exercising all four happy paths loads without error")
{
    registerS094Properties();
    VamlReader reader;

    const std::string doc =
        std::string("<StackPanel ") + kXamlNs + " x:Class=\"AppRoot\" x:Name=\"root\">"
        "  <Button S094Width=\"100\" S094Text=\"Go\"/>"
        "  <Text S094Text=\"Label\"/>"
        "</StackPanel>";

    std::unique_ptr<DependencyObject> root = reader.Load(doc);

    // The taxonomy gate is not an all-errors gate: a correct doc is clean.
    SHOULD(ShouldlyIntegral(
               dynamic_cast<StackPanel*>(root.get()) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyString(reader.retained_objects().size()).ShouldBe(2u));
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("AppRoot")));

    if (reader.retained_objects().size() == 2)
    {
        // Known property set from a valid plain literal — the integer converted.
        SHOULD(ShouldlyString(
                   reader.retained_objects()[0]->Get(PropertyId::of("S094Width")))
                   .ShouldBe(Value::Int32(100)));
        SHOULD(ShouldlyString(
                   reader.retained_objects()[0]->Get(PropertyId::of("S094Text")))
                   .ShouldBe(Value::String("Go")));
    }
}
