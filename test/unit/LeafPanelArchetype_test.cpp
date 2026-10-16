// S125 (dep:S124) — Leaf elements (Text) measure to content; panels measure children.
//
// ARCHITECTURE: S125 is delivered as a TEST-ONLY archetype gate (accepted at plan time).
// It proves the two layout archetypes on the S123/S124 seam — the S127-upgraded
// protected MeasureOverride() (ex-S123 measure_natural_size, renamed at S127) +
// Arrange/final_rect + clip_to_bounds (S124) — via test-local UIElement subclasses. It
// does NOT re-base / extend the seeded `Text` (RESOLVED BY S400) or `StackPanel`
// (RESOLVED BY S129) controls: repo-wide convention — MINIMAL REAL control seeds are
// re-based only at their own resolution sprint, and S125 declares no resolution. Zero
// new product symbols.
//
// ARCHETYPE 1 — "Leaf elements (Text) measure to content":
//   A leaf (TextLikeLeaf) carries a real content extent and returns it from
//   MeasureOverride(). The S123 Measure contract then finalizes it (stretch-vs-wrap,
//   infinity-aware) into desired_size(); the S124 Arrange contract aligns it inside a slot
//   into final_rect(). Proves leaves measure to their content.
//
// ARCHETYPE 2 — "Panels measure children":
//   A panel (ChildMeasuringPanel, single-column stack) — an *archetype rule only*; the
//   REAL StackPanel algorithm + last-child-stretch belongs to S129 (recorded NO-GAP, not
//   silent). It overrides MeasureOverride() to call child->Measure(...) on each child
//   and derive its own DesiredSize from the children's DesiredSizes (width = max, height =
//   sum), then ArrangeOverride() places each child into its column slot via child->Arrange.
//   Proves panels measure children + arrange each into its slot automatically through
//   Arrange (the S127 arrange-subgraph auto-drive).
//
// Handoffs (NO-GAP): S126 (LayoutManager) drives Measure+Arrange over the dirty subgraph;
// S127 (RESOLVED) upgraded the seam to MeasureOverride/ArrangeOverride and completed the
// arrange auto-drive; S129/S400 re-base the real StackPanel/Text controls onto this
// proven archetype. Nothing is left as a silent backfill.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"

using vaml::HorizontalAlignment;
using vaml::Rect;
using vaml::Size;
using vaml::Thickness;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// Constructs a rect from origin + extent for readable slot literals.
Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// ARCHETYPE 1 — a leaf whose content is a real extent ("Text measures to content").
// MeasureOverride() returns the content extent; the S123 Measure finalizes it
// (stretch-vs-wrap, infinity-aware) and S124 Arrange aligns it inside a slot.
class TextLikeLeaf : public UIElement
{
public:
    explicit TextLikeLeaf(Size content) : content_(content) {}

protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        return content_;
    }

private:
    Size content_;
};

// ARCHETYPE 2 — a single-column stack *archetype rule only* (real StackPanel algorithm +
// last-child-stretch -> S129, recorded NO-GAP). MeasureOverride() measures each child
// against the panel's available width + unbounded height and derives the panel's own
// DesiredSize from the children's DesiredSizes (width = max, height = sum). arrange_children()
// then places each child into its own column slot (full row width, child-desired height) via
// child->Arrange, so each child's own alignment is honored within its slot. (S127's
// ArrangeOverride auto-drive is proven in MeasureArrangeOverride_test.cpp; this archetype's
// manual arrange is retained unmodified as the zero-semantic-change rename target.)
class ChildMeasuringPanel : public UIElement
{
public:
    // Places each child into its column slot, advancing y by the child's measured desired
    // height. Each child is arranged into a slot of the full arrange width + its own desired
    // height, so its HorizontalAlignment pins it within the row (Left/Center/Right/Stretch).
    void arrange_children(const Rect& arrangeSlot)
    {
        float y = arrangeSlot.y;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            child->Arrange(rect_of(arrangeSlot.x, y, arrangeSlot.width, child->desired_size().height));
            y += child->desired_size().height;
        }
    }

    // Derives an arranged slot of the given width for the panel, sized to the sum of the
    // children's desired heights (the panel's content stack), at the given origin.
    Rect stack_slot(float x, float y, float width) const
    {
        float totalHeight = 0.0f;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            totalHeight += child_at(i)->desired_size().height;
        }
        return rect_of(x, y, width, totalHeight);
    }

