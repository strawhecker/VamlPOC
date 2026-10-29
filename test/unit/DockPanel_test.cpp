// S470 (dep:S466) — DockPanel control RESOLVED (Sprint 11c). The ONLY missing
// layout container (StackPanel/Grid/WrapPanel/Canvas are already REAL). Re-based
// onto UIElement and delivers the WPF-faithful dock-stacking algorithm:
//
//   1. ATTACHED `DockPanel.Dock` DP (Int32 path, enum Dock {0..4}, owner
//      "DockPanel", default Fill) resolves on ANY DependencyObject via the S043
//      attached-aware path — this RESOLVES the S462-recorded NO-GAP
//      ("DockPanel.Dock float is S470") at its own step.
//   2. MeasureOrder: children offered unbounded (S123 natural), accumulating the
//      consumed dock track (Left/Right consume width, Top/Bottom consume height);
//      the LAST child (Fill) takes the remaining slot.
//   3. ArrangeOrder: children placed at cumulative dock origins; the last child
//      fills the remaining interior rect.
//   4. classify_merge_element / classify_element -> MaterialKind::Solid.
//   5. Registered as a built-in (registry 16 -> 17).
//
// ADR-018: zero locks (pure layout over the borrowed tree).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"     // DockPanel, Dock, register_builtin_elements, TextBlock, Button
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"   // HorizontalAlignment / VerticalAlignment

using vaml::Dock;
using vaml::DockPanel;
using vaml::HorizontalAlignment;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::VerticalAlignment;
using vaml::register_builtin_elements;

namespace {

// A test-local leaf with a real content extent (the S125 TextLikeLeaf pattern)
// so the docked tracks are deterministic (content-sized, not default-boxed).
class DockLeaf : public UIElement
{
public:
    explicit DockLeaf(vaml::Size content) : content_(content) {}

protected:
    vaml::Size MeasureOverride(const vaml::Size& available) const override
    {
        (void)available;
        return content_;
    }

private:
    vaml::Size content_;
};

} // namespace

// ---------------------------------------------------------------------------
// 1. Attached Dock DP: default Fill + set/read on a plain DependencyObject
// ---------------------------------------------------------------------------

TEST_CASE("S470: DockPanel.Dock attached DP — default Fill, set/read on any object")
{
    // Constructing a DockPanel registers the attached DP globally (the S470
    // self-registration). A plain UIElement child (not even a DockPanel) then
    // carries the attached Dock.
    DockPanel reg;
    (void)reg;
    UIElement child;
    // Default is Fill (4) — a child with no explicit dock is the Fill slot.
    SHOULD(ShouldlyIntegral(DockPanel::dock(child)).ShouldBe(4));

    // Set/read on the plain object (the attached-aware path).
    (void)DockPanel::set_dock(child, static_cast<std::int32_t>(Dock::Left));
    SHOULD(ShouldlyIntegral(DockPanel::dock(child)).ShouldBe(0)); // Left = 0
    (void)DockPanel::set_dock(child, static_cast<std::int32_t>(Dock::Top));
    SHOULD(ShouldlyIntegral(DockPanel::dock(child)).ShouldBe(1));
    (void)DockPanel::set_dock(child, static_cast<std::int32_t>(Dock::Right));
    SHOULD(ShouldlyIntegral(DockPanel::dock(child)).ShouldBe(2));
    (void)DockPanel::set_dock(child, static_cast<std::int32_t>(Dock::Bottom));
    SHOULD(ShouldlyIntegral(DockPanel::dock(child)).ShouldBe(3));
}

// ---------------------------------------------------------------------------
// 2. Dock order placement: Left/Top/Right/Bottom + last-child Fill
// ---------------------------------------------------------------------------

