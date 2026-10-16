// S128 (dep:S127) — Dirty-only subpass: changing a child re-measures only
// subtree (TEST-ONLY gate).
//
// SCOPE / ARCHITECTURE — the FORMAL dirty-only-subpass gate over the S126/S127
// layout driver + dirty-flag contract, proven on a MULTI-LEVEL tree. S126
// delivered the two dirty bits (measureDirty/arrangeDirty) + InvalidateMeasure/
// InvalidateArrange (propagate up the parent chain, short-circuit) +
// LayoutManager::UpdateLayout (measure pass iff root measure-dirty, arrange
// pass iff root arrange-dirty). S127 delivered the MeasureOverride/ArrangeOverride
// virtual pair, completing the arrange-subgraph AUTO-DRIVE. S128 certifies the
// headline: changing a CHILD confines re-measure/re-arrange to that child's
// SUBTREE-INFLUENCE (the leaf + its ancestor chain), leaving friend subtrees
// and clean siblings untouched — on a real R > (P1 > (A,B), P2 > (C,D)) tree.
//
// The tree uses a test-local dirty-aware stack (DirtyStack, the S125 archetype
// rule; real StackPanel -> S129) that realizes the S126 dirty-respect inside
// the S127 ArrangeOverride auto-drive:
//   * MeasureOverride re-measures ONLY dirty children (a clean child's stored
//     desired_size() is reused); derives width = max + height = sum.
//   * ArrangeOverride re-arranges ONLY dirty children (a clean child's
//     final_rect is left untouched) — auto-invoked by Arrange(final_rect()).
// DirtyLeaf counts BOTH MeasureOverride (mutable) and ArrangeOverride
// invocations, so BOTH axes are observable per leaf.
//
// NO-GAP clean (recorded, not silent): S129 (real StackPanel) + S131 (Grid)
// adopt this exact dirty-respect in their real algorithms — the explicit
// resolution points. No MINIMAL REAL introduced or resolved. Test-local
// subclasses only (zero product symbols, mirrors the S125 archetype pattern);
// each lies on the REAL UIElement + LayoutManager product surface.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutManager.h"
#include "vaml/core/UIElement.h"

#include <cstddef>

using vaml::Rect;
using vaml::Size;
using vaml::UIElement;

namespace {

Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// A leaf with real content extent, counting BOTH MeasureOverride (mutable; the
// S127 seam is const-protected) and ArrangeOverride (non-const) invocations so
// a test can assert exactly which leaves were re-measured / re-arranged.
class DirtyLeaf : public UIElement
{
public:
    explicit DirtyLeaf(Size content) : content_(content) {}

    int measureCount() const { return measureCount_; }
    int arrangeCount() const { return arrangeCount_; }

protected:
    Size MeasureOverride(const Size& available) const override
    {
        ++measureCount_;
        (void)available;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        ++arrangeCount_;
        lastArrange_ = finalSlot;
    }

private:
    mutable int measureCount_ = 0;
    int arrangeCount_ = 0;
    Rect lastArrange_;
    Size content_;
};

// A dirty-aware single-column stack (S125 archetype rule; real StackPanel ->
// S129). The S128 contract: MeasureOverride re-measures ONLY dirty children (a
// clean child's stored desired_size() is reused); ArrangeOverride re-arranges
// ONLY dirty children (a clean child's final_rect is left untouched) — the S126
// dirty-respect realized inside the S127 auto-drive. Counters let a test
// assert the subpass touches exactly the subtree under change.
class DirtyStack : public UIElement
{
public:
    int measureCount() const { return measureCount_; }
    int arrangeCount() const { return arrangeCount_; }

protected:
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

    void ArrangeOverride(const Rect& finalSlot) override
    {
        ++arrangeCount_;
        float y = finalSlot.y;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_arrange_dirty())
            {
                child->Arrange(rect_of(finalSlot.x, y, finalSlot.width, child->desired_size().height));
            }
            y += child->desired_size().height;
        }
    }

private:
    mutable int measureCount_ = 0;
    int arrangeCount_ = 0;
};

// The canonical S128 tree: R(root stack) > (P1 > (A, B), P2 > (C, D)). Captures
// non-owning pointers so tests can observe/act on any node.
struct TwoBranchTree
{
    DirtyStack root;
    DirtyStack* p1 = nullptr;
    DirtyLeaf* a = nullptr;
    DirtyLeaf* b = nullptr;
    DirtyStack* p2 = nullptr;
    DirtyLeaf* c = nullptr;
    DirtyLeaf* d = nullptr;

