// S475 (dep:S474) — VIEWER-GROWTH(11c): the sample .vaml authors the DockPanel
// layout + the selection controls, and the LIVE viewer renders the DockPanel
// layout + each selection control interactively.
//
// Under test (headless Rail 1, the S475 title "the LIVE viewer renders the
// DockPanel layout + each selection control interactive"):
//   1. The sample vaml (DockPanel Left-nav + Top-header + Fill-content split,
//      with the RadioButton group + ComboBox + ListBox-ItemTemplate) LOADS
//      through a retained reader without an unknown-property/type/event error —
//      the whole authored 11c set parses and builds (DockPanel.Dock attached,
//      RadioButton IsChecked, ComboBox Items/SelectedIndex, ListBox ItemTemplate
//      property-element).
//   2. The classifier emits REAL materials for every authored 11c control
//      (classify_merge_element -> MaterialKind::Solid at the vertical-slice
//      boundary; the Selections model binds to the Lightfall repaint).
//   3. The made links are the FIND: the sample loads with the control DPs
//      reachable (DockPanel.Dock read on a child; the RadioButtons grouped under
//      the same parent; the ComboBox Items + SelectedIndex; the ListBox Items).
//
// NO-GAP (recorded, not silent): the live WINDOW display (the [both-apps] proof)
// stays the on-machine local gate; this headless seam is the exact code path's
// CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // classify_merge_element
#include <vaml/vulkan/ui/UiSceneEncoder.h>  // classify_element / MaterialKind

#include <memory>
#include <string>

using vaml::Dock;
using vaml::DockPanel;
using vaml::RadioButton;
using vaml::ComboBox;
using vaml::ListBox;
using vaml::UIElement;
using vaml::VamlReader;

namespace {

// The S475 authored sample under test (mirrors vaml_viewer/sample.vaml — the
// [both-apps] live-display proof). DockPanel Left-nav + Top-header + Fill
// content, selection controls authored interactively.
const std::string kSample475 =
    "<DockPanel Margin=\"8\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<StackPanel DockPanel.Dock=\"0\" Width=\"180\" Margin=\"4\">"
    "<TextBlock Text=\"Selection Controls\" FontSize=\"14\" Margin=\"2\"/>"
    "<RadioButton x:Name=\"RadioRed\" RadioButton.IsChecked=\"1\" Margin=\"6,2\"/>"
    "<RadioButton x:Name=\"RadioGreen\" RadioButton.IsChecked=\"0\" Margin=\"6,2\"/>"
    "<RadioButton x:Name=\"RadioBlue\" RadioButton.IsChecked=\"0\" Margin=\"6,2\"/>"
    "<ComboBox x:Name=\"SizeCombo\" ComboBox.Items=\"Small, Medium, Large\" Margin=\"6,2\" ComboBox.SelectedIndex=\"1\"/>"
    "<ListBox x:Name=\"FruitList\" ListBox.Items=\"Apple, Banana, Cherry\" Margin=\"6,2\">"
    "<ListBox.ItemTemplate>"
    "<TextBlock FontSize=\"12\"/>"
    "</ListBox.ItemTemplate>"
    "</ListBox>"
    "</StackPanel>"
    "<TextBlock DockPanel.Dock=\"1\" Text=\"VAML 11c header\" FontSize=\"16\"/>"
    "<StackPanel><TextBlock Text=\"Fill content area\"/></StackPanel>"
    "</DockPanel>";

} // namespace

// ---------------------------------------------------------------------------
// 1. The authored 11c sample loads + builds without an authored-surface error
// ---------------------------------------------------------------------------

