// S130 (dep:S129) — StackPanel DesiredSize + stretch correctness (exhaustive suite).
//
// S129 shipped the real StackPanel (MeasureOverride/ArrangeOverride + last-child-
// stretch + Orientation DP) with FOCUSED tests and recorded S130 as the named
// NO-GAP exhaustive gate. This TU is that gate — TEST-ONLY, zero new product
// symbols (the S040/S042/S044/S125/S128 test-only precedent). It exercises the
// real StackPanel against the S123 infinity-aware finalizer
// (compute_measured_desired), the S124 arrange math (compute_final_rect), and
// the S126/S127 dirty-driver.
//
// The exclusive instrument is a test-local RecorderLeaf (the S125 TextLikeLeaf +
// S128 DirtyLeaf pattern): it overrides MeasureOverride/ArrangeOverride to RECORD
// the exact available offer + final slot it receives and returns a configured
// natural content extent. Because StackPanel::MeasureOverride offers each child a
// real {available.width, ∞} (vertical) / {∞, available.height} (horizontal)
// bound, and Measure finalizes it against the child's own alignment, the
// Stretch-vs-Wrap behavior at BOTH measure and arrange becomes OBSERVABLE — the
// crux of "stretch correctness". Counters are kept on both axes so dirty-scoping
// stays visible (MeasureOverride is const, so the measure counter + offer are
// mutable; ArrangeOverride is non-const).
//
// NO-GAP accounting: no MINIMAL REAL introduced/resolved (S129 already resolved
// the StackPanel seed at its own step); S130 is the recorded exhaustive suite,
// not a backfill. Every Part A–D assertion reflects the ACTUAL shipped S129
// behavior (test-the-truth, per the accepted S130 blocker resolution) — see the
// inline notes where a WPF-faithful future change differs (D17).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"       // StackPanel, Orientation
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"

using vaml::HorizontalAlignment;
using vaml::Orientation;
using vaml::Rect;
using vaml::Size;
using vaml::StackPanel;
using vaml::Thickness;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// A test-local leaf with a real content extent that RECORDS the exact available
// offer (MeasureOverride) + final slot (ArrangeOverride) it receives. The S125
// TextLikeLeaf + S128 DirtyLeaf pattern: both axes observable per leaf, and the
// cross-axis/stack-axis offers the panel actually makes are provable.
class RecorderLeaf : public UIElement
{
public:
    explicit RecorderLeaf(Size content) : content_(content) {}

    int measure_count() const noexcept { return measureCalls; }
    int arrange_count() const noexcept { return arrangeCalls; }
    Size last_available() const noexcept { return lastAvailable; }
    Rect last_slot() const noexcept { return lastSlot; }

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
// PART A — Panel DesiredSize: the S123 infinity-aware stretch-vs-wrap rule,
// applied to the PANEL's own DesiredSize (4 orientations × finalizer).
// Vertical children natural = {maxW=100, Σh=50} (A={100,30}, B={50,20}).
// ============================================================

TEST_CASE("S130 A1: Stretch/Stretch panel + finite available -> desired fills BOTH axes")
{
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a;
    (void)b;
    // Panel is default Stretch/Stretch. A finite available -> MeasureOverride
    // yields the natural {100,50}, then compute_measured_desired (Stretch +
    // finite) fills BOTH axes to the offered bound.
    panel.Measure(Size{ 300.0f, 200.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(200.0f));
}

TEST_CASE("S130 A2: Stretch/Stretch panel + unbounded available -> desired WRAPS to natural")
{
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a;
    (void)b;
    // Panel default Stretch/Stretch, offered {∞,∞} (top-level via LayoutManager).
    // Cannot stretch into infinity -> WRAPS to content natural {100,50}; no
    // infinite escapes a DesiredSize (S123).
    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));
}

TEST_CASE("S130 A3: Stretch/Stretch panel + mixed axes -> finite width fills, stack axis wraps")
{
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a;
    (void)b;
    // {300,∞}: the panel fills the finite width 300 (Stretch) but WRAPS its
    // stack axis (infinity) to Σ child heights = 50.
    panel.Measure(Size{ 300.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));
}

