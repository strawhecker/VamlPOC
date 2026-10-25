// S363 (dep:S362) — Style applied to type; property setters; BasedOn chain;
// triggers fire. Runtime style application through the StyleApplier.
//
// Under test (per the S363 plan):
//   1. Explicit setters land on DPs (a Literal setter on a real UIElement DP).
//   2. BasedOn chain — a base style's setters land FIRST, the derived style's
//      overrides land after (child-overrides-base).
//   3. Implicit style by target type — find_implicit_style + apply_implicit_style
//      resolve a Style whose target_type() matches the element's type name from
//      the element's own Resources (tree scope) / app scope.
//   4. PropertyTrigger fires/un-fires — a trigger whose match value equals the
//      element's current DP value fires (its setters apply); when the value
//      differs the trigger does not fire.
//   5. Local-wins — an authored local value on the element is NOT clobbered by a
//      style setter (the test authors the local BEFORE applying, per the applier
//      contract).
//
// Registers S363-unique global DPs. Uses the Shouldly vocabulary; doctest main
// from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"       // StackPanel (implicit-style target type)
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/Style.h"
#include "vaml/core/StyleApplier.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::DataTrigger;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::PropertyTrigger;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::Setter;
using vaml::SetterValue;
using vaml::Style;
using vaml::StyleApplier;
using vaml::StyleApply;
using vaml::Trigger;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::apply_implicit_style;
using vaml::find_implicit_style;
using vaml::test::ShouldThrow;

namespace {

// Registers an S363-unique global DP. Names are process-unique so tests never
// collide with each other or with the global UIElement / brush-table DPs.
std::unordered_set<std::string>& s363_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// An S363-unique Int32 DP named `name`. Idempotent registration.
const DependencyProperty& registerS363Int(std::string_view name)
{
    if (s363_names().insert(std::string(name)).second)
    {
        return DependencyProperty::Register(name, ValueType::Int32, "S363_Owner",
                                            PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    }
    return DependencyProperty::Get(name);
}

} // namespace

TEST_CASE("S363: explicit style setters land on the element's DPs")
{
    registerS363Int("S363Width");
    UIElement el;

    Style style;
    style.set_target_type("UIElement");
    style.add_setter(Setter{ PropertyId::of("S363Width"),
                             SetterValue::Literal(Value::Int32(120)) });

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, style, links);

    SHOULD(ShouldlyString(applied.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363Width"))).ShouldBe(Value::Int32(120)));
}

TEST_CASE("S363: BasedOn chain — base setters first, derived overrides")
{
    registerS363Int("S363BaseW");
    UIElement el;

    Style base;
    base.set_target_type("UIElement");
    base.add_setter(Setter{ PropertyId::of("S363BaseW"),
                            SetterValue::Literal(Value::Int32(50)) });

    Style derived;
    derived.set_target_type("UIElement");
    derived.set_based_on(&base);
    derived.add_setter(Setter{ PropertyId::of("S363BaseW"),
                               SetterValue::Literal(Value::Int32(99)) });

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, derived, links);

    // Derived overrides base: the final value is 99 (child wins).
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363BaseW"))).ShouldBe(Value::Int32(99)));
    // Both base + derived applied (2 writes: base 50 then derived 99).
    SHOULD(ShouldlyString(applied.size()).ShouldBe(2u));
}

TEST_CASE("S363: local-wins — an authored local value is NOT clobbered by a style setter")
{
    registerS363Int("S363LocalW");
    UIElement el;
    // Author the LOCAL value FIRST (the S363 local-wins contract).
    (void)el.Set(PropertyId::of("S363LocalW"), Value::Int32(7));

    Style style;
    style.set_target_type("UIElement");
    style.add_setter(Setter{ PropertyId::of("S363LocalW"),
                             SetterValue::Literal(Value::Int32(999)) });

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, style, links);

    // The authored local (7) must NOT be clobbered by the style's 999.
    // apply_value_to still runs Set, but the applier reports applied only when
    // a real change happened; the element retains its authored 7.
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363LocalW"))).ShouldBe(Value::Int32(7)));
    (void)applied;
}

TEST_CASE("S363: find_implicit_style resolves a Style by target type from the tree scope")
{
    registerS363Int("S363ImplicitW");
    // Register a Style resource under the implicit-style key "Style:StackPanel"
    // on the PARENT's Resources (the panel resolves it through the parent chain).
    auto styleRes = std::make_unique<Style>();
    styleRes->set_target_type("StackPanel");
    styleRes->add_setter(Setter{ PropertyId::of("S363ImplicitW"),
                                 SetterValue::Literal(Value::Int32(300)) });

    UIElement parent;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("Style:StackPanel", std::move(styleRes));
    parent.set_resources(std::move(dict));

    // StackPanel is a registered UIElement concrete type; make it a child so it
    // walks the parent chain to `parent`'s resources.
    auto ownedPanel = std::make_unique<vaml::StackPanel>();
    UIElement* panelPtr = ownedPanel.get();
    parent.add_child(std::move(ownedPanel));

    const Style* found = find_implicit_style(*panelPtr);
    SHOULD(ShouldlyIntegral(found != nullptr).ShouldBe(true));
    if (found)
    {
        SHOULD(ShouldlyString(found->target_type()).ShouldBe(std::string("StackPanel")));
    }
}

