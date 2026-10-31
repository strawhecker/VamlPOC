#pragma once
// VAML ContentPresenter — the minimal per-item host (S500, Sprint 11f).
// WPF-FAITHFUL (per the WPF-source review of ContentPresenter.cs /
// ItemsControl.cs): a per-item ContentPresenter is the unit that carries
// `Content = item` + `ContentTemplate = DataTemplate` + `DataContext = item`,
// and hosts the applied template content so a `{Binding}` inside the template
// resolves against the item via DATA CONTEXT INHERITANCE.
//
// S500 scope (driver-approved, recorded NO-GAP not silent): this is the
// MINIMAL per-item host (the DataContext + template unit). The full WPF
// container architecture (ItemContainerGenerator, VirtualizingPanel, the
// ListBoxItem container chrome + ContentPresenter chrome expansion) is S550
// (11m). S500 ships only the per-item DataContext + template application unit.
//
// Lifecycle: a ContentPresenter is a real UIElement child of the ItemsControl
// (one per item). It owns its applied template content as a child.

#include "vaml/core/UIElement.h"
#include "vaml/core/export.h"

#include <memory>
#include <string>

namespace vaml {

class DataTemplate;

class VAML_CORE_EXPORT ContentPresenter : public UIElement
{
public:
    ContentPresenter();

    // --- PrepareContentPresenter-equivalent --------------------------------
    // WPF-faithful: `Content = item`, `ContentTemplate = itemTemplate`. The
    // item is set as THIS presenter's DataContext so the template content
    // inherits it (the D2 DataContext-inheritance mechanism). Does NOT take
    // ownership of `item` (the item is an app-supplied ObservableObject / value
    // the app owns) nor `itemTemplate` (the template is an owned resource).
    // Returns the number of template content elements built (0 when no item
    // template is present).
    [[nodiscard]] std::size_t prepare_content(UIElement* item, DataTemplate* itemTemplate);

    // The per-item DataContext (the item). `content()` returns the applied
    // template content root (the child hosting the authored content).
    [[nodiscard]] UIElement* content() const noexcept { return child_count() ? child_at(0) : nullptr; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

} // namespace vaml