TEST_CASE("S130 A4: non-Stretch panel + finite available -> desired = min(natural, available)")
{
    // A Left/Top panel (non-Stretch) cannot fill a finite bound; it WRAPS to
    // min(natural, available) and never exceeds nor goes negative. Children are
    // also Left (non-Stretch) so the panel's measured natural stays {100,50}
    // (a Stretch child would itself fill the offered width and inflate natural).
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    // natural (100,50) <= available (300,200) -> desired == natural.
    panel.Measure(Size{ 300.0f, 200.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));

    // available smaller than natural -> desired clamps to available, never
    // negative (children Left wrap to the narrower offer, Σh clamps to 30).
    panel.Measure(Size{ 40.0f, 30.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f));
}

TEST_CASE("S130 A4h: horizontal mirror of the non-Stretch desired rule")
{
    // Horizontal non-Stretch (Left/Top) panel; children Top so natural stays
    // {Σw=150, maxH=30}. (A={100,30}, B={50,20}.)
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    // natural (150,30) <= available (300,200) -> desired == natural.
    panel.Measure(Size{ 300.0f, 200.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(150.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f));
}

// ============================================================
// PART B — Cross-axis stretch correctness of CHILDREN IN THE STACK: the real
// subtlety. The panel offers each child a real cross-axis bound; whether the
// child ends up stretching across it (or wrapping) is determined by the child's
// OWN alignment + the offered available — observable via the recorder.
// ============================================================

TEST_CASE("S130 B5: vertical stack — a Stretch child stretches cross-axis, wraps stack-axis")
{
    // Vertical panel measured with a FINITE width {300,∞}: each child is offered
    // {300,∞}. A default-Stretch child finalizes width->300 (fills the finite
    // cross width) but wraps height->content (infinite stack axis). The offer is
    // observable on the recorder.
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });

    panel.Measure(Size{ 300.0f, Size::Infinite().height });

    // Child a offered the finite cross width + infinite stack height.
    SHOULD(ShouldlyString(a->last_available().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(a->last_available().height)
               .ShouldBe(std::numeric_limits<float>::infinity()));
    // Stretch cross-axis -> fills 300; stack-axis ∞ -> wraps to content height.
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(a->desired_size().height).ShouldBe(30.0f));
    // b mirrors (content height 20).
    SHOULD(ShouldlyString(b->desired_size().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(b->desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S130 B6: vertical stack — a non-Stretch child does NOT blindly stretch cross-axis")
{
    // A Left child in the same finite-width vertical stack: it wraps its width
    // to min(content, offered) instead of filling the 300 cross width — no blind
    // cross-axis stretch; height still wraps to content.
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    // Panel non-Stretch too, so its natural DesiredSize (max child width / Σh)
    // is directly observable rather than the Stretch-fill default.
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ 300.0f, Size::Infinite().height });

    SHOULD(ShouldlyString(a->last_available().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(100.0f)); // content, not 300
    SHOULD(ShouldlyString(a->desired_size().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f)); // natural maxW
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f)); // Σh
}

TEST_CASE("S130 B7: unbounded panel — even a Stretch child WRAPS both axes to content")
{
    // Offered {∞,∞}: there is nothing to stretch into, so a default-Stretch
    // child wraps BOTH axes to its content — never fabricates a size against
    // infinity (the S123 anti-infinite guarantee at the child level).
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });

    panel.Measure(Size::Infinite());

    SHOULD(ShouldlyString(a->last_available().width)
               .ShouldBe(std::numeric_limits<float>::infinity()));
    SHOULD(ShouldlyString(a->last_available().height)
               .ShouldBe(std::numeric_limits<float>::infinity()));
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(100.0f)); // wraps, not infinite
    SHOULD(ShouldlyString(a->desired_size().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f));
}

