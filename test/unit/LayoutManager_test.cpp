// S126 (dep:S125) — InvalidateMeasure/InvalidateArrange; LayoutManager drives
// measure+arrange over the dirty subgraph.
//
// SCOPE / ARCHITECTURE — the S126 layout driver + dirty-flag contract, proven
// on the CURRENT S123/S124/S125 seam:
//   * UIElement carries TWO dirty bits (measureDirty, arrangeDirty), initally
//     TRUE (a fresh element is never laid out). InvalidateMeasure /
//     InvalidateArrange SET the local flag (short-circuit when already set, so
//     a redundant invalidation does NOT re-walk) and PROPAGATE it up the parent
//     chain to the root (a parent's DesiredSize derives from its children).
//   * vaml::LayoutManager is the NON-OWNING driver: UpdateLayout() runs a
//     Measure pass iff the root is measure-dirty, an Arrange pass iff the root
//     is arrange-dirty, so a clean UpdateLayout() is a TRUE NO-OP. The measure-
//     subgraph is driven through the virtual MeasureOverride() seam (S127,
//     upgraded from S123's measure_natural_size): a panel re-measures ONLY
//     dirty children, reading clean children's stored desired_size(). The root
//     is arranged into its own measured DesiredSize at origin 0 (a top-level
//     element has no external slot; a real window viewport arrives at the
//     window sprints, recorded NO-GAP).
//   * S127 (RESOLVED) upgraded the seam to MeasureOverride/ArrangeOverride; the
//     arrange-subgraph auto-driving is completed THERE (the S123/S124 seam
//     deliberately had NO virtual Arrange — recorded at S124, not silent). S126
//     PROVES the dirty-respect mechanism on BOTH axes so S127 plugs it in via
//     ArrangeOverride: the dirty-aware panel re-arranges only dirty children,
//     leaving clean children's final_rect untouched.
//
// Test-local subclasses (zero product symbols, mirrors the S125 archetype
// pattern). Each lies on the REAL UIElement + LayoutManager product surface.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutManager.h"
#include "vaml/core/UIElement.h"

using vaml::Rect;
using vaml::Size;
using vaml::UIElement;

namespace {

Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// A leaf with real content extent, used to COUNT measure invocations (the S127
// MeasureOverride seam is virtual, so a leaf can observe how many times it is
// measured) and to carry a per-instance desired size. Arrange dirty-respect is
// observed via final_rect() changes, not a per-instance arrange counter.
class CountingLeaf : public UIElement
{
public:
    explicit CountingLeaf(Size content) : content_(content) {}

    int measureCount() const { return measureCount_; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        ++measureCount_;
        (void)available;
        return content_;
    }

private:
    mutable int measureCount_ = 0;
    Size content_;
};

// A dirty-aware single-column stack (S125 archetype rule; real StackPanel ->
// S129). The KEY S126 contract: MeasureOverride() re-measures ONLY dirty
// children (a clean child's stored desired_size() is reused, so a clean child
// is NOT re-measured); a dirty-aware arrange re-arranges ONLY dirty children
// (a clean child's final_rect is left untouched). measureCount_ lets a test
// assert exactly one measure pass collapses many invalidations.
class DirtyAwarePanel : public UIElement
{
public:
    int measureCount() const { return measureCount_; }

    // Re-arranges ONLY dirty children into their column slot, advancing y by
    // each child's desired height. A clean child is left UNTOUCHED (its
    // final_rect from its prior arrange stays), proving arrange dirty-respect.
    // This is the S125-pattern arrange the manager's top-level arrange pass
    // delegates to (kept here as the S126 dirty-respect proof; S127's
    // ArrangeOverride auto-drive is proven in MeasureArrangeOverride_test.cpp,
    // and S129 integrates dirty-respect into the real arrange override).
    void arrange_dirty(const Rect& arrangeSlot)
    {
        float y = arrangeSlot.y;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_arrange_dirty())
            {
                child->Arrange(rect_of(arrangeSlot.x, y, arrangeSlot.width, child->desired_size().height));
            }
            y += child->desired_size().height;
        }
    }

protected:
    // The panel's own content is its children: re-measure ONLY dirty children
    // against a column offer (available width, unbounded height), reuse clean
    // children's stored desired_size(), derive width = max + height = sum.
    Size MeasureOverride(const Size& available) const override
    {
        ++measureCount_;
        float width = 0.0f;
        float height = 0.0f;
        const Size columnAvail{ available.width, Size::Infinite().height };
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_measure_dirty())
            {
                child->Measure(columnAvail); // clears the child's measure bit
            }
            width = (child->desired_size().width > width) ? child->desired_size().width : width;
            height += child->desired_size().height;
        }
        return { width, height };
    }

private:
    mutable int measureCount_ = 0;
};

} // namespace

// --- 1. Fresh root is dirty; first UpdateLayout measures+arranges, both clean -------

