// S137 (dep:S136) — Canvas tests.
//
// Tests the S136 REAL Canvas (resolves at S136 — NO-GAP, not a backfill):
// a UIElement-derived panel presenting TWO ATTACHED DPs, Canvas.Left /
// Canvas.Top (default NaN = "not set" -> effective 0), with a real
// absolute-position measure/arrange algorithm in Element.cpp.
//
// WHAT IS TESTED:
//   1. Derivation + attached Left/Top (default NaN; typed round-trip on finite
//      values; explicit 0 is DISTINCT from unset; resolves globally via the
//      S043 attached-aware path on a plain DependencyObject).
//   2. Measure: children measured at natural extent on BOTH axes (+inf offer);
//      empty canvas -> DesiredSize 0; DesiredSize = bounding extent including
//      finite offsets and IGNORING NaN axes (treated as 0); clamp >= 0 for
//      negative coordinates.
//   3. Arrange/positioning: a child at Left=10,Top=20 lands exactly there at
//      measured size; an un-set axis (NaN) collapses to the origin (0,0);
//      multiple children at distinct origins are ABSOLUTE (no panel flow);
//      negative Left/Top honored at the origin.
//   4. Margin/alignment interplay: a margin'd child still lands at its origin
//      with margin honored (S130_1); a default-Stretch child keeps its natural
//      extent (nothing to stretch into).
//   5. Dirty-respect (S128): a re-measured/re-arranged invalidated child does
//      not touch a clean sibling.
//
// ARCHITECTURE: uses the S125/S130 test-local RecorderLeaf pattern — a REAL
// content extent leaf that records the measure offer AND the arrange slot, so
// both the natural-extent offer and the final origin are observable per leaf.
//
// NO MINIMAL REAL introduced by this step (TEST-ONLY, per the S040/S042/S044/
// S125/S128/S130/S133/S135 precedent).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // Canvas
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <cmath>
#include <limits>

using vaml::Canvas;
using vaml::DependencyObject;
using vaml::HorizontalAlignment;
using vaml::PropertyId;
using vaml::Rect;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// The unbounded-available sentinel's value (Size::Infinite() uses +inf). A
// local constant so the "measured at natural extent" assertions are readable.
constexpr float kInf = std::numeric_limits<float>::infinity();

// A test-local leaf with a real content extent (the S125/S130 RecorderLeaf
// pattern), recording the MEASURE OFFER + the ARRANGE SLOT on every invoke so
// both the natural-extent offer and the final arranged origin are observable.
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

// --- Derivation + attached Left/Top DPs -------------------------------------

TEST_CASE("S137: Canvas derives UIElement (visual-tree base), not DependencyObject directly")
{
    Canvas canvas;
    UIElement* asUi = dynamic_cast<UIElement*>(&canvas);
    SHOULD(ShouldlyIntegral(asUi != nullptr).ShouldBe(true));
    DependencyObject* asDobj = dynamic_cast<DependencyObject*>(&canvas);
    SHOULD(ShouldlyIntegral(asDobj != nullptr).ShouldBe(true));
}

TEST_CASE("S137: Canvas.Left/Top default to NaN (not set)")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    // A child with NO explicit coordinate -> both attached DPs read as NaN.
    SHOULD(ShouldlyIntegral(std::isnan(Canvas::left(*a)) != 0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(std::isnan(Canvas::top(*a)) != 0).ShouldBe(true));
}

TEST_CASE("S137: Canvas.Left/Top typed round-trip on finite values")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Canvas::set_left(*a, 42.5);
    (void)Canvas::set_top(*a, -7.0);
    SHOULD(ShouldlyString(Canvas::left(*a)).ShouldBe(42.5));
    SHOULD(ShouldlyString(Canvas::top(*a)).ShouldBe(-7.0));
}

