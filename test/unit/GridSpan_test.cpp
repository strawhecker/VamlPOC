// S132 (dep:S131) — Grid ColumnSpan/RowSpan + span-aware measure/arrange math.
//
// S131 delivered the re-based Grid: row/column definitions + Absolute/Auto/Star
// resolution + attached Grid.Row/Grid.Column + the BASIC no-span cell
// measure/arrange. S131 explicitly recorded S132 as the NO-GAP owner of the
// SPAN dimension ("cells consumed by spans") + the span-aware two-pass math —
// this TU is that step's FOCUSED suite (the exhaustive star+span gate is S133).
//
// S132 decisions (settled at plan review):
//   * D2 (Auto equal-division): a spanning child's content contributes
//     desired / span-count to EACH spanned Auto track (WPF-faithful). For a
//     single cell this is just the child's desired extent (S131 is a special
//     case — NO-GAP preserved).
//   * D1 (star min/re-award): every child sets a content MINIMUM on each
//     spanned track; a resolved Star track grows to at least that minimum, so a
//     spanning (or single-cell) child whose content exceeds its proportional
//     share forces the star track to fit the content.
//
// New product surface: ATTACHED DPs Grid.RowSpan / Grid.ColumnSpan (default 1,
// clamped >= 1 and to grid bounds) + typed statics Grid::set_row_span /
// set_column_span / row_span / column_span. NO MINIMAL REAL introduced.
//
// Test-only instrument = RecorderLeaf (S125/S128 pattern): records the measure
// offer + arrange slot. Track/span extents are inferred from children's FINAL
// arranged rects (deterministic, position-based, as in the S131 Grid suite).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // Grid, RowDefinition, ColumnDefinition
#include "vaml/core/Geometry.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/UIElement.h"

using vaml::ColumnDefinition;
using vaml::DependencyObject;
using vaml::Grid;
using vaml::GridLength;
using vaml::HorizontalAlignment;
using vaml::Rect;
using vaml::RowDefinition;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// Test-local leaf returning a configured content extent, RECORDING the last
// measure offer + arrange slot (the S125/S128 RecorderLeaf pattern). Both axes
// observable so span/merged-cell extents are provable from arrange positions.
class RecorderLeaf : public UIElement
{
public:
    explicit RecorderLeaf(Size content) : content_(content) {}

    Size last_available() const noexcept { return lastAvailable; }
    Rect last_slot() const noexcept { return lastSlot; }
    int arrange_count() const noexcept { return arrangeCalls; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        lastAvailable = available;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        lastSlot = finalSlot;
        ++arrangeCalls;
    }

private:
    Size content_;
    mutable Size lastAvailable;
    Rect lastSlot;
    int arrangeCalls = 0;
};

} // namespace

// --- Attached RowSpan / ColumnSpan DPs --------------------------------------

TEST_CASE("S132: Grid.RowSpan / Grid.ColumnSpan default to 1 on a plain child")
{
    DependencyObject child;
    SHOULD(ShouldlyString(Grid::row_span(child)).ShouldBe(1));
    SHOULD(ShouldlyString(Grid::column_span(child)).ShouldBe(1));
}

TEST_CASE("S132: attached Grid.RowSpan / Grid.ColumnSpan set + read (S043)")
{
    DependencyObject child;
    (void)Grid::set_row_span(child, 3);
    (void)Grid::set_column_span(child, 2);
    SHOULD(ShouldlyString(Grid::row_span(child)).ShouldBe(3));
    SHOULD(ShouldlyString(Grid::column_span(child)).ShouldBe(2));
    // Raw attached-aware Set/Get path (global name resolution).
    SHOULD(ShouldlyString(child.Get(vaml::PropertyId::of("Grid.RowSpan")).get<std::int32_t>()).ShouldBe(3));
    SHOULD(ShouldlyString(child.Get(vaml::PropertyId::of("Grid.ColumnSpan")).get<std::int32_t>()).ShouldBe(2));
}

// --- D2: Auto equal-division across a span ---------------------------------

TEST_CASE("S132: a 2-wide ColumnSpan child distributes its content 50/50 across two Auto columns (D2)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Auto() } });
    // S spans columns 0-1 with content width 100 -> each Auto track gets 100/2=50.
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 100.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    // R in column 1 (content 10) lets us observe column 0's resolved width as R's x.
    RecorderLeaf* r = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*r, 1);
    (void)r->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)r->set_vertical_alignment(VerticalAlignment::Top);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // Both Auto columns = 50; R in column 1 starts at x = 50 (col0 width).
    SHOULD(ShouldlyString(r->final_rect().x).ShouldBe(50.0f));
    // S's merged cell = col0(50) + col1(50) = 100 wide.
    SHOULD(ShouldlyString(s->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->final_rect().width).ShouldBe(100.0f));
}