TEST_CASE("S126: fresh root is dirty; first UpdateLayout measures+arranges and cleans both axes")
{
    // A single-node tree (the root IS the whole tree): the manager's measure +
    // arrange passes fully drive it. A fresh element is dirty on BOTH axes.
    CountingLeaf root{ Size{ 80.0f, 20.0f } };
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(root.is_layout_dirty()).ShouldBe(true));

    vaml::LayoutManager lm(&root);
    SHOULD(ShouldlyString(lm.root() == &root).ShouldBe(true));

    lm.UpdateLayout();

    // Measured: the leaf wraps to its content under infinite available (S123).
    SHOULD(ShouldlyString(root.desired_size().width).ShouldBe(80.0f));
    SHOULD(ShouldlyString(root.desired_size().height).ShouldBe(20.0f));
    // Arranged into its own desired rect at origin 0.
    SHOULD(ShouldlyString(root.final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(root.final_rect().y).ShouldBe(0.0f));
    // Both dirt axes cleared: the freshly-dirty element is now fully clean.
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_layout_dirty()).ShouldBe(false));
}

// --- 2. No-op when clean: a clean UpdateLayout does not re-measure -----------------

TEST_CASE("S126: a clean UpdateLayout is a true no-op (leaf is not re-measured)")
{
    CountingLeaf root{ Size{ 60.0f, 10.0f } };
    vaml::LayoutManager lm(&root);

    lm.UpdateLayout();
    const int measuredOnce = root.measureCount();
    SHOULD(ShouldlyString(measuredOnce).ShouldBe(1));

    // Root is now clean; a second UpdateLayout is gated off the root's measure-
    // dirty bit (already cleared) -> the leaf is NOT re-measured.
    lm.UpdateLayout();
    SHOULD(ShouldlyString(root.measureCount()).ShouldBe(measuredOnce));
    SHOULD(ShouldlyString(root.measureCount()).ShouldBe(1));
}

// --- 3. Upward propagation: a leaf invalidation dirties every ancestor -------

