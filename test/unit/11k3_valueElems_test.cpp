// 11k-3 (dep:S453/S361/S089/11j-9) — AUTHORABLE VALUE-TYPE RESOURCE ELEMENTS.
//
// The existing value PARSERS (S089 parse_color / parse_thickness, S440 Double
// converter, 11j-3 parse_grid_length) are PROMOTED to authorable ELEMENT types
// inside a ResourceDictionary — authored by x:Key from their INLINE CONTENT into
// a typed Value, stored as a PLAIN resource (required for S361 {StaticResource}
// consumption on a value DP; Object statics are rejected on a value DP).
//
// This is the Themes.Colors / Theme.FontFamily / Themes.Layouts /
// AddPhysicianWindow pattern, and the WPF-fidelity resolution of the fidelity
// review: each value type is stored TYPED, matching WPF's TypeConverter-at-parse
// behavior (validated against E:\GitRepos\wpf — GridLengthConverter/ColorConverter/
// ThicknessConverter/double parser).
//
// <Geometry> is deliberately NOT authored here — WPF Geometry is abstract (not
// authorable as a bare element); it is OUT-OF-SCOPE(11k-4), recorded in the
// spine docs (DAG_controls.md / appendix_c). NOT a silent omission (NO-GAP).
//
// Coverage:
//   1. each value-type element (Color/FontFamily/Double/Thickness/GridLength)
//      authored by x:Key -> PLAIN typed Value in the S360 dict (per-type);
//   2. {StaticResource} on a matching value DP applies the Plain value (S361);
//   3. duplicate x:Key -> ResourceAlreadyRegistered (dedupe unchanged);
//   4. no-x:Key value-type resource -> structured reader error;
//   5. malformed inline content -> ValueConversionException{MalformedValue}
//      (fail loud, never silent);
//   6. find_for_element three-scope (element's own -> ancestor -> app/system);
//   7. public parse_grid_length round-trip (Auto / N* / *) + malformed;
//   8. 11j-9 Grid-track regression (Grid column defs still parse via the
//      hoisted public parse_grid_length).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/ResourceResolver.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::GridLength;
using vaml::GridUnitType;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::ResourceAlreadyRegistered;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::StackPanel;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::find_for_element;
using vaml::parse_grid_length;

namespace {

std::unordered_set<std::string>& s11k3_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS11k3Global(const std::string& name, ValueType type,
                                              PropertyFlags flags, Value def)
{
    if (s11k3_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "11k3_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS11k3()
{
    // A Color DP, a Thickness DP, a Double DP, a GridLength DP, a FontFamily String DP.
    (void)registerS11k3Global("S11k3Color", ValueType::Color, PropertyFlags::None,
                              Value::Color(Color{0, 0, 0, 0}));
    (void)registerS11k3Global("S11k3Thickness", ValueType::Thickness, PropertyFlags::None,
                              Value::Thickness(Thickness{0, 0, 0, 0}));
    (void)registerS11k3Global("S11k3Double", ValueType::Double, PropertyFlags::None,
                              Value::Double(0.0));
    (void)registerS11k3Global("S11k3GridLength", ValueType::GridLength, PropertyFlags::None,
                              Value::GridLength(GridLength::Auto()));
    (void)registerS11k3Global("S11k3Font", ValueType::String, PropertyFlags::None,
                              Value::String(""));
    vaml::register_builtin_elements();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Per-type: value element authored by x:Key -> PLAIN typed Value
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: <Color> authored by x:Key is a PLAIN typed Color value")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<Color x:Key=\"Accent\">#FF2E7AF2</Color>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("Accent");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(v->kind()) == static_cast<int>(ResourceValue::Kind::Plain)).ShouldBe(true));
            const Value& val = v->value();
            SHOULD(ShouldlyIntegral(static_cast<int>(val.type()) == static_cast<int>(ValueType::Color)).ShouldBe(true));
            const Color got = val.get<Color>();
            SHOULD(ShouldlyIntegral(got.r).ShouldBe(0x2E));
            SHOULD(ShouldlyIntegral(got.g).ShouldBe(0x7A));
            SHOULD(ShouldlyIntegral(got.b).ShouldBe(0xF2));
            SHOULD(ShouldlyIntegral(got.a).ShouldBe(0xFF));
        }
    }
}

TEST_CASE("11k-3: <Thickness> authored by x:Key is a PLAIN typed Thickness value")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<Thickness x:Key=\"CardPad\">8,12,8,12</Thickness>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("CardPad");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(v->kind()) == static_cast<int>(ResourceValue::Kind::Plain)).ShouldBe(true));
            const Value& val = v->value();
            SHOULD(ShouldlyIntegral(static_cast<int>(val.type()) == static_cast<int>(ValueType::Thickness)).ShouldBe(true));
            const Thickness got = val.get<Thickness>();
            SHOULD(ShouldlyIntegral(got.left).ShouldBe(8u));
            SHOULD(ShouldlyIntegral(got.top).ShouldBe(12u));
            SHOULD(ShouldlyIntegral(got.right).ShouldBe(8u));
            SHOULD(ShouldlyIntegral(got.bottom).ShouldBe(12u));
        }
    }
}

