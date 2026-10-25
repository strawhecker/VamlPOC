// S362 (dep:S361) — Style + Setter + BasedOn + PropertyTrigger + DataTrigger.
// The Sprint-9 style DATA MODEL + seams.
//
// Under test (per the S362 plan):
//   1. Setter round-trip + kinds — Literal (a Value), StaticResource, DynamicResource,
//      TemplateBinding each wrap their payload and report the right kind via
//      setter_value_kind + the kind predicates.
//   2. Style members — target_type, ordered setters (insertion order), triggers.
//   3. Trigger kind discrimination — TriggerKind::Property vs Data via
//      trigger_kind; a PropertyTrigger carries its PropertyId + match value;
//      a DataTrigger carries an S240 PropertyPath.
//   4. style_valid — non-empty target type is authoring-valid; an empty target
//      type is not.
//   5. BasedOn pointer wiring — a Style's based_on() reflects the parent Style*
//      it was given.
//
// Uses the Shouldly-for-doctest vocabulary. doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Style.h"

#include <memory>
#include <string>

using vaml::DataTrigger;
using vaml::PropertyId;
using vaml::PropertyPath;
using vaml::PropertyTrigger;
using vaml::Setter;
using vaml::SetterValue;
using vaml::Style;
using vaml::Trigger;
using vaml::TriggerKind;
using vaml::Value;
using vaml::setter_value_kind;
using vaml::style_valid;
using vaml::trigger_kind;
using vaml::test::ShouldThrow; // (unused but kept for the convention)

TEST_CASE("S362: SetterValue literal round-trip reports Literal kind")
{
    const SetterValue sv = SetterValue::Literal(Value::Int32(42));
    SHOULD(ShouldlyIntegral(static_cast<int>(sv.kind())).
               ShouldBe(static_cast<int>(SetterValue::Kind::Literal)));
    SHOULD(ShouldlyIntegral(static_cast<int>(setter_value_kind(sv))).
               ShouldBe(static_cast<int>(SetterValue::Kind::Literal)));
    SHOULD(ShouldlyIntegral(static_cast<int>(sv.is_literal())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(sv.is_static_resource())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(sv.is_dynamic_resource())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(sv.is_template_binding())).ShouldBe(0));
    SHOULD(ShouldlyString(sv.literal()).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S362: SetterValue resource kinds report their reference key")
{
    const SetterValue st = SetterValue::StaticResource("MyBrush");
    SHOULD(ShouldlyIntegral(static_cast<int>(st.kind())).
               ShouldBe(static_cast<int>(SetterValue::Kind::StaticResource)));
    SHOULD(ShouldlyIntegral(static_cast<int>(st.is_static_resource())).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(st.reference())).ShouldBe(std::string("MyBrush")));

    const SetterValue dyn = SetterValue::DynamicResource("LiveColor");
    SHOULD(ShouldlyIntegral(static_cast<int>(dyn.kind())).
               ShouldBe(static_cast<int>(SetterValue::Kind::DynamicResource)));
    SHOULD(ShouldlyIntegral(static_cast<int>(dyn.is_dynamic_resource())).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(dyn.reference())).ShouldBe(std::string("LiveColor")));

    const SetterValue tb = SetterValue::TemplateBinding("Background");
    SHOULD(ShouldlyIntegral(static_cast<int>(tb.kind())).
               ShouldBe(static_cast<int>(SetterValue::Kind::TemplateBinding)));
    SHOULD(ShouldlyIntegral(static_cast<int>(tb.is_template_binding())).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(tb.reference())).ShouldBe(std::string("Background")));
}

