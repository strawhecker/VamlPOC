// S092 (dep:S091) — VamlReader defers markup-extension values (S2 tag) +
// event hooks (S8); records the x:Name registry.
//
// Three strands, all validated with the Shouldly vocabulary:
//   A. x:Name / x:Class registry:
//      1. a root x:Name is registered (named() returns the root);
//      2. child x:Names are registered in document order (names()/name_count());
//      3. named() returns nullptr for an unknown name;
//      4. root_class() records the authored x:Class on the root;
//      5. child x:Name lookup — a name on a nested child resolves to it;
//      6. duplicate x:Name within one Load -> VamlReaderException(DuplicateName);
//      7. a fresh Load clears the registry (no cross-Load leakage).
//   B. Deferred markup extensions (S2 tag):
//      8. an isExtension attribute is retained as DeferredExtension with the
//         ORIGINAL source text, in document order, and NOT set as a property;
//      9. deferred_extensions() is empty when no extension is present;
//     10. a fresh Load clears prior deferred extensions.
//   C. Event hooks (MINIMAL REAL seed -> S322):
//     11. register_event("Click") causes a Click="OnClick" attr to be routed to
//         DeferredEventHandler (eventName + handlerName) — NOT a property set,
//         NOT an unknown-property throw, even when "Click" has no DP;
//     12. an unregistered key is still treated as a property (unknown -> throw);
//     13. deferred_event_handlers() clears per fresh Load.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
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
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::Button;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::ParsedElement;
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
using vaml::parse_vaml;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique process-wide (S039). Each
// S092-unique name carries the "S092" prefix; the set guards against accidental
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
        return DependencyProperty::Register(name, type, "S092_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// S092 fixture: registers the S092-unique global DP the docs use to prove an
// isExtension attribute is NOT set as a property (its value stays default).
void registerS092Properties()
{
    (void)registerGlobal("S092Text", ValueType::String, PropertyFlags::None, Value::String(""));
}

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
// A. x:Name / x:Class registry
// ---------------------------------------------------------------------------

TEST_CASE("S092: a root x:Name is registered; named() returns the root")
{
    registerS092Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"root\"/>");

    assertConcrete<StackPanel>(root);
    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(reader.named("root") == root.get()).ShouldBe(true));
    SHOULD(ShouldlyString(std::string(reader.names()[0].first)).ShouldBe(std::string("root")));
}

TEST_CASE("S092: child x:Names are registered in document order")
{
    registerS092Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "  <Button x:Name=\"btn\"/>"
        "  <Text x:Name=\"lbl\"/>"
        "</StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(reader.named("btn") == retained[0].get()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(reader.named("lbl") == retained[1].get()).ShouldBe(true));

    // names() preserves document (registration) order, not construction order.
    SHOULD(ShouldlyString(std::string(reader.names()[0].first)).ShouldBe(std::string("btn")));
    SHOULD(ShouldlyString(std::string(reader.names()[1].first)).ShouldBe(std::string("lbl")));
}

TEST_CASE("S092: named() returns nullptr for an unknown name")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"root\"/>");
    SHOULD(ShouldlyIntegral(reader.named("missing") == nullptr).ShouldBe(true));
}

TEST_CASE("S092: root_class() records the authored x:Class on the root")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Class=\"MyWindow\"/>");
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("MyWindow")));
}

TEST_CASE("S092: a nested child x:Name resolves to the deepest/named object")
{
    registerS092Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "  <Button x:Name=\"btn\"><Text x:Name=\"inner\"/></Button>"
        "</StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    // retained[0] = the nested Text (constructed depth-first first); retained[1] = Button.
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        SHOULD(ShouldlyIntegral(reader.named("inner") == retained[0].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(reader.named("btn") == retained[1].get()).ShouldBe(true));
    }
}