TEST_CASE("11k-3: <Double> authored by x:Key is a PLAIN typed Double value")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<Double x:Key=\"IconSize\">24</Double>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("IconSize");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(v->kind()) == static_cast<int>(ResourceValue::Kind::Plain)).ShouldBe(true));
            const Value& val = v->value();
            SHOULD(ShouldlyIntegral(static_cast<int>(val.type()) == static_cast<int>(ValueType::Double)).ShouldBe(true));
            SHOULD(ShouldlyString(val.get<double>()).ShouldBe(24.0));
        }
    }
}

TEST_CASE("11k-3: <FontFamily> authored by x:Key is a PLAIN String value (string-backed DP)")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<FontFamily x:Key=\"AppFont\">Segoe UI</FontFamily>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("AppFont");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(v->kind()) == static_cast<int>(ResourceValue::Kind::Plain)).ShouldBe(true));
            const Value& val = v->value();
            SHOULD(ShouldlyIntegral(static_cast<int>(val.type()) == static_cast<int>(ValueType::String)).ShouldBe(true));
            SHOULD(ShouldlyString(val.get<std::string>()).ShouldBe(std::string("Segoe UI")));
        }
    }
}

TEST_CASE("11k-3: <GridLength> authored by x:Key is a PLAIN typed GridLength value")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<GridLength x:Key=\"SplitWidth\">2*</GridLength>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        const ResourceValue* v = dict->try_get("SplitWidth");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(v->kind()) == static_cast<int>(ResourceValue::Kind::Plain)).ShouldBe(true));
            const Value& val = v->value();
            SHOULD(ShouldlyIntegral(static_cast<int>(val.type()) == static_cast<int>(ValueType::GridLength)).ShouldBe(true));
            const GridLength got = val.get<GridLength>();
            SHOULD(ShouldlyIntegral(static_cast<int>(got.unitType) == static_cast<int>(GridUnitType::Star)).ShouldBe(true));
            SHOULD(ShouldlyIntegral(got.value).ShouldBe(2.0f));
        }
    }
}

// ---------------------------------------------------------------------------
// 2. {StaticResource} on a value DP applies the PLAIN value (S361)
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: {StaticResource} on a Color DP applies the authored PLAIN Color value")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<Color x:Key=\"Accent\">#FF123456</Color>"
        "</StackPanel.Resources>"
        "<StackPanel S11k3Color=\"{StaticResource Accent}\"/></StackPanel>");

    bool threw = false;
    try
    {
        (void)reader.BindResources();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(false));

    // The inner StackPanel applied the Plain value (Color) to its Color DP.
    const StackPanel* inner = nullptr;
    for (const auto& o : reader.retained_objects())
    {
        if (const StackPanel* sp = dynamic_cast<StackPanel*>(o.get()))
        {
            if (sp != root.get())
            {
                inner = sp;
                break;
            }
        }
    }
    SHOULD(ShouldlyIntegral(inner != nullptr).ShouldBe(true));
    if (inner)
    {
        const Value applied = inner->Get(vaml::PropertyId::of("S11k3Color"));
        SHOULD(ShouldlyIntegral(static_cast<int>(applied.type()) == static_cast<int>(ValueType::Color)).ShouldBe(true));
        const Color got = applied.get<Color>();
        SHOULD(ShouldlyIntegral(got.r).ShouldBe(0x12));
        SHOULD(ShouldlyIntegral(got.g).ShouldBe(0x34));
        SHOULD(ShouldlyIntegral(got.b).ShouldBe(0x56));
        SHOULD(ShouldlyIntegral(got.a).ShouldBe(0xFF));
    }
}

