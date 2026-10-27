// S452 (dep:S451) — PROPERTY-ELEMENT authoring grammar, COMPILED-PATH (S7)
// parity. The S7 vamlcc flat-property-element grammar must reproduce the SAME
// DP-set outcome as the S2 text reader for BOTH value kinds:
//   (a) LITERAL-content `<Owner.Prop>Literal</...>` — folded into the parent's
//       attributes with propertyElement=true -> the S7 reader Sets the parent's
//       DP from the literal (exactly like S2's literal property-element).
//   (b) OBJECT-value `<Owner.Prop><Element/></...>` — emitted as a normal child
//       NODE marked propertyElementTarget=<Prop> -> the S7 reader Sets the
//       parent's DP to Value::UntypedPtr(child) and EXCLUDES it from the S121
//       visual-children transfer (matching S2's "not a visual child at S452").
// Plus: unknown-dotted -> the S7 VBamlException(UnknownProperty).
//
// DUAL-PATH PARITY via the S288 comparator is already proven in S444/AppShell;
// here we assert the S7 reader's property-element outcomes directly
// (headless, ADR-021), re-using the S452-unique DP registrations by name so both
// sides share the same global props.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h" // StackPanel/Window + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/tools/Vamlc.h"      // compile_to_bytes
#include "vaml/tools/VBamlReader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::tools::VBamlException;
using vaml::tools::VBamlError;
using vaml::tools::VBamlReader;

namespace {

std::unordered_set<std::string>& s7s452_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS7S452Global(const std::string& name, ValueType type,
                                               PropertyFlags flags, Value def)
{
    if (s7s452_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S452_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// Same prop names as the S2 test (PropertyElement_s452_test.cpp) so the two
// reader paths share one global definition set — the parity is on the VALUE, not
// on registration mechanics.
void registerS7S452()
{
    (void)registerS7S452Global("S452Content", ValueType::UntypedPtr, PropertyFlags::None,
                               Value::UntypedPtr(nullptr));
    (void)registerS7S452Global("S452Fill", ValueType::Color, PropertyFlags::None,
                               Value::Color(Color{0, 0, 0, 0}));
    vaml::register_builtin_elements();
}

} // namespace

// ---------------------------------------------------------------------------
// (a) LITERAL-content property-element on the S7 path
// ---------------------------------------------------------------------------

TEST_CASE("S452 (S7): a literal property-element compile+load Sets the parent's DP")
{
    registerS7S452();
    const std::string golden =
        "<StackPanel><StackPanel.S452Fill>#00FF00</StackPanel.S452Fill></StackPanel>";

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(golden);
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(bytes);

    // The folded literal (propertyElement flag) Set the parent StackPanel's DP.
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S452Fill")))
               .ShouldBe(Value::Color(Color{0, 255, 0, 255})));
    // No extra retained children (the literal is NOT an object child).
    SHOULD(ShouldlyString(reader.retained_objects().size()).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// (b) OBJECT-value property-element on the S7 path
// ---------------------------------------------------------------------------

TEST_CASE("S452 (S7): an object property-element Sets the parent DP, NOT a visual child")
{
    registerS7S452();
    const std::string golden =
        "<Window><StackPanel><StackPanel.S452Content><Text/></StackPanel.S452Content></StackPanel></Window>";

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(golden);
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(bytes);

    // Post-order retained: [Text (the object value), StackPanel (the Window's
    // visual child)].
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(2u));
    if (retained.size() == 2)
    {
        StackPanel* panel = dynamic_cast<StackPanel*>(retained[1].get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            // The Text landed on the panel's S452Content DP as an UntypedPtr...
            void* contentPtr = panel->Get(PropertyId::of("S452Content")).get<void*>();
            SHOULD(ShouldlyIntegral(contentPtr != nullptr).ShouldBe(true));
            // ...and IS the retained Text object (the same reader-owned instance).
            SHOULD(ShouldlyIntegral(contentPtr == retained[0].get()).ShouldBe(true));
        }
    }
}

// ---------------------------------------------------------------------------
// Unknown dotted property -> S7 UnknownProperty
// ---------------------------------------------------------------------------

TEST_CASE("S452 (S7): an unknown dotted property throws VBamlException(UnknownProperty)")
{
    registerS7S452();
    const std::string golden =
        "<StackPanel><StackPanel.S452NoSuchThing>1</StackPanel.S452NoSuchThing></StackPanel>";

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(golden);
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(bytes);
    }
    catch (const VBamlException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VBamlError::UnknownProperty)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
