// S503 (dep:S502) — VIEWER-GROWTH(11f): the sample authors a DataTemplate + a
// converter + a behavior; the LIVE viewer renders templated items + the
// converter output, and the behavior fires the command on the routed event.
//
// Under test (headless Rail 1, vaml-presentation-tests — it links vaml-presentation
// so the S212 `encode_visual_slice` render-plan is provable):
//   1. LOAD — the authored 11f sample (ListBox.ItemTemplate DataTemplate +
//      Converter= BoolVis binding + <i:Interaction.Behaviors> EventTrigger ->
//      InvokeCommandAction) loads + binds through a RETAINED reader without an
//      unknown-property/type/command error; the DataTemplate, converter output,
//      and the attached BehaviorCollection all resolve.
//   2. RENDER-PLAN — the S212 encode_visual_slice counts: the templated ListBox
//      (+ its per-item ContentPresenters/TextBlocks) emits SOLID + TEXT draws,
//      and the Status TextBlock (converter-driven Visible) emits its TEXT draw.
//   3. BEHAVIOR-FIRES-COMMAND — raising the Click routed event on the root fires
//      the attached EventTrigger -> InvokeCommandAction -> executes the DoIt
//      RelayCommand with the authored CommandParameter ("templated").
//
// NO-GAP (recorded, not silent): the LIVE WINDOW display (the [both-apps] proof)
// stays the on-machine local gate. This headless seam is the exact code path's
// CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"            // register_builtin_elements
#include "vaml/core/RelayCommand.h"       // S246 — the DoIt command
#include "vaml/core/RoutedEvent.h"        // ClickEvent / RoutedEventArgs
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/vulkan/ui/UiScene.h"       // DrawItem / DrawItemKind
#include "viewer_growth_11f.h"            // S503 — the ADR-021 consumer seam

#include <memory>
#include <string>
#include <vector>

using vaml::RelayCommand;
using vaml::RoutedEventArgs;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11f_plan;
using vaml::viewer::viewer_growth_11f_render_plan;

// The S503 authored sample under test — mirrors vaml_viewer/sample.11f.vaml (the
// [both-apps] live-display proof). It AUTHORS the DataTemplate (ListBox.ItemTemplate),
// the converter binding (Converter=BoolVis), and the behavior (<i:Interaction.Behaviors>).
const std::string kSample11f =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "xmlns:i=\"http://schemas.microsoft.com/expression/2010/interactivity\" "
    "x:Name=\"Root\" Width=\"280\" Height=\"240\" Margin=\"4\">"
    "<ListBox x:Name=\"FruitList\" ListBox.Items=\"Apple, Banana, Cherry\" Margin=\"4\">"
    "<ListBox.ItemTemplate>"
    "<TextBlock FontSize=\"12\"/>"
    "</ListBox.ItemTemplate>"
    "</ListBox>"
    "<TextBlock x:Name=\"Status\" Text=\"Converter output\" "
    "Visibility=\"{Binding IsAvailable, Converter=BoolVis}\" Margin=\"4\"/>"
    "<i:Interaction.Behaviors>"
    "<i:EventTrigger EventName=\"Click\">"
    "<i:InvokeCommandAction Command=\"{Command DoIt}\" CommandParameter=\"templated\"/>"
    "</i:EventTrigger>"
    "</i:Interaction.Behaviors>"
    "</StackPanel>";

namespace {

int count_kind(const std::vector<vaml::vulkan::ui::DrawItem>& items, DrawItemKind kind)
{
    int n = 0;
    for (const auto& it : items)
    {
        if (it.kind == kind)
        {
            ++n;
        }
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. LOAD — the authored 11f sample (DataTemplate + converter + behavior) loads
// ---------------------------------------------------------------------------

TEST_CASE("S503: the 11f sample (DataTemplate + Converter + behavior) loads + binds")
{
    vaml::register_builtin_elements();
    int executed = 0;
    Value lastParam = Value::Null();
    RelayCommand doIt([&](const Value& p) { ++executed; lastParam = p; });

    auto r = run_viewer_growth_11f_plan(kSample11f, &doIt);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    // The {Binding IsAvailable, Converter=BoolVis} bound (S252/S501) + the
    // Command/CommandParameter rows count at least one bound extension.
    SHOULD(ShouldlyIntegral(r.bound >= 1u ? 1 : 0).ShouldBe(1));

    // The authored DataTemplate is present: the ListBox rendered N per-item
    // ContentPresenters (S500). Reach the ListBox by x:Name.
    auto* list = dynamic_cast<vaml::ListBox*>(r.reader->named("FruitList"));
    SHOULD(ShouldlyIntegral(list != nullptr ? 1 : 0).ShouldBe(1));
    if (list)
    {
        // 3 authored items -> 3 per-item ContentPresenters.
        SHOULD(ShouldlyIntegral(list->child_count()).ShouldBe(3u));
    }

    // The converter output landed: IsAvailable=true -> BoolVis -> Visible(0)
    // on the authored Status visibility target.
    auto* status = dynamic_cast<UIElement*>(r.reader->named("Status"));
    SHOULD(ShouldlyIntegral(status != nullptr ? 1 : 0).ShouldBe(1));
    if (status)
    {
        SHOULD(ShouldlyIntegral(status->visibility() == vaml::Visibility::Visible ? 1 : 0)
                   .ShouldBe(1));
    }

    // The behavior is attached to the root (S502, the i: surface).
    auto* root = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(root != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(
        vaml::element_has_behaviors(const_cast<const UIElement&>(*root)) ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. RENDER-PLAN — the S212 encode_visual_slice counts (templated items + the
//    converter/hidden-state handling, via the default classifier)
// ---------------------------------------------------------------------------

TEST_CASE("S503: the render-plan emits the templated items + the converter-driven text")
{
    vaml::register_builtin_elements();
    int executed = 0;
    RelayCommand doIt([&](const Value&) { ++executed; });

    auto r = run_viewer_growth_11f_plan(kSample11f, &doIt);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t n =
        viewer_growth_11f_render_plan(r.root.get(), items, 280.0f, 240.0f);
    // The templated ListBox + the Status text emit real draws.
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));
    // The ListBox fill (SolidRect) + the templated per-item TextBlocks + the
    // Status text (TextGlyph) are present in the S206 draw list.
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. BEHAVIOR-FIRES-COMMAND — the behavior executes the command on the routed event
// ---------------------------------------------------------------------------

TEST_CASE("S503: the behavior FIRES the DoIt command on the root's Click (the title headline)")
{
    vaml::register_builtin_elements();
    int executed = 0;
    Value lastParam = Value::Null();
    RelayCommand doIt([&](const Value& p) { ++executed; lastParam = p; });

    auto r = run_viewer_growth_11f_plan(kSample11f, &doIt);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    auto* root = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(root != nullptr ? 1 : 0).ShouldBe(1));

    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
    // Raise Click on the root -> the attached EventTrigger fires the
    // InvokeCommandAction -> DoIt executes with the authored CommandParameter.
    RoutedEventArgs args;
    args.source = root;
    (void)root->RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));
    // The authored CommandParameter "templated" reached the command.
    SHOULD(ShouldlyString(lastParam).ShouldBe(Value::String("templated")));
}