protected:
    // The panel's own content is its children: measure each child against a column offer
    // (available width, unbounded height — a vertical stack lets each child wrap to content
    // vertically), then derive width = max of children, height = sum of children.
    Size MeasureOverride(const Size& available) const override
    {
        float width = 0.0f;
        float height = 0.0f;
        const Size columnAvail{ available.width, Size::Infinite().height };
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            child->Measure(columnAvail);
            width = (child->desired_size().width > width) ? child->desired_size().width : width;
            height += child->desired_size().height;
        }
        return { width, height };
    }
};

} // namespace

// --- ARCHETYPE 1: leaf measures to content -------------------------------------------

TEST_CASE("S125: leaf — non-Stretch DesiredSize equals the content extent (measures to content)")
{
    // A non-Stretch (Left/Top) leaf with 100x30 content, offered a larger finite bound:
    // the measured DesiredSize WRAPS to the content extent (100x30), not the available.
    TextLikeLeaf leaf{ Size{ 100.0f, 30.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    leaf.Measure(Size{ 500.0f, 400.0f });
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(30.0f));
}

TEST_CASE("S125: leaf — infinite-available Stretch wraps to content (cannot stretch to infinity)")
{
    // A Stretch leaf with unbounded (Size::Infinite) available cannot stretch into infinity,
    // so its DesiredSize WRAPS to the content extent (the S123 infinity-aware rule).
    TextLikeLeaf leaf{ Size{ 80.0f, 20.0f } };
    // default Stretch on both axes
    leaf.Measure(Size::Infinite());
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(80.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S125: leaf — finite-available Stretch fills the available bound")
{
    // A Stretch leaf with a finite offered bound fills it (DesiredSize == available), NOT its
    // smaller content — stretch measures to the slot.
    TextLikeLeaf leaf{ Size{ 100.0f, 30.0f } };
    leaf.Measure(Size{ 400.0f, 200.0f });
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(400.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(200.0f));
}

TEST_CASE("S125: leaf — Arrange pins the measured content inside the slot per alignment")
{
    // A leaf measured to content then Center-aligned into a larger slot: final_rect() is
    // content-sized and centered — x = (400-100)/2 = 150, y = (200-30)/2 = 85. The leaf
    // occupies exactly its content, placed inside the slot (leaf measures to content; the
    // slot places content).
    TextLikeLeaf leaf{ Size{ 100.0f, 30.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Center);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Center);
    leaf.Measure(Size{ 400.0f, 200.0f });
    leaf.Arrange(rect_of(0.0f, 0.0f, 400.0f, 200.0f));
    SHOULD(ShouldlyString(leaf.final_rect().x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(leaf.final_rect().y).ShouldBe(85.0f));
    SHOULD(ShouldlyString(leaf.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.final_rect().height).ShouldBe(30.0f));
}

TEST_CASE("S125: leaf — Margin + alignment interplay (origin advances; content still not overflows)")
{
    // A Left/Top leaf with a 10x5 margin arranged into a slot: the origin advances by the
    // margin and the content-sized final_rect lands inset, within the slot — content never
    // escapes the slot.
    TextLikeLeaf leaf{ Size{ 50.0f, 20.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    (void)leaf.set_margin(Thickness{ 10.0f, 5.0f, 0.0f, 0.0f });
    leaf.Measure(Size{ 200.0f, 100.0f });
    leaf.Arrange(rect_of(0.0f, 0.0f, 200.0f, 100.0f));
    SHOULD(ShouldlyString(leaf.final_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.final_rect().y).ShouldBe(5.0f));
    SHOULD(ShouldlyString(leaf.final_rect().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(leaf.final_rect().height).ShouldBe(20.0f));
    // content stays within the slot bounds (never overflows)
    SHOULD(ShouldlyString(leaf.final_rect().right() <= 200.0f).ShouldBe(true));
    SHOULD(ShouldlyString(leaf.final_rect().bottom() <= 100.0f).ShouldBe(true));
}

TEST_CASE("S125: leaf — content clamped when the offered bound is smaller (never overflows)")
{
    // A non-Stretch leaf whose content (100x50) exceeds a smaller offered bound (60x40):
    // the desired WRAPS to min(content, available) = 60x40 — content is clamped to the slot,
    // never overflowing (the S123 non-Stretch finite rule).
    TextLikeLeaf leaf{ Size{ 100.0f, 50.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    leaf.Measure(Size{ 60.0f, 40.0f });
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(60.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(40.0f));
    // Arrange into a slot exactly that size -> never negative geometry escapes.
    leaf.Arrange(rect_of(0.0f, 0.0f, 60.0f, 40.0f));
    SHOULD(ShouldlyString(leaf.final_rect().width >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(leaf.final_rect().height >= 0.0f).ShouldBe(true));
}

// --- ARCHETYPE 2: panels measure children + arrange each into its slot ------------------

TEST_CASE("S125: panel — DesiredSize derives from children (width=max, height=sum)")
{
    // Two content leaves (100x30 and 50x20) in a non-Stretch panel: the panel's DesiredSize
    // is derived from the children — width = max(100,50) = 100, height = 30+20 = 50. The
    // children are non-Stretch (Left) so they wrap to their content within the column offer.
    ChildMeasuringPanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    TextLikeLeaf* b = panel.create_child<TextLikeLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));
}

TEST_CASE("S125: panel — each child's DesiredSize reflects its own measure against the column offer")
{
    // After the panel measures, each child was measured against a column offer (full width,
    // unbounded height) and its DesiredSize reflects its OWN content (non-Stretch -> wraps).
    ChildMeasuringPanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    TextLikeLeaf* b = panel.create_child<TextLikeLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(a->desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->desired_size().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(b->desired_size().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(b->desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S125: panel — arrange_children places each child into its column slot in order")
{
    // Two Left/Top leaves; arrange the panel into a 1000-wide slot and stack them: child A at
    // row y=0, child B at row y=30 (A's desired height). Each final_rect is content-sized and
    // left-placed (Left alignment within its full-width row).
    ChildMeasuringPanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    TextLikeLeaf* b = panel.create_child<TextLikeLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    const Rect slot = panel.stack_slot(5.0f, 10.0f, 1000.0f);
    panel.arrange_children(slot);
    // A: row 0 within the stack
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(10.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
    // B: row 1, stacked below A
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(5.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(40.0f));
    SHOULD(ShouldlyString(b->final_rect().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(b->final_rect().height).ShouldBe(20.0f));
}

TEST_CASE("S125: panel — per-child HorizontalAlignment honored within its own slot")
{
    // Each child occupies the full row width; the child's OWN HorizontalAlignment resolves
    // its x within that row. A Left leaf sits at the row's left; a Center leaf of the same
    // width-in-slot is centered. Both are in the same panel row (same y via stack).
    ChildMeasuringPanel panel;
    TextLikeLeaf* left = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    TextLikeLeaf* center = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    (void)left->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)center->set_horizontal_alignment(HorizontalAlignment::Center);
    // Top so each occupies its content height in its row; row width 1000.
    (void)left->set_vertical_alignment(VerticalAlignment::Top);
    (void)center->set_vertical_alignment(VerticalAlignment::Top);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    const Rect slot = panel.stack_slot(0.0f, 0.0f, 1000.0f);
    panel.arrange_children(slot);
    // Left leaf: pinned to the row's left edge (x = 0).
    SHOULD(ShouldlyString(left->final_rect().x).ShouldBe(0.0f));
    // Center leaf: centered in the 1000-wide row -> x = (1000-100)/2 = 450.
    SHOULD(ShouldlyString(center->final_rect().x).ShouldBe(450.0f));
}

TEST_CASE("S125: panel — empty panel has zero DesiredSize")
{
    // A panel with no children measures to zero on both axes (nothing to derive from).
    ChildMeasuringPanel panel;
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(0.0f));
}

TEST_CASE("S125: panel — per-Measure recompute reflects a changed child set (never cumulates)")
{
    // A panel is measured, then gains a second child and is re-measured: the DesiredSize
    // RECOMPUTES from the current children (recompute, never accumulate the prior result).
    ChildMeasuringPanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
    // Add a second child and re-measure: height recomputes to 30 + 20 = 50.
    TextLikeLeaf* b = panel.create_child<TextLikeLeaf>(Size{ 50.0f, 20.0f });
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    panel.Measure(Size{ 1000.0f, Size::Infinite().height });
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(50.0f));
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(100.0f));
}