    TwoBranchTree()
    {
        p1 = root.create_child<DirtyStack>();
        b = p1->create_child<DirtyLeaf>(Size{ 30.0f, 12.0f });
        a = p1->create_child<DirtyLeaf>(Size{ 20.0f, 10.0f });
        p2 = root.create_child<DirtyStack>();
        c = p2->create_child<DirtyLeaf>(Size{ 25.0f, 8.0f });
        d = p2->create_child<DirtyLeaf>(Size{ 15.0f, 5.0f });
    }
};

// A deeper chain for the whole-tree-clean + no-op cases: D > (P1 > (P2 > (X))).
struct DeepTree
{
    DirtyStack root;
    DirtyStack* p1 = nullptr;
    DirtyStack* p2 = nullptr;
    DirtyLeaf* x = nullptr;

    DeepTree()
    {
        p1 = root.create_child<DirtyStack>();
        p2 = p1->create_child<DirtyStack>();
        x = p2->create_child<DirtyLeaf>(Size{ 40.0f, 6.0f });
    }
};

} // namespace

// --- 1. Leaf change confines to the ancestor chain (measure axis) -------

TEST_CASE("S128: invalidating leaf A re-measures only A + its ancestors; a clean sibling and friend branch stay untouched")
{
    TwoBranchTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout(); // full clean pass on both axes

    const int rootBase = t.root.measureCount();
    const int p1Base = t.p1->measureCount();
    const int aBase = t.a->measureCount();
    const int bBase = t.b->measureCount();
    const int p2Base = t.p2->measureCount();
    const int cBase = t.c->measureCount();
    const int dBase = t.d->measureCount();

    t.a->InvalidateMeasure(); // only A (dirty propagates to P1, R)
    lm.UpdateLayout();

    // The measure subpass re-measures A + its TWO ancestors (A's subtree-influence).
    SHOULD(ShouldlyString(t.a->measureCount()).ShouldBe(aBase + 1));
    SHOULD(ShouldlyString(t.p1->measureCount()).ShouldBe(p1Base + 1));
    SHOULD(ShouldlyString(t.root.measureCount()).ShouldBe(rootBase + 1));
    // B (clean sibling IN THE SAME subtree) is NOT re-measured.
    SHOULD(ShouldlyString(t.b->measureCount()).ShouldBe(bBase));
    // The friend branch P2 + its descendants are NOT re-measured.
    SHOULD(ShouldlyString(t.p2->measureCount()).ShouldBe(p2Base));
    SHOULD(ShouldlyString(t.c->measureCount()).ShouldBe(cBase));
    SHOULD(ShouldlyString(t.d->measureCount()).ShouldBe(dBase));
    // The whole tree is measure-clean again after the confined subpass.
    SHOULD(ShouldlyString(t.root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.a->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.p2->is_measure_dirty()).ShouldBe(false));
}

// --- 2. Branch confinement is directional --------------------------------

TEST_CASE("S128: invalidating C under P2 re-measures only the P2 branch; the P1 branch stays untouched")
{
    TwoBranchTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout();

    const int p1Base = t.p1->measureCount();
    const int aBase = t.a->measureCount();
    const int bBase = t.b->measureCount();
    const int p2Base = t.p2->measureCount();
    const int cBase = t.c->measureCount();
    const int dBase = t.d->measureCount();

    t.c->InvalidateMeasure(); // only C (under P2)
    lm.UpdateLayout();

    SHOULD(ShouldlyString(t.c->measureCount()).ShouldBe(cBase + 1));
    SHOULD(ShouldlyString(t.p2->measureCount()).ShouldBe(p2Base + 1));
    SHOULD(ShouldlyString(t.d->measureCount()).ShouldBe(dBase)); // clean sibling under P2
    // The P1 branch is entirely untouched.
    SHOULD(ShouldlyString(t.p1->measureCount()).ShouldBe(p1Base));
    SHOULD(ShouldlyString(t.a->measureCount()).ShouldBe(aBase));
    SHOULD(ShouldlyString(t.b->measureCount()).ShouldBe(bBase));
}

// --- 3. Subgraph coalescing: two dirty children, one subtree, one subpass --

TEST_CASE("S128: invalidating A AND B coalesces to ONE subpass over their subtree")
{
    TwoBranchTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout();

    const int rootBase = t.root.measureCount();
    const int p1Base = t.p1->measureCount();
    const int aBase = t.a->measureCount();
    const int bBase = t.b->measureCount();
    const int p2Base = t.p2->measureCount();
    const int cBase = t.c->measureCount();

    t.a->InvalidateMeasure();
    t.b->InvalidateMeasure();
    lm.UpdateLayout();

    // Exactly ONE subpass: P1 + R each +1 (collapse), both dirty leaves +1.
    SHOULD(ShouldlyString(t.root.measureCount()).ShouldBe(rootBase + 1));
    SHOULD(ShouldlyString(t.p1->measureCount()).ShouldBe(p1Base + 1));
    SHOULD(ShouldlyString(t.a->measureCount()).ShouldBe(aBase + 1));
    SHOULD(ShouldlyString(t.b->measureCount()).ShouldBe(bBase + 1));
    // P2's subtree untouched.
    SHOULD(ShouldlyString(t.p2->measureCount()).ShouldBe(p2Base));
    SHOULD(ShouldlyString(t.c->measureCount()).ShouldBe(cBase));
}