TEST_CASE("S092: a duplicate x:Name within one Load throws DuplicateName")
{
    registerS092Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "  <Button x:Name=\"dup\"/>"
            "  <Text x:Name=\"dup\"/>"
            "</StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::DuplicateName)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("dup")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S092: a fresh Load clears the name registry (no cross-Load leakage)")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"first\"/>");
    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(1u));

    // Second Load has no x:Name; the registry must be empty again.
    (void)reader.Load("<StackPanel/>");
    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(reader.named("first") == nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// B. Deferred markup extensions (S2 tag)
// ---------------------------------------------------------------------------

TEST_CASE("S092: an isExtension attribute is retained as DeferredExtension (S2 tag)")
{
    registerS092Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel><Button S092Text=\"{Binding Title}\"/></StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& ext = reader.deferred_extensions();
    SHOULD(ShouldlyIntegral(ext.size()).ShouldBe(1u));
    if (ext.size() == 1)
    {
        const auto& retained = reader.retained_objects();
        SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
        if (retained.size() == 1)
        {
            // The extension targets the Button, carries the key and the ORIGINAL
            // source text verbatim (the S2 tag — S252 re-parses this).
            SHOULD(ShouldlyIntegral(ext[0].target == retained[0].get()).ShouldBe(true));
            SHOULD(ShouldlyString(ext[0].key).ShouldBe(std::string("S092Text")));
            SHOULD(ShouldlyString(ext[0].source).ShouldBe(std::string("{Binding Title}")));
        }
    }

    // NOT set as a property: S092Text still has its default (empty String).
    const auto& retained = reader.retained_objects();
    if (retained.size() == 1)
    {
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S092Text")))
                   .ShouldBe(Value::String("")));
    }
}

TEST_CASE("S092: deferred extensions are recorded in document order")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel>"
        "  <Button S092Text=\"{Binding A}\"/>"
        "  <Button S092Text=\"{Binding B}\"/>"
        "</StackPanel>");

    const auto& ext = reader.deferred_extensions();
    SHOULD(ShouldlyString(ext.size()).ShouldBe(2u));
    if (ext.size() == 2)
    {
        SHOULD(ShouldlyString(ext[0].source).ShouldBe(std::string("{Binding A}")));
        SHOULD(ShouldlyString(ext[1].source).ShouldBe(std::string("{Binding B}")));
    }
}

TEST_CASE("S092: deferred_extensions() is empty when no extension is present")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load("<StackPanel><Button S092Text=\"plain\"/></StackPanel>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(0u));
}

TEST_CASE("S092: a fresh Load clears prior deferred extensions")
{
    registerS092Properties();
    VamlReader reader;
    (void)reader.Load("<StackPanel><Button S092Text=\"{Binding A}\"/></StackPanel>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(1u));

    (void)reader.Load("<StackPanel/>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// C. Event hooks (MINIMAL REAL seed -> S322)
// ---------------------------------------------------------------------------

TEST_CASE("S092: register_event routes a matching key to DeferredEventHandler")
{
    registerS092Properties();
    VamlReader reader;
    reader.register_event("Click");

    // "Click" has NO global DP — yet it must NOT throw UnknownProperty: it is a
    // registered event, routed to a deferred record with the handler name.
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel><Button Click=\"OnClick\"/></StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& handlers = reader.deferred_event_handlers();
    SHOULD(ShouldlyIntegral(handlers.size()).ShouldBe(1u));
    if (handlers.size() == 1)
    {
        const auto& retained = reader.retained_objects();
        SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
        if (retained.size() == 1)
        {
            SHOULD(ShouldlyIntegral(handlers[0].target == retained[0].get()).ShouldBe(true));
            SHOULD(ShouldlyString(handlers[0].eventName).ShouldBe(std::string("Click")));
            SHOULD(ShouldlyString(handlers[0].handlerName).ShouldBe(std::string("OnClick")));
        }
    }
}

TEST_CASE("S092: an unregistered event key is still treated as a property (unknown -> throw)")
{
    registerS092Properties();
    VamlReader reader;
    reader.register_event("Click");

    // "MouseMove" was never registered as an event — it has no DP, so it is an
    // unknown property (S091 baseline). register_event only routes Click.
    bool threw = false;
    try
    {
        (void)reader.Load("<Button MouseMove=\"OnMove\"/>");
    }
    catch (const VamlReaderException& e)
    {
        threw = (e.kind() == VamlReaderError::UnknownProperty);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S092: a fresh Load clears prior deferred event handlers")
{
    registerS092Properties();
    VamlReader reader;
    reader.register_event("Click");
    (void)reader.Load("<Button Click=\"OnClick\"/>");
    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(1u));

    (void)reader.Load("<StackPanel/>");
    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(0u));
}