TEST_CASE("S362: a Setter carries a target property + a value")
{
    const Setter s{ PropertyId::of("Width"), SetterValue::Literal(Value::Int32(120)) };
    SHOULD(ShouldlyIntegral(static_cast<int>(s.targetProperty == PropertyId::of("Width"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(s.value.is_literal())).ShouldBe(1));
}

TEST_CASE("S362: a Style records target_type + ordered setters")
{
    Style style;
    style.set_target_type("Button");
    style.add_setter(Setter{ PropertyId::of("Width"), SetterValue::Literal(Value::Int32(100)) });
    style.add_setter(Setter{ PropertyId::of("Height"), SetterValue::Literal(Value::Int32(40)) });

    SHOULD(ShouldlyString(style.target_type()).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(style.setters().size()).ShouldBe(2u));
    // Insertion order is preserved.
    SHOULD(ShouldlyString(std::string(style.setters()[0].targetProperty.name())).
               ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(std::string(style.setters()[1].targetProperty.name())).
               ShouldBe(std::string("Height")));
}

TEST_CASE("S362: BasedOn pointer wiring")
{
    Style base;
    base.set_target_type("Button");
    Style derived;
    derived.set_target_type("Button");
    SHOULD(ShouldlyIntegral(derived.based_on() == nullptr).ShouldBe(true));

    derived.set_based_on(&base);
    SHOULD(ShouldlyIntegral(derived.based_on() == &base).ShouldBe(true));
}

TEST_CASE("S362: style_valid — non-empty target type is valid")
{
    Style valid;
    valid.set_target_type("Button");
    SHOULD(ShouldlyIntegral(static_cast<int>(style_valid(valid))).ShouldBe(1));

    Style invalid; // empty target type
    SHOULD(ShouldlyIntegral(static_cast<int>(style_valid(invalid))).ShouldBe(0));
}

TEST_CASE("S362: trigger kind discrimination — Property vs Data")
{
    PropertyTrigger pt;
    pt.property = PropertyId::of("IsEnabled");
    pt.value = Value::Bool(false);
    pt.setters.push_back(Setter{ PropertyId::of("Opacity"), SetterValue::Literal(Value::Double(0.5)) });
    const Trigger tProperty = Trigger::MakeProperty(pt);
    SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(tProperty))).
               ShouldBe(static_cast<int>(TriggerKind::Property)));
    SHOULD(ShouldlyString(tProperty.property.setters.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(
               static_cast<int>(tProperty.property.property == PropertyId::of("IsEnabled"))).
               ShouldBe(1));
    SHOULD(ShouldlyString(tProperty.property.value).ShouldBe(Value::Bool(false)));

    DataTrigger dt;
    dt.path = PropertyPath::parse("State");
    dt.value = Value::Int32(2);
    dt.setters.push_back(Setter{ PropertyId::of("Background"), SetterValue::Literal(
        Value::Color(vaml::Color{ 255, 0, 0, 255 })) });
    const Trigger tData = Trigger::MakeData(dt);
    SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(tData))).
               ShouldBe(static_cast<int>(TriggerKind::Data)));
    SHOULD(ShouldlyString(tData.data.setters.size()).ShouldBe(1u));
    // The S240 PropertyPath is carried on the DataTrigger.
    SHOULD(ShouldlyString(tData.data.path.segment_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(tData.data.path.original()).ShouldBe(std::string("State")));
}

TEST_CASE("S362: a Style carries PropertyTrigger and DataTrigger")
{
    Style style;
    style.set_target_type("Button");

    PropertyTrigger pt;
    pt.property = PropertyId::of("IsEnabled");
    pt.value = Value::Bool(false);
    style.add_trigger(Trigger::MakeProperty(pt));

    DataTrigger dt;
    dt.path = PropertyPath::parse("Role");
    dt.value = Value::String("admin");
    style.add_trigger(Trigger::MakeData(dt));

    SHOULD(ShouldlyString(style.triggers().size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(style.triggers()[0]))).
               ShouldBe(static_cast<int>(TriggerKind::Property)));
    SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(style.triggers()[1]))).
               ShouldBe(static_cast<int>(TriggerKind::Data)));
}