TEST_CASE("S470: dock order placement — Left/Top + last-child Fill")
{
    (void)vaml::register_builtin_elements();
    // Need a DockPanel constructed to register the attached DP.
    DockPanel reg;
    (void)reg;
    DockPanel panel;
    // Left child = 30 wide x 100 tall; Top child = 100 wide x 20 tall; the last
    // (Fill) child takes the remaining interior.
    DockLeaf* left = panel.create_child<DockLeaf>(vaml::Size{ 30.0f, 100.0f });
    DockLeaf* top  = panel.create_child<DockLeaf>(vaml::Size{ 100.0f, 20.0f });
    DockLeaf* fill = panel.create_child<DockLeaf>(vaml::Size{ 10.0f, 10.0f });

    (void)DockPanel::set_dock(*left, static_cast<std::int32_t>(Dock::Left));
    (void)DockPanel::set_dock(*top,  static_cast<std::int32_t>(Dock::Top));
    (void)DockPanel::set_dock(*fill, static_cast<std::int32_t>(Dock::Fill)); // default

    panel.Measure(vaml::Size{ 100.0f, 100.0f });
    panel.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const vaml::Rect leftSlot = left->final_rect();
    const vaml::Rect topSlot  = top->final_rect();
    const vaml::Rect fillSlot = fill->final_rect();

    // The Left child spans the full height (the Top child comes AFTER it, so the
    // Left track is not cut by the top's height).
    SHOULD(ShouldlyString(leftSlot.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(leftSlot.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(leftSlot.width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(leftSlot.height).ShouldBe(100.0f));

    // The Top child is placed after the Left track (x=30, since Left consumed
    // 30 width), at the top edge, spanning the remaining width.
    SHOULD(ShouldlyString(topSlot.x).ShouldBe(30.0f));      // after the Left track
    SHOULD(ShouldlyString(topSlot.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(topSlot.width).ShouldBe(70.0f)); // 100 - 30
    SHOULD(ShouldlyString(topSlot.height).ShouldBe(20.0f));

    // The Fill child takes the remaining interior: right of Left, below Top.
    // Left consumed 30 width, Top consumed 20 height -> Fill = {30, 20, 70, 80}.
    SHOULD(ShouldlyString(fillSlot.x).ShouldBe(30.0f));
    SHOULD(ShouldlyString(fillSlot.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(fillSlot.width).ShouldBe(70.0f));
    SHOULD(ShouldlyString(fillSlot.height).ShouldBe(80.0f));
}

TEST_CASE("S470: dock order — Right and Bottom dock against their edges")
{
    (void)vaml::register_builtin_elements();
    DockPanel reg;
    (void)reg;
    DockPanel panel;
    DockLeaf* right = panel.create_child<DockLeaf>(vaml::Size{ 40.0f, 100.0f });
    DockLeaf* bottom = panel.create_child<DockLeaf>(vaml::Size{ 100.0f, 35.0f });
    DockLeaf* fill = panel.create_child<DockLeaf>(vaml::Size{ 10.0f, 10.0f });

    (void)DockPanel::set_dock(*right, static_cast<std::int32_t>(Dock::Right));
    (void)DockPanel::set_dock(*bottom, static_cast<std::int32_t>(Dock::Bottom));
    (void)DockPanel::set_dock(*fill, static_cast<std::int32_t>(Dock::Fill));

    panel.Measure(vaml::Size{ 100.0f, 100.0f });
    panel.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    // Right docks to the right edge.
    SHOULD(ShouldlyString(right->final_rect().x).ShouldBe(60.0f)); // 100 - 40
    SHOULD(ShouldlyString(right->final_rect().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(right->final_rect().height).ShouldBe(100.0f));
    // Bottom docks to the bottom edge.
    SHOULD(ShouldlyString(bottom->final_rect().y).ShouldBe(65.0f)); // 100 - 35
    SHOULD(ShouldlyString(bottom->final_rect().height).ShouldBe(35.0f));
    // Fill takes the remaining interior: right of left-edge, above bottom.
    SHOULD(ShouldlyString(fill->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(fill->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(fill->final_rect().width).ShouldBe(60.0f));
    SHOULD(ShouldlyString(fill->final_rect().height).ShouldBe(65.0f));
}

// ---------------------------------------------------------------------------
// 3. DesiredSize = Σ consumed tracks (a docked child contributes its extent)
// ---------------------------------------------------------------------------

TEST_CASE("S470: DesiredSize sums the consumed dock tracks")
{
    (void)vaml::register_builtin_elements();
    DockPanel reg;
    (void)reg;
    DockPanel panel;
    // Make the PANEL itself non-stretch so DesiredSize == the natural dock extent.
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    // A Left (30 wide) + a Top (20 tall) both NON-last, + a last Fill child
    // (consumes no track). DesiredSize = {30, 20} — the Σ consumed tracks.
    DockLeaf* left = panel.create_child<DockLeaf>(vaml::Size{ 30.0f, 100.0f });
    DockLeaf* top = panel.create_child<DockLeaf>(vaml::Size{ 100.0f, 20.0f });
    DockLeaf* fill = panel.create_child<DockLeaf>(vaml::Size{ 10.0f, 10.0f });
    (void)DockPanel::set_dock(*left, static_cast<std::int32_t>(Dock::Left));
    (void)DockPanel::set_dock(*top, static_cast<std::int32_t>(Dock::Top));
    (void)DockPanel::set_dock(*fill, static_cast<std::int32_t>(Dock::Fill));

    panel.Measure(vaml::Size{ 100.0f, 100.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(20.0f));
}

// ---------------------------------------------------------------------------
// 4. Registered as a built-in (registry 16 -> 17) + derives UIElement
// ---------------------------------------------------------------------------

TEST_CASE("S470: DockPanel derives UIElement + is a registered built-in")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();
    auto e = g.try_create("DockPanel");
    SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
    if (e)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::DockPanel*>(e.get()) != nullptr)
                   .ShouldBe(true));
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                   .ShouldBe(true));
    }
}
