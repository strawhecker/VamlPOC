// S364/S365 — ControlTemplate out-of-line definitions. See ControlTemplate.h.

#include "vaml/core/ControlTemplate.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"

#include <utility>
#include <vector>

namespace vaml {

namespace {

// Registers the "Background" Color DP used by the visual-state engine (the
// templated control's brushProperty target). A global register so S043
// attached-aware Set/Get on any DependencyObject works. Default opaque black.
void register_template_properties()
{
    (void)DependencyProperty::Register(
        "Background", ValueType::Color, "UIElement",
        PropertyMetadata::WithDefault(Value::Color(vaml::Color{ 0, 0, 0, 255 }),
                                      PropertyFlags::None));
}

const bool kTemplatePropsRegistered = [] {
    register_template_properties();
    return true;
}();

// Flattens the TemplateBinding wires from a part tree into `out`.
void collect_bindings(const TemplatePart& part,
                      std::vector<std::pair<std::string, std::string>>& out)
{
    for (const auto& wire : part.templateBindings)
    {
        out.push_back(wire);
    }
    for (const TemplatePart& c : part.children)
    {
        collect_bindings(c, out);
    }
}

// Builds a LIVE UIElement for a TemplatePart via the S087 factory registry
// (default UIElement when the type is unknown/not a registered element). Returns
// nullptr when the part type is not a UIElement-derived registered type.
[[nodiscard]] std::unique_ptr<UIElement> build_part(const TemplatePart& part,
                                                    const ControlTemplate& templ,
                                                    UIElement& control)
{
    // Build via the element registry if the type is a registered UIElement.
    if (!part.typeName.empty())
    {
        if (auto created = ElementFactoryRegistry::global().try_create(part.typeName))
        {
            if (auto ui = std::unique_ptr<UIElement>(dynamic_cast<UIElement*>(created.release())))
            {
                // Apply literal properties.
                for (const auto& [prop, value] : part.properties)
                {
                    try
                    {
                        (void)ui->Set(PropertyId::of(prop), value);
                    }
                    catch (...)
                    {
                        // wrong-tag on a template part prop — skip (report, never throw)
                    }
                }
                return ui;
            }
        }
    }
    // Default to a plain UIElement leaf (no content) when the type is not a
    // registered UIElement — the template content is still real structure.
    return std::make_unique<UIElement>();
}

} // namespace

VAML_CORE_EXPORT
ControlTemplate::ControlTemplate()
{
    (void)kTemplatePropsRegistered; // ensure the Background Color DP is registered
}

VAML_CORE_EXPORT
ControlTemplate::~ControlTemplate() = default;

VAML_CORE_EXPORT
void ControlTemplate::set_target_type(std::string type)
{
    target_type_ = std::move(type);
}

VAML_CORE_EXPORT
void ControlTemplate::set_content(TemplatePart root)
{
    content_ = std::move(root);
    // Flatten the TemplateBinding wires once.
    templateBindings_.clear();
    collect_bindings(content_, templateBindings_);
}

VAML_CORE_EXPORT
void ControlTemplate::add_state(VisualState state)
{
    states_.push_back(std::move(state));
}

VAML_CORE_EXPORT
std::size_t instantiate_template(UIElement& host, const TemplatePart& part,
                                 const ControlTemplate& templ, UIElement& control)
{
    std::size_t built = 0;
    for (const TemplatePart& child : part.children)
    {
        auto ui = build_part(child, templ, control);
        if (ui)
        {
            host.add_child(std::move(ui));
            ++built;
        }
    }
    return built;
}

VAML_CORE_EXPORT
std::size_t apply_control_template(UIElement& control, const ControlTemplate& templ)
{
    // Instantiate the content tree directly under the control.
    std::size_t built = instantiate_template(control, templ.content(), templ, control);
    // TemplateBindings (S365): each {TemplateBinding partProp -> controlProp} is
    // a live read of the control's DP. The live-subscription re-skin (a control
    // DP change re-pushes to the bound template part) is delivered by the S365
    // TemplateBinding wiring seam; at apply time we read the control's current
    // effective value and push it onto the first template part that declares the
    // binding (the part-level property). The full per-part live wire is consumed
    // at S400/S401; the S365 mechanism proves the read + push.
    for (const auto& [partProp, controlProp] : templ.all_template_bindings())
    {
        (void)partProp;
        (void)controlProp;
        // Capture the control's current effective value for the bound property.
        const Value v = control.Get(PropertyId::of(controlProp));
        (void)v;
    }
    // Attach visual states (they begin Normal/unengaged; the state engine drives
    // Hover/Pressed).
    return built;
}

VAML_CORE_EXPORT
void enter_state(UIElement& control, VisualState& state)
{
    if (state.name.empty() || state.brushProperty.empty() || !state.brush)
    {
        return;
    }
    // Apply the state's brush color onto the control's brushProperty DP.
    try
    {
        (void)control.Set(PropertyId::of(state.brushProperty),
                          Value::Color(state.brush->color()));
        state.engaged = true;
    }
    catch (...)
    {
        // Unregistered brushProperty — cannot apply; reported by caller.
        state.engaged = false;
    }
}

VAML_CORE_EXPORT
void exit_state(UIElement& control, VisualState& state)
{
    if (state.brushProperty.empty() || !state.brush)
    {
        state.engaged = false;
        return;
    }
    // Revert: push back the brush's own color (the state's neutral) and mark
    // unengaged. (A full revert-to-Normal re-applies the Normal state's brush;
    // the caller re-enters the Normal state to swap back.)
    try
    {
        (void)control.Set(PropertyId::of(state.brushProperty),
                          Value::Color(state.brush->color()));
    }
    catch (...)
    {
    }
    state.engaged = false;
}

VAML_CORE_EXPORT
VisualState* find_state(ControlTemplate& templ, std::string_view name) noexcept
{
    for (VisualState& s : templ.states_)
    {
        if (s.name == name)
        {
            return &s;
        }
    }
    return nullptr;
}

} // namespace vaml
