// S135 (dep:S134) — WrapPanel tests.
//
// Tests the S134 REAL WrapPanel (resolves at S134 — NO-GAP, not a backfill):
// a UIElement-derived panel presenting `WrapDirection` (default Horizontal,
// not inherited) with a real flow/wrap/align algorithm in Element.cpp.
//
// WHAT IS TESTED:
//   1. Derivation + WrapDirection (default Horizontal; typed round-trip;
//      resolves globally via the S043 attached-aware path on a plain object).
//   2. Flow + wrap: default-Horizontal children overflow the offered width and
//      WRAP to a new row; a child wider than the whole boundary gets its own
//      row; DesiredSize = {max row width, Σ row heights}.
//   3. Aligned rows: row height = tallest child in the row; each child's OWN
//      VerticalAlignment is honored WITHIN its row (Top/Center/Bottom pin it,
//      default-Stretch fills the row's content height).
//   4. Vertical mirror: WrapDirection=Vertical flows top-to-bottom, wraps on
//      height, DesiredSize = {Σ column widths, max column height}.
//   5. Empty panel -> DesiredSize 0, no crash on arrange.
//   6. Dirty-respect (S128): a clean child is not re-measured / not re-arranged.
//   7. Margin enters DesiredSize (S130_1 contract) + is honored at arrange.
//
// ARCHITECTURE: uses the S125/S130 test-local RecorderLeaf pattern — a REAL
// content extent leaf that records the measure offer AND the arrange slot, so
// both the wrap decision (natural extents) and the aligned-row slots are
// observable. The S134 measure offers children at NATURAL (unbounded) size on
// BOTH axes so the wrap decision uses content-correct extents and rows are
// content-heighted (refining the accepted "natural-width basis" to a
// "natural-extent basis" — see Element.cpp S134 notes).
//
// SCOPE (recorded NO-GAP, not silent): ItemWidth/ItemHeight (WPF fixed-item
// mode) is NOT in the S134 title and is deliberately not fabricated here — it
// is a deferred, recorded gap if a later step wants WPF's fixed-item wrapping.
// No MINIMAL REAL introduced by this step.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // WrapPanel, Orientation
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <limits>

using vaml::DependencyObject;
using vaml::HorizontalAlignment;
using vaml::Orientation;
using vaml::PropertyId;
using vaml::Rect;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;
using vaml::WrapPanel;

namespace {

// The unbounded-available sentinel's value (Size::Infinite() uses +inf). A
// local constant so the "measured at natural extent" assertions are readable.
constexpr float kInf = std::numeric_limits<float>::infinity();

// A test-local leaf with a real content extent (the S125/S130 RecorderLeaf
// pattern), recording the MEASURE OFFER + the ARRANGE SLOT on every invoke so
// both the wrap decision's natural-extent basis and the final arranged slot
// are observable per leaf.
class RecorderLeaf : public UIElement
{
public:
    explicit RecorderLeaf(Size content) : content_(content) {}

    Size content() const { return content_; }
    Size last_measure_offer() const { return lastOfferM; }
    Rect last_arrange_slot() const { return lastSlotA; }
    int measure_count() const noexcept { return measureCalls; }
    int arrange_count() const noexcept { return arrangeCalls; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        lastOfferM = available;
        ++measureCalls;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        lastSlotA = finalSlot;
        ++arrangeCalls;
    }

private:
    Size content_;
    mutable Size lastOfferM;
    mutable int measureCalls = 0;
    Rect lastSlotA;
    int arrangeCalls = 0;
};

} // namespace

// --- Derivation + WrapDirection DP ------------------------------------------

TEST_CASE("S135: WrapPanel derives UIElement (visual-tree base), not DependencyObject directly")
{
    WrapPanel panel;
    UIElement* asUi = dynamic_cast<UIElement*>(&panel);
    SHOULD(ShouldlyIntegral(asUi != nullptr).ShouldBe(true));
    DependencyObject* asDobj = dynamic_cast<DependencyObject*>(&panel);
    SHOULD(ShouldlyIntegral(asDobj != nullptr).ShouldBe(true));
}

TEST_CASE("S135: WrapDirection defaults to Horizontal")
{
    WrapPanel panel;
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.wrap_direction()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Horizontal)));
}

