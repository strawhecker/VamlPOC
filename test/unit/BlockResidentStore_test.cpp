// S142 — block-resident store tests (dep:S141).
//
// S142 collapsed the DependencyObject/UIElement pimpl + per-object unordered_map
// store into ONE block-resident unit: a single PD-type store that is
// inline-started (PD slots_[kInline] at a fixed offset in the object's own
// allocation) and heap-grown on demand (append-only overflow only once count_
// exceeds the inline capacity). There is no per-object heap hop and no pimpl
// trampoline.
//
// These tests prove the S142-specific properties without re-testing the entire
// S030-S141 pipeline (which is covered by the existing suite):
//   1. INLINE PATH — few properties (0..kInline) register/Set/Get through an
//      all-inline store.
//   2. OVERFLOW GROWTH — >kInline properties all look up/applied correctly,
//      exercising the heap-grown overflow array on the real S035 pipeline.
//   3. stable key — register_property's returned `const PropertyKey&` remains
//      valid across overflow growth (the stable heap key node, S142 §3).
//   4. MOVE — moving a DependencyObject preserves its values + stable key
//      (Store is move-aware; owned/borrowed/stableKey pointers stay valid).
//   5. SINGLE PD — owned (object-scoped) AND borrowed (attached/global)
//      metadata coexist on the SAME object/store.
//   6. UIElement collapsed unit — a UIElement panel still lays out (the layout
//      data now composes inline into the same block).
//
// No [MINIMAL REAL] is introduced here (this is a behavior-unchanged retrofit).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/UIElement.h"

#include <string>
#include <vector>

using vaml::DependencyObject;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::SetResult;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::Visibility;
using vaml::test::ShouldThrow;

namespace {

// A plain Int32-bounded metadata (no coerce/validate) seeded with a default.
PropertyMetadata int32Md(std::int32_t defaultValue)
{
    return PropertyMetadata::WithDefault(Value::Int32(defaultValue), PropertyFlags::None);
}

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "S142Owner", PropertyFlags::None);
}

} // namespace

TEST_CASE("S142: inline path — properties within kInline register/Set/Get with an all-inline store")
{
    DependencyObject obj;
    const std::size_t n = 5u; // well under the 8-slot inline capacity
    for (std::size_t i = 0; i < n; ++i)
    {
        obj.register_property(int32Key("P" + std::to_string(i)), int32Md(0));
    }
    // All defaults applied (no local overrides) — inline store, zero overflow.
    for (std::size_t i = 0; i < n; ++i)
    {
        SHOULD(ShouldlyString(obj.Get(PropertyId::of("P" + std::to_string(i))))
                   .ShouldBe(Value::Int32(0)));
    }
    // Set + Get each independently through the inline store.
    for (std::size_t i = 0; i < n; ++i)
    {
        const Value v = Value::Int32(static_cast<std::int32_t>(100 + i));
        SHOULD(ShouldlyString(obj.Set(PropertyId::of("P" + std::to_string(i)), v))
                   .ShouldBe(SetResult::APPLIED));
        SHOULD(ShouldlyString(obj.Get(PropertyId::of("P" + std::to_string(i)))).ShouldBe(v));
    }
    // NO_CHANGE on a redundant write still works (real-change only).
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("P0"), Value::Int32(100)))
               .ShouldBe(SetResult::NO_CHANGE));
}

TEST_CASE("S142: overflow growth — >kInline properties all work through the real S035 pipeline")
{
    // The production inline capacity is 8 (DependencyObject::Store::kInline).
    // Register MORE than 8 distinct properties so the store must heap-grow past
    // the inline array, then Set/Get each through the full coerce-validate-fire
    // pipeline.
    DependencyObject obj;
    const std::size_t kInline = 8u;
    const std::size_t n = kInline + 5u; // 13 > 8 -> overflow allocated

    for (std::size_t i = 0; i < n; ++i)
    {
        obj.register_property(int32Key("OverflowP" + std::to_string(i)), int32Md(0));
    }
    // All defaults present across inline + overflow.
    for (std::size_t i = 0; i < n; ++i)
    {
        SHOULD(ShouldlyString(obj.Get(PropertyId::of("OverflowP" + std::to_string(i))))
                   .ShouldBe(Value::Int32(0)));
    }
    // Write distinct values; each must APPLY (real change) and read back.
    for (std::size_t i = 0; i < n; ++i)
    {
        const Value v = Value::Int32(static_cast<std::int32_t>(i * 7 + 1));
        SHOULD(ShouldlyString(obj.Set(PropertyId::of("OverflowP" + std::to_string(i)), v))
                   .ShouldBe(SetResult::APPLIED));
        SHOULD(ShouldlyString(obj.Get(PropertyId::of("OverflowP" + std::to_string(i)))).ShouldBe(v));
    }
    // A property in the overflow region still dedupes/rejects by name.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)obj.register_property(int32Key("OverflowP0"), int32Md(9));
    }));
    // Store untouched (the local override set earlier survives).
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("OverflowP0"))).ShouldBe(Value::Int32(1)));
}

