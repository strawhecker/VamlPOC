// S129 (dep:S128) — StackPanel (vertical+horizontal; orientation prop; last-child-stretch).
//
// Resolves the S087 StackPanel MINIMAL REAL seed at its own sprint (NO-GAP, not
// a backfill): re-based onto UIElement and delivered as a REAL panel with a real
// Orientation DP + MeasureOverride + ArrangeOverride + last-child-stretch.
//
// ARCHITECTURE: this is the S125 ChildMeasuringPanel archetype (which S125
// explicitly annotated "real StackPanel algorithm + last-child-stretch -> S129")
// raised to the real control. The real StackPanel:
//   * derives UIElement (visual-tree + Measure/Arrange participation);
//   * self-registers a real "Orientation" DP (default Vertical, not inherited)
//     via the S043 attached-aware global path;
//   * MeasureOverride is orientation-aware (vertical: {max child w, Σ child h};
//     horizontal: {Σ child w, max child h}) and re-measures ONLY dirty children
//     (S128 dirty-only-subpass pattern realized inside the real panel);
//   * ArrangeOverride places children top-to-bottom / left-to-right and gives
//     the LAST child the remaining track (last-child-stretch), honoring each
//     child's own alignment inside its slot via child->Arrange, and re-arranges
//     ONLY dirty children.
//
// Scope boundary (S129 ships focused tests per the established
// implementation-ships-tests precedent): the DEDICATED exhaustive DesiredSize +
// stretch suite is S130 (dep:S129) — recorded NO-GAP, not silent. Tests here
// use the S129-unique "Orientation" global DP (S039 discipline: names carry the
// owning prefix "StackPanel" from the real registration).
//
// NO-GAP: resolves the StackPanel seed (no MINIMAL REAL introduced/resolved
// beyond that); S130 = exhaustive stretch/DesiredSize suite; S131 (Grid) adopts
// the panel Measure/Arrange pattern. Nothing left as a silent backfill.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // StackPanel, Orientation
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

using vaml::DependencyObject;
using vaml::HorizontalAlignment;
using vaml::Orientation;
using vaml::PropertyId;
using vaml::Rect;
using vaml::Size;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// A test-local leaf with a real content extent (the S125 TextLikeLeaf pattern),
// counting BOTH MeasureOverride + ArrangeOverride so both axes are observable
// per leaf (the S128 DirtyLeaf discipline). MeasureOverride is const, so the
// measure counter is mutable; ArrangeOverride is non-const.
class CountingLeaf : public UIElement
{
public:
    explicit CountingLeaf(Size content) : content_(content) {}

    int measure_count() const noexcept { return measureCalls; }
    int arrange_count() const noexcept { return arrangeCalls; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        ++measureCalls;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        (void)finalSlot;
        ++arrangeCalls;
    }

private:
    Size content_;
    mutable int measureCalls = 0;
    int arrangeCalls = 0;
};

} // namespace

// --- Derivation + Orientation DP -------------------------------------------

TEST_CASE("S129: StackPanel derives UIElement (visual-tree base), not DependencyObject directly")
{
    StackPanel panel;
    // The real panel participates in Measure/Arrange + the visual tree.
    UIElement* asUi = dynamic_cast<UIElement*>(&panel);
    SHOULD(ShouldlyIntegral(asUi != nullptr).ShouldBe(true));
    // It is also still a DependencyObject transitively.
    DependencyObject* asDobj = dynamic_cast<DependencyObject*>(&panel);
    SHOULD(ShouldlyIntegral(asDobj != nullptr).ShouldBe(true));
}

TEST_CASE("S129: Orientation defaults to Vertical")
{
    StackPanel panel;
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.orientation()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Vertical)));
}

TEST_CASE("S129: Orientation typed round-trip (Vertical <-> Horizontal)")
{
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.orientation()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Horizontal)));
    (void)panel.set_orientation(Orientation::Vertical);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.orientation()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Vertical)));
}