// ---------------------------------------------------------------------------
// 3. Duplicate x:Key -> ResourceAlreadyRegistered (dedupe unchanged)
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: duplicate value-type x:Key throws ResourceAlreadyRegistered")
{
    registerS11k3();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
            "<Color x:Key=\"K\">#FF000000</Color>"
            "<Color x:Key=\"K\">#FFFFFFFF</Color>"
            "</StackPanel.Resources></StackPanel>");
    }
    catch (const ResourceAlreadyRegistered&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 4. No-x:Key value-type resource -> structured reader error
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: a value-type resource with no x:Key throws a structured reader error")
{
    registerS11k3();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
            "<Color>#FF000000</Color>"
            "</StackPanel.Resources></StackPanel>");
    }
    catch (const VamlReaderException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 5. Malformed inline content -> ValueConversionException{MalformedValue}
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: malformed value-type content fails loud (ValueConversionException)")
{
    registerS11k3();
    // Malformed Color
    {
        VamlReader reader;
        bool threw = false;
        try
        {
            (void)reader.Load(
                "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
                "<Color x:Key=\"Bad\">not-a-color</Color>"
                "</StackPanel.Resources></StackPanel>");
        }
        catch (const ValueConversionException& e)
        {
            threw = true;
            SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()) == static_cast<int>(ValueConversionError::MalformedValue)).ShouldBe(true));
        }
        SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
    }
    // Malformed GridLength
    {
        VamlReader reader;
        bool threw = false;
        try
        {
            (void)reader.Load(
                "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
                "<GridLength x:Key=\"Bad\">NaN*</GridLength>"
                "</StackPanel.Resources></StackPanel>");
        }
        catch (const ValueConversionException&)
        {
            threw = true;
        }
        SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
    }
    // Malformed Thickness
    {
        VamlReader reader;
        bool threw = false;
        try
        {
            (void)reader.Load(
                "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
                "<Thickness x:Key=\"Bad\">a,b</Thickness>"
                "</StackPanel.Resources></StackPanel>");
        }
        catch (const ValueConversionException&)
        {
            threw = true;
        }
        SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// 6. find_for_element three-scope (element own -> ancestor -> app/system)
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: find_for_element resolves authored value resources across scopes")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<Color x:Key=\"Scoped\">#FF010203</Color>"
        "</StackPanel.Resources>"
        "<StackPanel><StackPanel.Resources>"
        "<Color x:Key=\"Scoped\">#FF0A0B0C</Color>"
        "</StackPanel.Resources></StackPanel></StackPanel>");

    StackPanel* outer = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(outer != nullptr).ShouldBe(true));

    // Inner panel resolves its OWN "Scoped" (shadowing).
    const StackPanel* inner = nullptr;
    for (const auto& o : reader.retained_objects())
    {
        if (const StackPanel* sp = dynamic_cast<StackPanel*>(o.get()))
        {
            if (sp != outer)
            {
                inner = sp;
                break;
            }
        }
    }
    SHOULD(ShouldlyIntegral(inner != nullptr).ShouldBe(true));
    if (inner)
    {
        const ResourceValue* v = find_for_element(*inner, "Scoped");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            const Color got = v->value().get<Color>();
            SHOULD(ShouldlyIntegral(got.r).ShouldBe(0x0A)); // inner (shadow) wins
        }
        // The outer panel has only the outer "Scoped".
        const ResourceValue* ov = find_for_element(*outer, "Scoped");
        SHOULD(ShouldlyIntegral(ov != nullptr).ShouldBe(true));
        if (ov)
        {
            const Color ogot = ov->value().get<Color>();
            SHOULD(ShouldlyIntegral(ogot.r).ShouldBe(0x01)); // outer value
        }
    }
}

// ---------------------------------------------------------------------------
// 7. Public parse_grid_length round-trip (hoisted from 11j-9)
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: public parse_grid_length rounds Auto / N* / numeric")
{
    const GridLength auto_ = parse_grid_length("Auto");
    SHOULD(ShouldlyIntegral(static_cast<int>(auto_.unitType) == static_cast<int>(GridUnitType::Auto)).ShouldBe(true));

    const GridLength star = parse_grid_length("2*");
    SHOULD(ShouldlyIntegral(static_cast<int>(star.unitType) == static_cast<int>(GridUnitType::Star)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(star.value).ShouldBe(2.0f));

    const GridLength bareStar = parse_grid_length("*");
    SHOULD(ShouldlyIntegral(static_cast<int>(bareStar.unitType) == static_cast<int>(GridUnitType::Star)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(bareStar.value).ShouldBe(1.0f));

    const GridLength px = parse_grid_length("24");
    SHOULD(ShouldlyIntegral(static_cast<int>(px.unitType) == static_cast<int>(GridUnitType::Pixel)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(px.value).ShouldBe(24.0f));

    // case-insensitive Auto
    const GridLength autoLower = parse_grid_length("auto");
    SHOULD(ShouldlyIntegral(static_cast<int>(autoLower.unitType) == static_cast<int>(GridUnitType::Auto)).ShouldBe(true));
}

TEST_CASE("11k-3: public parse_grid_length rejects malformed literals")
{
    bool threw = false;
    try { (void)parse_grid_length("NaN*"); }
    catch (const ValueConversionException&) { threw = true; }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));

    threw = false;
    try { (void)parse_grid_length("abc"); }
    catch (const ValueConversionException&) { threw = true; }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));

    threw = false;
    try { (void)parse_grid_length(""); }
    catch (const ValueConversionException&) { threw = true; }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 8. 11j-9 Grid-track regression via the hoisted public parse_grid_length
// ---------------------------------------------------------------------------

TEST_CASE("11k-3: Grid column definitions still parse via the hoisted public parse_grid_length")
{
    registerS11k3();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<Grid xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.ColumnDefinitions>"
        "<ColumnDefinition Width=\"2*\"/>"
        "<ColumnDefinition Width=\"Auto\"/>"
        "<ColumnDefinition Width=\"120\"/>"
        "</Grid.ColumnDefinitions>"
        "</Grid>");

    // Loading the Grid with column defs must not throw (parse path intact).
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
}
