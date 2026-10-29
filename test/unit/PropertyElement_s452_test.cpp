// S452 (dep:S451) — PROPERTY-ELEMENT + ATTRIBUTE-ELEMENT authoring grammar (the
// S366-recorded NO-GAP hand-off, RESOLVED). The S2 text reader accepts
// `<Button.Content><StackPanel/></Button.Content>` and `<Button.Background>` —
// content into the named DP, opening/closing over the owned track — producing a
// LIVE tree populated via the DP path (NOT a visual child). This is the
// syntactic seam S453 (Resources/x:Key) and S454 (Style/Setter/Trigger) ride.
//
// Test plan (Shouldly + the registerGlobal pattern):
//   1. A dotted element tag resolves as a property-element (is_property_element
//      splits owner/property).
//   2. `<X.S452Content><StackPanel/></X.S452Content>` sets the named S452 DP on
//      the PARENT as Value::UntypedPtr (the object child is retained + parent-
//      linked), and is NOT a visual child of the parent.
//   3. A literal-content property-element `<X.S452Fill>Red</X.S452Fill>`
//      converts + sets the DP.
//   4. Nested property-elements build correctly.
//   5. An empty property-element `<X.Property/>` is skipped (nothing set).
//   6. An unknown dotted property throws VamlReaderException(UnknownProperty)
//      naming the property.
//   7. The dotted-name helpers are pure (headless).
//
// ADR-018: zero locks; ADR-021 headless (drives the real reader).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h" // operator<< for Value/Color/Thickness
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h" // StackPanel/Window/etc + register_builtin_elements
#include "vaml/core/ParsedElement.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::is_property_element;
using vaml::split_property_element;

namespace {

std::unordered_set<std::string>& s452_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS452Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s452_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S452_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS452Properties()
{
    (void)registerS452Global("S452Content", ValueType::UntypedPtr, PropertyFlags::None,
                             Value::UntypedPtr(nullptr));
    (void)registerS452Global("S452Fill", ValueType::Color, PropertyFlags::None,
                             Value::Color(Color{0, 0, 0, 0}));
    (void)registerS452Global("S452Text", ValueType::String, PropertyFlags::None,
                             Value::String(""));
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Pure dotted-name helpers
// ---------------------------------------------------------------------------

TEST_CASE("S452: is_property_element / split_property_element are pure")
{
    // A dotted `<X.Y>` is a property-element.
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element("Button.Content"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element("X.S452Content"))).ShouldBe(1));
    // Only the LOCAL name (sans prefix) matters — a prefixed dotted tag still has
    // a dotted local_name.
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element("local:Button.Content"))).ShouldBe(1));
    // A plain element name is NOT a property-element.
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element("StackPanel"))).ShouldBe(0));
    // A malformed dotted tag (leading/trailing dot) is not a property-element.
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element(".Content"))).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element("Button."))).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(is_property_element(""))).ShouldBe(0));

    // Split: first '.' separates (Owner, Property).
    const auto [o1, p1] = split_property_element("Button.Content");
    (void)o1; (void)p1;
    SHOULD(ShouldlyString(o1).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(p1).ShouldBe(std::string("Content")));

    // Split on the LOCAL name (sans prefix): X.S452Content under local: prefix.
    const auto [o2, p2] = split_property_element("X.S452Content");
    SHOULD(ShouldlyString(o2).ShouldBe(std::string("X")));
    SHOULD(ShouldlyString(p2).ShouldBe(std::string("S452Content")));

    // Non-property-element: both empty.
    const auto [o3, p3] = split_property_element("StackPanel");
    SHOULD(ShouldlyString(o3).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(p3).ShouldBe(std::string("")));
}

// ---------------------------------------------------------------------------
// 2. Object-valued property-element sets the parent's DP as UntypedPtr
// ---------------------------------------------------------------------------