TEST_CASE("S130 B8: horizontal mirror — cross axis is height")
{
    // Horizontal panel measured {∞,500}: cross axis = height. A default-Stretch
    // child fills the finite 500 height but wraps the infinite stack (width) to
    // content. A non-Stretch (Top) child keeps its content height.
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    RecorderLeaf* stretch = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* top = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)top->set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size{ Size::Infinite().width, 500.0f });

    // Stretch child: fills cross height 500, wraps stack width to content 100.
    SHOULD(ShouldlyString(stretch->desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(stretch->desired_size().height).ShouldBe(500.0f));
    // Top (non-Stretch) child: keeps content height 20 (no blind cross stretch).
    SHOULD(ShouldlyString(top->desired_size().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(top->desired_size().height).ShouldBe(20.0f));
}

// ============================================================
// PART C — Child arrange/stretch INSIDE the panel (final_rect correctness).
// ============================================================

TEST_CASE("S130 C9: per-child alignment within its full-width row at arrange")
{
    // Vertical panel fills a 300-wide arrange slot; each child's row IS the full
    // 300-wide track. A default-Stretch child fills the row width; Left/Center
    // children pin within it via their own alignment.
    StackPanel panel;
    RecorderLeaf* left = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* stretch = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* center = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    (void)left->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)center->set_horizontal_alignment(HorizontalAlignment::Center);
    (void)left->set_vertical_alignment(VerticalAlignment::Top);
    (void)stretch->set_vertical_alignment(VerticalAlignment::Top);
    (void)center->set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size::Infinite());
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // Left child pinned to the row's left edge, content width.
    SHOULD(ShouldlyString(left->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(left->final_rect().width).ShouldBe(100.0f));
    // Default-Stretch child fills the full 300 row width.
    SHOULD(ShouldlyString(stretch->final_rect().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(stretch->final_rect().x).ShouldBe(0.0f));
    // Center child centered: x = (300-100)/2 = 100.
    SHOULD(ShouldlyString(center->final_rect().x).ShouldBe(100.0f));
    SHOULD(ShouldlyString(center->final_rect().width).ShouldBe(100.0f));
}

TEST_CASE("S130 C10: last-child-stretch — Stretch fills remaining, non-Stretch keeps content")
{
    // Panel fills a 100x100 slot. First non-stretch keeps content height 30.
    // The LAST default-Stretch child fills the remaining 70 (last-child-stretch).
    {
        StackPanel panel;
        RecorderLeaf* first = panel.create_child<RecorderLeaf>(Size{ 50.0f, 30.0f });
        RecorderLeaf* last = panel.create_child<RecorderLeaf>(Size{ 50.0f, 10.0f });
        (void)first->set_vertical_alignment(VerticalAlignment::Top);
        (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
        (void)last->set_horizontal_alignment(HorizontalAlignment::Left);

        panel.Measure(Size::Infinite());
        panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

        SHOULD(ShouldlyString(first->final_rect().y).ShouldBe(0.0f));
        SHOULD(ShouldlyString(first->final_rect().height).ShouldBe(30.0f));
        SHOULD(ShouldlyString(last->final_rect().y).ShouldBe(30.0f));
        SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(70.0f)); // remaining
    }
    // The same layout with a non-Stretch LAST child: it keeps its content height
    // 10 instead of over-filling the remaining 70 (no blind stretch).
    {
        StackPanel panel;
        RecorderLeaf* first = panel.create_child<RecorderLeaf>(Size{ 50.0f, 30.0f });
        RecorderLeaf* last = panel.create_child<RecorderLeaf>(Size{ 50.0f, 10.0f });
        (void)first->set_vertical_alignment(VerticalAlignment::Top);
        (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
        (void)last->set_vertical_alignment(VerticalAlignment::Top);
        (void)last->set_horizontal_alignment(HorizontalAlignment::Left);

        panel.Measure(Size::Infinite());
        panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

        SHOULD(ShouldlyString(first->final_rect().height).ShouldBe(30.0f));
        SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(10.0f)); // NOT 70
    }
}

TEST_CASE("S130 C11: single child (first == last) fills the entire panel slot")
{
    // One default-Stretch child is BOTH first and last -> the degenerate
    // last-child-stretch: it fills the full panel 100x100 slot.
    StackPanel panel;
    RecorderLeaf* only = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });

    panel.Measure(Size::Infinite());
    panel.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    SHOULD(ShouldlyString(only->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(only->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(only->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(only->final_rect().height).ShouldBe(100.0f));
}

TEST_CASE("S130 C12: a non-Stretch panel produces NO phantom leftover space")
{
    // A non-Stretch (Left/Top) panel measured to natural then arranged into an
    // OVERSIZED slot collapses to its natural Σ-h; the last Stretch child's
    // "remaining" is exactly its natural content segment — no negative, no
    // over-fill when there is no leftover.
    StackPanel panel;
    RecorderLeaf* first = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* last = panel.create_child<RecorderLeaf>(Size{ 100.0f, 20.0f });
    (void)first->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)first->set_vertical_alignment(VerticalAlignment::Top);
    (void)last->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f)); // Σh

    // Oversized arrange slot 1000x1000; the non-Stretch panel collapses to its
    // natural (100,50), so the last child's remaining = 50 - 30 = 20 == content.
    panel.Arrange(Rect{ 0.0f, 0.0f, 1000.0f, 1000.0f });
    SHOULD(ShouldlyString(panel.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.final_rect().height).ShouldBe(50.0f));
    SHOULD(ShouldlyString(first->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(first->final_rect().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(last->final_rect().y).ShouldBe(30.0f));
    // last is Stretch but its remaining == its own content segment 20 (no phantom).
    SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(20.0f));
}

