// 11s (dep:S803) — VIEWER-GROWTH(11s): THE COEXISTENCE LIVE PROOF (S804) +
// the un-revert re-addition (S805) headless half.
//
// A corpus-shaped `sample.s11s.vaml` authors THREE same-name/different-owner DP
// pairs (Slider.Delay/RepeatButton.Delay; Popup.AllowsTransparency/Window.
// AllowsTransparency; TextBlock.FontSize/Control.FontSize) + the re-addition
// surface (Image/Viewbox StretchDirection; Border/Control Padding). Each pair
// must RESOLVE + RENDER independently through the generic renderer — the S222_1
// RELOAD re-demonstrated against the composite (name, owner) key. This is the
// on-screen proof that the name-only DP hosing is ABOLISHED.
//
// Under test (vaml-presentation-tests — links vaml-presentation so the S212
// encode_visual_slice render-plan is provable):
//   1. LOAD — the coexistence sample loads + finalizes through a RETAINED
//      reader; every control is reachable by x:Name.
//   2. COEXISTENCE-RESOLUTION — each same-name/different-owner pair reads its
//      OWN owner's authored value through the generic `DependencyObject::Get(
//      PropertyId::of(name, owner))` path (the composite-key resolution proof).
//   3. RENDER-PLAN — the S212 encode_visual_slice emits NON-DEGENERATE items
//      for the coexisting tree (the generic renderer proves the DPs rendered).
//
// NO-GAP (recorded, not silent): the LIVE WINDOW display of the coexistence
// sample (the [both-apps] proof) stays the on-machine local gate; this headless
// seam is the exact code path's CI-checkable half. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"            // Slider/RepeatButton/Popup/Window/TextBlock/Button/Image/Viewbox/Border
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem / DrawItemKind
#include "viewer_growth_s11s.h"           // the 11s ADR-021 consumer seam

#include <memory>
#include <string>
#include <vector>

using vaml::Border;
using vaml::Button;
using vaml::Image;
using vaml::Popup;
using vaml::RepeatButton;
using vaml::Slider;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::VamlReader;
using vaml::Viewbox;
using vaml::Window;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_s11s_plan;
using vaml::viewer::viewer_growth_s11s_render_plan;

// The authored sample under test — mirrors vaml_viewer/sample.s11s.vaml (the
// [both-apps] live-display proof). Kept as a verbatim inline copy so the
// headless test exercises EXACTLY the bytes the LIVE viewer loads (no drift).
const std::string kSampleS11s =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"720\" Height=\"520\" Margin=\"6\" "
    "Background=\"#F0F0F0\">"
    "<TextBlock Text=\"11s — coexistence\" FontSize=\"16\" Margin=\"4\"/>"
    "<StackPanel Orientation=\"0\" Margin=\"4\">"
    "<Slider x:Name=\"SliderDelay\" Minimum=\"0\" Maximum=\"1000\" Value=\"40\" "
    "Width=\"300\" Height=\"28\" Delay=\"500\"/>"
    "<RepeatButton x:Name=\"RepeatDelay\" Delay=\"120\" Interval=\"80\" "
    "Width=\"300\" Height=\"24\" Margin=\"2\"/>"
    "</StackPanel>"
    "<StackPanel Orientation=\"1\" Margin=\"4\">"
    "<Popup x:Name=\"PopupAT\" Popup.IsOpen=\"0\" AllowsTransparency=\"true\" "
    "Width=\"140\" Height=\"60\" HorizontalOffset=\"4\" VerticalOffset=\"4\"/>"
    "<Window x:Name=\"WindowAT\" AllowsTransparency=\"false\" Title=\"11s window\" "
    "Width=\"160\" Height=\"70\"/>"
    "</StackPanel>"
    "<StackPanel Orientation=\"0\" Margin=\"4\">"
    "<TextBlock x:Name=\"TbFe\" Text=\"TextBlock.FontSize=24 (own owner)\" "
    "FontSize=\"24\"/>"
    "<Button x:Name=\"BtnCtrl\" FontSize=\"14\" Width=\"280\" Height=\"26\" "
    "IsDefault=\"1\">Control.FontSize=14 (Control owner)</Button>"
    "</StackPanel>"
    "<StackPanel Orientation=\"1\" Margin=\"4\">"
    "<Image x:Name=\"ImgSD\" StretchDirection=\"1\" Source=\"11s-sample.png\" "
    "Width=\"80\" Height=\"60\"/>"
    "<Viewbox x:Name=\"VbSD\" StretchDirection=\"2\" Width=\"100\" Height=\"60\">"
    "<Border x:Name=\"BrdPad\" Padding=\"4\" BorderThickness=\"2\" "
    "CornerRadius=\"2\" Width=\"80\" Height=\"40\" Background=\"#CCCCFF\"/>"
    "</Viewbox>"
    "</StackPanel>"
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
// 1. LOAD — the coexistence sample loads + finalizes; controls reachable.
// ---------------------------------------------------------------------------

TEST_CASE("11s: the coexistence sample (3 same-name/different-owner DP pairs) loads cleanly")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_s11s_plan(kSampleS11s);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // Every control is reachable by x:Name.
    SHOULD(ShouldlyIntegral(dynamic_cast<Slider*>(r.reader->named("SliderDelay")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<RepeatButton*>(r.reader->named("RepeatDelay")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Popup*>(r.reader->named("PopupAT")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(r.reader->named("WindowAT")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<TextBlock*>(r.reader->named("TbFe")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Button*>(r.reader->named("BtnCtrl")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Image*>(r.reader->named("ImgSD")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Viewbox*>(r.reader->named("VbSD")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Border*>(r.reader->named("BrdPad")) != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. COEXISTENCE-RESOLUTION — each same-name pair reads its OWN owner's value.
// ---------------------------------------------------------------------------

TEST_CASE("11s: same-name/different-owner DP pairs resolve to their OWN owner's value")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_s11s_plan(kSampleS11s);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // Pair 1: Slider.Delay (500, own owner) vs RepeatButton.Delay (120, own
    // owner) — BOTH resolve, no bare-name pick (the name-only hosing abolished).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.sliderDelay)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.repeatDelay)).ShouldBe(1));

    // Pair 2: Popup.AllowsTransparency (true) vs Window.AllowsTransparency
    // (false) — distinct Bool owners.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.popupAT)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.windowAT)).ShouldBe(1));

    // Pair 3: TextBlock.FontSize (24, TextElement branch) vs Control.FontSize
    // (14, Control branch) — the FrameworkElement vs Control separation.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.textFeFont)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.controlFont)).ShouldBe(1));

    // Re-addition surface: Image.StretchDirection (1) vs Viewbox.
    // StretchDirection (2); Border.Padding (4) own while Control default 0.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.imageSD)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.viewboxSD)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.borderPad)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.controlPad)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. RENDER-PLAN — the S212 encode_visual_slice renders the coexisting tree.
// ---------------------------------------------------------------------------

TEST_CASE("11s: the render-plan emits NON-DEGENERATE items for the coexisting tree")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_s11s_plan(kSampleS11s);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t n = viewer_growth_s11s_render_plan(r.root.get(), items, 760.0f, 560.0f);
    // The coexisting tree renders via the generic classifier (S222_1 RELOAD) —
    // NON-degenerate items, proving the authored same-name DPs rendered.
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
}
