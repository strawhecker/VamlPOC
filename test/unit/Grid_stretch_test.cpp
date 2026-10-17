// S133 (dep:S132) — Grid star-sizing + span tests (exhaustive suite).
//
// TEST-ONLY gate (the S040/S042/S044/S125/S128/S130 precedent): zero new
// product symbols. It exercises the REAL S132 Grid (span-aware two-pass
// measure/arrange) against the D1/D2 decisions settled at S132 review:
//   * D2 Auto equal-division (content/span per spanned Auto track);
//   * D1 star min/re-award (a Star track grows to at least its content minimum).
// This suite is the recorded exhaustive star-sizing + span gate; S132 shipped
// the focused math + product, S133 asserts it exactly (test-the-truth).
//
// Instrument = test-local RecordLeaf (S125/S130 pattern) recording the exact
// measure offer + final arrange slot on both axes, so Stretch-vs-Wrap and the
// merged-cell span extents are OBSERVABLE. Track extents are inferred from
// children's FINAL arranged rects + recorders (deterministic, position-based).
//
// NO MINIMAL REAL introduced (S132 already resolved the span surface; S133 adds
// no product symbol).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"       // Grid, RowDefinition, ColumnDefinition
#include "vaml/core/Geometry.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/UIElement.h"

using vaml::ColumnDefinition;
using vaml::Grid;
using vaml::GridLength;
using vaml::HorizontalAlignment;
using vaml::Rect;
using vaml::RowDefinition;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// Test-local RecorderLeaf (S125/S130 pattern): configured content extent +
// RECORDS the measure offer + arrange slot on both axes. Track/span extents are
// inferred from children's arranged slots (position-based, deterministic).
class RecordLeaf : public UIElement
{
public:
    explicit RecordLeaf(Size content) : content_(content) {}

    Size last_available() const noexcept { return lastAvailable; }
    Rect last_slot() const noexcept { return lastSlot; }
    int measure_count() const noexcept { return measureCalls; }
    int arrange_count() const noexcept { return arrangeCalls; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        lastAvailable = available;
        ++measureCalls;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        lastSlot = finalSlot;
        ++arrangeCalls;
    }

private:
    Size content_;
    mutable int measureCalls = 0;
    int arrangeCalls = 0;
    mutable Size lastAvailable;
    Rect lastSlot;
};

} // namespace

// ============================================================
// PART A — Star proportional sizing (no spans)
// ============================================================

TEST_CASE("S133 A1: star 1:1 splits remaining equally")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(1.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    // No fixed/auto -> remaining = 300 -> each star 150.
    RecordLeaf* a = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    RecordLeaf* b = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    SHOULD(ShouldlyString(a->last_slot().width).ShouldBe(150.0f));
    SHOULD(ShouldlyString(a->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->last_slot().x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(b->last_slot().width).ShouldBe(150.0f));
}

TEST_CASE("S133 A2: star 3:1 splits remaining by weight")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(3.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    RecordLeaf* a = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    RecordLeaf* b = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 100.0f });

    // remaining 400 -> col0 = 300, col1 = 100.
    SHOULD(ShouldlyString(a->last_slot().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(b->last_slot().x).ShouldBe(300.0f));
    SHOULD(ShouldlyString(b->last_slot().width).ShouldBe(100.0f));
}

TEST_CASE("S133 A3: star splits remaining AFTER absolute + auto are taken")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Star(2.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(50.0f) } });
    // Fixed = 100 (pixel) + 50 (auto content) = 150. Available 450 -> remaining
    // 300 exactly, split 2:1 -> col2 = 200, col3 = 100 (clean division, so the
    // float path is exact — no tolerance needed).
    RecordLeaf* autoLeaf = g.create_child<RecordLeaf>(Size{ 50.0f, 10.0f });
    (void)Grid::set_column(*autoLeaf, 1);
    RecordLeaf* s2 = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*s2, 2);
    RecordLeaf* s1 = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*s1, 3);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 450.0f, 100.0f });

    // auto column at x=100..150; star2 at x=150..350; star1 at x=350..450.
    SHOULD(ShouldlyString(autoLeaf->last_slot().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(autoLeaf->last_slot().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(s2->last_slot().x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(s2->last_slot().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(s1->last_slot().x).ShouldBe(350.0f));
    SHOULD(ShouldlyString(s1->last_slot().width).ShouldBe(100.0f));
}

TEST_CASE("S133 A4: star clamps when absolute+auto exceed available (no negative)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(300.0f) },
                               ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    RecordLeaf* autoLeaf = g.create_child<RecordLeaf>(Size{ 100.0f, 10.0f });
    (void)Grid::set_column(*autoLeaf, 1);
    RecordLeaf* star = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*star, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // fixed 300+100=400 > 300 -> remaining 0 -> star 0.
    SHOULD(ShouldlyString(star->last_slot().x).ShouldBe(400.0f));
    SHOULD(ShouldlyString(star->last_slot().width).ShouldBe(0.0f));
}

TEST_CASE("S133 A5: totalStar==0 leaves all star tracks at zero")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(0.0f) },
                               ColumnDefinition{ GridLength::Star(0.0f) } });
    RecordLeaf* a = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    RecordLeaf* b = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // Both stars weight 0 -> totalStar 0 -> both 0 wide.
    SHOULD(ShouldlyString(a->last_slot().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->last_slot().width).ShouldBe(0.0f));
}

