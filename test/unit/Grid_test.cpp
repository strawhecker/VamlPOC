// S131 (dep:S130_1) — Grid: row/col definitions; Absolute/Auto/Star resolution.
//
// Resolves the S087 Grid MINIMAL REAL seed at its own sprint (NO-GAP, not a
// backfill): re-based onto UIElement and delivered as a REAL panel (row/column
// definitions + Absolute/Auto/Star resolution + basic no-span cell measure/
// arrange + attached Grid.Row/Grid.Column DPs).
//
// Scope boundary (established plan, recorded NO-GAP, not silent): S131 ships
// focused tests for the DEFINITION + RESOLUTION + basic-placement surface;
// RowSpan/ColumnSpan + the extended measure/arrange math are S132's NAMED
// scope, and the exhaustive star-sizing + span test suite is S133.
//
// The headline TESTABLE PRIMITIVE is vaml::resolve_grid_lengths — a pure
// public function (defs, autoMeasures, available) -> resolved lengths, with a
// dedicated math suite below + integration through the real Grid.
//
// Naming: the attached DPs use the REAL global names "Grid.Row"/"Grid.Column"
// (S043 attached-aware), so tests use the real names + the typed statics
// Grid::set_row/set_column.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // Grid, RowDefinition, ColumnDefinition
#include "vaml/core/Geometry.h"
#include "vaml/core/GridLength.h"    // GridLength, GridUnitType, resolve_grid_lengths
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <vector>

using vaml::ColumnDefinition;
using vaml::DependencyObject;
using vaml::Grid;
using vaml::GridLength;
using vaml::GridUnitType;
using vaml::HorizontalAlignment;
using vaml::Rect;
using vaml::RowDefinition;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// A test-local leaf with a real content extent (the S125/S129 TextLikeLeaf
// pattern), counting BOTH MeasureOverride + ArrangeOverride so both axes are
// observable per leaf (the S128 DirtyLeaf discipline).
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

// --- Derivation + definitions ------------------------------------------------

TEST_CASE("S131: Grid derives UIElement (visual-tree base), not DependencyObject directly")
{
    Grid g;
    UIElement* asUi = dynamic_cast<UIElement*>(&g);
    SHOULD(ShouldlyIntegral(asUi != nullptr).ShouldBe(true));
    DependencyObject* asDobj = dynamic_cast<DependencyObject*>(&g);
    SHOULD(ShouldlyIntegral(asDobj != nullptr).ShouldBe(true));
}

TEST_CASE("S131: a Grid with no explicit definitions has one implicit Auto row + column")
{
    Grid g;
    SHOULD(ShouldlyIntegral(g.row_definitions().size()).ShouldBe(std::size_t(1)));
    SHOULD(ShouldlyIntegral(g.column_definitions().size()).ShouldBe(std::size_t(1)));
    SHOULD(ShouldlyIntegral(std::size_t(g.row_definitions()[0].height.unitType))
               .ShouldBe(std::size_t(GridUnitType::Auto)));
    SHOULD(ShouldlyIntegral(std::size_t(g.column_definitions()[0].width.unitType))
               .ShouldBe(std::size_t(GridUnitType::Auto)));
}

TEST_CASE("S131: row/column definitions round-trip; emptiness collapses to the implicit Auto track")
{
    Grid g;
    g.set_column_definitions({
        ColumnDefinition{ GridLength::Pixel(100.0f) },
        ColumnDefinition{ GridLength::Auto() },
        ColumnDefinition{ GridLength::Star(2.0f) },
    });
    g.set_row_definitions({
        RowDefinition{ GridLength::Pixel(40.0f) },
        RowDefinition{ GridLength::Star(1.0f) },
    });
    SHOULD(ShouldlyIntegral(g.column_definitions().size()).ShouldBe(std::size_t(3)));
    SHOULD(ShouldlyString(g.column_definitions()[0].width.value).ShouldBe(100.0f));
    SHOULD(ShouldlyIntegral(std::size_t(g.column_definitions()[0].width.unitType))
               .ShouldBe(std::size_t(GridUnitType::Pixel)));
    SHOULD(ShouldlyIntegral(std::size_t(g.column_definitions()[2].width.unitType))
               .ShouldBe(std::size_t(GridUnitType::Star)));
    SHOULD(ShouldlyString(g.row_definitions()[1].height.value).ShouldBe(1.0f));

    // Empty list collapses back to the single implicit Auto track.
    g.set_column_definitions({});
    SHOULD(ShouldlyIntegral(g.column_definitions().size()).ShouldBe(std::size_t(1)));
    SHOULD(ShouldlyIntegral(std::size_t(g.column_definitions()[0].width.unitType))
               .ShouldBe(std::size_t(GridUnitType::Auto)));
}

