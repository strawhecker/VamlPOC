// S146 — steady-state heap-churn counter + public-API string_view/intern audit.
//
// Scope (feeds S628):
//   1. heap_allocation_count()/heap_allocation_bytes() are the PUBLIC funnel
//      counter (see HeapStats.h). This test asserts the S146 headline: after a
//      warmup pass, an IDENTICAL steady-state workload performs ZERO VAML-owned
//      heap churn (flat counter across repeated passes).
//   2. The public API audit: every register*/find/try_*/Get/Set/named/names/
//      Load entry point accepts a std::string_view literal AND an
//      intern()-produced view — no owned std::string is required on any
//      lookup/registration path (the refactor-gate contract must survive).

#include "doctest/doctest.h"

#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"       // StackPanel, Canvas, Orientation (built-in elements)
#include "vaml/core/Geometry.h"      // Size, Rect
#include "vaml/core/HeapStats.h"     // S146 public counters
#include "vaml/core/Intern.h"        // vaml::intern() (audit + churn)
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyRegistry.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <memory>

using vaml::Canvas;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyId;
using vaml::PropertyRegistry;
using vaml::Rect;
using vaml::Size;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::heap_allocation_bytes;
using vaml::heap_allocation_count;
using vaml::intern;

namespace {

// A test-local leaf with a real content extent (the S125 TextLikeLeaf pattern).
class ContentLeaf : public UIElement
{
public:
    explicit ContentLeaf(Size content) : content_(content) {}

protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        return content_;
    }

    void ArrangeOverride(const Rect& finalSlot) override
    {
        (void)finalSlot;
    }

private:
    Size content_;
};

// Runs one full Measure/Arrange pass over a representative tree.
void run_layout_pass(StackPanel& root)
{
    root.Measure(Size{ 1000.0f, 500.0f });
    root.Arrange(Rect{ 0.0f, 0.0f, 500.0f, 500.0f });
}

// A fixed, bounded VAML document. Uses ONLY REAL globally-registered NON-ATTACHED
// DP names whose values the built-in converters recognize (Visibility is
// Int32-backed with an Int32 converter) so the reader resolves them without
// throwing — and after the first pass the whole document is pure content-dedup
// + slab recycle (the S146 churn-free steady state).
constexpr std::string_view kMarkup =
    "<StackPanel><Button Visibility=\"1\"/><Button Visibility=\"2\"/></StackPanel>";

} // namespace

// ---------------------------------------------------------------------------
// S146 public API audit: string_view literal + intern()-produced view on the
// real name-keyed entry points (no owned std::string required anywhere).
// ---------------------------------------------------------------------------
TEST_CASE("S146: public API accepts string_view literals and intern() views (register/find/Get/named/Load)")
{
    // Constructing a UIElement guarantees the S120 self-registration of the
    // REAL DPs (Visibility/Opacity/Margin/... onto the global DependencyProperty
    // table) has run, so the audit lookups below resolve.
    StackPanel auditPanel;

    // The name-keyed DP read paths take a bare string_view literal AND an
    // intern()-produced view — both must resolve (S039/S043).
    const std::string_view lit = "Opacity";
    const std::string_view interned = intern(lit);

    SHOULD(ShouldlyPointer(DependencyProperty::TryGet(lit)).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(DependencyProperty::TryGet(interned)).ShouldNotBeNull());
    (void)DependencyProperty::Get(lit); // Compilation proof: a string_view enters Get().

    // PropertyRegistry (S032/S039) name-keyed loads — literal + intern view.
    const PropertyRegistry& reg = PropertyRegistry::global();
    SHOULD(ShouldlyPointer(reg.try_find(lit)).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(reg.try_find(interned)).ShouldNotBeNull());
    (void)reg.find(lit);       // Compilation proof.
    (void)reg.id_of(lit);      // Compilation proof.
    (void)reg.try_id(interned);// Compilation proof.

    // PropertyId's .name() round-trips a string_view without owning it.
    SHOULD(ShouldlyString(std::string(reg.id_of(lit).name())).ShouldBe(std::string("Opacity")));

    // VamlReader::Load takes a string_view (this kMarkup is exactly that) and
    // named()/names() take string_view. Compilation + runtime proof.
    VamlReader reader;
    auto root = reader.Load(kMarkup);
    SHOULD(ShouldlyPointer(root.get()).ShouldNotBeNull());
    (void)reader.named(std::string_view("nope")); // Compilation proof; no x:Name in markup.
    (void)reader.names();                         // Compilation proof.
}

// ---------------------------------------------------------------------------
// S146 headline: steady state is FLAT (zero VAML-owned heap churn after warmup).
// ---------------------------------------------------------------------------
TEST_CASE("S146: identically-repeated layout passes do zero heap churn after the first pass")
{
    // Build the tree ONCE. First-touch may grow intern/pool stores (warmup).
    StackPanel root;
    for (int i = 0; i < 4; ++i)
    {
        (void)root.create_child<ContentLeaf>(Size{ 50.0f, 20.0f });
        (void)root.create_child<Canvas>();
    }

    run_layout_pass(root); // warmup pass (may allocate on first-touch)

    const std::size_t countAfterWarmup = heap_allocation_count();
    const std::size_t bytesAfterWarmup = heap_allocation_bytes();

    // Identical steady-state passes: the SlabPool recycles, the intern store
    // content-dedups, the object pools relax — so the funnel advances NOTHING.
    for (int i = 0; i < 50; ++i)
    {
        root.InvalidateMeasure();
        run_layout_pass(root);
    }

    SHOULD(ShouldlyIntegral(heap_allocation_count()).ShouldBe(countAfterWarmup));
    SHOULD(ShouldlyIntegral(heap_allocation_bytes()).ShouldBe(bytesAfterWarmup));
}

TEST_CASE("S146: repeated Load of an identical bounded document is churn-free after the first pass")
{
    // After the FIRST Load warms the intern store + element + parse pools,
    // reloading the SAME bounded document is pure content-dedup + slab recycle:
    // the funnel advances nothing in steady state.
    {
        VamlReader first;
        auto t = first.Load(kMarkup);
        SHOULD(ShouldlyPointer(t.get()).ShouldNotBeNull());
    }

    const std::size_t countAfterWarmup = heap_allocation_count();
    const std::size_t bytesAfterWarmup = heap_allocation_bytes();

    for (int i = 0; i < 20; ++i)
    {
        VamlReader r;
        auto tree = r.Load(kMarkup);
        SHOULD(ShouldlyPointer(tree.get()).ShouldNotBeNull());
    }

    SHOULD(ShouldlyIntegral(heap_allocation_count()).ShouldBe(countAfterWarmup));
    SHOULD(ShouldlyIntegral(heap_allocation_bytes()).ShouldBe(bytesAfterWarmup));
}
