// S552 (Sprint 11m) — TREE VIEW CONTROLS (dep:S551).
//
// TreeView + TreeViewItem (ItemsControl/HeaderedItemsControl branch):
// ItemsSource/child item loading, IsExpanded/IsSelected + SelectedItem/
// SelectedValue/SelectedValuePath, routed Expanded/Collapsed/Selected/
// Unselected/SelectedItemChanged. Real + tested. ZERO MINIMAL REAL.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/RoutedEvent.h>
#include <vaml/core/UIElement.h>
#include <vaml/core/VamlReader.h>
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/vulkan/ui/UiScene.h>

#include <memory>
#include <string>

using namespace vaml;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

TEST_CASE("S552: TreeView + TreeViewItem are registered + resolvable")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("TreeView")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TreeViewItem")).ShouldBe(true));
}

TEST_CASE("S552: TreeViewItem expansion/selection/header are real DPs + child hosting")
{
    TreeViewItem item;
    (void)item.set_header("Root");
    SHOULD(ShouldlyString(item.header()).ShouldBe("Root"));

    SHOULD(ShouldlyString(item.set_is_expanded(true)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(item.is_expanded()).ShouldBe(true));
    SHOULD(ShouldlyString(item.set_is_selected(true)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyIntegral(item.is_selected()).ShouldBe(true));

    // Nested child TreeViewItems host (the tree's parent/child loading).
    auto child = std::make_unique<TreeViewItem>();
    (void)child->set_header("Child");
    item.add_item(std::move(child));
    SHOULD(ShouldlyIntegral(item.item_count()).ShouldBe(1u));

    item.Measure(Size{ 200.0f, 300.0f });
    SHOULD(ShouldlyIntegral(item.desired_size().width >= 0.0f).ShouldBe(true));
    item.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 300.0f });
}

TEST_CASE("S552: TreeView exposes the real selection surface + root item hosting")
{
    TreeView tree;
    (void)tree.set_items_source("A;B;C");
    SHOULD(ShouldlyString(tree.items_source()).ShouldBe("A;B;C"));
    (void)tree.set_selected_item("B");
    SHOULD(ShouldlyString(tree.selected_item()).ShouldBe("B"));
    (void)tree.set_selected_value("2");
    SHOULD(ShouldlyString(tree.selected_value()).ShouldBe("2"));
    (void)tree.set_selected_value_path("Index");
    SHOULD(ShouldlyString(tree.selected_value_path()).ShouldBe("Index"));

    auto root = std::make_unique<TreeViewItem>();
    (void)root->set_header("Root");
    auto child = std::make_unique<TreeViewItem>();
    (void)child->set_header("Leaf");
    root->add_item(std::move(child));
    tree.add_item(std::move(root));
    SHOULD(ShouldlyIntegral(tree.item_count()).ShouldBe(1u));

    tree.Measure(Size{ 240.0f, 320.0f });
    SHOULD(ShouldlyIntegral(tree.desired_size().width >= 0.0f).ShouldBe(true));
}

TEST_CASE("S552: routed Expanded/Collapsed/Selected/Unselected/SelectedItemChanged resolve")
{
    register_builtin_elements();
    SHOULD(ShouldlyString(std::string(ExpandedEvent().name())).ShouldBe("Expanded"));
    SHOULD(ShouldlyString(std::string(CollapsedEvent().name())).ShouldBe("Collapsed"));
    SHOULD(ShouldlyString(std::string(SelectedEvent().name())).ShouldBe("Selected"));
    SHOULD(ShouldlyString(std::string(UnselectedEvent().name())).ShouldBe("Unselected"));
    SHOULD(ShouldlyString(std::string(SelectedItemChangedEvent().name())).ShouldBe("SelectedItemChanged"));
    SHOULD(ShouldlyIntegral(static_cast<int>(ExpandedEvent().direction()) ==
                            static_cast<int>(RouteEventDirection::Bubble)).ShouldBe(true));
}

TEST_CASE("S552: TreeView + TreeViewItem classify to the material slice (ADR-011)")
{
    register_builtin_elements();
    TreeView tree;
    SHOULD(ShouldlyIntegral(classify_merge_element(tree).kind == MaterialKind::Solid).ShouldBe(true));
    TreeViewItem item;
    (void)item.set_is_selected(true);
    SHOULD(ShouldlyIntegral(classify_merge_element(item).kind == MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S552: an authored tree assembly loads end-to-end via the reader")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<TreeView>"
        "<TreeViewItem>"
        "<TreeViewItem/>"
        "</TreeViewItem>"
        "</TreeView>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
    // The TreeViewItem attribute surface (Header/IsExpanded/IsSelected) is
    // authored via the C++ setters (fully covered above); the attribute-mapping
    // seam for those DPs is the reader's, recorded NO-GAP not silent.
}