// --- GridLength value type --------------------------------------------------

TEST_CASE("S131: GridLength factories + equality + unit round-trip")
{
    const GridLength a = GridLength::Auto();
    const GridLength p = GridLength::Pixel(42.5f);
    const GridLength s = GridLength::Star(3.0f);
    SHOULD(ShouldlyIntegral(std::size_t(a.unitType)).ShouldBe(std::size_t(GridUnitType::Auto)));
    SHOULD(ShouldlyIntegral(std::size_t(p.unitType)).ShouldBe(std::size_t(GridUnitType::Pixel)));
    SHOULD(ShouldlyString(p.value).ShouldBe(42.5f));
    SHOULD(ShouldlyIntegral(std::size_t(s.unitType)).ShouldBe(std::size_t(GridUnitType::Star)));
    SHOULD(ShouldlyString(s.value).ShouldBe(3.0f));
    // Value equality: same unit + value are equal; differing unit or value are not.
    SHOULD(ShouldlyIntegral(GridLength::Pixel(2.0f) == GridLength::Pixel(2.0f)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(GridLength::Pixel(2.0f) == GridLength::Pixel(3.0f)).ShouldBe(false));
    SHOULD(ShouldlyIntegral(GridLength::Pixel(2.0f) == GridLength::Star(2.0f)).ShouldBe(false));
    SHOULD(ShouldlyIntegral(GridLength::Auto() == GridLength::Auto()).ShouldBe(true));
}

// --- resolve_grid_lengths (the pure, headline resolution primitive) ---------

TEST_CASE("S131: resolve — all Absolute tracks return their values")
{
    const std::vector<GridLength> defs = {
        GridLength::Pixel(100.0f), GridLength::Pixel(50.0f), GridLength::Pixel(25.0f),
    };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, {}, 1000.0f);
    SHOULD(ShouldlyString(resolved.size()).ShouldBe(std::size_t(3)));
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(50.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(25.0f));
}

TEST_CASE("S131: resolve — Auto tracks use the auto-measured extents")
{
    const std::vector<GridLength> defs = { GridLength::Auto(), GridLength::Auto() };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, { 30.0f, 70.0f }, 500.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(30.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(70.0f));
}

TEST_CASE("S131: resolve — Star tracks split the remaining space proportionally by weight")
{
    // Absolute 100 + Auto 50 taken; remaining 1000 - 150 = 850 split 3:1 -> 637.5 / 212.5.
    const std::vector<GridLength> defs = {
        GridLength::Pixel(100.0f), GridLength::Auto(), GridLength::Star(3.0f), GridLength::Star(1.0f),
    };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, { 0.0f, 50.0f }, 1000.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(50.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(850.0f * 3.0f / 4.0f));
    SHOULD(ShouldlyString(resolved[3]).ShouldBe(850.0f * 1.0f / 4.0f));
}

TEST_CASE("S131: resolve — mixed Absolute+Auto+Star (2:1) across all three units")
{
    const std::vector<GridLength> defs = {
        GridLength::Pixel(200.0f), // absolute
        GridLength::Auto(),        // auto (measured 100)
        GridLength::Star(2.0f),    // star weight 2
        GridLength::Star(1.0f),    // star weight 1
    };
    // available 700; fixed = 200 + 100 = 300; remaining = 400 -> star 2 = 266.67, star 1 = 133.33.
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, { 0.0f, 100.0f }, 700.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(200.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(400.0f * 2.0f / 3.0f));
    SHOULD(ShouldlyString(resolved[3]).ShouldBe(400.0f * 1.0f / 3.0f));
}

