// S091 (dep:S090) — VamlReader: build a LIVE dependency-object tree from parsed
// VAML, setting properties and linking children to parents. The runtime reader
// entry point consuming S088 resolution + S089/S090 value conversion over the
// global property system.
//
// Test plan (Shouldly vocabulary):
//   1. Load(text) -> root concrete type (StackPanel), zero errors, object built.
//   2. children built + parent-linked: retained_objects() count == element
//      count (minus root); each child's parent() == its expected parent.
//   3. properties set to converted values: Int32 / String / Bool / Thickness /
//      Color read back correctly; a dotted attached key set on a child.
//   4. isExtension + directive attributes skipped (not set, no throw, object
//      still built) — the S092/S361/S084 deferral proof.
//   5. unknown-property attr -> VamlReaderException (kind UnknownProperty).
//   6. malformed literal for a known prop -> ValueConversionException
//      (MalformedValue) propagates.
//   7. unknown element -> ElementResolutionException naming type+line propagates.
//   8. Load(ParsedElement) overload parity with Load(text) on the same doc.
//   9. two-level nesting builds the correct parent chain.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // Button/StackPanel/Text + register_builtin_elements
#include "vaml/core/ElementResolver.h"    // ElementResolutionException
#include "vaml/core/ParsedElement.h"      // ParsedElement + parse_vaml
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::Button;
using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::ElementResolutionException;
using vaml::ParsedElement;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Text;
using vaml::Thickness;
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
// S091-unique name carries the "S091" prefix; the set guards against accidental
// reuse within this TU across multiple test registrations.
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
        return DependencyProperty::Register(name, type, "S091_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// S091 fixture: registers the S091-unique global DPs the docs use in tests.
void registerS091Properties()
{
    (void)registerGlobal("S091Width",      ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S091Text",       ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S091IsEnabled",  ValueType::Bool, PropertyFlags::None, Value::Bool(true));
    (void)registerGlobal("S091Margin",     ValueType::Thickness, PropertyFlags::None,
                         Value::Thickness(Thickness{0, 0, 0, 0}));
    (void)registerGlobal("S091Fill",       ValueType::Color, PropertyFlags::None,
                         Value::Color(Color{0, 0, 0, 0}));
    // ATTACHED: owner is "S091_Owner"; the value lands on whatever object Set
    // is called on (S043 attached-aware global resolution by name).
    (void)registerGlobal("S091Grid.Row",   ValueType::Int32, PropertyFlags::None, Value::Int32(0));
}

// Asserts `p` is non-null AND dynamic_cast<Concrete*>(p) is non-null.
template <typename Concrete>
void assertConcrete(const std::unique_ptr<DependencyObject>& p)
{
    SHOULD(ShouldlyIntegral(p != nullptr).ShouldBe(true));
    if (p)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Concrete*>(p.get()) != nullptr).ShouldBe(true));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Load(text) builds a live tree with the correct root type
// ---------------------------------------------------------------------------

TEST_CASE("S091: Load(text) builds a live StackPanel root")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load("<StackPanel/>");
    assertConcrete<StackPanel>(root);
}

// ---------------------------------------------------------------------------
// 2. children built + retained + parent-linked
// ---------------------------------------------------------------------------

TEST_CASE("S091: children are built, retained, and parent-linked")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel><Button/><Text/></StackPanel>");

    // Root is returned; the two children are retained (owned exactly once).
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    assertConcrete<StackPanel>(root);

    // Each retained child's parent() points back to the root.
    for (const auto& child : retained)
    {
        SHOULD(ShouldlyIntegral(child->parent() == root.get()).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// 3. properties set to converted values (incl. attached)
// ---------------------------------------------------------------------------

TEST_CASE("S091: plain literal attributes are converted and set")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<Button S091Width=\"100\" S091Text=\"Hello\" S091IsEnabled=\"true\" "
        "S091Margin=\"10,20\" S091Fill=\"#FF0000\"/>");
    assertConcrete<Button>(root);

    SHOULD(ShouldlyString(root->Get(PropertyId::of("S091Width")))
               .ShouldBe(Value::Int32(100)));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S091Text")))
               .ShouldBe(Value::String("Hello")));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S091IsEnabled")))
               .ShouldBe(Value::Bool(true)));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S091Margin")))
               .ShouldBe(Value::Thickness(Thickness{10, 20, 10, 20})));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S091Fill")))
               .ShouldBe(Value::Color(Color{255, 0, 0, 255})));
}