TEST_CASE("S130 C13: a Stretch child's cross-axis fill honors its Margin")
{
    // A default-Stretch-horizontal / Top child with Margin(10,10,10,10) fills
    // its row INSET by the margin: final width = 300-20 = 280 at x = 10, via
    // compute_final_rect (S124).
    StackPanel panel;
    RecorderLeaf* child = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)child->set_margin(Thickness{ 10.0f, 10.0f, 10.0f, 10.0f });
    (void)child->set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size::Infinite());
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    SHOULD(ShouldlyString(child->final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(child->final_rect().y).ShouldBe(10.0f));
    SHOULD(ShouldlyString(child->final_rect().width).ShouldBe(280.0f)); // 300 - 10 - 10
    SHOULD(ShouldlyString(child->final_rect().height).ShouldBe(20.0f)); // Top keeps content
}

TEST_CASE("S130 C14: horizontal last-child-stretch + per-child VerticalAlignment in the column")
{
    // Horizontal panel fills a 300x100 slot. The cross axis (height) honors each
    // child's VerticalAlignment within the full-height column (Top/Center/Bottom);
    // the LAST default-Stretch child fills the remaining width AND the full height.
    StackPanel panel;
    (void)panel.set_orientation(Orientation::Horizontal);
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 20.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 30.0f });
    RecorderLeaf* c = panel.create_child<RecorderLeaf>(Size{ 60.0f, 25.0f });
    RecorderLeaf* last = panel.create_child<RecorderLeaf>(Size{ 40.0f, 10.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_vertical_alignment(VerticalAlignment::Center);
    (void)c->set_vertical_alignment(VerticalAlignment::Bottom);

    panel.Measure(Size::Infinite());
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });

    // a: Top -> y=0, content height 20.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(20.0f));
    // b: Center -> y = (100-30)/2 = 35.
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(35.0f));
    // c: Bottom -> y = 100 - 25 = 75.
    SHOULD(ShouldlyString(c->final_rect().y).ShouldBe(75.0f));
    // last: Stretch fills remaining width 90 AND the full column height 100.
    SHOULD(ShouldlyString(last->final_rect().x).ShouldBe(210.0f)); // 100+50+60
    SHOULD(ShouldlyString(last->final_rect().width).ShouldBe(90.0f)); // 300-210
    SHOULD(ShouldlyString(last->final_rect().height).ShouldBe(100.0f));
}

// ============================================================
// PART D — DesiredSize edges + dirty scoping.
// ============================================================

TEST_CASE("S130 D15: empty panel DesiredSize collapses to 0 — never negative, never infinite")
{
    // Wherever an empty panel cannot stretch it collapses to {0,0}: under an
    // unbounded available (Stretch wraps to content), or under a finite
    // available with a non-Stretch alignment. (A default-Stretch empty panel
    // with FINITE available returns `available` — the Part A Stretch-fill rule;
    // that is the S123 contract, not an empty-panel special case.)
    {
        StackPanel panel; // default Stretch/Stretch
        panel.Measure(Size::Infinite());
        SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(0.0f));
        SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(0.0f));
    }
    {
        StackPanel panel;
        (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
        (void)panel.set_vertical_alignment(VerticalAlignment::Top);
        panel.Measure(Size{ 300.0f, 200.0f }); // finite + non-stretch -> 0
        SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(0.0f));
        SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(0.0f));
    }
    {
        StackPanel panel;
        (void)panel.set_orientation(Orientation::Horizontal);
        panel.Measure(Size::Infinite()); // horizontal empty + unbounded
        SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(0.0f));
        SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(0.0f));
    }
}