TEST_CASE("S131: resolve — star clamps when Absolute+Auto exceed available (no negative remaining)")
{
    // fixed 400 (300 abs + 100 auto) > available 250 -> remaining clamps to 0;
    // star tracks resolve 0, never negative.
    const std::vector<GridLength> defs = {
        GridLength::Pixel(300.0f), GridLength::Auto(), GridLength::Star(1.0f),
    };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, { 0.0f, 100.0f }, 250.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(300.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(0.0f));
}

TEST_CASE("S131: resolve — a zero-weight star resolves 0; a positive weight gets the full remaining share")
{
    // Star(0) has weight 0 -> gets 0; Star(2) has weight 2 -> gets the ENTIRE
    // remaining (totalStar = 0+2 = 2, so 2/2 * remaining = remaining). This is
    // the WPF-faithful weight semantics: a track with zero weight takes no
    // proportional share, but it does NOT zero out its siblings.
    const std::vector<GridLength> defs = {
        GridLength::Pixel(100.0f), GridLength::Star(0.0f), GridLength::Star(2.0f),
    };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, {}, 500.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(0.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(400.0f));
}

TEST_CASE("S131: resolve — totalStar == 0 (all star weights zero) leaves every star track at zero")
{
    // ALL star weights are 0 -> totalStar == 0, so no proportional share exists
    // and every star track resolves 0 (never a divide-by-zero / NaN).
    const std::vector<GridLength> defs = {
        GridLength::Pixel(100.0f), GridLength::Star(0.0f), GridLength::Star(0.0f),
    };
    const std::vector<float> resolved = vaml::resolve_grid_lengths(defs, {}, 500.0f);
    SHOULD(ShouldlyString(resolved[0]).ShouldBe(100.0f));
    SHOULD(ShouldlyString(resolved[1]).ShouldBe(0.0f));
    SHOULD(ShouldlyString(resolved[2]).ShouldBe(0.0f));
}

// --- Attached Grid.Row / Grid.Column DPs ------------------------------------

TEST_CASE("S131: Grid.Row / Grid.Column default to 0 on a plain child")
{
    DependencyObject child;
    SHOULD(ShouldlyString(Grid::row(child)).ShouldBe(0));
    SHOULD(ShouldlyString(Grid::column(child)).ShouldBe(0));
}

TEST_CASE("S131: attached Grid.Row / Grid.Column set + read on a plain DependencyObject (S043)")
{
    DependencyObject child;
    (void)Grid::set_row(child, 2);
    (void)Grid::set_column(child, 3);
    SHOULD(ShouldlyString(Grid::row(child)).ShouldBe(2));
    SHOULD(ShouldlyString(Grid::column(child)).ShouldBe(3));
    // Read through the raw attached-aware Set/Get path (global name resolution).
    SHOULD(ShouldlyString(child.Get(vaml::PropertyId::of("Grid.Row")).get<std::int32_t>()).ShouldBe(2));
    SHOULD(ShouldlyString(child.Get(vaml::PropertyId::of("Grid.Column")).get<std::int32_t>()).ShouldBe(3));
}

// --- Basic cell measure/arrange (no-span) ------------------------------------