TEST_CASE("S132: a 3-row RowSpan child distributes its height 1/3 across three Auto rows (D2)")
{
    Grid g;
    g.set_row_definitions({ RowDefinition{ GridLength::Auto() },
                            RowDefinition{ GridLength::Auto() },
                            RowDefinition{ GridLength::Auto() } });
    // S spans rows 0-2 with content height 90 -> each Auto row gets 90/3=30.
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 10.0f, 90.0f });
    (void)Grid::set_row_span(*s, 3);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    // R in row 2 lets us observe row extents via R's y.
    RecorderLeaf* r = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_row(*r, 2);
    (void)r->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)r->set_vertical_alignment(VerticalAlignment::Top);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 200.0f });

    // rows 0,1 each 30 -> R at row 2 starts at y = 60.
    SHOULD(ShouldlyString(r->final_rect().y).ShouldBe(60.0f));
    // S merged height = 30+30+30 = 90.
    SHOULD(ShouldlyString(s->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->final_rect().height).ShouldBe(90.0f));
}

// --- Span placement / arrange ----------------------------------------------

TEST_CASE("S132: a child spans a merged rect — origin at anchor, size = sum of spanned tracks")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(50.0f) },
                               ColumnDefinition{ GridLength::Pixel(200.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(30.0f) },
                            RowDefinition{ GridLength::Pixel(70.0f) } });
    // S at anchor (1,0) spans columns 1-2 and rows 0-1.
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*s, 1);
    (void)Grid::set_row(*s, 0);
    (void)Grid::set_column_span(*s, 2);
    (void)Grid::set_row_span(*s, 2);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 350.0f, 100.0f });

    // Origin = anchor origin (x=100, y=0); size = Σ spans: w=50+200=250, h=30+70=100.
    SHOULD(ShouldlyString(s->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(s->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->final_rect().width).ShouldBe(250.0f));
    SHOULD(ShouldlyString(s->final_rect().height).ShouldBe(100.0f));
}

TEST_CASE("S132: span clamps — a huge span collapses to grid bounds; negatives become 1")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(50.0f) } });
    // S spans far past the 2-column / 1-row bounds -> collapses to the bounds.
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*s, 99);
    (void)Grid::set_row_span(*s, 9);
    // D in column 1 lets us confirm the bound collapse (S fills both columns).
    RecorderLeaf* d = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*d, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // S merged = col0+col1 = 200 wide, single row 50 tall.
    SHOULD(ShouldlyString(s->final_rect().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(s->final_rect().height).ShouldBe(50.0f));
    SHOULD(ShouldlyString(d->final_rect().x).ShouldBe(100.0f));

    // Negative span clamps >= 1: a colspan -3 child occupies ONE column (100).
    RecorderLeaf* n = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*n, 1);
    (void)Grid::set_column_span(*n, -3);
    g.InvalidateMeasure();
    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    SHOULD(ShouldlyString(n->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(n->final_rect().width).ShouldBe(100.0f));
}

// --- Stretch inside a merged cell ------------------------------------------

TEST_CASE("S132: a default-Stretch child fills its whole merged (spanned) cell")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2); // spans both 100px columns; Stretch by default

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // Stretch fills the merged 200-wide cell (not just the 100px anchor).
    SHOULD(ShouldlyString(s->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(s->final_rect().width).ShouldBe(200.0f));
}

// --- D1: star min/re-award with spans --------------------------------------

TEST_CASE("S132: a spanning child whose content exceeds the star's proportional share re-awards it (D1)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(50.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    // S spans the pixel(50) + star track; content width 160 -> equal-division
    // (D2) puts 80 content-min on the star track. Arranged width 120 leaves 70
    // proportional, so D1 re-awards the star track up to 80 (grows to fit).
    RecorderLeaf* s = g.create_child<RecorderLeaf>(Size{ 160.0f, 10.0f });
    (void)Grid::set_column_span(*s, 2);
    (void)s->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)s->set_vertical_alignment(VerticalAlignment::Top);
    // R in the star column (x = 50 = pixel width). R is default-Stretch so it
    // FILLS the re-awarded star track, making the track extent observable as
    // R's slot width.
    RecorderLeaf* r = g.create_child<RecorderLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*r, 1);

    g.Measure(Size::Infinite());
    g.Arrange(Rect{ 0.0f, 0.0f, 120.0f, 100.0f });

    // star proportional = 120-50 = 70, but content-min 80 -> star grows to 80.
    // S merged = 50 + 80 = 130.
    SHOULD(ShouldlyString(s->final_rect().width).ShouldBe(130.0f));
    // R (in star col, Left) still anchors at x = 50 (pixel width).
    SHOULD(ShouldlyString(r->final_rect().x).ShouldBe(50.0f));
    // The re-awarded star track (80) is observable as R's slot width.
    SHOULD(ShouldlyString(r->last_slot().width).ShouldBe(80.0f));
}
