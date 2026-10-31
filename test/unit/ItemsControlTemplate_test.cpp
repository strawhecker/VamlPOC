// S500 (Sprint 11f, dep:S495) — ItemsControl + DataTemplate AUTHORING:
// authored `ListBox.ItemTemplate`/`ListView.ItemTemplate`/`DataGrid` item
// templates from .vaml so items render authored per-item content, with the
// per-item DataContext-inheritance binding mechanism (D2, WPF-faithful:
// ContentPresenter carries DataContext = item so `{Binding Path}` inside the
// template resolves against the item).
//
// S500 real deliverables (driver-approved, recorded NO-GAP not silent):
//   - NEW core `DataTemplate` (DependencyObject; DataType optional; owned
//     authored content root; `load_content()` -> fresh UIElement per item).
//   - NEW minimal per-item `ContentPresenter` host (Content + ContentTemplate
//     + DataContext = item; hosts applied template content).
//   - Per-element `UIElement::DataContext` (WPF-faithful Inherits — local-wins,
//     effective_data_context walks the parent chain).
//   - ListBox.render_items -> N per-item ContentPresenters with the authored
//     template content applied.
//
// NO-GAP forward (recorded, not silent): the full WPF container architecture
// (ItemContainerGenerator/VirtualizingPanel/ListBoxItem chrome) is S550 (11m);
// DataGrid cell/column editing is S649/11k-11; DataContext only binds WHERE the
// item Observables are wired (the per-item item source is a later/S550
// refinement). ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ContentPresenter.h"
#include "vaml/core/DataTemplate.h"
#include "vaml/core/Element.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/ObservableObject.h"

#include <cstdint>

using vaml::ContentPresenter;
using vaml::DataTemplate;
using vaml::DataTemplateBinding;
using vaml::ListBox;
using vaml::ObservableObject;
using vaml::UIElement;
using vaml::Value;

// Casts a pointer to its integer address for a ShouldlyIntegral equality
// comparison (Shouldly's ShouldlyVal stringifies via std::to_string, which
// does not accept a raw pointer).
template <typename T>
[[nodiscard]] std::uintptr_t ptr_num(T* p)
{
    return reinterpret_cast<std::uintptr_t>(p);
}

// ---------------------------------------------------------------------------
// 1. DataTemplate — the authored per-item template record + load_content.
// ---------------------------------------------------------------------------