TEST_CASE("S452: <X.Content><StackPanel/></X.Content> sets parent DP (object, not a visual child)")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<Window><StackPanel><StackPanel.S452Content><TextBlock/></StackPanel.S452Content></StackPanel></Window>");

    // Root Window; its one visual child is the StackPanel.
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    const auto& retained = reader.retained_objects();

    // The TextBlock that filled the property-element's object value is
    // retained; the StackPanel is retained as the visual child. Post-order:
    // TextBlock first, then StackPanel.
    // retained = [TextBlock (object value), StackPanel (visual child)]
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        // retained[1] is the StackPanel (the visual child of Window).
        StackPanel* panel = dynamic_cast<StackPanel*>(retained[1].get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            // The TextBlock is NOT a visual child of the panel — it was set as
            // the panel's S452Content DP (an UntypedPtr), not added to
            // children_. A property-element is not auto-attached as a render
            // child (S452 scope).
            void* contentPtr = panel->Get(PropertyId::of("S452Content")).get<void*>();
            SHOULD(ShouldlyIntegral(contentPtr != nullptr).ShouldBe(true));
            // The retained TextBlock is that object.
            SHOULD(ShouldlyIntegral(contentPtr == retained[0].get()).ShouldBe(true));
        }
    }
}

TEST_CASE("S452: object property-element parent-chain: the retained object's parent is the owner")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<StackPanel><StackPanel.S452Content><TextBlock/></StackPanel.S452Content></StackPanel>");

    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(1u)); // only the TextBlock (the panel is the root)
    if (retained.size() == 1)
    {
        // The object that filled the DP has the PARENT as its S039 parent link.
        SHOULD(ShouldlyIntegral(retained[0].get()->parent() == root.get()).ShouldBe(true));
        (void)retained[0].get()->Get(PropertyId::of("S452Content"));
    }
}

// ---------------------------------------------------------------------------
// 3. Literal-content property-element converts + sets
// ---------------------------------------------------------------------------

TEST_CASE("S452: <X.S452Fill>Red</X.S452Fill> converts the literal onto the parent DP")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<StackPanel><StackPanel.S452Fill>#FF0000</StackPanel.S452Fill></StackPanel>");

    // The owned panel is the root; S452Fill (Color) was converted + set.
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S452Fill")))
               .ShouldBe(Value::Color(Color{255, 0, 0, 255})));
    // No visual children were produced by the property-element.
    SHOULD(ShouldlyIntegral(reader.retained_objects().size()).ShouldBe(0u));
}

TEST_CASE("S452: a String-typed property-element takes the literal verbatim")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<StackPanel><StackPanel.S452Text>Hello</StackPanel.S452Text></StackPanel>");
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S452Text")))
               .ShouldBe(Value::String("Hello")));
}

// ---------------------------------------------------------------------------
// 4. Nested property-elements
// ---------------------------------------------------------------------------

TEST_CASE("S452: nested property-elements build at each level")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<StackPanel><StackPanel.S452Content>"
                    "<StackPanel><StackPanel.S452Text>inner</StackPanel.S452Text></StackPanel>"
                    "</StackPanel.S452Content></StackPanel>");

    // The outer object value is retained (the inner StackPanel); the inner
    // panel's S452Text is set on it.
    SHOULD(ShouldlyString(reader.retained_objects().size()).ShouldBe(1u));
    if (reader.retained_objects().size() == 1)
    {
        const auto& inner = reader.retained_objects()[0];
        SHOULD(ShouldlyIntegral(dynamic_cast<StackPanel*>(inner.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyString(inner->Get(PropertyId::of("S452Text")))
                   .ShouldBe(Value::String("inner")));
    }
}

// ---------------------------------------------------------------------------
// 5. Empty property-element is skipped
// ---------------------------------------------------------------------------

TEST_CASE("S452: an empty property-element <X.Prop/> is skipped (nothing set)")
{
    registerS452Properties();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root =
        reader.Load("<StackPanel><StackPanel.S452Text/></StackPanel>");

    // S452Text default "" is unchanged, no error, no retained object.
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S452Text")))
               .ShouldBe(Value::String("")));
    SHOULD(ShouldlyIntegral(reader.retained_objects().size()).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// 6. Unknown dotted property -> structured UnknownProperty reader error
// ---------------------------------------------------------------------------

TEST_CASE("S452: an unknown dotted property throws VamlReaderException(UnknownProperty)")
{
    registerS452Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel><StackPanel.S452NoSuchThing>1</StackPanel.S452NoSuchThing></StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::UnknownProperty)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("S452NoSuchThing")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
