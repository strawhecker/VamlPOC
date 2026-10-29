// S093 (dep:S092) — Integration gate: load a realistic StackPanel+Button document
// from PURE TEXT (no event hooks, no markup extensions), with a property set from
// a plain literal, and verify the resulting tree on BOTH axes:
//
//   A. STRUCTURAL axis (S091): root type, retained children, parent links.
//   B. NAME axis (S092): the x:Name registry is consumed live — named()/names()/
//      name_count() resolve this document's names exactly.
//   C. NO-HOOKS / NO-EXTENSIONS axis (the headline): a doc with no register_event
//      call and no {Binding} yields deferred_event_handlers() AND deferred_extensions()
//      BOTH empty — tree + properties came purely from text, no latent wiring.
//   D. NO SILENT AUTO-WIRING: an event-shaped key that was never registered (and has
//      no DP) stays a property -> VamlReaderException(UnknownProperty); nothing
//      fabricates a handler behind the reader's back.
//
// S091/S092 own ALL product surface. S093 is TEST-ONLY: zero new product symbols,
// NO-GAP clean, no new MINIMAL REAL, no resolution step.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // Button/StackPanel/Text + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>

using vaml::Button;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::TextBlock;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;

namespace {

// The GLOBAL DependencyProperty table is name-unique process-wide (S039). Each
// S093-unique name carries the "S093" prefix; the set guards against accidental
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
        return DependencyProperty::Register(name, type, "S093_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// S093 fixture: registers the S093-unique global DPs the S093 document sets from
// pure text. These are the "pure text" properties the realistic doc consumes.
void registerS093Properties()
{
    (void)registerGlobal("S093Width",     ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S093Height",    ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S093Content",   ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S093IsEnabled", ValueType::Bool, PropertyFlags::None, Value::Bool(true));
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
// 1. Tree structure correct (structural axis)
// ---------------------------------------------------------------------------

TEST_CASE("S093: StackPanel+Button tree has the correct structure")
{
    registerS093Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"root\">"
        "  <Button x:Name=\"btn\" S093Content=\"Click me\" S093Width=\"100\" "
        "S093Height=\"40\" S093IsEnabled=\"true\"/>"
        "  <TextBlock x:Name=\"lbl\" S093Content=\"Hello\"/>"
        "</StackPanel>");

    // Root type + exactly two retained children, in document order.
    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        assertConcrete<Button>(retained[0]);
        assertConcrete<TextBlock>(retained[1]);

        // Each child's parent points back to the root.
        SHOULD(ShouldlyIntegral(retained[0]->parent() == root.get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(retained[1]->parent() == root.get()).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// 2. Property set from pure text (the headline)
// ---------------------------------------------------------------------------

TEST_CASE("S093: properties are set on the Button from pure text")
{
    registerS093Properties();
    VamlReader reader;
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "  <Button x:Name=\"btn\" S093Content=\"Click me\" S093Width=\"100\" "
        "S093Height=\"40\" S093IsEnabled=\"true\"/>"
        "</StackPanel>");

    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
    if (retained.size() == 1)
    {
        assertConcrete<Button>(retained[0]);

        // All four pure-text properties read back with their converted values.
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S093Content")))
                   .ShouldBe(Value::String("Click me")));
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S093Width")))
                   .ShouldBe(Value::Int32(100)));
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S093Height")))
                   .ShouldBe(Value::Int32(40)));
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S093IsEnabled")))
                   .ShouldBe(Value::Bool(true)));
    }
}

// ---------------------------------------------------------------------------
// 3. S092 name registry consumed end-to-end (name axis)
// ---------------------------------------------------------------------------

TEST_CASE("S093: the x:Name registry resolves the full document")
{
    registerS093Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"root\">"
        "  <Button x:Name=\"btn\" S093Content=\"Click me\" S093Width=\"100\" />"
        "  <TextBlock x:Name=\"lbl\" S093Content=\"Hello\"/>"
        "</StackPanel>");

    // named() resolves root and both children to the live objects.
    SHOULD(ShouldlyIntegral(reader.named("root") == root.get()).ShouldBe(true));

    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        SHOULD(ShouldlyIntegral(reader.named("btn") == retained[0].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(reader.named("lbl") == retained[1].get()).ShouldBe(true));
    }

    // name_count + names() in document (registration) order: root, btn, lbl.
    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(3u));
    const auto& names = reader.names();
    SHOULD(ShouldlyString(names.size()).ShouldBe(3u));
    if (names.size() == 3)
    {
        SHOULD(ShouldlyString(std::string(names[0].first)).ShouldBe(std::string("root")));
        SHOULD(ShouldlyString(std::string(names[1].first)).ShouldBe(std::string("btn")));
        SHOULD(ShouldlyString(std::string(names[2].first)).ShouldBe(std::string("lbl")));
    }
}

// ---------------------------------------------------------------------------
// 4. NO HOOKS / NO EXTENSIONS (the headline: tree from pure text)
// ---------------------------------------------------------------------------

TEST_CASE("S093: no event hooks and no extensions are recorded from pure text")
{
    registerS093Properties();
    VamlReader reader;
    // No register_event call, no {Binding}: the S093 realistic doc.
    (void)reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"root\">"
        "  <Button x:Name=\"btn\" S093Content=\"Click me\" S093Width=\"100\" "
        "S093Height=\"40\" S093IsEnabled=\"true\"/>"
        "  <TextBlock x:Name=\"lbl\" S093Content=\"Hello\"/>"
        "</StackPanel>");

    // Both deferred surfaces are empty — nothing was wired behind the scenes.
    SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// 5. No silent auto-wiring
// ---------------------------------------------------------------------------

TEST_CASE("S093: an unregistered event-shaped key stays a property (unknown -> throw)")
{
    registerS093Properties();
    VamlReader reader;
    // "Click" is NEVER registered as an event and has no DP: it must be treated
    // as a plain property and throw UnknownProperty — nothing creates a handler
    // behind the reader's back.
    bool threw = false;
    try
    {
        (void)reader.Load("<Button Click=\"OnClick\"/>");
    }
    catch (const VamlReaderException& e)
    {
        threw = (e.kind() == VamlReaderError::UnknownProperty);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
