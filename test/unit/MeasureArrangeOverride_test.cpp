// S127 (dep:S126) — MeasureOverride/ArrangeOverride virtuals for panels.
//
// S127 upgrades the S123 measure seam (measure_natural_size -> MeasureOverride,
// a NAMED upgrade resolved at its own step, not silent churn) and adds the
// arrange-side virtual (ArrangeOverride) that completes the arrange-subgraph
// AUTO-DRIVE: LayoutManager -> root Arrange -> panel ArrangeOverride ->
// children arranged. Arrange() now computes final_rect() (the S124 contract,
// unchanged) and then invokes ArrangeOverride(final_rect()) so a panel lays
// out its children into its OWN render slot automatically — no manual
// arrange-call needed.
//
// Under test (Shouldly-for-doctest vocabulary, S036_5 standing convention;
// doctest's main comes from smoke_test.cpp):
//   1. Base MeasureOverride returns {0,0} (renamed seam, still no content);
//      base ArrangeOverride is a no-op (arranging a base element does NOT
//      auto-arrange its children).
//   2. HEADLINE — arrange auto-drive: a panel overriding MeasureOverride +
//      ArrangeOverride, with children, lm.UpdateLayout() ALONE (no manual
//      arrange call) produces children arranged into the panel's render slot
//      with correct stacked final_rects.
//   3. ArrangeOverride receives the panel's final_rect (after margin/alignment):
//      a panel with a margin offsets its children's placement by the inset origin.
//   4. MeasureOverride is invoked by Measure; ArrangeOverride is invoked by
//      Arrange (counting overrides prove both rename/auto-drive are live).
//   5. Measure still finalizes first (stretch-vs-wrap intact): a panel whose
//      MeasureOverride returns natural, Stretch finite fills available into
//      desired_size.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutManager.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"

#include <cstddef>

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

// A leaf whose content is a real extent, mirroring Text/panel overrides
// (S129/400): MeasureOverride reports the natural extent; the S123 Measure
// finalizes it (stretch-vs-wrap, infinity-aware) and S124 Arrange aligns it
// inside a slot.
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

// A single-column stack panel PROVING the S127 arrange auto-drive. It overrides
// MeasureOverride (measure each child against a column offer, derive width =
// max + height = sum) AND ArrangeOverride (stack children into finalSlot via
// child->Arrange). Since Arrange() now invokes ArrangeOverride(final_rect()),
// the panel's children are arranged AUTOMATICALLY — the real StackPanel (S129)
// adopts this exact pattern. measureCount_/arrangeCount_ let tests verify the
// virtuals are actually invoked (the rename/auto-drive is live, not dead).
class AutoDrivePanel : public UIElement
{
public:
    int measureCount() const { return measureCount_; }
    int arrangeCount() const { return arrangeCount_; }

protected:
    // The panel's own content is its children: measure each child against a
    // column offer (available width, unbounded height — a vertical stack lets
    // each child wrap to content vertically), then derive width = max of
    // children, height = sum of children.
    Size MeasureOverride(const Size& available) const override
    {
        ++measureCount_;
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

    // S127 arrange auto-drive: arrange each child into its column slot within
    // the panel's OWN final render slot (full row width, child-desired height),
    // advancing y by each child's desired height. Invoked automatically by
    // Arrange(final_rect()) — no manual arrange call.
    void ArrangeOverride(const Rect& finalSlot) override
    {
        ++arrangeCount_;
        float y = finalSlot.y;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            child->Arrange(rect_of(finalSlot.x, y, finalSlot.width, child->desired_size().height));
            y += child->desired_size().height;
        }
    }

private:
    mutable int measureCount_ = 0;
    int arrangeCount_ = 0;
};

} // namespace

// --- 1. Base MeasureOverride {0,0} + base ArrangeOverride no-op -----------------

