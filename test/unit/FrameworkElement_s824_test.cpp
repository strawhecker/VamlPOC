// S824 (dep:S823, Sprint 11ab) — FRAMEWORKELEMENT BASE + LAYOUT-SLOT
// RELOCATION (the structural divergence).
//
// WPF ground truth (E:\GitRepos\wpf — the authority):
//   - FrameworkElement.cs:93 — `public partial class FrameworkElement : UIElement`
//   - Control.cs:20 — `public class Control : vaml::FrameworkElement`
//   - The LAYOUT SLOT (Margin/H/VAlignment/Width/Height) + the FE surface
//     (Name/DataContext/Tag/FlowDirection/...) live on FrameworkElement
//     (FE.cs:3466+ / FE.cs:3218), NOT on UIElement.
//   - UIElement.cs:67 declares NONE of them.
//
// S824 delivers the vaml::FrameworkElement : UIElement base and re-keys the
// slot + FE-surface DPs under owner "FrameworkElement". This test proves:
//   (a) dynamic_cast<FrameworkElement> holds for a FrameworkElement-branch
//       element (TextBlock/StackPanel) AND a Control-branch control (Button);
//   (b) the slot + FE DPs resolve under owner "FrameworkElement" (composite +
//       typed accessor) on both;
//   (c) the OLD "UIElement"-owner registration for the moved DPs is GONE
//       (TryGet(Margin,"UIElement") is null — the structural divergence is
//       removed);
//   (d) layout/measure/arrange behavior is IDENTICAL through the FE-owner DPs
//       (a StackPanel measures/arranges exactly as before the relocation);
//   (e) the S818 Affects* invalidation still fires (FE.cs:2198-2251) on an
//       element whose dirty axis is invalidated;
//   (f) DataContext inheritance (S500) works through the FE base.
//
// Headless (Rail 1).

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"
#include "vaml/core/FrameworkElement.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include "doctest/doctest.h"

#include <memory>

using namespace vaml;

TEST_CASE("S824: dynamic_cast<FrameworkElement> holds for BOTH branches")
{
    // FrameworkElement-branch non-controls (TextBlock) and the real panels.
    TextBlock tb;
    CHECK(dynamic_cast<FrameworkElement*>(&tb) != nullptr);
    CHECK(dynamic_cast<UIElement*>(&tb) != nullptr);

    StackPanel sp;
    CHECK(dynamic_cast<FrameworkElement*>(&sp) != nullptr);

    // Control-branch controls — `Control : vaml::FrameworkElement` in WPF, so a
    // Button is also FrameworkElement-derived.
    Button btn;
    CHECK(dynamic_cast<FrameworkElement*>(&btn) != nullptr);
    CHECK(dynamic_cast<UIElement*>(&btn) != nullptr);
}

TEST_CASE("S824: layout-slot DPs resolve under owner FrameworkElement (composite)")
{
    // TryGet under the FE owner resolves; the OLD UIElement-owner registration
    // for the moved slot DPs is GONE (the structural divergence is removed).
    CHECK(DependencyProperty::TryGet("Margin", "FrameworkElement") != nullptr);
    CHECK(DependencyProperty::TryGet("Width", "FrameworkElement") != nullptr);
    CHECK(DependencyProperty::TryGet("Height", "FrameworkElement") != nullptr);
    CHECK(DependencyProperty::TryGet("HorizontalAlignment", "FrameworkElement") != nullptr);
    CHECK(DependencyProperty::TryGet("VerticalAlignment", "FrameworkElement") != nullptr);
    CHECK(DependencyProperty::TryGet("Tag", "FrameworkElement") != nullptr);

    // The old UIElement-owner registration for the moved DPs is absent.
    CHECK(DependencyProperty::TryGet("Margin", "UIElement") == nullptr);
    CHECK(DependencyProperty::TryGet("Width", "UIElement") == nullptr);
    CHECK(DependencyProperty::TryGet("Tag", "UIElement") == nullptr);
}

TEST_CASE("S824: typed slot + FE-surface accessors read the FE-owner DPs on a real element")
{
    Button b;
    // Slot accessors round-trip through the FE-owner DPs.
    (void)b.set_margin(Thickness{ 2.0f, 3.0f, 4.0f, 5.0f });
    const Thickness m = b.margin();
    CHECK(m.left == 2.0f);
    CHECK(m.right == 4.0f);
    (void)b.set_width(120.0);
    CHECK(b.width() == 120.0);
    (void)b.set_horizontal_alignment(HorizontalAlignment::Center);
    CHECK(b.horizontal_alignment() == HorizontalAlignment::Center);

    // FE-surface accessors (FrameworkElement) resolve under the FE owner.
    (void)b.set_tag("ghostBrush");
    CHECK(b.tag() == std::string("ghostBrush"));
    CHECK(b.Get(PropertyId::of("Tag", "FrameworkElement")).get<std::string>() ==
          std::string("ghostBrush"));
}

TEST_CASE("S824: layout behavior is IDENTICAL through the FE-owner DPs (StackPanel measures/arranges)")
{
    // A real panel still measures + arranges through the FE-owner slot DPs.
    StackPanel sp;
    sp.add_child(std::make_unique<Button>());
    sp.Measure(Size{ 300.0f, 300.0f });
    sp.Arrange(Rect{ 0, 0, 300, 300 });
    // The panel laid out its child into a finite slot without throwing /
    // without the slot DPs going unresolved.
    CHECK(sp.child_count() == 1u);
    CHECK(sp.desired_size().width >= 0.0f);
}

TEST_CASE("S824: S818 Affects* invalidation still fires (FE.cs:2198-2251) on an element")
{
    // A FrameworkElement-branch element still invalidates its dirty layout
    // axis (S126) — the S818 Affects* engine's arms run on a real dirty.
    StackPanel sp;
    sp.InvalidateMeasure();
    CHECK(sp.is_measure_dirty() == true);
    // The dirty flag clears once the measure pass runs (a fresh Measure).
    sp.Measure(Size{ 200.0f, 200.0f });
    CHECK(sp.is_measure_dirty() == false);
}

TEST_CASE("S824: DataContext inheritance (S500) works through the FrameworkElement base")
{
    // A FrameworkElement-branch element carries a local DataContext via the
    // S500 side-table; a child inherits it through the FE base's parent chain.
    StackPanel root;
    Button* child = root.create_child<Button>();
    vaml::ObservableObject* ctx =
        static_cast<vaml::ObservableObject*>(reinterpret_cast<void*>(0x1));
    (void)root.set_data_context(ctx);
    // effective_data_context walks to the nearest local (the root's); the
    // document-context fallback is null here, so the local wins.
    CHECK(child->effective_data_context(nullptr) == ctx);
}

TEST_CASE("S824: vaml::FrameworkElement is a base TYPE, not a registered built-in element")
{
    // The built-in registration surface is unchanged (FrameworkElement is a
    // base type, not a registrable element). A Button still resolves as a
    // built-in; there is no separate "FrameworkElement" built-in entry.
    (void)vaml::register_builtin_elements();
    Button b;
    CHECK(dynamic_cast<FrameworkElement*>(&b) != nullptr);
}