TEST_CASE("S091: a dotted attached key is set on a child via S043")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel><Button S091Grid.Row=\"3\"/></StackPanel>");

    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
    if (retained.size() == 1)
    {
        assertConcrete<Button>(retained[0]);
        // The attached S091Grid.Row value lands on the Button (Set is attached-
        // aware by name — S043); the value stored to the child, not the root.
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S091Grid.Row")))
                   .ShouldBe(Value::Int32(3)));
        SHOULD(ShouldlyIntegral(root->Get(PropertyId::of("S091Grid.Row"))
                   == Value::Int32(3)).ShouldBe(0)); // root untouched
    }
}

// ---------------------------------------------------------------------------
// 4. isExtension + directive attributes are skipped (deferral proof)
// ---------------------------------------------------------------------------

TEST_CASE("S091: markup-extension and directive attributes are skipped, object still built")
{
    registerS091Properties();
    VamlReader reader;

    // S091Text="{Binding Title}" is a MarkupExtension (isExtension); x:Name /
    // x:Key are x-language directives. None has a global DP, but ALL are
    // deferred — the object must still build WITHOUT an unknown-property error.
    const std::string doc =
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "  <Button S091Text=\"{Binding Title}\" x:Name=\"btn\" x:Key=\"k\"/>"
        "</StackPanel>";

    std::unique_ptr<DependencyObject> root = reader.Load(doc);
    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
    if (retained.size() == 1)
    {
        assertConcrete<Button>(retained[0]);
        // Nothing was set: S091Text still has its default (empty String).
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S091Text")))
                   .ShouldBe(Value::String("")));
    }
}

// ---------------------------------------------------------------------------
// 5. unknown property -> VamlReaderException
// ---------------------------------------------------------------------------

TEST_CASE("S091: an attribute naming an unknown property throws VamlReaderException")
{
    registerS091Properties();
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load("<Button NotARealProperty=\"1\"/>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::UnknownProperty)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("NotARealProperty")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S091: ShouldThrow form for the unknown-property error")
{
    registerS091Properties();
    VamlReader reader;
    SHOULD(ShouldThrow<VamlReaderException>([&] {
        (void)reader.Load("<Button Bogus=\"1\"/>");
    }));
}

// --- 11d — DESIGN-TIME attributes are stripped (prefix `d`), never resolved.
// WPF-faithful: d:DesignHeight / d:DataContext / any d:* is skipped verbatim,
// with NO dependency on mc:Ignorable (the designer abbreviation is auto-ignored).

TEST_CASE("11d: a d: design-time attribute is ignored without mc:Ignorable")
{
    registerS091Properties();
    VamlReader reader;
    // No mc:Ignorable declared: the d:* attributes must still be stripped, not
    // thrown as unknown properties.
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<Window xmlns:d=\"http://schemas.microsoft.com/expression/blend/2008\""
                    "        d:DesignHeight=\"420\">"
                    "  <StackPanel><Button d:DataContext=\"{Binding X}\"/></StackPanel>"
                    "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));
}

TEST_CASE("11d: an mc:Ignorable-declared d: attribute is ignored (belt-and-suspenders)")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<Window xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\""
                    "        mc:Ignorable=\"d\" d:DesignHeight=\"420\">"
                    "  <StackPanel><Button d:IsHitTestVisible=\"False\"/></StackPanel>"
                    "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));
}