TEST_CASE("S133 A6: a zero-weight star beside a positive sibling — zero gets 0, positive gets the share")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(0.0f) },
                               ColumnDefinition{ GridLength::Star(2.0f) } });
    RecordLeaf* a = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    RecordLeaf* b = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 400.0f, 100.0f });

    // totalStar = 2 -> positive star gets whole remaining 400; zero star 0.
    SHOULD(ShouldlyString(a->last_slot().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->last_slot().width).ShouldBe(400.0f));
}

TEST_CASE("S133 A7: star never negative — star re-award (D1) with single-cell content min")
{
    // Gated D1: a Star track is re-awarded to its content minimum ONLY when it
    // received a POSITIVE proportional share. A child whose content is SMALLER
    // than the proportional share keeps the star at its (larger) proportional
    // extent (content-min does not inflate when content fits). A child whose
    // content EXCEEDS the share re-awards the star to the content (D1).
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(1.0f) } });
    // Stretch so the leaf FILLS the star track -> its slot width == the track.
    RecordLeaf* small = g.create_child<RecordLeaf>(Size{ 20.0f, 10.0f });

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // Star proportional = 300; content 20 < 300 -> star stays 300 (fill).
    SHOULD(ShouldlyString(small->last_slot().width).ShouldBe(300.0f));

    // Now with a star track offered only 40 while content is 120 -> D1 re-awards
    // the star to 120 (grows to fit content).
    Grid g2;
    g2.set_column_definitions({ ColumnDefinition{ GridLength::Star(1.0f) } });
    RecordLeaf* big = g2.create_child<RecordLeaf>(Size{ 120.0f, 10.0f });

    g2.Measure(Size::Infinite());
    g2.Arrange(Rect{ 0.0f, 0.0f, 40.0f, 100.0f });
    SHOULD(ShouldlyString(big->last_slot().width).ShouldBe(120.0f)); // re-awarded, not 40
}

// ============================================================
// PART B — Star + Span interplay (D1/D2 across the real Grid)
// ============================================================

TEST_CASE("S133 B8: a spanning child re-awards the spanned star track (D1 with spans)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(50.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    // S spans pixel + star; content 160 -> D2 puts 80 content-min on the star.
    // Arranged 120 leaves 70 proportional -> D1 raises the star to 80.
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 160.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    // R default-Stretch fills the re-awarded star track -> its slot width == the
    // star track extent.
    RecordLeaf* r = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*r, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 120.0f, 100.0f });

    // S merged = 50 + 80 = 130; star track (R's slot) = 80.
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(130.0f));
    SHOULD(ShouldlyString(r->last_slot().width).ShouldBe(80.0f));
}

TEST_CASE("S133 B9: span across star + auto tracks (mixed)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    // R in auto col0 (content 40) -> auto col0 = 40. S spans col0+col1; its
    // content 60 -> D2 puts 30 on auto (already 40) + 30 content-min on star.
    // Arranged 200: star = 200-40 = 160 proportional; content-min 30 < 160 ->
    // star stays 160.
    RecordLeaf* r = g.create_child<RecordLeaf>(Size{ 40.0f, 10.0f });
    (void)Grid::set_column(*r, 0);
    (void)r->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)r->set_vertical_alignment(VerticalAlignment::Top);
    // s default-Stretch FILLS the merged span (auto 40 + star 160 = 200).
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 60.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // auto col0 = 40; star col1 = 160; S merged = 40 + 160 = 200.
    SHOULD(ShouldlyString(r->last_slot().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(s->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(200.0f));
}

TEST_CASE("S133 B10: a Stretch child fills a merged star cell")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(1.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    // S spans both star columns, Stretch by default. Star split remaining 300
    // equally -> each 150; merged = 300.
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    SHOULD(ShouldlyString(s->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(300.0f));
}

// ============================================================
// PART C — Auto + Span (D2 equal-division, exhaustive)
// ============================================================

TEST_CASE("S133 C11: 2-wide auto span distributes content 50/50 (D2)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Auto() } });
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 100.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    RecordLeaf* r = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*r, 1);
    (void)r->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)r->set_vertical_alignment(VerticalAlignment::Top);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    SHOULD(ShouldlyString(r->last_slot().x).ShouldBe(50.0f));  // col0 = 100/2
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(100.0f)); // merged 100
}