TEST_CASE("S137: explicit 0 is DISTINCT from unset (NaN) — round-trips as 0")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Canvas::set_left(*a, 0.0);
    // set to 0, NOT NaN.
    SHOULD(ShouldlyIntegral(std::isnan(Canvas::left(*a)) != 0).ShouldBe(false));
    SHOULD(ShouldlyString(Canvas::left(*a)).ShouldBe(0.0));
    // Top is still unset -> NaN.
    SHOULD(ShouldlyIntegral(std::isnan(Canvas::top(*a)) != 0).ShouldBe(true));
}

TEST_CASE("S137: Canvas.Left/Top resolve globally on a plain DependencyObject (S043 attached-aware)")
{
    // The attached DPs are registered globally (owner "Canvas"), so a plain
    // DependencyObject — which never locally registers them — reads/writes them
    // through the S043 attached-aware path. Default is NaN.
    DependencyObject fresh;
    SHOULD(ShouldlyIntegral(std::isnan(
               fresh.Get(PropertyId::of("Canvas.Left")).get<double>()) != 0)
               .ShouldBe(true));
    DependencyObject plain;
    (void)plain.Set(PropertyId::of("Canvas.Left"), vaml::Value::Double(15.0));
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Canvas.Left")).get<double>())
               .ShouldBe(15.0));
}

// --- Measure -----------------------------------------------------------------

TEST_CASE("S137: children are measured at NATURAL extent (both axes) — +inf offer")
{
    // The S136 measure offers every child Size::Infinite() on BOTH axes (the
    // S123 infinity rule), so even a default-Stretch leaf reports its content
    // extent. Verify the recorded offer is infinite on both axes.
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ 500.0f, 300.0f });
    SHOULD(ShouldlyIntegral(a->last_measure_offer().width == kInf).ShouldBe(true));
    SHOULD(ShouldlyIntegral(a->last_measure_offer().height == kInf).ShouldBe(true));
}

TEST_CASE("S137: empty canvas -> DesiredSize 0 and arrange is a no-op")
{
    Canvas canvas;
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ 400.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(canvas.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(canvas.desired_size().height).ShouldBe(0.0f));
    // Arrange with no children must not crash.
    canvas.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 200.0f });
    SHOULD(ShouldlyIntegral(canvas.child_count()).ShouldBe(0u));
}

TEST_CASE("S137: DesiredSize = bounding extent including finite offsets; NaN axes ignored as 0")
{
    // Two leaves: a at (10,20), extent 100x30 -> right 110, bottom 50. b at
    // unset (0,0), extent 50x60 -> right 50, bottom 60. Bounding extent = {110,
    // 60}. The canvas is Top/Left (non-stretch) so DesiredSize = the natural
    // bounding extent.
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = canvas.create_child<RecorderLeaf>(Size{ 50.0f, 60.0f });
    (void)Canvas::set_left(*a, 10.0);
    (void)Canvas::set_top(*a, 20.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    SHOULD(ShouldlyString(canvas.desired_size().width).ShouldBe(110.0f));
    SHOULD(ShouldlyString(canvas.desired_size().height).ShouldBe(60.0f));
}

TEST_CASE("S137: DesiredSize clamps to >= 0 for negative coordinates")
{
    // A single leaf at Left=-100, Top=-50, extent 100x30 -> right -90, bottom
    // -20. Both edges negative -> DesiredSize clamps to 0 (a child fully
    // above/left of the origin still fits a 0 box; never negative).
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)Canvas::set_left(*a, -100.0);
    (void)Canvas::set_top(*a, -50.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    SHOULD(ShouldlyString(canvas.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(canvas.desired_size().height).ShouldBe(0.0f));
}

// --- Arrange / positioning ---------------------------------------------------

TEST_CASE("S137: a child at Left=10,Top=20 lands exactly there at measured size")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)Canvas::set_left(*a, 10.0);
    (void)Canvas::set_top(*a, 20.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
}

TEST_CASE("S137: an un-set axis (NaN) collapses to the origin (0,0)")
{
    // A child with NO explicit coordinates: both axes are NaN -> effective 0,
    // so it lands at the canvas origin at its natural size (the effective
    // WPF-faithful top-left).
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 80.0f, 40.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(80.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(40.0f));
}

TEST_CASE("S137: multiple children at distinct origins are ABSOLUTE (no panel flow)")
{
    // Three children at distinct origins do NOT stack/flow — each lands exactly
    // at its own explicit coordinate (absolute positioning, the Canvas
    // headline). None overlaps the others casually despite the panel flow that
    // StackPanel/WrapPanel would have imposed.
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 50.0f, 50.0f });
    RecorderLeaf* b = canvas.create_child<RecorderLeaf>(Size{ 50.0f, 50.0f });
    RecorderLeaf* c = canvas.create_child<RecorderLeaf>(Size{ 50.0f, 50.0f });
    (void)Canvas::set_left(*a, 0.0);
    (void)Canvas::set_top(*a, 0.0);
    (void)Canvas::set_left(*b, 200.0);
    (void)Canvas::set_top(*b, 100.0);
    (void)Canvas::set_left(*c, 400.0);
    (void)Canvas::set_top(*c, 300.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)c->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)c->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 1000.0f, 500.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(200.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(100.0f));
    SHOULD(ShouldlyString(c->final_rect().x).ShouldBe(400.0f));
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(300.0f));
}