TEST_CASE("S129: Orientation resolves globally on a plain DependencyObject (S043 attached-aware)")
{
    // The Orientation DP is registered globally (owner "StackPanel"), so a
    // plain DependencyObject — which never locally registers it — reads/writes
    // it through the S043 attached-aware path.
    DependencyObject plain;
    (void)plain.Set(PropertyId::of("Orientation"), vaml::Value::Int32(1)); // Horizontal
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Orientation")))
               .ShouldBe(vaml::Value::Int32(0 + 1)));
    // Default (no set yet) is Vertical(0).
    DependencyObject fresh;
    SHOULD(ShouldlyString(fresh.Get(PropertyId::of("Orientation")))
               .ShouldBe(vaml::Value::Int32(0)));
}

// --- Vertical stacking ------------------------------------------------------

TEST_CASE("S129: vertical — children stack top-to-bottom in order with correct final_rects")
{
    // Measure the panel (Stretch fills the offered bound on both axes — clean
    // leaves), then arrange into a 1000-wide x 200-tall slot.
    StackPanel panel;
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 1000.0f, 200.0f });
    // ArrangeOverride auto-drive (S127) places each child into its column slot.

    // A: row 0, content width, child height.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
    // B: row 1 stacked below A (A's desired height 30).
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(b->final_rect().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(b->final_rect().height).ShouldBe(20.0f));
}

TEST_CASE("S129: vertical — per-child HorizontalAlignment honored within its full-width row")
{
    // In a vertical stack each child's slot is the full panel width (its row),
    // so the child's OWN HorizontalAlignment pins it within that row: a Left
    // leaf sits at the row's left, a Center leaf of equal desired width is
    // centered. This is the S125 per-child-alignment-in-slot pattern realized
    // by the real panel.
    StackPanel panel;
    CountingLeaf* left = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    CountingLeaf* center = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    (void)left->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)center->set_horizontal_alignment(HorizontalAlignment::Center);
    (void)left->set_vertical_alignment(VerticalAlignment::Top);
    (void)center->set_vertical_alignment(VerticalAlignment::Top);
    // Panel is default Stretch/Stretch -> fills the 1000-wide arrange slot, so
    // each child's row is the full 1000-wide track (alignment is meaningful).

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 1000.0f, 200.0f });

    // Left leaf pinned to the row's left edge (x = 0).
    SHOULD(ShouldlyString(left->final_rect().x).ShouldBe(0.0f));
    // Center leaf centered in the 1000-wide row -> x = (1000-100)/2 = 450.
    SHOULD(ShouldlyString(center->final_rect().x).ShouldBe(450.0f));
}

// --- Horizontal stacking ----------------------------------------------------

TEST_CASE("S129: horizontal — children stack left-to-right in order")
{
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, 500.0f });
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 200.0f });

    // A: column 0.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    // B: column 1, to the right of A (A's desired width 100).
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(b->final_rect().width).ShouldBe(50.0f));
}

// --- Last-child-stretch -----------------------------------------------------

