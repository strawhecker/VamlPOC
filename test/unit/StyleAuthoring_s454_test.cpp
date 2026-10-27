// S454 (dep:S453) — STYLE AUTHORING + TRIGGERS (the S362/S363 engine AUTHORED
// from .vaml, was engine-only). The S2 text reader authors:
//   * `<Style TargetType="Button" BasedOn="{StaticResource base}">` — the S362
//     Style data model from markup (target_type, base wired at end of Load),
//   * `<Setter Property="..." Value="..."/>` — literal / {StaticResource} /
//     {DynamicResource} / {TemplateBinding} SetterValues in authored order,
//   * `<Style.Triggers>` with `<PropertyTrigger Property Value>` (+child setters)
//     and `<DataTrigger Binding Value>` (+child setters) — S362 Discrimination,
//   * explicit-vs-implicit application: an x:Key'd Style is applied explicitly
//     via apply_style; a Style with NO x:Key whose TargetType matches an
//     ElementFactoryRegistry type applies IMPLICITLY via apply_implicit_style
//     (registered under the "Style:{TargetType}" key).
//
// ADR-018 zero locks; ADR-021 headless (drives the real VamlReader).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"               // StackPanel + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/Style.h"
#include "vaml/core/StyleApplier.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::Setter;
using vaml::SetterValue;
using vaml::StackPanel;
using vaml::Style;
using vaml::StyleApplier;
using vaml::StyleApply;
using vaml::Trigger;
using vaml::TriggerKind;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::apply_implicit_style;
using vaml::find_implicit_style;
using vaml::setter_value_kind;
using vaml::trigger_kind;

namespace {

std::unordered_set<std::string>& s454_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS454Int(std::string_view name)
{
    if (s454_names().insert(std::string(name)).second)
    {
        return DependencyProperty::Register(name, ValueType::Int32, "S454_Owner",
                                            PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    }
    return DependencyProperty::Get(name);
}

void registerS454()
{
    (void)registerS454Int("S454Size");
    (void)registerS454Int("S454Hover");
    vaml::register_builtin_elements();
}

const ResourceDictionary* authoredDict(StackPanel* panel)
{
    return vaml::detail::get_element_resources(panel);
}

const Style* findStyle(const ResourceDictionary* dict, std::string_view key)
{
    if (dict == nullptr)
    {
        return nullptr;
    }
    const ResourceValue* v = dict->try_get(key);
    if (v == nullptr || v->kind() != ResourceValue::Kind::Object)
    {
        return nullptr;
    }
    return dynamic_cast<const Style*>(v->object());
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Authored Style: TargetType + Setters
// ---------------------------------------------------------------------------

TEST_CASE("S454: <Style TargetType Setter> is authored into the S362 model")
{
    registerS454();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"S\" TargetType=\"StackPanel\">"
        "<Setter Property=\"S454Size\" Value=\"42\"/>"
        "<Setter Property=\"S454Hover\" Value=\"1\"/>"
        "</Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const Style* s = findStyle(authoredDict(panel), "S");
    SHOULD(ShouldlyIntegral(s != nullptr).ShouldBe(true));
    if (s)
    {
        SHOULD(ShouldlyString(s->target_type()).ShouldBe(std::string("StackPanel")));
        SHOULD(ShouldlyString(s->setters().size()).ShouldBe(2u));
        if (s->setters().size() == 2)
        {
            // First setter: S454Size literal 42.
            SHOULD(ShouldlyString(s->setters()[0].targetProperty.name()).ShouldBe(std::string("S454Size")));
            SHOULD(ShouldlyIntegral(static_cast<int>(setter_value_kind(s->setters()[0].value)))
                       .ShouldBe(static_cast<int>(SetterValue::Kind::Literal)));
            SHOULD(ShouldlyString(s->setters()[0].value.literal()).ShouldBe(Value::Int32(42)));
            // Second setter: S454Hover literal 1.
            SHOULD(ShouldlyString(s->setters()[1].targetProperty.name()).ShouldBe(std::string("S454Hover")));
            SHOULD(ShouldlyString(s->setters()[1].value.literal()).ShouldBe(Value::Int32(1)));
        }
    }
}

// ---------------------------------------------------------------------------
// 2. apply_style applies an AUTHORED style's literal setters to a UIElement
// ---------------------------------------------------------------------------

TEST_CASE("S454: apply_style applies the authored style's setters to a UIElement")
{
    registerS454();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"S\" TargetType=\"UIElement\">"
        "<Setter Property=\"S454Size\" Value=\"77\"/>"
        "</Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const Style* s = findStyle(authoredDict(panel), "S");
    SHOULD(ShouldlyIntegral(s != nullptr).ShouldBe(true));
    if (s)
    {
        UIElement target;
        StyleApplier applier;
        std::vector<vaml::DynamicResourceLink> links;
        const std::vector<StyleApply> applied = applier.apply_style(target, *s, links);
        SHOULD(ShouldlyString(applied.size()).ShouldBe(1u));
        SHOULD(ShouldlyString(target.Get(PropertyId::of("S454Size"))).ShouldBe(Value::Int32(77)));
    }
}

// ---------------------------------------------------------------------------
// 3. Authored BasedOn chain: derived overrides base
// ---------------------------------------------------------------------------

TEST_CASE("S454: authored BasedOn resolves; apply walks base-first then child overrides")
{
    registerS454();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"base\" TargetType=\"UIElement\"><Setter Property=\"S454Size\" Value=\"10\"/></Style>"
        "<Style x:Key=\"derived\" TargetType=\"UIElement\" BasedOn=\"{StaticResource base}\"><Setter Property=\"S454Size\" Value=\"99\"/></Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const Style* base = findStyle(authoredDict(panel), "base");
    const Style* derived = findStyle(authoredDict(panel), "derived");
    SHOULD(ShouldlyIntegral(base != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(derived != nullptr).ShouldBe(true));
    if (base && derived)
    {
        // BasedOn was wired at end-of-Load (resolve_deferred_based_on).
        SHOULD(ShouldlyIntegral(derived->based_on() == base).ShouldBe(true));

        UIElement target;
        StyleApplier applier;
        std::vector<vaml::DynamicResourceLink> links;
        (void)applier.apply_style(target, *derived, links);
        // Base sets 10 then derived overrides to 99 (child wins).
        SHOULD(ShouldlyString(target.Get(PropertyId::of("S454Size"))).ShouldBe(Value::Int32(99)));
    }
}

// ---------------------------------------------------------------------------
// 4. Authored PropertyTrigger + DataTrigger
// ---------------------------------------------------------------------------

TEST_CASE("S454: authored <Style.Triggers> produces PropertyTrigger + DataTrigger records")
{
    registerS454();
    registerS454Int("S454Path");
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"S\" TargetType=\"UIElement\">"
        "<Style.Triggers>"
        "<PropertyTrigger Property=\"S454Hover\" Value=\"1\"><Setter Property=\"S454Size\" Value=\"5\"/></PropertyTrigger>"
        "<DataTrigger Binding=\"IsActive\" Value=\"true\"><Setter Property=\"S454Size\" Value=\"9\"/></DataTrigger>"
        "</Style.Triggers>"
        "</Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const Style* s = findStyle(authoredDict(panel), "S");
    SHOULD(ShouldlyIntegral(s != nullptr).ShouldBe(true));
    if (s)
    {
        SHOULD(ShouldlyString(s->triggers().size()).ShouldBe(2u));
        if (s->triggers().size() >= 1)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(s->triggers()[0])))
                       .ShouldBe(static_cast<int>(TriggerKind::Property)));
            SHOULD(ShouldlyString(s->triggers()[0].property.property.name()).ShouldBe(std::string("S454Hover")));
            SHOULD(ShouldlyString(s->triggers()[0].property.value).ShouldBe(Value::Int32(1)));
            if (!s->triggers()[0].property.setters.empty())
            {
                SHOULD(ShouldlyString(s->triggers()[0].property.setters[0].value.literal())
                           .ShouldBe(Value::Int32(5)));
            }
        }
        if (s->triggers().size() >= 2)
        {
            SHOULD(ShouldlyIntegral(static_cast<int>(trigger_kind(s->triggers()[1])))
                       .ShouldBe(static_cast<int>(TriggerKind::Data)));
            SHOULD(ShouldlyIntegral(static_cast<int>(s->triggers()[1].data.path.valid())).ShouldBe(1));
            SHOULD(ShouldlyString(s->triggers()[1].data.value).ShouldBe(Value::String("true")));
        }
    }
}