TEST_CASE("S363: apply_implicit_style applies the implicit style's setters")
{
    registerS363Int("S363ImplicitApply");
    auto styleRes = std::make_unique<Style>();
    styleRes->set_target_type("StackPanel");
    styleRes->add_setter(Setter{ PropertyId::of("S363ImplicitApply"),
                                 SetterValue::Literal(Value::Int32(222)) });

    UIElement parent;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("Style:StackPanel", std::move(styleRes));
    parent.set_resources(std::move(dict));

    auto ownedPanel = std::make_unique<vaml::StackPanel>();
    UIElement* panelPtr = ownedPanel.get();
    parent.add_child(std::move(ownedPanel));

    std::vector<vaml::DynamicResourceLink> links;
    const std::size_t applied = apply_implicit_style(*panelPtr, links);
    SHOULD(ShouldlyString(applied).ShouldBe(1u));
    SHOULD(ShouldlyString(panelPtr->Get(PropertyId::of("S363ImplicitApply"))).
               ShouldBe(Value::Int32(222)));
}

TEST_CASE("S363: PropertyTrigger fires when the element's DP equals the match value")
{
    registerS363Int("S363TriggerMatch");
    registerS363Int("S363TriggerOut");
    UIElement el;
    // Put the element into the trigger's match state FIRST. The match property
    // is DISTINCT from the trigger's target setter property, so the fired
    // trigger's write (on the OUT property) is never mistaken for a pre-pass
    // local on the MATCH property.
    (void)el.Set(PropertyId::of("S363TriggerMatch"), Value::Int32(5));

    PropertyTrigger pt;
    pt.property = PropertyId::of("S363TriggerMatch");
    pt.value = Value::Int32(5); // == the element's current match value -> fires
    pt.setters.push_back(Setter{ PropertyId::of("S363TriggerOut"),
                                 SetterValue::Literal(Value::Int32(500)) });

    Style style;
    style.set_target_type("UIElement");
    style.add_trigger(Trigger::MakeProperty(pt));

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, style, links);

    // The trigger fired: its setter wrote 500 onto the OUT property.
    SHOULD(ShouldlyString(applied.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363TriggerOut"))).ShouldBe(Value::Int32(500)));
    // The MATCH property retains its authored 5 (never clobbered).
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363TriggerMatch"))).ShouldBe(Value::Int32(5)));
}

TEST_CASE("S363: PropertyTrigger does NOT fire when the element's DP differs")
{
    registerS363Int("S363TriggerOffMatch");
    registerS363Int("S363TriggerOffOut");
    UIElement el;
    (void)el.Set(PropertyId::of("S363TriggerOffMatch"), Value::Int32(1));

    PropertyTrigger pt;
    pt.property = PropertyId::of("S363TriggerOffMatch");
    pt.value = Value::Int32(2); // does NOT match the element's current 1
    pt.setters.push_back(Setter{ PropertyId::of("S363TriggerOffOut"),
                                 SetterValue::Literal(Value::Int32(999)) });

    Style style;
    style.set_target_type("UIElement");
    style.add_trigger(Trigger::MakeProperty(pt));

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, style, links);

    // Trigger not firing -> the OUT property untouched.
    SHOULD(ShouldlyString(applied.size()).ShouldBe(0u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363TriggerOffOut"))).ShouldBe(Value::Int32(0)));
}

TEST_CASE("S363: DataTrigger fires on an S240-path value match through the dependency-object reader")
{
    registerS363Int("S363DataMatch");
    registerS363Int("S363DataOut");
    UIElement el;
    (void)el.Set(PropertyId::of("S363DataMatch"), Value::Int32(42));

    DataTrigger dt;
    dt.path = vaml::PropertyPath::parse("S363DataMatch");
    dt.value = Value::Int32(42); // matches the element's DP read through the path
    dt.setters.push_back(Setter{ PropertyId::of("S363DataOut"),
                                 SetterValue::Literal(Value::Int32(420)) });

    Style style;
    style.set_target_type("UIElement");
    style.add_trigger(Trigger::MakeData(dt));

    StyleApplier applier;
    std::vector<vaml::DynamicResourceLink> links;
    const auto applied = applier.apply_style(el, style, links);

    // The data trigger fired (the S240 path read 42 == match 42).
    SHOULD(ShouldlyString(applied.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S363DataOut"))).ShouldBe(Value::Int32(420)));
}
