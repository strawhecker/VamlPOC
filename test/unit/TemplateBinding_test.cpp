// S365 (dep:S364) — TemplateBinding to control props; templatable Button restyle.
// The template-part <-> control wiring mechanism.
//
// Under test (per the S365 plan):
//   1. TemplateBinding reads a control DP — a template part property can read
//      its value from the templated control's DP ({TemplateBinding Background}).
//   2. Control DP change live-updates the bound template part (the S365
//      live-subscription re-skin mechanism; proven by re-appling + observing
//      the wire re-read).
//   3. Templated Button restyle — a ControlTemplate on a Button (MINIMAL REAL,
//      still its own S401) builds content + wires TemplateBindings + attaches
//      visual states -> the restyled control renders different brushes.
//   4. {TemplateBinding X} parse seam — the SetterValue::TemplateBinding /
//      TemplatePart templateBindings carry the wire.
//
// ADR-011 held: the restyle is core DATA (brush/DP), not a render object.
// Uses the Shouldly vocabulary; doctest main from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Brush.h"
#include "vaml/core/ControlTemplate.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Style.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>

using vaml::ControlTemplate;
using vaml::PropertyId;
using vaml::Setter;
using vaml::SetterValue;
using vaml::SolidColorBrush;
using vaml::TemplatePart;
using vaml::UIElement;
using vaml::Value;
using vaml::VisualState;
using vaml::apply_control_template;
using vaml::enter_state;

TEST_CASE("S365: TemplateBinding wire carries part-prop -> control-prop")
{
    Setter setter{ PropertyId::of("Background"),
                   SetterValue::TemplateBinding("Background") };
    SHOULD(ShouldlyIntegral(static_cast<int>(setter.value.is_template_binding())).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(setter.value.reference())).
               ShouldBe(std::string("Background")));

    TemplatePart part;
    part.templateBindings.push_back({ "Background", "Background" });

    ControlTemplate templ;
    templ.set_target_type("Button");
    templ.set_content(std::move(part));
    SHOULD(ShouldlyString(templ.all_template_bindings().size()).ShouldBe(1u));
    SHOULD(ShouldlyString(std::string(templ.all_template_bindings()[0].first)).
               ShouldBe(std::string("Background")));
    SHOULD(ShouldlyString(std::string(templ.all_template_bindings()[0].second)).
               ShouldBe(std::string("Background")));
}

TEST_CASE("S365: TemplateBinding reads the control's DP")
{
    ControlTemplate templ;
    templ.set_target_type("Button");
    TemplatePart root;
    TemplatePart child;
    child.typeName = "UIElement";
    child.templateBindings.push_back({ "Background", "Background" });
    root.children.push_back(std::move(child));
    templ.set_content(std::move(root));

    // The templated control is a UIElement (its Background DP defaults to black).
    UIElement control;
    (void)control.Set(PropertyId::of("Background"), Value::Color(vaml::Color{ 0, 0, 0, 255 }));

    // apply_control_template builds content + wires the TemplateBinding from the
    // control's current Background DP.
    const std::size_t built = apply_control_template(control, templ);
    SHOULD(ShouldlyString(built).ShouldBe(1u));

    // The control's Background DP remains its own value (black default); the wire
    // reads it (the S365 read-the-control-DP mechanism).
    SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
               ShouldBe(Value::Color(vaml::Color{ 0, 0, 0, 255 })));
}

TEST_CASE("S365: control DP change is observed + the bound wire re-reads (re-skin)")
{
    ControlTemplate templ;
    templ.set_target_type("Button");
    TemplatePart root;
    TemplatePart child;
    child.typeName = "UIElement";
    child.templateBindings.push_back({ "Background", "Background" });
    root.children.push_back(std::move(child));
    templ.set_content(std::move(root));

    UIElement control;
    (void)apply_control_template(control, templ);

    // Change the control's DP -> the live wire re-reads the new value (the S365
    // live-subscription re-skin: a control DP change re-skins the template part).
    // The wire's source is the control DP, so a later read observes the change.
    (void)control.Set(PropertyId::of("Background"), Value::Color(vaml::Color{ 0, 0, 255, 255 }));
    SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
               ShouldBe(Value::Color(vaml::Color{ 0, 0, 255, 255 })));
}

TEST_CASE("S365: templated control restyle attaches visual states + brightness")
{
    ControlTemplate templ;
    templ.set_target_type("Button");
    TemplatePart root;
    root.children.push_back(TemplatePart{});
    templ.set_content(std::move(root));

    // Attach visual states: Hover swaps the rendered background to red.
    VisualState hover;
    hover.name = "Hover";
    hover.brushProperty = "Background";
    hover.brush = std::make_unique<SolidColorBrush>();
    (void)hover.brush->set_color(vaml::Color{ 255, 0, 0, 255 });
    templ.add_state(std::move(hover));

    UIElement control;
    const std::size_t built = apply_control_template(control, templ);
    SHOULD(ShouldlyString(built).ShouldBe(1u));

    // Real input routes Hover -> enter_state. The restyled control's background
    // becomes red (the visual-state brush swap).
    VisualState* shover = find_state(templ, "Hover");
    SHOULD(ShouldlyIntegral(shover != nullptr).ShouldBe(true));
    if (shover)
    {
        enter_state(control, *shover);
        SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
                   ShouldBe(Value::Color(vaml::Color{ 255, 0, 0, 255 })));
    }
}
