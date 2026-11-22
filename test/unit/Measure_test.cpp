// S123 (dep:S122) — Measure contract, S827/S828/S829 (11ac) drift closure:
// Measure(available) -> DesiredSize = un-clamped content + margins, ALIGNMENT-
// AGNOSTIC (Stretch vs wrap is an ARRANGE concern, not Measure); result may
// EXCEED the offer. Measure entry mirrors WPF (reentrancy guard / Collapsed
// skip / clean-element early-out).
//
// Under test:
//   1. UIElement::Measure(available) -> writes desired_size() (S122 field)
//      via the S827/S828 finalizer compute_measured_desired(natural).
//   2. The protected MeasureOverride() seam (S127) — base returns {0,0}; a
//      test-local subclass overrides it to prove content-extent flows through
//      the finalizer (the seam S129/S400 fills for panels/Text).
//   3. compute_measured_desired directly — the pure value domain: returns the
//      un-clamped natural extent (>= 0), independent of alignment/available.
//   4. S829 measure entry — reentrancy guard, Collapsed skip, clean early-out.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/UIElement.h"

#include <limits>

using vaml::HorizontalAlignment;
using vaml::Rect;
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;
using vaml::Visibility;

namespace {

// A test-local leaf that carries a real content extent through the S127 seam.
// Mirrors what S129/400 Text/panel overrides do: MeasureOverride reports the
// un-finalized content size for the given available bound.
class ContentLeaf : public UIElement
{
public:
    explicit ContentLeaf(Size natural) : natural_(natural) {}

protected:
    Size MeasureOverride(const Size& available) const override
    {
        // S127 content seam: ignore `available` for a fixed-extent leaf and
        // report the natural extent (proves Measure is alignment/offer
        // agnostic). (void) available for the base-behavior case.
        (void)available;
        return natural_;
    }

private:
    Size natural_;
};

// A leaf that counts MeasureOverride invocations (for the S829 clean early-out).
class CountingLeaf : public UIElement
{
public:
    explicit CountingLeaf(Size natural) : natural_(natural) {}
    mutable int measure_calls = 0;

protected:
    Size MeasureOverride(const Size& available) const override
    {
        ++measure_calls;
        (void)available;
        return natural_;
    }

private:
    Size natural_;
};

// A leaf whose MeasureOverride re-enters Measure on ITSELF — the S829 reentrancy
// guard must make that a no-op (never infinite recursion / stack overflow).
class RecursiveLeaf : public UIElement
{
protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        const_cast<RecursiveLeaf*>(this)->Measure(Size{ 10.0f, 10.0f });
        return Size{ 5.0f, 5.0f };
    }
};

} // namespace

// --- compute_measured_desired (pure value domain) -----------------------------