TEST_CASE("S131: fixed 2x2 grid — children land at their cell origins with cell sizes")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(200.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(40.0f) },
                            RowDefinition{ GridLength::Pixel(60.0f) } });
    CountingLeaf* a = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f }); // (0,0)
    CountingLeaf* b = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f }); // (0,1)
    CountingLeaf* c = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f }); // (1,0)
    CountingLeaf* d = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f }); // (1,1)
    (void)Grid::set_row(*b, 0);
    (void)Grid::set_column(*b, 1);
    (void)Grid::set_row(*c, 1);
    (void)Grid::set_column(*c, 0);
    (void)Grid::set_row(*d, 1);
    (void)Grid::set_column(*d, 1);

    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // a at (0,0): origin (0,0), cell 100x40.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    // b at (0,1): origin x=100, cell 200x40.
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(0.0f));
    // c at (1,0): origin y=40, cell 100x60.
    SHOULD(ShouldlyString(c->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(40.0f));
    // d at (1,1): origin (100,40), cell 200x60.
    SHOULD(ShouldlyString(d->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(d->final_rect().y).ShouldBe(40.0f));
}

TEST_CASE("S131: Auto column sizes to the widest content child in that column")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Auto() } });
    // Column 0 children: widths 30 and 80 -> Auto column = 80 (max).
    // Column 1 child: width 50 -> Auto column = 50.
    CountingLeaf* a0 = g.create_child<CountingLeaf>(Size{ 30.0f, 10.0f });
    CountingLeaf* b0 = g.create_child<CountingLeaf>(Size{ 80.0f, 10.0f });
    CountingLeaf* a1 = g.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    (void)Grid::set_column(*a0, 0);
    (void)Grid::set_column(*b0, 0);
    (void)Grid::set_column(*a1, 1);
    (void)a0->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b0->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a1->set_horizontal_alignment(HorizontalAlignment::Left);

    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    SHOULD(ShouldlyString(g.desired_size().width).ShouldBe(130.0f)); // 80 + 50
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // Column 0 = 80 wide; a1 at column 1 starts at x=80.
    SHOULD(ShouldlyString(b0->final_rect().width).ShouldBe(80.0f));
    SHOULD(ShouldlyString(a1->final_rect().x).ShouldBe(80.0f));
}

TEST_CASE("S131: Star column splits remaining width proportionally; a Stretch child fills its star cell")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Star(2.0f) },
                               ColumnDefinition{ GridLength::Star(1.0f) } });
    CountingLeaf* a = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f });
    CountingLeaf* b = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    g.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // 300 width split 2:1 -> column 0 = 200, column 1 = 100.
    // Default-Stretch children fill their cells.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(200.0f));
    SHOULD(ShouldlyString(b->final_rect().width).ShouldBe(100.0f));
}

TEST_CASE("S131: panel DesiredSize derives from Absolute + Auto content (star contributes 0 minimum)")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Auto() },
                               ColumnDefinition{ GridLength::Star(2.0f) } });
    g.set_row_definitions({ RowDefinition{ GridLength::Pixel(40.0f) },
                            RowDefinition{ GridLength::Auto() } });
    CountingLeaf* autoLeaf = g.create_child<CountingLeaf>(Size{ 60.0f, 25.0f });
    (void)Grid::set_column(*autoLeaf, 1); // the Auto column (2nd)
    (void)Grid::set_row(*autoLeaf, 1);
    (void)autoLeaf->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)autoLeaf->set_vertical_alignment(VerticalAlignment::Top);

    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    // width = 100 (px) + 60 (auto) = 160 (star contributes 0 minimum).
    // height = 40 (px) + 25 (auto) = 65.
    SHOULD(ShouldlyString(g.desired_size().width).ShouldBe(160.0f));
    SHOULD(ShouldlyString(g.desired_size().height).ShouldBe(65.0f));
}

// --- Dirty-respect (S128 discipline inside the real Grid) -------------------

TEST_CASE("S131: dirty-respect — a clean child is not re-measured nor re-arranged")
{
    Grid g;
    g.set_column_definitions({ ColumnDefinition{ GridLength::Pixel(100.0f) },
                               ColumnDefinition{ GridLength::Pixel(100.0f) } });
    CountingLeaf* a = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f });
    CountingLeaf* b = g.create_child<CountingLeaf>(Size{ 10.0f, 10.0f });
    (void)Grid::set_column(*b, 1);

    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });
    // Both leaves measured (pass 1 unbounded + pass 2 cell) and arranged once.
    SHOULD(ShouldlyIntegral(a->measure_count()).ShouldBeGreaterThanOrEqualTo(1));
    SHOULD(ShouldlyIntegral(b->arrange_count()).ShouldBe(1));

    // Invalidate ONLY a's measure. Re-run both passes.
    a->InvalidateMeasure();
    g.Measure(Size{ Size::Infinite().width, Size::Infinite().height });
    g.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 100.0f });

    // a re-measured; clean b NOT re-measured (its before-count unchanged).
    const int bBefore = b->measure_count();
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(bBefore));
    // a's invalidation did not dirty b's arrange axis; b stays at 1 arrange.
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}