TEST_CASE("S137: negative Left/Top is honored at the origin")
{
    // A child at Left=-50, Top=-20 lands at (canvas.x-50, canvas.y-20). The
    // canvas arranges from its own final render slot origin (0,0 here), so the
    // child's final rect x/y go negative by the explicit offset.
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 40.0f });
    (void)Canvas::set_left(*a, -50.0);
    (void)Canvas::set_top(*a, -20.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(-50.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(-20.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(40.0f));
}

// --- Margin / alignment interplay --------------------------------------------

TEST_CASE("S137: a margin'd child lands at its origin with margin honored (S130_1)")
{
    // Leaf 100x30 at Left=10,Top=20 with margin 10 on each side. Arranged into
    // slot {10,20,100,30} (the explicit origin + natural size); the leaf's OWN
    // margin is applied inside that slot (S124 compute_final_rect + S130_1), so
    // its final rect is inset by 10 -> {20,30}, still 100x30 (its content).
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)Canvas::set_left(*a, 10.0);
    (void)Canvas::set_top(*a, 20.0);
    (void)a->set_margin(vaml::Thickness{ 10.0f, 10.0f, 10.0f, 10.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(20.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
}

TEST_CASE("S137: a default-Stretch child keeps its natural extent (nothing to stretch into)")
{
    // A default-Stretch (WPF-faithful) leaf, un-set Left/Top -> lands at (0,0)
    // at its NATURAL content extent. Despite default-Stretch, there is nothing
    // for it to stretch into — the Canvas slots it at its content size, so its
    // final rect == its content (the S123 infinity rule).
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 120.0f, 60.0f });
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(120.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(60.0f));
}

// --- Dirty-respect (S128 discipline inside the real panel) ------------------

TEST_CASE("S137: dirty-respect — re-measuring an invalidated child does not touch a clean sibling")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)Canvas::set_left(*a, 10.0);
    (void)Canvas::set_top(*a, 20.0);
    (void)Canvas::set_left(*b, 200.0);
    (void)Canvas::set_top(*b, 50.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    // First full layout: both measured + arranged (counters at 1).
    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's measure. Re-run both passes.
    a->InvalidateMeasure();
    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

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

TEST_CASE("S137: dirty-respect — an arrange-invalidated child is re-arranged but a clean one is not")
{
    Canvas canvas;
    RecorderLeaf* a = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = canvas.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)Canvas::set_left(*a, 10.0);
    (void)Canvas::set_top(*a, 20.0);
    (void)Canvas::set_left(*b, 200.0);
    (void)Canvas::set_top(*b, 50.0);
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)canvas.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)canvas.set_vertical_alignment(VerticalAlignment::Top);

    canvas.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's arrange. Re-run the arrange pass.
    a->InvalidateArrange();
    canvas.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 200.0f });

    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}