TEST_CASE("S827: compute_measured_desired returns the natural extent unchanged")
{
    // S827/S828: Measure no longer folds alignment or the offer into DesiredSize
    // — it returns the element's content extent, period.
    const Size d = vaml::compute_measured_desired(Size{ 5.0f, 5.0f });
    SHOULD(ShouldlyString(d.width).ShouldBe(5.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(5.0f));
}

TEST_CASE("S827: compute_measured_desired does NOT clamp a large natural to any offer")
{
    // The removed drift: a content of 500x300 is NOT clamped to a former offer;
    // DesiredSize legitimately exceeds whatever `available` the parent passed.
    const Size d = vaml::compute_measured_desired(Size{ 500.0f, 300.0f });
    SHOULD(ShouldlyString(d.width).ShouldBe(500.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(300.0f));
}

TEST_CASE("S827: compute_measured_desired never yields negative geometry")
{
    const Size d = vaml::compute_measured_desired(Size{ -5.0f, -5.0f });
    SHOULD(ShouldlyString(d.width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(0.0f));
}

// --- UIElement::Measure -> desired_size() ------------------------------------

TEST_CASE("S827: base Measure writes a zero DesiredSize (no content) regardless of offer")
{
    UIElement el;
    // A base element has no content; DesiredSize is 0 — NOT the filled offer.
    el.Measure(Size{ 100.0f, 50.0f });
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(0.0f));
}

TEST_CASE("S827: base Measure on INFINITE available stays 0")
{
    UIElement el;
    el.Measure(Size::Infinite());
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(0.0f));
}

TEST_CASE("S828: content leaf on a finite offer reports its CONTENT, not the offer (Stretch is arrange-only)")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    // Default Stretch no longer fills at Measure time.
    leaf.Measure(Size{ 200.0f, 100.0f });
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S827: content leaf DesiredSize MAY EXCEED the offer (un-clamped content)")
{
    ContentLeaf leaf{ Size{ 500.0f, 300.0f } };
    leaf.Measure(Size{ 100.0f, 50.0f });
    // The signal WPF's measure contract must carry: the child needs 500x300.
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(500.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(300.0f));
}

TEST_CASE("S827: the arrange-side render slot is STILL parent-bounded (final rect clipped to slot)")
{
    ContentLeaf leaf{ Size{ 500.0f, 300.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    leaf.Measure(Size{ 100.0f, 50.0f });
    leaf.Arrange(Rect{ 0.0f, 0.0f, 100.0f, 50.0f });
    // DesiredSize exceeds the offer, but the FINAL render slot stays bounded by
    // the parent's slot until S832 (11ad) adds the arrange-side clip.
    SHOULD(ShouldlyString(leaf.final_rect().width <= 100.0f).ShouldBe(true));
    SHOULD(ShouldlyString(leaf.final_rect().height <= 50.0f).ShouldBe(true));
}

TEST_CASE("S827/S828: alignment does NOT change a measured DesiredSize (alignment-agnostic)")
{
    ContentLeaf stretch{ Size{ 10.0f, 20.0f } };
    stretch.Measure(Size{ 200.0f, 100.0f });

    ContentLeaf left{ Size{ 10.0f, 20.0f } };
    (void)left.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)left.set_vertical_alignment(VerticalAlignment::Top);
    left.Measure(Size{ 200.0f, 100.0f });

    SHOULD(ShouldlyString(left.desired_size().width).ShouldBe(stretch.desired_size().width));
    SHOULD(ShouldlyString(left.desired_size().height).ShouldBe(stretch.desired_size().height));
    SHOULD(ShouldlyString(left.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(left.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S828: content leaf on INFINITE available keeps its content (no infinity escapes)")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    leaf.Measure(Size::Infinite());
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S130_1: Measure adds the Margin back into DesiredSize (un-clamped)")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    (void)leaf.set_margin(vaml::Thickness{ 5.0f, 5.0f, 5.0f, 5.0f });
    leaf.Measure(Size{ 200.0f, 100.0f });
    // content 10x20 + margin 5+5 = 20x30.
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(20.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(30.0f));
}

// --- S829 measure entry: clean early-out / Collapsed skip / reentrancy --------

TEST_CASE("S829: a clean Measure at the SAME constraint does NOT recompute")
{
    CountingLeaf leaf{ Size{ 10.0f, 20.0f } };
    leaf.Measure(Size{ 200.0f, 100.0f });
    SHOULD(ShouldlyString(leaf.measure_calls).ShouldBe(1));
    // Same constraint + clean => early-out, no second MeasureOverride.
    leaf.Measure(Size{ 200.0f, 100.0f });
    SHOULD(ShouldlyString(leaf.measure_calls).ShouldBe(1));
    // Changed constraint => recompute (re-measures, cached DesiredSize refreshed).
    leaf.Measure(Size{ 50.0f, 50.0f });
    SHOULD(ShouldlyString(leaf.measure_calls).ShouldBe(2));
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S829: a Collapsed element is SKIPPED (kept dirty) and re-measured on reveal")
{
    CountingLeaf leaf{ Size{ 10.0f, 20.0f } };
    (void)leaf.set_visibility(Visibility::Collapsed);
    leaf.Measure(Size{ 200.0f, 100.0f });
    // Skipped: never measured (DesiredSize still default 0), MeasureOverride not
    // invoked, and the measure-dirty bit is KEPT so a reveal re-measures.
    SHOULD(ShouldlyString(leaf.measure_calls).ShouldBe(0));
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(leaf.is_measure_dirty()).ShouldBe(true));

    // Reveal => the next Measure runs and reports content.
    (void)leaf.set_visibility(Visibility::Visible);
    leaf.Measure(Size{ 200.0f, 100.0f });
    SHOULD(ShouldlyString(leaf.measure_calls).ShouldBe(1));
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S829: reentrant self-Measure is a guarded no-op (no infinite recursion)")
{
    RecursiveLeaf leaf;
    leaf.Measure(Size{ 100.0f, 100.0f });
    // The nested Measure inside MeasureOverride must be ignored; the outer pass
    // completes with the leaf's content.
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(5.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(5.0f));
}