TEST_CASE("S129: last-child-stretch — last default-Stretch child fills the remaining vertical space")
{
    // Two leaves; the LAST is default-Stretch on the vertical axis, the first
    // is not. The panel itself is default-Stretch (both axes) so it FILLS the
    // 100x100 arrange slot; the last child's slot = the remaining panel height
    // below the first child; being Stretch it fills that remaining track
    // (last-child-stretch), while the first keeps its own content height.
    StackPanel panel;
    CountingLeaf* first = panel.create_child<CountingLeaf>(Size{ 50.0f, 30.0f });
    CountingLeaf* last = panel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    (void)first->set_vertical_alignment(VerticalAlignment::Top);
    (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)last->set_horizontal_alignment(HorizontalAlignment::Left);
    // Panel is default Stretch/Stretch -> fills the 100x100 arrange slot.

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    // first: its own content height (30), row 0.
    SHOULD(ShouldlyString(first->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(first->final_rect().height).ShouldBe(30.0f));
    // last: fills the remaining panel height (100 - 30 = 70), row 30.
    SHOULD(ShouldlyString(last->final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(70.0f));
}

TEST_CASE("S129: last-child-stretch — a non-Stretch last child does NOT over-fill the remaining space")
{
    // Same layout as above but the LAST child is Top (non-Stretch): its slot is
    // the full remaining 70 (the panel fills the 100x100 slot), but being
    // non-Stretch it keeps its CONTENT height (10) instead of filling (no
    // over-fill / no blind stretch).
    StackPanel panel;
    CountingLeaf* first = panel.create_child<CountingLeaf>(Size{ 50.0f, 30.0f });
    CountingLeaf* last = panel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    (void)first->set_vertical_alignment(VerticalAlignment::Top);
    (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)last->set_vertical_alignment(VerticalAlignment::Top);
    (void)last->set_horizontal_alignment(HorizontalAlignment::Left);
    // Panel is default Stretch/Stretch -> fills the 100x100 arrange slot.

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    SHOULD(ShouldlyString(first->final_rect().height).ShouldBe(30.0f));
    // last is non-Stretch: content height 10, NOT the 70 remaining.
    SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(10.0f));
}

TEST_CASE("S129: last-child-stretch (horizontal) — last default-Stretch child fills remaining width")
{
    // Vertical analogue: the panel fills the 200-wide arrange slot; the last
    // (default-Stretch horizontal) child fills the remaining width after the
    // first child's content width.
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    CountingLeaf* first = panel.create_child<CountingLeaf>(Size{ 100.0f, 40.0f });
    CountingLeaf* last = panel.create_child<CountingLeaf>(Size{ 30.0f, 20.0f });
    (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)first->set_vertical_alignment(VerticalAlignment::Top);
    (void)last->set_vertical_alignment(VerticalAlignment::Top);
    // Panel is default Stretch/Stretch -> fills the 200x100 arrange slot.

    panel.Measure(Size{ Size::Infinite().width, 500.0f });
    panel.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // first: content width 100, column 0.
    SHOULD(ShouldlyString(first->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(first->final_rect().width).ShouldBe(100.0f));
    // last: default-Stretch horizontal fills the remaining width (200-100=100).
    SHOULD(ShouldlyString(last->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(last->final_rect().width).ShouldBe(100.0f));
}

// --- DesiredSize derivation -------------------------------------------------

TEST_CASE("S129: vertical panel DesiredSize = (max child width, sum of child heights)")
{
    StackPanel panel;
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));  // max(100,50)
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));  // 30+20
}

TEST_CASE("S129: horizontal panel DesiredSize = (sum of child widths, max child height)")
{
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 100.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, 500.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(150.0f));  // 100+50
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f));  // max(30,20)
}

// --- Dirty-respect (S128 discipline inside the real panel) -----------------

TEST_CASE("S129: dirty-respect — a clean child is not re-measured nor re-arranged")
{
    StackPanel panel;
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 50.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 40.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    // First full layout: both measured + arranged (counters at 1).
    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's measure (b stays clean). Re-run both passes.
    a->InvalidateMeasure();
    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    // a re-measured (2); clean b NOT re-measured (stays 1) — the S128
    // dirty-only-subpass confined to a + its ancestor chain.
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));

    // Arrange: a's invalidation also dirtied a's arrange axis (InvalidateMeasure
    // does not clear arrange); but the arrange pass re-arranges only what is
    // arrange-dirty. a was arrange-clean after the first pass and was not
    // arrange-invalidated -> a is NOT re-arranged here; b also clean. Both stay
    // at 1 arrange invocation.
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}

TEST_CASE("S129: dirty-respect — an arrange-invalidated child is re-arranged but a clean one is not")
{
    StackPanel panel;
    CountingLeaf* a = panel.create_child<CountingLeaf>(Size{ 50.0f, 30.0f });
    CountingLeaf* b = panel.create_child<CountingLeaf>(Size{ 40.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's arrange. Re-run the arrange pass.
    a->InvalidateArrange();
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    // a re-arranged (2); clean b NOT re-arranged (stays 1).
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}