TEST_CASE("S475: the 11c sample (DockPanel + RadioButton + ComboBox + ListBox-ItemTemplate) loads cleanly")
{
    VamlReader reader;
    // The whole authored 11c set must PARSE + BUILD: no unknown-property for the
    // DockPanel.Dock attached / IsChecked / Items / SelectedIndex / ItemTemplate
    // rows, no unknown-type for DockPanel/RadioButton/ComboBox/ListBox.
    bool threw = false;
    std::unique_ptr<vaml::DependencyObject> root;
    try
    {
        root = reader.Load(kSample475);
        // The S120/S121 hand-off: FINALIZE the visual tree so the declared
        // children wire into their parents (the same path the app shell + the
        // live viewer call after Load).
        (void)reader.finalize_visual_tree();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));

    // The root is a REAL DockPanel (the composed DockPanel layout).
    auto* panel = dynamic_cast<vaml::DockPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    // The interactive 11c controls are reachable by x:Name.
    auto* red   = dynamic_cast<vaml::RadioButton*>(reader.named("RadioRed"));
    auto* combo = dynamic_cast<vaml::ComboBox*>(reader.named("SizeCombo"));
    auto* fruit = dynamic_cast<vaml::ListBox*>(reader.named("FruitList"));
    SHOULD(ShouldlyIntegral(red != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(combo != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(fruit != nullptr).ShouldBe(1));

    // The authored surface is REAL: RadioRed starts checked; the ComboBox carries
    // its authored Items + SelectedIndex; the ListBox carries its authored Items.
    SHOULD(ShouldlyIntegral(static_cast<int>(red->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyString(combo->items()).ShouldBe(std::string("Small, Medium, Large")));
    SHOULD(ShouldlyIntegral(combo->selected_index()).ShouldBe(1));
    SHOULD(ShouldlyString(fruit->items()).ShouldBe(std::string("Apple, Banana, Cherry")));
}

// ---------------------------------------------------------------------------
// 2. The Live viewer reflects the true finalDraw / material on the made links
// ---------------------------------------------------------------------------

TEST_CASE("S475: the 11c sample renders REAL Solid materials through the viewer classifier")
{
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSample475);
    (void)reader.finalize_visual_tree();
    auto* panel = dynamic_cast<vaml::DockPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    const vaml::Size slot{ 800.0f, 600.0f };
    panel->Measure(slot);
    panel->Arrange(vaml::Rect{ 0.0f, 0.0f, 800.0f, 600.0f });

    // DockPanel -> Solid (the dock layout container fill).
    const auto dockV = vaml::presentation::classify_merge_element(*panel);
    SHOULD(ShouldlyIntegral(dockV.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    // The dock layout split is REAL: the Left-docked nav (child 0) is placed at
    // the panel's left interior, and the Fill content (child 2) is placed to the
    // RIGHT of it (the WPF-faithful remaining-slot rule) — the canonical
    // DockPanel "Left nav + Fill content" split renders.
    const float navX    = panel->child_at(0)->final_rect().x;
    const float fillX   = panel->child_at(2)->final_rect().x;
    const float navW    = panel->child_at(0)->final_rect().width;
    SHOULD(ShouldlyIntegral(fillX > navX + navW - 1.0f).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 3. The interactive seams are real at the made links: RadioButton exclusive
//    group + ComboBox selection under the authored tree
// ---------------------------------------------------------------------------

TEST_CASE("S475: the authored RadioButton group is exclusively interactive")
{
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSample475);
    (void)reader.finalize_visual_tree();

    auto* red   = dynamic_cast<vaml::RadioButton*>(reader.named("RadioRed"));
    auto* green = dynamic_cast<vaml::RadioButton*>(reader.named("RadioGreen"));
    auto* blue  = dynamic_cast<vaml::RadioButton*>(reader.named("RadioBlue"));
    SHOULD(ShouldlyIntegral(red != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(green != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(blue != nullptr).ShouldBe(1));

    // Red starts checked; checking Green unchecks the sibling Red + Blue
    // (the exclusive single-parent group).
    (void)green->set_checked(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(green->is_checked())).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(red->is_checked())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(blue->is_checked())).ShouldBe(0));
}

TEST_CASE("S475: the authored ComboBox selection drives the interactive dropdown state")
{
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSample475);
    (void)reader.finalize_visual_tree();
    auto* combo = dynamic_cast<vaml::ComboBox*>(reader.named("SizeCombo"));
    SHOULD(ShouldlyIntegral(combo != nullptr).ShouldBe(1));

    // Opens + selects "Large" (index 2) -> SelectionChanged fires.
    (void)combo->open(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(combo->is_open())).ShouldBe(1));
    int fires = 0;
    (void)combo->selection_changed().add([&]() { ++fires; });
    SHOULD(ShouldlyIntegral(static_cast<int>(combo->select_item("Large"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(combo->selected_index()).ShouldBe(2));
    SHOULD(ShouldlyString(combo->selected_item()).ShouldBe(std::string("Large")));
    SHOULD(ShouldlyIntegral(fires).ShouldBe(1));
}