TEST_CASE("11d: a non-design unknown property still throws UnknownProperty")
{
    registerS091Properties();
    VamlReader reader;
    // The designer-prefix guard strips d:* ONLY; a genuinely-unknown property
    // on an unprefixed prefix is still the S091 UnknownProperty error.
    SHOULD(ShouldThrow<VamlReaderException>([&] {
        (void)reader.Load("<Button Bogus=\"1\"/>");
    }));
}

// ---------------------------------------------------------------------------
// 6. malformed literal -> ValueConversionException propagates
// ---------------------------------------------------------------------------

TEST_CASE("S091: a malformed literal for a known property throws ValueConversionException")
{
    registerS091Properties();
    VamlReader reader;

    bool threw = false;
    try
    {
        (void)reader.Load("<Button S091Width=\"abc\"/>");
    }
    catch (const ValueConversionException& e)
    {
        threw = (e.kind() == ValueConversionError::MalformedValue);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 7. unknown element -> ElementResolutionException propagates (type + line)
// ---------------------------------------------------------------------------

TEST_CASE("S091: an unknown element type propagates ElementResolutionException")
{
    VamlReader reader; // startup registrations populate the real element registry

    bool threw = false;
    try
    {
        (void)reader.Load("<StackPanel><Zorp/></StackPanel>");
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Zorp")));
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(vaml::ElementResolutionError::UnknownType)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 8. Load(ParsedElement) parity with Load(text)
// ---------------------------------------------------------------------------

TEST_CASE("S091: Load(ParsedElement) and Load(text) produce identical trees")
{
    registerS091Properties();
    const std::string doc =
        "<StackPanel><Button S091Width=\"7\"/><Text S091Text=\"hi\"/></StackPanel>";

    // ParsedElement path.
    ParsedElement parsed = parse_vaml(doc);
    VamlReader r1;
    std::unique_ptr<DependencyObject> root1 = r1.Load(parsed);

    // Text path.
    VamlReader r2;
    std::unique_ptr<DependencyObject> root2 = r2.Load(doc);

    // Same root type + same number of retained children.
    assertConcrete<StackPanel>(root1);
    assertConcrete<StackPanel>(root2);
    SHOULD(ShouldlyString(r1.retained_objects().size()).ShouldBe(r2.retained_objects().size()));
    SHOULD(ShouldlyString(r1.retained_objects().size()).ShouldBe(2u));

    // Same property values on the corresponding nodes.
    SHOULD(ShouldlyString(r1.retained_objects()[0]->Get(PropertyId::of("S091Width")))
               .ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(r2.retained_objects()[0]->Get(PropertyId::of("S091Width")))
               .ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(r1.retained_objects()[1]->Get(PropertyId::of("S091Text")))
               .ShouldBe(Value::String("hi")));
    SHOULD(ShouldlyString(r2.retained_objects()[1]->Get(PropertyId::of("S091Text")))
               .ShouldBe(Value::String("hi")));
}

// ---------------------------------------------------------------------------
// 9. two-level nesting parent chain
// ---------------------------------------------------------------------------

TEST_CASE("S091: two-level nesting builds the correct parent chain")
{
    registerS091Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel><Button><Text/></Button></StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    // Two retained objects — retained in POST-ORDER depth-first order (each
    // child is pushed as soon as build() returns it): the Text (deepest,
    // pushed during Button's recursion) comes first, the Button after it.
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        // retained[0] = the nested Text; its parent is the Button (retained[1]).
        assertConcrete<Text>(retained[0]);
        assertConcrete<Button>(retained[1]);
        SHOULD(ShouldlyIntegral(retained[0]->parent() == retained[1].get()).ShouldBe(true));
        // retained[1] = the Button; its parent is the root StackPanel.
        SHOULD(ShouldlyIntegral(retained[1]->parent() == root.get()).ShouldBe(true));
        // Neither child's parent is the other-loop root twice (exactly-once ownership).
        SHOULD(ShouldlyIntegral(retained[0]->parent() == root.get()).ShouldBe(0));
    }
}
