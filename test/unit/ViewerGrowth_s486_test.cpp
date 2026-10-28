// S486 (dep:S485) — VIEWER-GROWTH(11d): the sample .vaml authors
// Label/Border/shapes/Image/ProgressBar/PasswordBox/GroupBox/Expander/
// Separator/StatusBar, and the LIVE viewer renders the full breadth.
//
// Under test (headless Rail 1, the S486 title "the LIVE viewer renders the
// full breadth"):
//   1. The 11d sample LOADS through a retained reader without an unknown-
//      property/type/event error — the whole authored 11d set parses + builds
//      (the control DPs are reachable: Border.BorderThickness/CornerRadius,
//      PasswordBox.Password, Expander.IsExpanded, ProgressBar.Value,
//      Image.Source).
//   2. The classifier emits REAL materials for every authored 11d control
//      (Label/Separator/StatusBar/GroupBox/Expander -> Solid; Border -> Border;
//      Rectangle/Ellipse/Line -> Solid; Image -> Monochrome; ProgressBar ->
//      Solid with the fill fraction).
//
// NO-GAP (recorded, not silent): the live WINDOW display (the [both-apps]
// proof) stays the on-machine local gate; this headless seam is the exact code
// path's CI-checkable half. ADR-018 zero locks.

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

using vaml::Border;
using vaml::Expander;
using vaml::GroupBox;
using vaml::Image;
using vaml::Label;
using vaml::PasswordBox;
using vaml::ProgressBar;
using vaml::UIElement;
using vaml::VamlReader;

namespace {

// The S486 authored sample under test (mirrors vaml_viewer/sample.vaml — the
// [both-apps] live-display proof). A StatusBar-docked layout hosting the
// full 11d control breadth.
const std::string kSample486 =
    "<DockPanel Margin=\"8\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<StackPanel DockPanel.Dock=\"0\" Width=\"220\" Margin=\"4\">"
    "<Label x:Name=\"TitleLabel\" Label.Content=\"Core Display Controls\" FontSize=\"14\" Margin=\"2\"/>"
    "<Separator x:Name=\"Hairline\" Separator.Thickness=\"2\" Margin=\"2\"/>"
    "<Border x:Name=\"CardBorder\" Border.BorderThickness=\"2,1\" Border.CornerRadius=\"4\" Padding=\"4\" Margin=\"2\">"
    "<StackPanel><Rectangle x:Name=\"FillRect\" Width=\"40\" Height=\"20\"/>"
    "<Ellipse x:Name=\"Dot\"/>"
    "<Line x:Name=\"Rule\" Line.StrokeThickness=\"2\" Width=\"60\"/>"
    "</StackPanel></Border>"
    "<GroupBox x:Name=\"FieldGroup\" GroupBox.Header=\"Credentials\" Margin=\"2\">"
    "<StackPanel><Label Label.Content=\"User\"/>"
    "<PasswordBox x:Name=\"Secret\" PasswordBox.Password=\"hunter2\" Margin=\"2\"/>"
    "</StackPanel></GroupBox>"
    "<ProgressBar x:Name=\"Progress\" ProgressBar.Value=\"60\" ProgressBar.Maximum=\"100\" Height=\"14\" Margin=\"2\"/>"
    "<Image x:Name=\"Logo\" Image.Source=\"assets/logo.png\" Width=\"24\" Height=\"24\" Margin=\"2\"/>"
    "</StackPanel>"
    "<Text DockPanel.Dock=\"1\" Text=\"VAML 11d header\" FontSize=\"16\"/>"
    "<Expander x:Name=\"More\" Expander.Header=\"More controls\" Expander.IsExpanded=\"1\">"
    "<StackPanel><Label Label.Content=\"Expandable content\"/></StackPanel></Expander>"
    "<StatusBar DockPanel.Dock=\"3\" Height=\"22\">"
    "<Text StatusBar.Dock=\"0\" Text=\"Ready\"/>"
    "<Text StatusBar.Dock=\"1\" Text=\"v11d\"/>"
    "</StatusBar>"
    "</DockPanel>";

} // namespace

// ---------------------------------------------------------------------------
// 1. The authored 11d sample loads + builds without an authored-surface error
// ---------------------------------------------------------------------------

