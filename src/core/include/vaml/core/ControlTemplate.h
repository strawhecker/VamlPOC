#pragma once
// VAML ControlTemplate + VisualState + the template instantiation + state engine.
// S364 (dep:S363) + S365 (dep:S364) — Sprint 9.
//
// A `ControlTemplate` is a template record that describes a control's INTERNAL
// tree. It holds:
//   - a template TARGET-TYPE name (e.g. "Button"),
//   - OWNERSHIP of the parsed template content (a `TemplatePart` tree — the
//     control's internal element structure),
//   - a set of `VisualState` entries (Normal / Hover / Pressed / focused) each
//     carrying a `{ state, brushProperty, Brush }` — the per-state brush swap
//     that changes the rendered fill.
//
// INSTANTIATION (S364): `instantiate(element)` builds a LIVE UIElement tree
// from the template's parsed content (a shared VamlReader-style build producing
// owned UIChildren on a templated container). The built tree is owned by the
// templated element.
//
// VISUAL STATE ENGINE (S364): `enter_state(element, state)` / `exit_state(state)`
// apply/revert the state's brush overrides (re-using S363 setter/apply). The
// state entries ride on the templated control's visual brushes; entering
// Hover then Pressed swaps the rendered fill. Real input (S323/S324) routes
// entering Hover/Pressed into enter_state; the template/control-viewer input
// calls the state engine. Real Button hover/pressed = S401.
//
// TEMPLATEBINDING (S365): a template-part property may read its value from the
// templated control's DP (`{TemplateBinding Background}`). A `TemplateBinding`
// record wires a template-part property to the control's DP, LIVE-subscribed
// so a control DP change re-skins the template part. `apply_control_template`
// applies a template to a control: builds the content tree, wires the
// TemplateBindings, and attaches the visual states.
//
// ADR-011 HELD: SolidColorBrush is core DATA (Brush.h). ADR-018: zero locks.

#include "vaml/core/Brush.h"             // SolidColorBrush (core data brush)
#include "vaml/core/DependencyObject.h"  // ControlTemplate IS a DependencyObject (resource model)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Style.h"             // Setter (TemplateBinding reuses the setter model)
#include "vaml/core/UIElement.h"         // UIElement (template content + templated element)
#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// TemplatePart — ONE node of the template's internal tree.
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT TemplatePart
{
    std::string typeName;              // element type to build (e.g. "UIElement"/"StackPanel")
    std::vector<std::pair<std::string, Value>> properties; // literal props on this part
    std::vector<TemplatePart> children;
    // TemplateBinding wires: {TemplateBinding <controlProp>} on a part property.
    // key = part property name (e.g. "Background"), controlProp = the control DP.
    std::vector<std::pair<std::string, std::string>> templateBindings;
};

// ---------------------------------------------------------------------------
// VisualState — a per-state brush override on the templated control.
// SolidColorBrush is move-only (a DependencyObject), so a VisualState owns its
// brush through a unique_ptr (making VisualState itself movable — shareable in
// the template's state vector).
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT VisualState
{
    std::string name;                 // "Normal" / "Hover" / "Pressed" / "focused"
    std::string brushProperty;        // which brush DP on the control this state drives
    std::unique_ptr<SolidColorBrush> brush; // the state's fill (core data brush, owned)
    bool engaged = false;             // whether the state is currently active
};

// ---------------------------------------------------------------------------
// ControlTemplate.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT ControlTemplate : public DependencyObject
{
public:
    ControlTemplate();
    ~ControlTemplate() override;

    void set_target_type(std::string type);
    [[nodiscard]] const std::string& target_type() const noexcept { return target_type_; }

    // The parsed content root tree (the control's internal structure).
    void set_content(TemplatePart root);
    [[nodiscard]] const TemplatePart& content() const noexcept { return content_; }

    // Visual states (Normal default; Hover/Pressed add the brush swaps).
    void add_state(VisualState state);
    [[nodiscard]] const std::vector<VisualState>& states() const noexcept { return states_; }

    // The template binding wiring collected across the content (part prop ->
    // control DP). Available after set_content.
    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>&
    all_template_bindings() const noexcept { return templateBindings_; }

    // find_state needs direct access to the non-const state vector.
    friend VAML_CORE_EXPORT VisualState* find_state(ControlTemplate&, std::string_view) noexcept;

private:
    std::string              target_type_;
    TemplatePart             content_;
    std::vector<VisualState> states_;
    // Flattened {TemplateBinding partProp -> controlProp} wires from set_content.
    std::vector<std::pair<std::string, std::string>> templateBindings_;
};

// ---------------------------------------------------------------------------
// Template instantiation + state engine + TemplateBinding wiring (S364/S365).
// ---------------------------------------------------------------------------

// Builds a LIVE UIElement tree from `part` and appends it as a child of
// `host` (the templated control's template content container), wiring each
// TemplateBinding to the control's DP. Returns the number of parts built.
[[nodiscard]] VAML_CORE_EXPORT std::size_t
instantiate_template(UIElement& host, const TemplatePart& part,
                     const ControlTemplate& templ, UIElement& control);

// Applies a ControlTemplate to `control`: instantiates the content tree under
// the control, wires the TemplateBindings (S365), and attaches the visual
// states. Returns the number of template parts built.
[[nodiscard]] VAML_CORE_EXPORT std::size_t
apply_control_template(UIElement& control, const ControlTemplate& templ);

// Visual-state engine (S364): `enter_state` applies `state`'s brush to the
// control's `state.brushProperty` DP and marks it engaged; `exit_state` reverts
// it (re-applies the Normal/default). Real input routes Hover/Pressed here.
VAML_CORE_EXPORT void enter_state(UIElement& control, VisualState& state);
VAML_CORE_EXPORT void exit_state(UIElement& control, VisualState& state);

// Convenience: finds a template's state by name (nullptr when absent).
[[nodiscard]] VAML_CORE_EXPORT VisualState*
find_state(ControlTemplate& templ, std::string_view name) noexcept;

} // namespace vaml