TEST_CASE("S133 C12: content-driven auto tracks grow for a spanning child")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Auto() } });
    // A spanning child (content 200) larger than any single non-span child: each
    // auto gets 100 (equal division), and the tracks grow to fit the span.
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 200.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    RecordLeaf* r = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*r, 1);
    (void)r->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)r->set_vertical_alignment(VerticalAlignment::Top);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 100.0f });

    SHOULD(ShouldlyString(r->last_slot().x).ShouldBe(100.0f)); // col0 = 200/2
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(200.0f)); // merged
}

// ============================================================
// PART D — Span placement / arrange (merged rect, clamps, whole-grid)
// ============================================================

TEST_CASE("S133 D13: mixed rowspan+columnspan arrangement — origin at anchor, merged size")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(50.0f) },
                               ColumnDefinition{ GridLength::Pixel(200.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(30.0f) },
                            RowDefinition{ GridLength::Pixel(70.0f) } });
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*s, 1);
    (void)Grid::set_row(*s, 0);
    (void)Grid::set_column_span(*s, 2);
    (void)Grid::set_row_span(*s, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 350.0f, 100.0f });

    SHOULD(ShouldlyString(s->last_slot().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(s->last_slot().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(250.0f));  // 50+200
    SHOULD(ShouldlyString(s->last_slot().height).ShouldBe(100.0f)); // 30+70
}

TEST_CASE("S133 D14: a whole-grid span fills the entire grid")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(200.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(40.0f) },
                            RowDefinition{ GridLength::Pixel(60.0f) } });
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)Grid::set_row_span(*s, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 350.0f, 100.0f });

    SHOULD(ShouldlyString(s->last_slot().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->last_slot().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(300.0f)); // 100+200
    SHOULD(ShouldlyString(s->last_slot().height).ShouldBe(100.0f)); // 40+60
}

TEST_CASE("S133 D15: per-child alignment honored inside the merged cell")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(100.0f) } });
    // Center/Bottom child spans both columns: anchors within the merged 200x100
    // cell by its own alignment.
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 50.0f, 20.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Center);
    (void)s->set_vertical_alignment(VerticalAlignment::Bottom);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    SHOULD(ShouldlyString(s->last_slot().x).ShouldBe(75.0f));  // (200-50)/2
    SHOULD(ShouldlyString(s->last_slot().y).ShouldBe(80.0f));  // 100-20
    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(s->last_slot().height).ShouldBe(20.0f));
}

TEST_CASE("S133 D16: span clamps to grid bounds and >= 1")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(50.0f) } });
    // Huge span collapses to bounds; negative clamps to 1.
    RecordLeaf* s = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*s, 99);
    (void)Grid::set_row_span(*s, 9);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    SHOULD(ShouldlyString(s->last_slot().width).ShouldBe(200.0f)); // both cols
    SHOULD(ShouldlyString(s->last_slot().height).ShouldBe(50.0f)); // single row

    RecordLeaf* n = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*n, 1);
    (void)Grid::set_column_span(*n, -3);
    g.InvalidateMeasure();
    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    SHOULD(ShouldlyString(n->last_slot().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(n->last_slot().width).ShouldBe(100.0f)); // single col
}

// ============================================================
// PART E — Dirty-respect with spans
// ============================================================

TEST_CASE("S133 E17: re-measuring/re-arranging ONLY dirty children still holds with spans")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(50.0f) } });
    RecordLeaf* span = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*span, 2); // spans both columns
    RecordLeaf* single = g.create_child<RecordLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*single, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    SHOULD(ShouldlyString(span->arrange_count()).ShouldBe(1));
    SHOULD(ShouldlyString(single->arrange_count()).ShouldBe(1));

    // Invalidate ONLY the SPAN child's arrange. Re-arrange.
    span->InvalidateArrange();
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // Span child re-arranged; clean single child NOT touched.
    SHOULD(ShouldlyString(span->arrange_count()).ShouldBe(2));
    SHOULD(ShouldlyString(single->arrange_count()).ShouldBe(1));
}