TEST_CASE("S130 D16: panel DesiredSize recomputes from the CURRENT children — never cumulates")
{
    // Removing a child + re-measuring reflects the NEW natural Σ, not a running
    // sum from the prior measure.
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a;
    (void)b;
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f)); // 30+20

    (void)panel.remove_child(1); // drop b
    panel.InvalidateMeasure();
    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f)); // only a, not 50
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
}

TEST_CASE("S130 D17: child Margin is an ARRANGE-time inset — it does NOT enter the panel's measure-time DesiredSize")
{
    // TEST-TRUTH (accepted S130 blocker resolution): the shipped S129
    // StackPanel::MeasureOverride derives its natural extent from each child's
    // raw desired_size(); it does NOT add child margins (a child's Margin only
    // insets at arrange time via compute_final_rect). So a margin'd child's
    // desired_size() is unchanged, and the panel's DesiredSize does NOT grow with
    // child margins. This diverges from WPF (which WOULD include child margins in
    // the panel DesiredSize); that fidelity gap is recorded NO-GAP for S131
    // (Grid) or a dedicated StackPanel margin step — never a silent omission.
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));

    // Give b a real margin; re-measure. b's desired_size() is UNCHANGED by its
    // margin, so the panel's DesiredSize is unchanged too (margins do not
    // propagate into the measure-time extent).
    (void)b->set_margin(Thickness{ 10.0f, 5.0f, 10.0f, 5.0f });
    panel.InvalidateMeasure();
    panel.Measure(Size::Infinite());
    SHOULD(ShouldlyString(b->desired_size().width).ShouldBe(50.0f)); // margin-invariant
    SHOULD(ShouldlyString(b->desired_size().height).ShouldBe(20.0f));
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f)); // NOT +20
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f)); // NOT +10
}

TEST_CASE("S130 D18: dirty scoping of stretch — re-measuring a dirty Stretch child recomputes; a clean sibling is untouched")
{
    // S128 dirty-respect realized on the real control, observed on the
    // cross-axis stretch axis: after a first layout, invalidating ONLY child a's
    // measure makes the panel re-measure a (offer changed, desired recomputed)
    // while clean sibling b's desired_size() is reused untouched. Then an
    // arrange-invalidation re-arranges only a.
    StackPanel panel;
    RecorderLeaf* a = panel.create_child<RecorderLeaf>(Size{ 100.0f, 30.0f });
    RecorderLeaf* b = panel.create_child<RecorderLeaf>(Size{ 50.0f, 20.0f });
    (void)a;
    (void)b;

    // First layout at width 300: both measured + arranged once.
    panel.Measure(Size{ 300.0f, Size::Infinite().height });
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(300.0f));
    SHOULD(ShouldlyString(b->desired_size().width).ShouldBe(300.0f));

    // Invalidate ONLY a's measure; re-measure the panel at a WIDER width.
    a->InvalidateMeasure();
    panel.Measure(Size{ 450.0f, Size::Infinite().height });

    // a re-measured (cross-axis stretch recomputed to the new 450 offer).
    SHOULD(ShouldlyString(a->measure_count()).ShouldBe(2));
    SHOULD(ShouldlyString(a->last_available().width).ShouldBe(450.0f));
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(450.0f));
    // b NOT re-measured — its desired_size is reused from the first pass.
    SHOULD(ShouldlyString(b->measure_count()).ShouldBe(1));
    SHOULD(ShouldlyString(b->desired_size().width).ShouldBe(300.0f));

    // Arrange axis: invalidate ONLY a's arrange; re-arrange.
    a->InvalidateArrange();
    panel.Arrange(Rect{ 0.0f, 0.0f, 300.0f, 100.0f });
    SHOULD(ShouldlyString(a->arrange_count()).ShouldBe(2));
    SHOULD(ShouldlyString(b->arrange_count()).ShouldBe(1));
}