TEST_CASE("S135: WrapDirection typed round-trip (Horizontal <-> Vertical)")
{
    WrapPanel panel;
    (void)panel.set_wrap_direction(Orientation::Vertical);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.wrap_direction()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Vertical)));
    (void)panel.set_wrap_direction(Orientation::Horizontal);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(panel.wrap_direction()))
               .ShouldBe(static_cast<std::int32_t>(Orientation::Horizontal)));
}

TEST_CASE("S135: WrapDirection resolves globally on a plain DependencyObject (S043 attached-aware)")
{
    // WrapDirection is registered globally (owner "WrapPanel"), so a plain
    // DependencyObject — which never locally registers it — reads/writes it
    // through the S043 attached-aware path. Default is Horizontal (1).
    DependencyObject fresh;
    SHOULD(ShouldlyString(fresh.Get(PropertyId::of("WrapDirection")))
               .ShouldBe(vaml::Value::Int32(1)));
    DependencyObject plain;
    (void)plain.Set(PropertyId::of("WrapDirection"), vaml::Value::Int32(0)); // Vertical
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("WrapDirection")))
               .ShouldBe(vaml::Value::Int32(0)));
}

// --- Flow + wrap (default Horizontal) ---------------------------------------

TEST_CASE("S135: default-Horizontal — children flow left-to-right; overflow WRAPS to a new row")
{
    // Three content-width 200 leaves (natural extent). Offered width 400: leaves
    // 0+1 fit row 0 (200+200=400), leaf 2 would exceed (400+200 > 400) so it
    // WRAPS to row 1. All leaves are measured at NATURAL (unbounded) extent.
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    // Each leaf Top so its own alignment does not stretch to the row height.
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)c->set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    // Row 0: a at (0,0), b at (200,0).
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(200.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(0.0f));
    // Row 1: c WRAPS below row 0 (row height 50 -> y=50).
    SHOULD(ShouldlyString(c->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(50.0f));
}

TEST_CASE("S135: a child wider than the whole boundary still gets its own row")
{
    // One leaf is 300 wide; the offered boundary is 200. The row is EMPTY when
    // it is considered -> the wrap guard (only when non-empty) does NOT fire, so
    // the over-wide child still gets its own row at x=0 (not wrapped to a zero
    // gap). Its content overflows the boundary (WPF-faithful: WrapPanel does not
    // clip/scale a child smaller than its content).
    WrapPanel panel;
    RecorderLeaf* wide = panel.create_child<RecorderLeaf>(Size{ 300.0f, 40.0f });
    (void)wide->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 200.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    SHOULD(ShouldlyString(wide->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(wide->final_rect().y).ShouldBe(0.0f));
    // Natural width preserved (content overflows the 200 boundary).
    SHOULD(ShouldlyString(wide->final_rect().width).ShouldBe(300.0f));
}

TEST_CASE("S135: DesiredSize (Horizontal) = max row width, sum of row heights")
{
    // Three 200-wide x 50-tall leaves, boundary 400 -> rows {a,b} and {c}.
    // Max row width = 400 (row 0), Σ row heights = 50 + 50 = 100.
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 200.0f, 50.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)c->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    // The panel is Top/Left (non-stretch) so DesiredSize = its natural amount
    // (Measure finalizes non-stretch to min(natural, available)).
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(400.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(100.0f));
}

TEST_CASE("S135: children are measured at NATURAL extent (both axes) so the wrap decision is content-correct")
{
    // The S134 measure offers children Size::Infinite() — even on the CROSS
    // axis — so a default-Stretch leaf still wraps to its content (it cannot
    // stretch against infinity). Verify each child's recorded measure offer is
    // infinite on BOTH axes.
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 20.0f });
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 500.0f, 300.0f });
    SHOULD(ShouldlyIntegral(a->last_measure_offer().width == kInf).ShouldBe(true));
    SHOULD(ShouldlyIntegral(a->last_measure_offer().height == kInf).ShouldBe(true));
}

// --- Aligned rows -----------------------------------------------------------

TEST_CASE("S135: aligned rows — row height is the tallest child; Stretch child fills the row height")
{
    // Row 0: a (height 50) + b (height 30). Row height = 50 (tallest). b is
    // default-Stretch vertical -> fills the row height (50), keeping its natural
    // width. This is the "aligned rows" headline.
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 50.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    // a: Top -> its own content extent (50) at row 0.
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(50.0f));
    // b: default-Stretch -> fills the ROW height (50), not its content 30.
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().height).ShouldBe(50.0f));
}

