#pragma once
// VAML DataTemplate — the per-item authored template (S500, Sprint 11f).
// WPF-faithful to `System.Windows.DataTemplate` (per the WPF-source review):
// a DataTemplate describes how ONE item renders. It holds an authored content
// ROOT (a UIElement template spec) + a `load_content()` instantiation seam that
// rebuilds a FRESH, independent UIElement per item (WPF `LoadContent()` —
// the template is a spec, never a live shared node).
//
// S500 scope (driver-approved, recorded NO-GAP not silent):
//   - `DataType` (optional target item-type name).
//   - Owned authored content root (a UIElement subtree).
//   - `load_content()` -> a FRESH UIElement (deep clone of the template root).
//   - Template-internal binding INTENTS, resolved per item against the item's
//     DataContext by the per-item ContentPresenter (S500).
//
// The WPF-FAITHFUL base is a FrameworkTemplate-analogue; VAML harmonizes with
// the S364 ControlTemplate, which is a DependencyObject (resource model) — so
// DataTemplate : DependencyObject is the faithful mapping (it is a valid owned
// resource / x:Key target).
//
// NO-GAP forward (recorded, not silent): the WPF VisualTree-FACTORY
// (FrameworkElementFactory) + the implicit DataType template selection
// (`ChooseTemplate`/`DefaultSelector`) are later refinements — S500 delivers
// the explicit per-item template + per-item DataContext-inheritance binding.
//
// ADR-011 held (template = core data/logic). ADR-018: zero locks.

#include "vaml/core/DependencyObject.h"  // DataTemplate IS a DependencyObject (resource model)
#include "vaml/core/UIElement.h"         // UIElement (the template content root)
#include "vaml/core/export.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

// ONE template-internal binding INTENT: an element NAMED `targetName` (the
// authored x:Name/Name of a content child) has a `{Binding <sourcePath>}` on
// its `propertyKey` DP. When the per-item ContentPresenter instantiates the
// template AND sets the item as DataContext, it resolves each intent into a
// live BindingExpression with source = the item (the per-item DataContext
// inheritance headline). Captured at authoring time, resolved per item.
struct VAML_CORE_EXPORT DataTemplateBinding
{
    std::string targetName;   // the named content child the binding is on
    std::string propertyKey;  // the bound DP on that child (e.g. "Text")
    std::string sourcePath;   // the binding path into the item (e.g. "Name")
};

class VAML_CORE_EXPORT DataTemplate : public DependencyObject
{
public:
    DataTemplate();
    ~DataTemplate() override;

    // --- DataType (the WPF `DataType` target item-type name, optional) ------
    void set_data_type(std::string type);
    [[nodiscard]] const std::string& data_type() const noexcept { return data_type_; }

    // --- The authored content root (the template spec) ---------------------
    // S500 scopes the authored content to a SINGLE root (WPF DataTemplate has
    // one VisualTree root). `set_template_root` takes OWNERSHIP.
    void set_template_root(std::unique_ptr<UIElement> root);
    [[nodiscard]] const UIElement* template_root() const noexcept { return template_root_.get(); }
    [[nodiscard]] UIElement* template_root() noexcept { return template_root_.get(); }

    // --- load_content() — the WPF LoadContent seam -------------------------
    // Returns a FRESH, independent UIElement built from the template root
    // (deep clone of the authored subtree — the template is a spec, never a
    // live shared node). Each per-item call yields an independent content tree.
    [[nodiscard]] std::unique_ptr<UIElement> load_content() const;

    // --- Template-internal binding intents (resolved per item) -------------
    void add_binding(const DataTemplateBinding& intent);
    [[nodiscard]] const std::vector<DataTemplateBinding>& bindings() const noexcept
    {
        return bindings_;
    }

private:
    std::string                 data_type_;
    std::unique_ptr<UIElement>  template_root_;
    std::vector<DataTemplateBinding> bindings_;
};

// Deep-clones an authored UIElement subtree into a FRESH shared_ptr<UIElement>,
// reconstructing each node's concrete type + its authored DPs + its children.
// Used by DataTemplate::load_content() for the per-item independent content.
// The clone recognizes the REAL control/panel/leaf types (the S10 + 11c+ set)
// and copies their authored DPs; an UNRECOGNIZED type falls back to a base
// UIElement clone (documented — the authored DP copy of a not-yet-clonable
// control is a S649 refinement, recorded NO-GAP not silent).
[[nodiscard]] VAML_CORE_EXPORT std::unique_ptr<UIElement> clone_element(const UIElement& source);

} // namespace vaml