// ---------------------------------------------------------------------------
// 5. Implicit style (no x:Key) applies by target type; explicit does not auto-apply
// ---------------------------------------------------------------------------

TEST_CASE("S454: implicit <Style> (no x:Key) is registered under Style:{TargetType} + applies")
{
    registerS454();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style TargetType=\"StackPanel\"><Setter Property=\"S454Hover\" Value=\"3\"/></Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    // The implicit style is registered under "Style:StackPanel" (the S363 key).
    const Style* impl = findStyle(authoredDict(panel), "Style:StackPanel");
    SHOULD(ShouldlyIntegral(impl != nullptr).ShouldBe(true));

    // find_implicit_style resolves it by the panel's element type name
    // (walks the panel's OWN authored Resources — that is the S360 tree scope).
    SHOULD(ShouldlyIntegral(find_implicit_style(*panel) != nullptr).ShouldBe(true));

    // apply_implicit_style applies its setters to the panel.
    std::vector<vaml::DynamicResourceLink> links;
    const std::size_t applied = apply_implicit_style(*panel, links);
    SHOULD(ShouldlyIntegral(static_cast<int>(applied > 0u)).ShouldBe(1));
    SHOULD(ShouldlyString(panel->Get(PropertyId::of("S454Hover"))).ShouldBe(Value::Int32(3)));
}

TEST_CASE("S454: an explicitly-keyed style does NOT auto-apply (explicit only)")
{
    registerS454();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"Explicit\" TargetType=\"StackPanel\"><Setter Property=\"S454Hover\" Value=\"8\"/></Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    // An explicitly-keyed style is NOT an implicit style (the implicit lookup
    // "Style:StackPanel" does not resolve it — the panel's dict has "Explicit",
    // not "Style:StackPanel").
    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(find_implicit_style(*panel) == nullptr).ShouldBe(true));
    // And a plain build does NOT apply it (explicit styles apply only via
    // apply_style) — the panel's S454Hover keeps its default 0.
    SHOULD(ShouldlyString(panel->Get(PropertyId::of("S454Hover"))).ShouldBe(Value::Int32(0)));
}