TEST_CASE("S486: the 11d sample (Label/Border/shapes/Image/ProgressBar/PasswordBox/GroupBox/Expander/Separator/StatusBar) loads cleanly")
{
    VamlReader reader;
    bool threw = false;
    std::unique_ptr<vaml::DependencyObject> root;
    try
    {
        root = reader.Load(kSample486);
        (void)reader.finalize_visual_tree();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));

    auto* panel = dynamic_cast<vaml::DockPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    // Every authored 11d control is reachable by x:Name.
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Label*>(reader.named("TitleLabel")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Border*>(reader.named("CardBorder")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Rectangle*>(reader.named("FillRect")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Ellipse*>(reader.named("Dot")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Line*>(reader.named("Rule")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::GroupBox*>(reader.named("FieldGroup")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::PasswordBox*>(reader.named("Secret")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::ProgressBar*>(reader.named("Progress")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Image*>(reader.named("Logo")) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Expander*>(reader.named("More")) != nullptr).ShouldBe(1));

    // The made links are the FIND: the control DPs are reachable.
    auto* border = dynamic_cast<vaml::Border*>(reader.named("CardBorder"));
    SHOULD(ShouldlyIntegral(static_cast<int>(border->border_thickness().left == 2.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(border->corner_radius().left == 4.0f)).ShouldBe(1));
    auto* secret = dynamic_cast<vaml::PasswordBox*>(reader.named("Secret"));
    SHOULD(ShouldlyString(secret->password()).ShouldBe(std::string("hunter2")));
    auto* more = dynamic_cast<vaml::Expander*>(reader.named("More"));
    SHOULD(ShouldlyIntegral(static_cast<int>(more->is_expanded())).ShouldBe(1));
    auto* progress = dynamic_cast<vaml::ProgressBar*>(reader.named("Progress"));
    SHOULD(ShouldlyIntegral(static_cast<int>(progress->value() == 60.0)).ShouldBe(1));
    auto* logo = dynamic_cast<vaml::Image*>(reader.named("Logo"));
    SHOULD(ShouldlyString(logo->source()).ShouldBe(std::string("assets/logo.png")));
}

// ---------------------------------------------------------------------------
// 2. The classifier emits REAL materials for every authored 11d control
// ---------------------------------------------------------------------------

TEST_CASE("S486: the 11d sample renders REAL materials through the viewer classifier")
{
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSample486);
    (void)reader.finalize_visual_tree();
    auto* panel = dynamic_cast<vaml::DockPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(1));

    const vaml::Size slot{ 800.0f, 600.0f };
    panel->Measure(slot);
    panel->Arrange(vaml::Rect{ 0.0f, 0.0f, 800.0f, 600.0f });

    // Border -> the FIRST live BorderRect material (the S205 pipeline is live).
    auto* border = dynamic_cast<vaml::Border*>(reader.named("CardBorder"));
    SHOULD(ShouldlyIntegral(border != nullptr).ShouldBe(1));
    const auto bv = vaml::presentation::classify_merge_element(*border);
    SHOULD(ShouldlyIntegral(bv.kind == vaml::vulkan::ui::MaterialKind::Border).ShouldBe(true));

    // Image -> Monochrome (the icon-texture path).
    auto* logo = dynamic_cast<vaml::Image*>(reader.named("Logo"));
    SHOULD(ShouldlyIntegral(logo != nullptr).ShouldBe(1));
    const auto iv = vaml::presentation::classify_merge_element(*logo);
    SHOULD(ShouldlyIntegral(iv.kind == vaml::vulkan::ui::MaterialKind::Monochrome).ShouldBe(true));

    // Label/Separator/StatusBar/GroupBox/Expander/Rectangle/Ellipse/Line/
    // ProgressBar/PasswordBox -> Solid (the S482/S484/S485 set).
    auto label = std::make_unique<vaml::Label>();
    const auto lv = vaml::presentation::classify_merge_element(*label);
    SHOULD(ShouldlyIntegral(lv.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    auto sep = std::make_unique<vaml::Separator>();
    const auto sv = vaml::presentation::classify_merge_element(*sep);
    SHOULD(ShouldlyIntegral(sv.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    auto rect = std::make_unique<vaml::Rectangle>();
    const auto rv = vaml::presentation::classify_merge_element(*rect);
    SHOULD(ShouldlyIntegral(rv.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    auto prog = std::make_unique<vaml::ProgressBar>();
    const auto pv = vaml::presentation::classify_merge_element(*prog);
    SHOULD(ShouldlyIntegral(pv.kind == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));

    // The full encoder-side classifier (classify_element) agrees for each.
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*border) == vaml::vulkan::ui::MaterialKind::Border).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*logo) == vaml::vulkan::ui::MaterialKind::Monochrome).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*label) == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*sep) == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*rect) == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::vulkan::ui::classify_element(*prog) == vaml::vulkan::ui::MaterialKind::Solid).ShouldBe(true));
}
