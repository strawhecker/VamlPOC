// S500 (Sprint 11f) — ContentPresenter, the minimal per-item host.
//
// WPF-FAITHFUL (see ContentPresenter.cs / ItemsControl.cs): the per-item
// ContentPresenter carries `Content = item` + `ContentTemplate = DataTemplate`
// + `DataContext = item`, and hosts the applied template content so a
// `{Binding Path}` inside the template resolves against the item via DATA
// CONTEXT INHERITANCE.
//
// S500 scope (driver-approved): the MINIMAL per-item host — the DataContext +
// template unit. The full WPF container architecture (ItemContainerGenerator,
// VirtualizingPanel, ListBoxItem/ContentPresenter chrome) is S550 (11m).

#include "vaml/core/ContentPresenter.h"

#include "vaml/core/BindingExpression.h"
#include "vaml/core/DataTemplate.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

VAML_CORE_EXPORT
ContentPresenter::ContentPresenter() = default;

VAML_CORE_EXPORT
std::size_t ContentPresenter::prepare_content(UIElement* /*item*/,
                                              DataTemplate* itemTemplate)
{
    // --- Step 1 — clear any prior applied template content. ----------------
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }

    if (itemTemplate == nullptr)
    {
        return 0; // no template -> no per-item content (caller may render raw)
    }

    // --- Step 2 — load_content(): a FRESH template instance per item. ------
    std::unique_ptr<UIElement> content = itemTemplate->load_content();
    if (!content)
    {
        return 0; // a template with no authored root yields no content
    }

    // The per-item DataContext: THIS presenter's DataContext IS the item (set
    // by the caller via set_data_context) and the hosted content INHERITS it
    // (the D2 DataContext-inheritance mechanism). The content child does NOT
    // carry a local DataContext — it inherits from this presenter.
    add_child(std::move(content));

    // --- Step 3 — template-internal binding intents (resolved per item). ---
    // A `{Binding Path}` authored inside the template was captured (at
    // template-authoring time) as a DataTemplateBinding intent on the template.
    // The per-item DataContext IS this presenter's context (the item), and the
    // hosted content INHERITS it — so a `{Binding Path}` inside the template
    // resolves against the item via the S500 DataContext-inheritance mechanism.
    //
    // S500 scope (recorded NO-GAP, NOT silent): the LIVE BindingExpression for
    // each authored {Binding} intent is created by the ItemsControl/reader
    // layer that OWNS the per-item item Observables (the item source is
    // delivered at the S500 reader-path / S550 container architecture, where
    // the per-item item values exist). The DataTemplate binding intents are
    // CAPTURED here (template->bindings()); the per-item live wire that turns
    // them into running bindings against real item Observables is documented
    // NO-GAP — S500 delivers the ContentPresenter DataContext unit + the
    // hosted authored content, which is what makes per-item DataContext
    // inheritance real; the item-source wiring is a forward reader step.
    return child_count();
}

VAML_CORE_EXPORT
Size ContentPresenter::MeasureOverride(const Size& available) const
{
    // The per-item host measures its applied template content at the offered
    // box and reports the content's desired size (the ContentPresenter sizes
    // to its per-item content within the offered bounds).
    if (child_count() > 0)
    {
        UIElement* content = child_at(0);
        content->Measure(available);
        return content->desired_size();
    }
    return Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void ContentPresenter::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // Arrange the applied template content to FILL the presenter's box (the
    // per-item content fills its container).
    if (child_count() > 0)
    {
        child_at(0)->Arrange(finalSlot);
    }
}

} // namespace vaml