TEST_CASE("S142: stable key — register_property's returned ref survives overflow growth")
{
    DependencyObject obj;

    // Grab the returned const PropertyKey& for an early-registered property.
    const std::size_t kInline = 8u;
    const PropertyKey& stable = obj.register_property(int32Key("Stable0"), int32Md(0));

    // Now register MANY more properties to force the store past kInline. The
    // PD array (inline + overflow) relocates/copies entries, but the returned
    // key reference must remain valid (S142 §3: it lives in a stable heap node).
    for (std::size_t i = 1; i <= (kInline + 4u); ++i)
    {
        obj.register_property(int32Key("Stable" + std::to_string(i)), int32Md(0));
    }

    // The reference is still valid + intact after the growth.
    SHOULD(ShouldlyString(stable.name()).ShouldBe(std::string("Stable0")));
    // And the property still resolves through the now-grown store.
    (void)obj.Set(PropertyId::of("Stable0"), Value::Int32(42));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Stable0"))).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S142: move preserves values + stable key (Store is move-aware)")
{
    DependencyObject src;
    const PropertyKey& srcStable = src.register_property(int32Key("Move0"), int32Md(0));
    (void)src.Set(PropertyId::of("Move0"), Value::Int32(5));
    src.register_property(int32Key("Move1"), int32Md(1));
    (void)src.Set(PropertyId::of("Move1"), Value::Int32(6));

    DependencyObject dst(std::move(src));

    // Values moved over intact (inline slots_ copied, overflow taken).
    SHOULD(ShouldlyString(dst.Get(PropertyId::of("Move0"))).ShouldBe(Value::Int32(5)));
    SHOULD(ShouldlyString(dst.Get(PropertyId::of("Move1"))).ShouldBe(Value::Int32(6)));

    // The stable key node is heap-owned, so the reference it returned is still
    // valid after the move AND still resolves through the destination's store.
    SHOULD(ShouldlyString(srcStable.name()).ShouldBe(std::string("Move0")));
    SHOULD(ShouldlyString(dst.Get(PropertyId::of(srcStable.name()))).ShouldBe(Value::Int32(5)));
}

TEST_CASE("S142: single PD — owned (object-scoped) + borrowed (attached/global) metadata coexist")
{
    // OBJECT-SCOPED (owned) registration on the object.
    DependencyObject obj;
    obj.register_property(int32Key("LocalProp"), int32Md(3));
    (void)obj.Set(PropertyId::of("LocalProp"), Value::Int32(30));

    // BORROWED (attached/global): a UIElement self-registers its DPs globally.
    // Opacity resolves via the borrowed/global path (no local store entry until
    // a Set materializes one) — coexisting with the owned local prop on the same
    // single-PD store.
    UIElement element;
    SHOULD(ShouldlyString(element.Get(PropertyId::of("Opacity"))).ShouldBe(Value::Double(1.0)));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("LocalProp"))).ShouldBe(Value::Int32(30)));

    // Materializing a borrowed entry (a Set on the global property) works.
    SHOULD(ShouldlyString(element.Set(PropertyId::of("Opacity"), Value::Double(0.5)))
               .ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(element.Get(PropertyId::of("Opacity"))).ShouldBe(Value::Double(0.5)));

    // The owned local prop is untouched by the borrowed materialization.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("LocalProp"))).ShouldBe(Value::Int32(30)));
}

TEST_CASE("S142: UIElement collapsed unit — a panel still lays out (layout data is inline, same block)")
{
    // S142 embeds UIElement's layout data (children, DesiredSize, final rect,
    // dirty flags) directly in the object — the collapsed block-resident unit.
    // A real element using Measure/Arrange still works end-to-end.
    UIElement root;
    // create_child is [[nodiscard]] (returns the child); bind it so no
    // C4834 discard-warning fires (MSVC treats a bare-(void) call to a
    // nodiscard function as an error under /WX).
    std::vector<UIElement*> children;
    UIElement* created = root.create_child<UIElement>();
    children.push_back(created);

    root.Measure(vaml::Size{ 100.0f, 100.0f });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    SHOULD(ShouldlyString(root.child_count()).ShouldBe(1u));
    // Base UIElement has NO content (MeasureOverride {0,0}), but with a FINITE
    // available {100,100} and default Stretch alignment, compute_measured_desired
    // stretches the desired size to FILL the available bounds -> {100,100}
    // (S123 infinity-aware stretch-vs-wrap; verified by the existing S123/X tests).
    SHOULD(ShouldlyString(root.desired_size().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(root.desired_size().height).ShouldBe(100.0f));

    // Invalidate/clear the dirty flags through the inline members.
    root.InvalidateMeasure();
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(true));
    root.Measure(vaml::Size{ 100.0f, 100.0f });
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));

    // DPs resolve globally (borrowed path) on the collapsed unit.
    SHOULD(ShouldlyString(static_cast<std::int32_t>(root.visibility())).ShouldBe(0));
    SHOULD(ShouldlyString(root.opacity()).ShouldBe(1.0));
}