// --- 4. Arrange-axis confinement via the S127 auto-drive -------------------

TEST_CASE("S128: arrange invalidation on A re-arranges only A + its ancestors; B's final_rect is untouched")
{
    TwoBranchTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout();

    // First arrange pass stacked all four leaves: within P1 the child ORDER is
    // [B, A] (B is created first, index 0), so B sits at y=0 and A below it at
    // y=B's height (12) — distinct y so the re-arrange is observable.
    SHOULD(ShouldlyString(t.b->final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(t.a->final_rect().y).ShouldBe(12.0f)); // B's height

    const int aArrangeBase = t.a->arrangeCount();
    const int bArrangeBase = t.b->arrangeCount();
    const float bY = t.b->final_rect().y;
    const int p2ArrangeBase = t.p2->arrangeCount();
    const int measureA = t.a->measureCount();

    t.a->InvalidateArrange(); // ARRANGE axis only (A, P1, R go arrange-dirty)
    lm.UpdateLayout();

    // A is re-arranged (+1); the arrange subpass does NOT touch its ancestors'
    // measures (measure axis frozen by an arrange-only invalidation).
    SHOULD(ShouldlyString(t.a->arrangeCount()).ShouldBe(aArrangeBase + 1));
    SHOULD(ShouldlyString(t.a->measureCount()).ShouldBe(measureA));
    // B is NOT re-arranged and its final_rect is REUSED unchanged.
    SHOULD(ShouldlyString(t.b->arrangeCount()).ShouldBe(bArrangeBase));
    SHOULD(ShouldlyString(t.b->final_rect().y).ShouldBe(bY));
    // The P2 friend branch is entirely untouched on the arrange axis.
    SHOULD(ShouldlyString(t.p2->arrangeCount()).ShouldBe(p2ArrangeBase));
    // The whole tree is arrange-clean again.
    SHOULD(ShouldlyString(t.a->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.root.is_arrange_dirty()).ShouldBe(false));
}

// --- 5. Whole tree clean on BOTH axes after a confined subpass ------------

TEST_CASE("S128: a deep 4-level tree is left fully clean on both axes after one confined subpass")
{
    DeepTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout();

    t.x->InvalidateMeasure(); // deepest leaf only
    lm.UpdateLayout();

    // Measure axis clean everywhere.
    SHOULD(ShouldlyString(t.root.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.p1->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.p2->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.x->is_measure_dirty()).ShouldBe(false));
    // Arrange axis clean everywhere (the arrange pass is gated off the root's
    // arrange bit, which was never dirtied by the measure-only invalidation —
    // it stayed clean from the first full pass).
    SHOULD(ShouldlyString(t.root.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.p1->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.p2->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(t.x->is_arrange_dirty()).ShouldBe(false));
}

// --- 6. A clean-root second UpdateLayout is still a true no-op ------------

TEST_CASE("S128: after a confined subpass, a clean UpdateLayout re-measures/re-arranges nothing")
{
    TwoBranchTree t;
    vaml::LayoutManager lm(&t.root);
    lm.UpdateLayout();

    // One confined subpass.
    t.a->InvalidateMeasure();
    lm.UpdateLayout();

    const int rootMeasure = t.root.measureCount();
    const int p1Measure = t.p1->measureCount();
    const int aMeasure = t.a->measureCount();
    const int rootArrange = t.root.arrangeCount();
    const int aArrange = t.a->arrangeCount();

    // The tree is clean; a second UpdateLayout is a TRUE no-op (both root bits
    // cleared) — the subpass is strictly dirty-driven.
    lm.UpdateLayout();
    SHOULD(ShouldlyString(t.root.measureCount()).ShouldBe(rootMeasure));
    SHOULD(ShouldlyString(t.p1->measureCount()).ShouldBe(p1Measure));
    SHOULD(ShouldlyString(t.a->measureCount()).ShouldBe(aMeasure));
    SHOULD(ShouldlyString(t.root.arrangeCount()).ShouldBe(rootArrange));
    SHOULD(ShouldlyString(t.a->arrangeCount()).ShouldBe(aArrange));
}