TEST_CASE("S500: DataTemplate round-trip (DataType + owned content root + load_content)")
{
    DataTemplate dt;
    dt.set_data_type("Person");
    SHOULD(ShouldlyString(dt.data_type()).ShouldBe(std::string("Person")));

    // Author a content root: a TextBlock.
    auto root = std::make_unique<vaml::TextBlock>();
    (void)root->set_text("placeholder");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    dt.set_template_root(std::move(root));
    SHOULD(ShouldlyIntegral(dt.template_root() != nullptr).ShouldBe(true));

    // load_content() -> a FRESH, independent UIElement per call (WPF
    // LoadContent: the template is a spec, never a live shared node).
    auto c1 = dt.load_content();
    auto c2 = dt.load_content();
    SHOULD(ShouldlyIntegral(c1 != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(c2 != nullptr).ShouldBe(true));
    // Two independent instances (NOT the same object).
    SHOULD(ShouldlyIntegral(c1.get() != c2.get()).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 2. Per-element DataContext — WPF-faithful inheritable context.
// ---------------------------------------------------------------------------

TEST_CASE("S500: UIElement DataContext local-wins + parent-chain inheritance")
{
    // A parent with a local DataContext; a child with NO local context inherits
    // it (effective = the parent's); a child with its own local context wins.
    ObservableObject parentCtx;
    ObservableObject childCtx;

    vaml::StackPanel parent;
    auto child = std::make_unique<vaml::TextBlock>();
    vaml::UIElement* childRaw = child.get();
    parent.add_child(std::move(child));

    SHOULD(ShouldlyIntegral(parent.data_context() == nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(
                childRaw->effective_data_context(static_cast<ObservableObject*>(nullptr)) == nullptr)
               .ShouldBe(true));

    // Set a local context on the parent; the child inherits it.
    (void)parent.set_data_context(&parentCtx);
    SHOULD(ShouldlyIntegral(
                ptr_num(childRaw->effective_data_context(static_cast<ObservableObject*>(nullptr))))
                       .ShouldBe(ptr_num(&parentCtx)));

    // The child's own local context wins (local-wins — WPF Inherits).
    (void)childRaw->set_data_context(&childCtx);
    SHOULD(ShouldlyIntegral(
                ptr_num(childRaw->effective_data_context(static_cast<ObservableObject*>(nullptr))))
                       .ShouldBe(ptr_num(&childCtx)));
    SHOULD(ShouldlyIntegral(ptr_num(childRaw->data_context())).ShouldBe(ptr_num(&childCtx)));

    // Clearing the child's local context reverts to the parent's.
    (void)childRaw->set_data_context(nullptr);
    SHOULD(ShouldlyIntegral(
                ptr_num(childRaw->effective_data_context(static_cast<ObservableObject*>(nullptr))))
                       .ShouldBe(ptr_num(&parentCtx)));
}

// ---------------------------------------------------------------------------
// 3. ContentPresenter — the minimal per-item host applies the template.
// ---------------------------------------------------------------------------

TEST_CASE("S500: ContentPresenter hosts the applied DataTemplate content per item")
{
    // Author a template whose root is a TextBlock (the per-item content).
    DataTemplate dt;
    auto root = std::make_unique<vaml::TextBlock>();
    (void)root->set_text("item-content");
    dt.set_template_root(std::move(root));

    ContentPresenter presenter;
    // prepare_content applies the template (load_content -> the content child).
    const std::size_t built = presenter.prepare_content(nullptr, &dt);
    SHOULD(ShouldlyIntegral(built).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(presenter.content() != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(presenter.child_count()).ShouldBe(1u));

    // The hosted content is a TextBlock with the authored text.
    auto* hosted = dynamic_cast<vaml::TextBlock*>(presenter.content());
    SHOULD(ShouldlyIntegral(hosted != nullptr).ShouldBe(true));
    if (hosted)
    {
        SHOULD(ShouldlyString(hosted->text()).ShouldBe(std::string("item-content")));
    }
}

// ---------------------------------------------------------------------------
// 4. ListBox.render_items -> N per-item ContentPresenters (authored content).
// ---------------------------------------------------------------------------

TEST_CASE("S500: ListBox.render_items produces per-item ContentPresenters with authored content")
{
    ListBox lb;
    (void)lb.set_items("Red, Green, Blue");

    // Author a DataTemplate object (a TextBlock content root) + set it as the
    // ListBox's ItemTemplate object.
    DataTemplate dt;
    auto root = std::make_unique<vaml::TextBlock>();
    (void)root->set_text("a-red");
    dt.set_template_root(std::move(root));
    lb.set_item_template_object(&dt);

    const std::size_t built = lb.render_items();
    SHOULD(ShouldlyIntegral(built).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(lb.child_count()).ShouldBe(3u));

    // Each item is a ContentPresenter hosting a TextBlock child.
    for (std::size_t i = 0; i < lb.child_count(); ++i)
    {
        auto* host = dynamic_cast<ContentPresenter*>(lb.child_at(i));
        SHOULD(ShouldlyIntegral(host != nullptr).ShouldBe(true));
        if (host)
        {
            SHOULD(ShouldlyIntegral(host->child_count()).ShouldBe(1u));
            SHOULD(ShouldlyIntegral(
                        dynamic_cast<vaml::TextBlock*>(host->content()) != nullptr)
                       .ShouldBe(true));
        }
    }
}

TEST_CASE("S500: ListBox.render_items with NO ItemTemplate keeps the S473 per-item text leaf")
{
    // Back-compat no-template path preserved (zero regression).
    ListBox lb;
    (void)lb.set_items("One, Two");
    const std::size_t built = lb.render_items();
    SHOULD(ShouldlyIntegral(built).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(lb.child_count()).ShouldBe(2u));
    // Each is a TextBlock leaf (the S473 behavior).
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::TextBlock*>(lb.child_at(0)) != nullptr)
                       .ShouldBe(true));
}