TEST_CASE("S126: leaf InvalidateMeasure propagates up to the root; a sibling branch stays clean")
{
    // Tree: root panel -> [leaf A, subpanel P]; P -> leaf B. Clean it fully so
    // the subsequent invalidation is observable as a delta.
    DirtyAwarePanel rootPanel;
    CountingLeaf* spreadA = rootPanel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    DirtyAwarePanel* subP = rootPanel.create_child<DirtyAwarePanel>();
    CountingLeaf* deepB = subP->create_child<CountingLeaf>(Size{ 40.0f, 8.0f });

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout();
    rootPanel.arrange_dirty(rect_of(0.0f, 0.0f, 500.0f, 400.0f));
    // Full clean: every node on both axes is clean.
    SHOULD(ShouldlyString(rootPanel.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(rootPanel.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(spreadA->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(subP->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(deepB->is_measure_dirty()).ShouldBe(false));

    // Invalidate MEASURE on the deep leaf B only.
    deepB->InvalidateMeasure();

    // The target + EVERY ancestor to the root go measure-dirty.
    SHOULD(ShouldlyString(deepB->is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(subP->is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(rootPanel.is_measure_dirty()).ShouldBe(true));
    // The unrelated sibling branch (A) is untouched.
    SHOULD(ShouldlyString(spreadA->is_measure_dirty()).ShouldBe(false));
    // Arrange axis is NOT touched by a measure invalidation (the manager/panel
    // prep only clears the arrange bits the manager and arrange_dirty actually
    // reach — the ROOT's arrange bit stays clean; a measure invalidation does
    // not dirty it).
    SHOULD(ShouldlyString(rootPanel.is_arrange_dirty()).ShouldBe(false));
}

// --- 4. Coalescing: many invalidations before ONE UpdateLayout -> one pass ------

TEST_CASE("S126: coalescing — two leaf invalidations before one UpdateLayout collapse to one measure pass")
{
    DirtyAwarePanel rootPanel;
    CountingLeaf* a = rootPanel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    CountingLeaf* b = rootPanel.create_child<CountingLeaf>(Size{ 40.0f, 8.0f });

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout();
    rootPanel.arrange_dirty(rect_of(0.0f, 0.0f, 500.0f, 400.0f));

    // Baseline after the clean pass.
    const int panelBaseline = rootPanel.measureCount();
    const int aBaseline = a->measureCount();
    const int bBaseline = b->measureCount();

    // Two separate invalidations on different leaves, THEN one UpdateLayout.
    a->InvalidateMeasure();
    b->InvalidateMeasure();
    lm.UpdateLayout();

    // Exactly ONE panel measure pass (collapsed), each dirty leaf re-measured once.
    SHOULD(ShouldlyString(rootPanel.measureCount()).ShouldBe(panelBaseline + 1));
    SHOULD(ShouldlyString(a->measureCount()).ShouldBe(aBaseline + 1));
    SHOULD(ShouldlyString(b->measureCount()).ShouldBe(bBaseline + 1));
}

// --- 5. Short-circuit: invalidating an already-dirty element is a no-op ---------

TEST_CASE("S126: invalidating an already-dirty element does not re-walk / change the dirty set")
{
    DirtyAwarePanel rootPanel;
    CountingLeaf* leaf = rootPanel.create_child<CountingLeaf>(Size{ 30.0f, 6.0f });

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout();
    rootPanel.arrange_dirty(rect_of(0.0f, 0.0f, 300.0f, 300.0f));

    // Invalidate the leaf, then invalidate it AGAIN (already measure-dirty ->
    // short-circuit no-op; no state change, no extra walk).
    leaf->InvalidateMeasure();
    const bool dirtyAfterFirst = leaf->is_measure_dirty();
    const bool rootDirtyAfterFirst = rootPanel.is_measure_dirty();
    leaf->InvalidateMeasure();
    SHOULD(ShouldlyString(leaf->is_measure_dirty()).ShouldBe(dirtyAfterFirst));
    SHOULD(ShouldlyString(rootPanel.is_measure_dirty()).ShouldBe(rootDirtyAfterFirst));

    // One UpdateLayout still fully cleans the accumulated invalidation.
    const int panelBaseline = rootPanel.measureCount();
    lm.UpdateLayout();
    SHOULD(ShouldlyString(leaf->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(rootPanel.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(rootPanel.measureCount()).ShouldBe(panelBaseline + 1));
}

// --- 6. Dirty-aware panel: re-measure/re-arrange ONLY the dirty child -----------

TEST_CASE("S126: panel re-measures only the dirty child (a clean leaf is not re-measured)")
{
    DirtyAwarePanel rootPanel;
    CountingLeaf* a = rootPanel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    CountingLeaf* b = rootPanel.create_child<CountingLeaf>(Size{ 40.0f, 8.0f });

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout(); // both A and B were dirty -> re-measured once each

    const int aAfterFirst = a->measureCount();
    const int bAfterFirst = b->measureCount();
    SHOULD(ShouldlyString(aAfterFirst).ShouldBe(1));
    SHOULD(ShouldlyString(bAfterFirst).ShouldBe(1));

    // Invalidate MEASURE on A ONLY. The panel propagates measure-dirty up to
    // the root; B stays measure-clean.
    a->InvalidateMeasure();

    lm.UpdateLayout();
    // A (dirty) is re-measured; B (clean) reuses its stored desired_size and is
    // NOT re-measured.
    SHOULD(ShouldlyString(a->measureCount()).ShouldBe(aAfterFirst + 1));
    SHOULD(ShouldlyString(b->measureCount()).ShouldBe(bAfterFirst));
}

TEST_CASE("S126: panel re-arranges only the dirty child (a clean leaf's final_rect is untouched)")
{
    DirtyAwarePanel rootPanel;
    CountingLeaf* a = rootPanel.create_child<CountingLeaf>(Size{ 50.0f, 10.0f });
    CountingLeaf* b = rootPanel.create_child<CountingLeaf>(Size{ 40.0f, 8.0f });

    vaml::LayoutManager lm(&rootPanel);
    lm.UpdateLayout();

    // First arrange pass: both A and B are arrange-dirty -> both arranged.
    const Rect slot1 = rect_of(0.0f, 0.0f, 500.0f, 400.0f);
    rootPanel.arrange_dirty(slot1);
    const float aY1 = a->final_rect().y;
    const float bY1 = b->final_rect().y;
    // Both were arranged into the S1 stack (they are now arrange-clean).
    SHOULD(ShouldlyString(a->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(b->is_arrange_dirty()).ShouldBe(false));

    // Invalidate ARRANGE on A ONLY.
    a->InvalidateArrange();
    SHOULD(ShouldlyString(a->is_arrange_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(b->is_arrange_dirty()).ShouldBe(false));

    // Re-arrange the panel into a DIFFERENT slot (origin y=100): the dirty child
    // A is re-arranged into the new slot; the clean child B is left untouched
    // (its final_rect stays from the S1 pass -> not re-arranged).
    const Rect slot2 = rect_of(0.0f, 100.0f, 500.0f, 400.0f);
    rootPanel.arrange_dirty(slot2);
    SHOULD(ShouldlyString(a->final_rect().y).ShouldBe(100.0f)); // re-arranged into S2
    SHOULD(ShouldlyString(b->final_rect().y).ShouldBe(bY1));    // B untouched (still S1 value)
    // A is now arrange-clean; B was already clean.
    SHOULD(ShouldlyString(a->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(b->is_arrange_dirty()).ShouldBe(false));
    (void)aY1;
}

// --- 7. InvalidateMeasure vs InvalidateArrange are independent axes -------------

TEST_CASE("S126: measure and arrange invalidation are independent axes")
{
    CountingLeaf root{ Size{ 60.0f, 10.0f } };
    vaml::LayoutManager lm(&root);
    lm.UpdateLayout();
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(false));

    // Measure-only invalidation: measure dirty, arrange stays clean.
    root.InvalidateMeasure();
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_layout_dirty()).ShouldBe(true));

    // Clean it again, then arrange-only invalidation on a fresh element.
    lm.UpdateLayout();
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(false));

    root.InvalidateArrange();
    SHOULD(ShouldlyString(root.is_arrange_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_layout_dirty()).ShouldBe(true));
}
