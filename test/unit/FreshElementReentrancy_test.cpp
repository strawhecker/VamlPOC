// S840 (dep:S839, Sprint 11ae) -- fresh-element NeverMeasured/NeverArranged +
// the !MeasureInProgress reentrancy guard + the previous-constraint cache
// (the element-level short-circuit primitive S842's panels rely on). D7 + D1.
//
// Ground truth: WPF PresentationCore UIElement.cs.
//   * Construction sets ONLY NeverMeasured/NeverArranged (:108-109); MeasureDirty/
//     ArrangeDirty stay clear. So a fresh element is IsMeasureValid/IsArrangeValid
//     (:225-240) yet still must run its first pass -- "valid" and "never measured"
//     are DISTINCT. is_measure_valid()/is_arrange_valid() mirror the raw WPF
//     accessors; is_measure_dirty()/is_arrange_dirty() are the S840 needs-pass
//     predicates (dirty || never).
//   * A never-measured element ALWAYS measures -- the !neverMeasured term defeats
//     the clean short-circuit (:619-626).
//   * A clean non-never element at an UNCHANGED constraint short-circuits via the
//     previous-constraint cache (isCloseToPreviousMeasure, :591/:619-622); a
//     changed constraint re-measures.
//   * InvalidateMeasure/InvalidateArrange issued DURING a pass are suppressed by
//     the !MeasureInProgress / !ArrangeInProgress guard (:251/:284-285): the
//     element ends clean, never re-dirties, never recurses.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 convention). doctest's main
// comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/UIElement.h"

using vaml::Rect;
using vaml::Size;
using vaml::UIElement;

namespace {

// Counts MeasureOverride / ArrangeOverride calls so a re-run is observable.
class CountingLeaf : public UIElement
{
public:
    explicit CountingLeaf(Size natural) : natural_(natural) {}
    mutable int measure_calls = 0;
    mutable int arrange_calls = 0;

protected:
    Size MeasureOverride(const Size& available) const override
    {
        ++measure_calls;
        (void)available;
        return natural_;
    }
    Size ArrangeOverride(Size finalSize) override
    {
        ++arrange_calls;
        (void)finalSize;
        return finalSize;
    }

private:
    Size natural_;
};

// Invalidates ITSELF from inside MeasureOverride -- the !MeasureInProgress guard
// must suppress that invalidation (element ends clean, no recursion).
class SelfInvalidatingLeaf : public UIElement
{
public:
    mutable int measure_calls = 0;

protected:
    Size MeasureOverride(const Size& available) const override
    {
        ++measure_calls;
        (void)available;
        const_cast<SelfInvalidatingLeaf*>(this)->InvalidateMeasure();
        return Size{ 5.0f, 5.0f };
    }
};

// Invalidates ITSELF from inside ArrangeOverride -- the !ArrangeInProgress guard
// must suppress that invalidation.
class SelfInvalidatingArrangeLeaf : public UIElement
{
public:
    mutable int arrange_calls = 0;

protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        return Size{ 5.0f, 5.0f };
    }
    Size ArrangeOverride(Size finalSize) override
    {
        ++arrange_calls;
        (void)finalSize;
        InvalidateArrange();
        return finalSize;
    }
};

} // namespace

// --- D7: fresh-element "never measured" vs "valid" are DISTINCT --------------

TEST_CASE("S840: a fresh element is never-measured/never-arranged yet valid")
{
    UIElement el;
    CHECK(el.never_measured());
    CHECK(el.never_arranged());
    // WPF IsMeasureValid / IsArrangeValid == !MeasureDirty / !ArrangeDirty:
    // a fresh element is "Clean" (valid) ...
    CHECK(el.is_measure_valid());
    CHECK(el.is_arrange_valid());
    // ... yet it still NEEDS a pass (the distinct NeverMeasured/NeverArranged).
    CHECK(el.is_measure_dirty());
    CHECK(el.is_arrange_dirty());
}

TEST_CASE("S840: after the first Measure the element is valid AND no longer needs a pass")
{
    CountingLeaf leaf(Size{ 10.0f, 10.0f });
    leaf.Measure(Size{ 100.0f, 100.0f });
    CHECK_FALSE(leaf.never_measured());
    CHECK(leaf.is_measure_valid());
    CHECK_FALSE(leaf.is_measure_dirty());
}

// --- never-measured always measures; the constraint cache short-circuits -----

TEST_CASE("S840: a never-measured element always measures on its first pass")
{
    CountingLeaf leaf(Size{ 10.0f, 10.0f });
    leaf.Measure(Size{ 100.0f, 100.0f });
    CHECK(leaf.measure_calls == 1);
}

TEST_CASE("S840: a clean element at an UNCHANGED constraint short-circuits (cache)")
{
    CountingLeaf leaf(Size{ 10.0f, 10.0f });
    leaf.Measure(Size{ 100.0f, 100.0f });
    CHECK(leaf.measure_calls == 1);
    leaf.Measure(Size{ 100.0f, 100.0f }); // clean + same constraint -> cache hit
    CHECK(leaf.measure_calls == 1);       // still a single pass
}

TEST_CASE("S840: a CHANGED constraint re-measures (previous-constraint cache miss)")
{
    CountingLeaf leaf(Size{ 10.0f, 10.0f });
    leaf.Measure(Size{ 100.0f, 100.0f });
    leaf.Measure(Size{ 50.0f, 50.0f }); // changed -> cache miss
    CHECK(leaf.measure_calls == 2);
}

// --- reentrancy: the !MeasureInProgress / !ArrangeInProgress guards ----------

TEST_CASE("S840: InvalidateMeasure during a Measure pass is suppressed")
{
    SelfInvalidatingLeaf leaf;
    leaf.Measure(Size{ 100.0f, 100.0f });
    CHECK(leaf.measure_calls == 1);       // no recursive re-entry
    CHECK_FALSE(leaf.is_measure_dirty()); // the in-pass invalidation was suppressed
}

TEST_CASE("S840: InvalidateArrange during an Arrange pass is suppressed")
{
    SelfInvalidatingArrangeLeaf leaf;
    leaf.Measure(Size{ 100.0f, 100.0f });
    leaf.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 100.0f });
    CHECK(leaf.arrange_calls == 1);
    CHECK_FALSE(leaf.is_arrange_dirty()); // the in-pass arrange invalidation was suppressed
}
