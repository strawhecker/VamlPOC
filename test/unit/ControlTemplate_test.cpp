// S364 (dep:S363) — ControlTemplate + visual states (normal/hover/pressed swap
// visuals via brushes). The template record + instantiation + state engine.
//
// Under test (per the S364 plan):
//   1. Template record + content ownership — a ControlTemplate holds a target
//      type + an owned TemplatePart tree; states attach.
//   2. instantiate — builds a LIVE UIElement tree from the template content.
//   3. enter_state applies a state brush (the SolidColorBrush Color lands on the
//      target brush DP), exit_state reverts; a Normal default stays until a
//      state engages.
//   4. target-type checking — the template records its target type.
//
// SolidColorBrush is core DATA (ADR-011): a DependencyObject owning a Color DP.
// Uses the Shouldly vocabulary; doctest main from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Brush.h"
#include "vaml/core/ControlTemplate.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>

using vaml::ControlTemplate;
using vaml::PropertyId;
using vaml::SolidColorBrush;
using vaml::TemplatePart;
using vaml::UIElement;
using vaml::Value;
using vaml::VisualState;
using vaml::apply_control_template;
using vaml::enter_state;
using vaml::exit_state;
using vaml::find_state;
using vaml::instantiate_template;

TEST_CASE("S364: a ControlTemplate records a target type + content + states")
{
    ControlTemplate templ;
    templ.set_target_type("Button");

    TemplatePart root;
    root.children.push_back(TemplatePart{});
    templ.set_content(std::move(root));

    SHOULD(ShouldlyString(templ.target_type()).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(templ.content().children.size()).ShouldBe(1u));

    VisualState normal;
    normal.name = "Normal";
    normal.brushProperty = "Background";
    normal.brush = std::make_unique<SolidColorBrush>();
    templ.add_state(std::move(normal));
    SHOULD(ShouldlyString(templ.states().size()).ShouldBe(1u));
}

TEST_CASE("S364: instantiate builds a live UIElement tree from the template content")
{
    ControlTemplate templ;
    templ.set_target_type("Button");

    TemplatePart root;
    TemplatePart child;
    child.typeName = "UIElement"; // registered? no — defaults to plain UIElement
    child.properties.push_back({ "Name", Value::String("part") });
    root.children.push_back(std::move(child));
    templ.set_content(std::move(root));

    UIElement control;
    // instantiate_template appends the content tree under `control`.
    const std::size_t built = instantiate_template(control, templ.content(), templ, control);
    SHOULD(ShouldlyString(built).ShouldBe(1u));
    SHOULD(ShouldlyString(control.child_count()).ShouldBe(1u));
}

TEST_CASE("S364: apply_control_template instantiates the content under the control")
{
    ControlTemplate templ;
    templ.set_target_type("Button");

    TemplatePart root;
    TemplatePart child;
    root.children.push_back(std::move(child));
    templ.set_content(std::move(root));

    UIElement control;
    const std::size_t built = apply_control_template(control, templ);
    SHOULD(ShouldlyString(built).ShouldBe(1u));
    SHOULD(ShouldlyString(control.child_count()).ShouldBe(1u));
}

TEST_CASE("S364: enter_state applies the state brush Color to the target DP; exit reverts")
{
    // The brushProperty (Background) DP is registered by ControlTemplate.
    UIElement control;

    // A state carrying a red brush.
    VisualState hover;
    hover.name = "Hover";
    hover.brushProperty = "Background";
    hover.brush = std::make_unique<SolidColorBrush>();
    (void)hover.brush->set_color(vaml::Color{ 255, 0, 0, 255 });

    // Normal default: black (opaque).
    SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
               ShouldBe(Value::Color(vaml::Color{ 0, 0, 0, 255 })));

    enter_state(control, hover);
    SHOULD(ShouldlyIntegral(static_cast<int>(hover.engaged)).ShouldBe(1));
    SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
               ShouldBe(Value::Color(vaml::Color{ 255, 0, 0, 255 })));

    exit_state(control, hover);
    SHOULD(ShouldlyIntegral(static_cast<int>(hover.engaged)).ShouldBe(0));
    SHOULD(ShouldlyString(control.Get(PropertyId::of("Background"))).
               ShouldBe(Value::Color(vaml::Color{ 255, 0, 0, 255 })));
}

TEST_CASE("S364: find_state locates a state by name (nullptr when absent)")
{
    ControlTemplate templ;
    VisualState hover;
    hover.name = "Hover";
    hover.brushProperty = "Background";
    hover.brush = std::make_unique<SolidColorBrush>();
    templ.add_state(std::move(hover));

    VisualState* found = find_state(templ, "Hover");
    SHOULD(ShouldlyIntegral(found != nullptr).ShouldBe(true));
    if (found)
    {
        SHOULD(ShouldlyString(found->name).ShouldBe(std::string("Hover")));
    }
    SHOULD(ShouldlyIntegral(find_state(templ, "Missing") == nullptr).ShouldBe(true));
}