TEST_CASE("S127: base MeasureOverride returns {0,0} and base ArrangeOverride is a no-op")
{
    // Base UIElement has no content -> MeasureOverride returns {0,0} (renamed
    // seam, still no content), so measuring with an infinite available wraps to
    // desired 0x0.
    UIElement root;
    UIElement* child = root.create_child<UIElement>();
    root.Measure(Size::Infinite());
    SHOULD(ShouldlyString(root.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(root.desired_size().height).ShouldBe(0.0f));

    // Base ArrangeOverride is a NO-OP: arranging the parent computes its own
    // final_rect but does NOT auto-arrange its children (the child's final_rect
    // stays 0 — a base element has no children-placing logic).
    root.Arrange(rect_of(0.0f, 0.0f, 100.0f, 100.0f));
    SHOULD(ShouldlyString(root.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(child->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(child->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(child->final_rect().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(child->final_rect().height).ShouldBe(0.0f));
}

// --- 2. HEADLINE: arrange auto-drive via UpdateLayout alone ---------------------

TEST_CASE("S127: arrange auto-drive — UpdateLayout alone stacks children into the panel's render slot")
{
    // A panel overriding MeasureOverride + ArrangeOverride, with children: the
    // LayoutManager's UpdateLayout() ALONE (no manual arrange call) measures
    // + arranges the root, which auto-invokes ArrangeOverride -> children are
    // arranged into the panel's render slot with correct stacked final_rects.
    AutoDrivePanel rootPanel;
    TextLikeLeaf* a = rootPanel.create_child<TextLikeLeaf>(Size{ 100.0f, 30.0f });
    TextLikeLeaf* b = rootPanel.create_child<TextLikeLeaf>(Size{ 50.0f, 20.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)b->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)b->set_vertical_alignment(VerticalAlignment::Top);
    (void)rootPanel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)rootPanel.set_vertical_alignment(VerticalAlignment::Top);

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout();

    // Panel measured: width = max(100,50) = 100, height = 30 + 20 = 50.
    SHOULD(ShouldlyString(rootPanel.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(rootPanel.desired_size().height).ShouldBe(50.0f));

    // Root arranged into its measured desired rect at origin 0; ArrangeOverride
    // stacked the children automatically (NO manual arrange call):
    // A at row 0 -> final_rect (0,0,100,30); B at row 1 -> (0,30,50,20).
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(b->final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(b->final_rect().width).ShouldBe(50.0f));
    SHOULD(ShouldlyString(b->final_rect().height).ShouldBe(20.0f));

    // The auto-drive arrangement cleared the children's arrange bits (they were
    // arranged by the override, which calls child->Arrange -> clears each bit).
    SHOULD(ShouldlyString(a->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(b->is_arrange_dirty()).ShouldBe(false));
}

// --- 3. ArrangeOverride receives the panel's final_rect (after margin/alignment) -

TEST_CASE("S127: ArrangeOverride receives final_rect — a panel margin offsets children placement")
{
    // A panel with a 20/30 margin arranged into a larger slot: compute_final_rect
    // (S124) shrinks the slot + advances the origin by the margin, and
    // ArrangeOverride receives THAT final_rect — so children are placed at the
    // inset origin (the panel's own render slot), not the outer arrangeSlot.
    AutoDrivePanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 40.0f, 10.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_margin(Thickness{ 20.0f, 30.0f, 0.0f, 0.0f });

    panel.Measure(Size{ 1000.0f, 1000.0f });
    panel.Arrange(rect_of(0.0f, 0.0f, 400.0f, 300.0f));

    // final_rect origin advances by the margin (20,30); ArrangeOverride stacks
    // the child into that inset origin -> child at (20,30), content-sized.
    SHOULD(ShouldlyString(a->final_rect().x).ShouldBe(20.0f));
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(30.0f));
    SHOULD(ShouldlyString(a->final_rect().width).ShouldBe(40.0f));
    SHOULD(ShouldlyString(a->final_rect().height).ShouldBe(10.0f));
}

// --- 4. The virtuals are live (renamed seam + auto-drive proven by invocation) ---

TEST_CASE("S127: Measure invokes MeasureOverride and Arrange invokes ArrangeOverride")
{
    // Counting overrides prove the rename is live (Measure calls MeasureOverride,
    // not a dead seam) and the auto-drive is wired (Arrange calls ArrangeOverride).
    AutoDrivePanel panel;
    TextLikeLeaf* a = panel.create_child<TextLikeLeaf>(Size{ 30.0f, 6.0f });
    (void)a->set_horizontal_alignment(HorizontalAlignment::Left);
    (void)a->set_vertical_alignment(VerticalAlignment::Top);
    (void)panel.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)panel.set_vertical_alignment(VerticalAlignment::Top);

    SHOULD(ShouldlyString(panel.measureCount()).ShouldBe(0));
    SHOULD(ShouldlyString(panel.arrangeCount()).ShouldBe(0));

    panel.Measure(Size{ 1000.0f, 1000.0f });
    SHOULD(ShouldlyString(panel.measureCount()).ShouldBe(1)); // MeasureOverride invoked
    SHOULD(ShouldlyString(panel.arrangeCount()).ShouldBe(0)); // no arrange yet

    panel.Arrange(rect_of(0.0f, 0.0f, 400.0f, 300.0f));
    SHOULD(ShouldlyString(panel.arrangeCount()).ShouldBe(1)); // ArrangeOverride auto-invoked
    // The override's child->Arrange arranged the child into its slot.
    SHOULD(ShouldlyString(a->is_arrange_dirty()).ShouldBe(false));
}

// --- 5. Measure finalizes first (stretch-vs-wrap intact) ------------------------

TEST_CASE("S127: Measure finalizes first — Stretch finite fills available into desired_size")
{
    // An empty panel's MeasureOverride returns natural {0,0}; with default
    // Stretch + a finite available, Measure STILL finalizes first into
    // desired_size == available (the S123 stretch-vs-wrap rule is intact after
    // the rename). This is the same finalizer S127 preserved while renaming the
    // seam (S123's header contract: Measure calls the content seam, THEN
    // compute_measured_desired).
    AutoDrivePanel panel;
    panel.Measure(Size{ 200.0f, 100.0f });
    SHOULD(ShouldlyString(panel.desired_size().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(panel.desired_size().height).ShouldBe(100.0f));
}
