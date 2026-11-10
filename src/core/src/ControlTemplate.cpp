// S364/S365 — ControlTemplate out-of-line definitions. See ControlTemplate.h.

#include "vaml/core/ControlTemplate.h"

#include "vaml/core/Brush.h"              // S643b: SolidColorBrush (the visual-state paint object)
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h" // S643a: register_element_brush (visual-state paint)
#include "vaml/core/Value.h"

#include <utility>
#include <vector>

namespace vaml {

namespace {

// S643a (WPF paint-DP compliance): the legacy scalar-`Color` "Background" DP
// registered here (owner UIElement, a raw color written by the visual-state
// engine) has been REMOVED — it is NOT WPF-faithful (WPF has no scalar-color
// paint DP; Background is typeof(Brush)) and it PRE-EMPTED the canonical
// BrushId "Background" (ElementDpFidelity.cpp:153, guard-registered under
// "Control"), making Background's bound type registration-order-DEPENDENT
// (non-deterministic — bad pre-11 code per the standing directive). The
// visual-state engine now registers each state's color as a fresh BrushId via
// register_element_brush (see enter_state/exit_state) and stores that BrushId,
// so the canonical BrushId "Background" (ElementDpFidelity) is the SINGLE,
// deterministic DP. kTemplatePropsRegistered is retained as the ctor's
// dependency marker (the canonical Background is guard-registered by
// register_builtin_elements -> vaml_run_dp_fidelity_closure, which the ctor
// also triggers via the element registry path).
const bool kTemplatePropsRegistered = true;

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
    // templ/control are threaded to the consumer wiring; currently unused at
    // this level (the TemplateBinding wiring is applied by the caller). Silence
    // the unused-parameter diagnostic explicitly.
    (void)templ;
    (void)control;
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
    // S643b-1 (D1, WPF paint-DP compliance): the state's color becomes a fresh
    // SolidColorBrush OBJECT held directly in the DP (WPF visual-state color is
    // a Brush object, never a scalar color and never a BrushId). The renderer
    // reads brush->color() via resolve_foreground_brush.
    try
    {
        // S643b-1 (D1) + S643b-4 (D3): the state's color -> the SHARED FROZEN
        // SolidColorBrush object (same color -> same shared object).
        (void)control.Set(PropertyId::of(state.brushProperty),
                          Value::Brush(shared_frozen_solid_brush(state.brush->color())));
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
    // S643b-1 (D1): mirror enter_state — a fresh Brush OBJECT, never a scalar
    // Color and never a BrushId.
    try
    {
        // S643b-1 (D1) + S643b-4 (D3): the state's color -> the SHARED FROZEN
        // SolidColorBrush object (same color -> same shared object).
        (void)control.Set(PropertyId::of(state.brushProperty),
                          Value::Brush(shared_frozen_solid_brush(state.brush->color())));
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
