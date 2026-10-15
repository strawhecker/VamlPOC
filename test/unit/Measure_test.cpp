// S123 (dep:S122) — Measure contract (infinity-aware):
// Measure(available) -> DesiredSize; stretch vs wrap.
//
// Under test:
//   1. UIElement::Measure(available) -> writes desired_size() (S122 field)
//      via the infinity-aware stretch-vs-wrap finalizer compute_measured_desired.
//   2. The protected measure_natural_size() seam — base returns {0,0}; a
//      test-local subclass overrides it to prove content-extent flows through
//      the finalizer (the seam S125 fills for Text/panels).
//   3. compute_measured_desired directly — the pure value-domain min/stretch
//      rules, incl. Size::Infinite() never escapes and DesiredSize is never
//      negative.
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
using vaml::Size;
using vaml::UIElement;
using vaml::VerticalAlignment;

namespace {

// A test-local leaf that carries a real content extent through the S123 seam.
// Mirrors what S125's Text/panel overrides will do: measure_natural_size
// reports the un-finalized content size for the given available bound.
class ContentLeaf : public UIElement
{
public:
    explicit ContentLeaf(Size natural) : natural_(natural) {}

protected:
    Size measure_natural_size(const Size& available) const override
    {
        // S125 content seam: ignore `available` for a fixed-extent leaf and
        // report the natural extent (proves the finalizer, not the seam, does
        // the stretch-vs-wrap). (void) available for the base-behavior case.
        (void)available;
        return natural_;
    }

private:
    Size natural_;
};

} // namespace

// --- compute_measured_desired (pure value domain) -----------------------------

TEST_CASE("S123: compute_measured_desired — Stretch fills a finite available (both axes)")
{
    const Size d = vaml::compute_measured_desired(
        Size{ 5.0f, 5.0f }, Size{ 100.0f, 50.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(d.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(50.0f));
}

TEST_CASE("S123: compute_measured_desired — Stretch on an INFINITE axis WRAPS to natural")
{
    // Cannot stretch into an unbounded axis; the DesiredSize wraps to content.
    const Size d = vaml::compute_measured_desired(
        Size{ 30.0f, 40.0f }, Size::Infinite(),
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(d.width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(40.0f));
}

TEST_CASE("S123: compute_measured_desired — non-Stretch finite clamps to min(natural, available)")
{
    // Left/Top: content 500x300 into a 100x50 slot wraps/clamps to the slot.
    const Size d = vaml::compute_measured_desired(
        Size{ 500.0f, 300.0f }, Size{ 100.0f, 50.0f },
        HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(d.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(50.0f));
}

TEST_CASE("S123: compute_measured_desired — non-Stretch finite keeps smaller natural")
{
    // Content smaller than slot: non-Stretch leaves it at natural (wrap keeps
    // the small content, does not grow).
    const Size d = vaml::compute_measured_desired(
        Size{ 10.0f, 20.0f }, Size{ 400.0f, 200.0f },
        HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(d.width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(20.0f));
}

TEST_CASE("S123: compute_measured_desired — non-Stretch on INFINITE axis returns natural")
{
    // min(x, inf) == x; the unbounded axis keeps content size (never infinity).
    const Size d = vaml::compute_measured_desired(
        Size{ 12.0f, 34.0f }, Size::Infinite(),
        HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(d.width).ShouldBe(12.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(34.0f));
}

TEST_CASE("S123: compute_measured_desired — mixed axes (finite width, infinite height)")
{
    // Stretch width on a finite 100 => fill 100; Stretch height on infinite =>
    // wrap to natural height (no unbounded fill).
    const Size d = vaml::compute_measured_desired(
        Size{ 7.0f, 9.0f }, Size{ 100.0f, std::numeric_limits<float>::infinity() },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(d.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(d.height).ShouldBe(9.0f));
}

TEST_CASE("S123: compute_measured_desired — never negative, never infinite")
{
    // Even a pathological negative/zero available yields a finite, non-negative
    // DesiredSize (clamped >= 0; infinity replaced before the clamp).
    const Size d = vaml::compute_measured_desired(
        Size{ -5.0f, -5.0f }, Size{ -100.0f, -100.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(d.width >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(d.height >= 0.0f).ShouldBe(true));
}

// --- UIElement::Measure -> desired_size() ------------------------------------

TEST_CASE("S123: base Measure writes desired_size(); default Stretch finite fills available")
{
    UIElement el;
    // Default Stretch on both axes; finite available 100x50 fills exactly.
    el.Measure(Size{ 100.0f, 50.0f });
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(50.0f));
}

TEST_CASE("S123: base Measure on INFINITE available wraps to 0 (no stretch into unbounded axis)")
{
    UIElement el;
    // Base element has no content (natural 0); an infinite available must NOT
    // stretch to infinity — DesiredSize stays 0 (wrap).
    el.Measure(Size::Infinite());
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(0.0f));
}

TEST_CASE("S123: base Measure respects a non-Stretch alignment (Left/Top keeps 0, no fill)")
{
    UIElement el;
    (void)el.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)el.set_vertical_alignment(VerticalAlignment::Top);
    // Non-Stretch finite: min(natural=0, available) = 0.
    el.Measure(Size{ 100.0f, 50.0f });
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(0.0f));
}

TEST_CASE("S123: repeated Measure with a changed available recomputes desired_size()")
{
    UIElement el;
    el.Measure(Size{ 100.0f, 50.0f });
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(100.0f));
    // Re-measure with a new bound; the field is overwritten (recompute, not cumulate).
    el.Measure(Size{ 30.0f, 30.0f });
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(30.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(30.0f));
}

// --- Seam: test-local content leaf flows through the finalizer -----------------

TEST_CASE("S123: content leaf — Stretch finite fills available, overriding natural extent")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    leaf.Measure(Size{ 200.0f, 100.0f });
    // Stretch dominates: desired == available, not the leaf's natural extent.
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(200.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(100.0f));
}

TEST_CASE("S123: content leaf — non-Stretch finite wraps to min(natural, available)")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    leaf.Measure(Size{ 200.0f, 100.0f });
    // Non-Stretch keeps the natural content extent (smaller than available).
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}

TEST_CASE("S123: content leaf — non-Stretch clamps oversized content to the slot")
{
    ContentLeaf leaf{ Size{ 500.0f, 300.0f } };
    (void)leaf.set_horizontal_alignment(HorizontalAlignment::Left);
    (void)leaf.set_vertical_alignment(VerticalAlignment::Top);
    leaf.Measure(Size{ 100.0f, 50.0f });
    // Content larger than the bounded slot wraps/clamps to the slot.
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(50.0f));
}

TEST_CASE("S123: content leaf — INFINITE available keeps natural (Stretch wraps to content)")
{
    ContentLeaf leaf{ Size{ 10.0f, 20.0f } };
    // Stretch (default) on an unbounded axis honors content, never infinity.
    leaf.Measure(Size::Infinite());
    SHOULD(ShouldlyString(leaf.desired_size().width).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.desired_size().height).ShouldBe(20.0f));
}
