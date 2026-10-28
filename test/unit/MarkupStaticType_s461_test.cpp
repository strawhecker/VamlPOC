// S461 (dep:S460) — MARKUP EXTENSIONS {x:Static} + {x:Type}.
//
// The S2 text reader resolves the remaining attribute surface:
//   * `{x:Static local:Constants.AppName}` — a static field/const access via the
//     READER (the S2-tag deferral CLOSED: an app-registered static VALUE is looked
//     up by its tail token and pushed onto the authored target's DP). WPF-faithful:
//     the app registers "Constants.AppName" (S250 app-owned composition, no reflection).
//   * `{x:Type Button}` — a TYPE OBJECT (Value::String("Button")) for TargetType /
//     ContentControl binding, resolved from the S461 type registry.
//   * The BindingMarkup parser gains the Static + Type KINDS (the grammar is
//     headless-testable on the pure parser surface).
//   * An unresolved static/type + a malformed {x:Static}/{x:Type} are Class A
//     AUTHORING errors — reported loudly, never silent.
//
// NO-GAP closed: this is the recorded S2-tag remainder from the binding sprint —
// resolved at ITS own step (S461), not backfilled.
//
// Registers no S461-unique DPs (resolves onto real DependencyObjects + the
// {x:Type} type object is a Value::String). Uses the Shouldly vocabulary. ADR-018
// zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/BindingMarkup.h"       // parse_markup_binding + MarkupExtensionKind
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::MarkupExtensionKind;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Text;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::parse_markup_binding;
using vaml::test::ShouldThrow;

namespace {

std::unordered_set<std::string>& s461_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers the S461-unique target DP (a String, so a resolved string/value lands).
const DependencyProperty& registerS461Global(const std::string& name)
{
    if (s461_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, ValueType::String, "S461_Owner",
                                            PropertyMetadata::WithDefault(
                                                Value::String(std::string{}), PropertyFlags::None));
    }
    return DependencyProperty::Get(name);
}

} // namespace

// ---------------------------------------------------------------------------
// 1. The BindingMarkup parser now recognizes {x:Static} + {x:Type} kinds
// ---------------------------------------------------------------------------

TEST_CASE("S461: parse_markup_binding recognizes {x:Static} + {x:Type}")
{
    // {x:Static local:Constants.AppName} -> kind Static, path = the full token.
    const auto st = parse_markup_binding("{x:Static local:Constants.AppName}");
    SHOULD(ShouldlyIntegral(static_cast<int>(st.kind))
               .ShouldBe(static_cast<int>(MarkupExtensionKind::Static)));
    SHOULD(ShouldlyString(std::string(st.path))
               .ShouldBe(std::string("local:Constants.AppName")));
    SHOULD(ShouldlyIntegral(static_cast<int>(st.malformed)).ShouldBe(0));

    // {x:Type Button} -> kind Type, path = "Button".
    const auto ty = parse_markup_binding("{x:Type Button}");
    SHOULD(ShouldlyIntegral(static_cast<int>(ty.kind))
               .ShouldBe(static_cast<int>(MarkupExtensionKind::Type)));
    SHOULD(ShouldlyString(std::string(ty.path)).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyIntegral(static_cast<int>(ty.malformed)).ShouldBe(0));

    // Malformed: {x:Static} with no token -> malformed.
    const auto stBad = parse_markup_binding("{x:Static }");
    SHOULD(ShouldlyIntegral(static_cast<int>(stBad.malformed)).ShouldBe(1));
    // A non-extension stays kind None.
    const auto none = parse_markup_binding("{StaticResource X}");
    SHOULD(ShouldlyIntegral(static_cast<int>(none.kind))
               .ShouldBe(static_cast<int>(MarkupExtensionKind::None)));
}

// ---------------------------------------------------------------------------
// 2. {x:Static ...} resolves an app-registered static VALUE onto the target DP
// ---------------------------------------------------------------------------

TEST_CASE("S461: {x:Static local:Constants.AppName} resolves a registered static value")
{
    (void)registerS461Global("S461Text");
    VamlReader reader;
    // The app registers its static const (S250 app-owned composition, no reflection).
    reader.register_static_value("Constants.AppName", Value::String("VAML Viewer"));

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" x:Name=\"Target\">"
        "<StackPanel S461Text=\"{x:Static local:Constants.AppName}\" x:Name=\"Inner\"/></StackPanel>");

    // BindExtensions leaves the Static kind deferred (not an error, not counted).
    SHOULD(ShouldlyIntegral(reader.BindExtensions()).ShouldBe(0u));

    // ResolveStaticTypeExtensions pushes the resolved string onto the target.
    const std::size_t resolved = reader.ResolveStaticTypeExtensions();
    SHOULD(ShouldlyIntegral(resolved).ShouldBe(1u));

    // The INNER target (the one that carries S461Text) now holds the resolved value.
    const auto* inner = dynamic_cast<StackPanel*>(reader.named("Inner"));
    SHOULD(ShouldlyIntegral(inner != nullptr).ShouldBe(1));
    if (inner)
    {
        SHOULD(ShouldlyString(std::string(inner->Get(PropertyId::of("S461Text")).get<std::string>()))
                   .ShouldBe(std::string("VAML Viewer")));
    }
}

// ---------------------------------------------------------------------------
// 3. {x:Type Button} resolves a TYPE OBJECT for TargetType/ContentControl
// ---------------------------------------------------------------------------

TEST_CASE("S461: {x:Type Button} resolves a type object (Value::String)")
{
    (void)registerS461Global("S461Type");
    VamlReader reader;
    reader.register_type_value("Button", "Button");

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel S461Type=\"{x:Type Button}\"/></StackPanel>");

    (void)reader.ResolveStaticTypeExtensions();
    const Value typeObj = reader.ResolveType("Button");
    SHOULD(ShouldlyString(std::string(typeObj.get<std::string>()))
               .ShouldBe(std::string("Button")));
}

// ---------------------------------------------------------------------------
// 4. An unresolved static/type is a Class A authoring error (never silent)
// ---------------------------------------------------------------------------

TEST_CASE("S461: an unresolved {x:Static}/{x:Type} throws a structured error")
{
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel S461Text=\"{x:Static local:Constants.Nope}\"/></StackPanel>");

    // No registration for Constants.Nope -> ResolveStaticTypeExtensions throws.
    SHOULD(ShouldThrow<VamlReaderException>([&] {
        (void)reader.ResolveStaticTypeExtensions();
    }));
}

TEST_CASE("S461: a malformed {x:Static} is a Class A authoring error")
{
    VamlReader reader;
    // {x:Static} with no token is malformed markup.
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel S461Text=\"{x:Static }\"/></StackPanel>");
    SHOULD(ShouldThrow<VamlReaderException>([&] {
        (void)reader.ResolveStaticTypeExtensions();
    }));
}