TEST_CASE("S135: aligned rows — per-child VerticalAlignment pins it within its row (Top/Center/Bottom)")
{
    // Two leaves in one row; a=Top, b=Center, c=Bottom, all in a row of height
    // 100 (the tallest leaf is 100 tall). Each is pinned within that 100 row by
    // its OWN alignment: a at y=0, c at y=100-30=70 (Bottom), b centered at
    // y=(100-30)/2=35.
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 100.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Center);
    (void)c->set_vertical_alignment(VerticalAlignment::Bottom);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    // a: Top -> y 0.
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    // b: Center within row height 100, content 30 -> (100-30)/2 = 35.
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(35.0f));
    // c: Bottom -> 100 - 30 = 70.
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(70.0f));
}

// --- Vertical mirror --------------------------------------------------------

TEST_CASE("S135: Vertical — children flow top-to-bottom; overflow WRAPS to a new column")
{
    WrapPanel panel;
    (void)panel.set_wrap_direction(Orientation::Vertical);
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)c->set_horizontal_alignment(HorizontalAlignment::Left);

    // Boundary height 400. Vertical flow: a+b stack column 0 (200+200=400), c
    // WRAPS to column 1 (left of a).
    panel.Measure(Size{ Size::Infinite().width, 400.0f });
    panel.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 400.0f });

    // Column 0: a at (0,0), b at (0,200).
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(200.0f));
    // Column 1: c WRAPS to the right of a (column width 100 -> x=100).
    SHOULD(ShouldlyString(c->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(0.0f));
}

TEST_CASE("S135: DesiredSize (Vertical) = sum of column widths, max column height")
{
    // Three 100-wide x 200-tall leaves, boundary height 400 -> columns {a,b}
    // and {c}. Σ column widths = 100 + 100 = 200; max column height = 400.
    WrapPanel panel;
    (void)panel.set_wrap_direction(Orientation::Vertical);
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 100.0f, 200.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)c->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, 400.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(400.0f));
}

// --- Empty panel ------------------------------------------------------------

TEST_CASE("S135: empty panel -> DesiredSize 0 and arrange is a no-op")
{
    WrapPanel panel;
    (void)panel.set_wrap_direction(Orientation::Horizontal);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(0.0f));
    // Arrange with no children must not crash.
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });
    SHOULD(ShouldlyIntegral(panel.child_count()).ShouldBe(0u));
}

// --- Dirty-respect (S128 discipline inside the real panel) ------------------

TEST_CASE("S135: dirty-respect — a clean child is not re-measured nor re-arranged")
{
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    // First full layout: both measured + arranged (counters at 1).
    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's measure. Re-run both passes.
    a->InvalidateMeasure();
    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    // a re-measured (2); clean b NOT re-measured (stays 1) — the S128
    // dirty-only-subpass confined to a + its ancestor chain.
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));

    // Arrange: a is arrange-clean after the first pass and was not
    // arrange-invalidated -> a is NOT re-arranged here; b also clean. Both stay
    // at 1 arrange invocation.
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}

TEST_CASE("S135: dirty-respect — an arrange-invalidated child is re-arranged but a clean one is not")
{
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 400.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's arrange. Re-run the arrange pass.
    a->InvalidateArrange();
    panel.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}

// --- Margin (S130_1 contract) -----------------------------------------------

TEST_CASE("S135: margin enters DesiredSize (S130_1) + is honored at arrange")
{
    // A single leaf, 100x30, with margin 10 on each side. Its margin-inflated
    // DesiredSize (120 x 50) propagates into the panel's DesiredSize (Measure
    // wraps the inner extent by the margin and adds it back).
    WrapPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    (void)a->set_margin(vaml::Thickness{ 10.0f, 10.0f, 10.0f, 10.0f });

    panel.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    // DesiredSize includes the margin (content 100 + 20 = 120 wide).
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(120.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));

    panel.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });
    // Arrange: slot = {cursor, rowY, childDesiredW, rowHeight}; the leaf's own
    // alignment inside that slot. Leaf 100x30 Top/Left -> its final rect is at
    // the slot origin (x=10, y=10 after the margin inset), size 100x30.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(10.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
}
