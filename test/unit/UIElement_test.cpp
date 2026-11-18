// S120 — UIElement base (dep:S097): visibility, opacity, transform owner
// slot, layout slot.
//
// Proves the four concern areas end-to-end against live UIElements:
//   1. Visibility — real DP, default Visible, NOT inherited; typed accessors
//      round-trip all three states; the standard Set/Get(PropertyId) path
//      works; a wrong-tag write throws ValueTypeMismatch.
//   2. Opacity — real DP, default 1.0, NOT inherited, accepts ANY double
//      (no [0,1] ValidateFunc — WPF contract, S822 dep:S821): 1.5 / -2 / NaN
//      all resolve; the S810_1 throw-on-invalid does NOT apply. opacity()
//      reads ONLY its own local — it does NOT cascade to a child (no DP
//      inheritance); the subtREE alpha is the S208 encoder's ancestor-chain
//      PRODUCT. The parent's synchronous metadata PropChanged fires on the
//      real change (S811 — WPF PropertyChangedCallback, not a fabricated
//      descendant-push postbox). NOT gated on any Inherits flag.
//   3. Transform owner slot — sentinel-only (verified by the searchable
//      sentinel comment in UIElement.h, resolved by S208); no fabricated
//      member/type now.
//   4. Layout slot — sentinel-only (verified by the searchable sentinel
//      comment in UIElement.h, resolved by S122).
//
// Also proves UIElement derives DependencyObject (dynamic_cast) and that BOTH
// Visibility AND Opacity are NOT inherited (each element's own effective
// value; Opacity's subtREE alpha is the S208 encoder ancestor product, not a
// DP cascade).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using vaml::DependencyObject;
using vaml::PropertyId;
using vaml::SetResult;
using vaml::InvalidPropertyValueException;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueTypeMismatch;
using vaml::Visibility;
using vaml::test::ShouldThrow;

namespace {
} // namespace

TEST_CASE("S120: UIElement derives DependencyObject and reveals itself at runtime")
{
    UIElement el;
    DependencyObject& base = el;                 // UIElement IS-A DependencyObject (compile-time)
    SHOULD(ShouldlyString(dynamic_cast<UIElement*>(&base) != nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(dynamic_cast<DependencyObject*>(&el) != nullptr).ShouldBe(true));
}

TEST_CASE("S120: Visibility — default Visible; typed round-trip of all three states")
{
    UIElement el;

    // Default effective value is Visible (0) — the DP resolves to its default.
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(0));

    // Typed round-trip for each state through set_visibility/visibility.
    SHOULD(ShouldlyString(el.set_visibility(Visibility::Hidden)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(1));

    SHOULD(ShouldlyString(el.set_visibility(Visibility::Collapsed)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(2));

    SHOULD(ShouldlyString(el.set_visibility(Visibility::Visible)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(0));
}

TEST_CASE("S120: Visibility — standard Set/Get(PropertyId) path reads the same value")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Visibility", "UIElement"); // string literal -> static lifetime

    // The generic Set/Get path and the typed accessor agree.
    SHOULD(ShouldlyString(el.Set(id, Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.Get(id).get<std::int32_t>()).ShouldBe(1));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(1));
}

TEST_CASE("S821: Visibility — WPF ValidateVisibility boundary gate (0/1/2 resolve, 3/-1 throw, store unchanged)")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Visibility", "UIElement");

    // Start from a non-default state so every in-domain Set below is a REAL
    // change (Set to the already-effective default value returns NO_CHANGE, the
    // no-op optimization — not a boundary signal).
    SHOULD(ShouldlyString(el.Set(id, Value::Int32(2))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(2));

    // The three in-domain values resolve — Visible(0)/Hidden(1)/Collapsed(2).
    SHOULD(ShouldlyString(el.Set(id, Value::Int32(0))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(0));
    SHOULD(ShouldlyString(el.Set(id, Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(1));
    SHOULD(ShouldlyString(el.Set(id, Value::Int32(2))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(2));

    // Out-of-domain values are REJECTED at the boundary (WPF ValidateVisibility,
    // UIElement.cs:2960): the ValidateFunc is false -> InvalidPropertyValueException
    // (S810_1 disposition; WPF ArgumentException(InvalidPropertyValue)), and the
    // store stays UNCHANGED (still Collapsed from the last in-domain Set).
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(id, Value::Int32(3));
    }));
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(id, Value::Int32(-1));
    }));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(2));

    // The typed accessor path flows through the same gate: an enum value always
    // maps into 0/1/2, so it keeps resolving; a synthetic out-of-range enum is
    // impossible, but the generic path above already proves the boundary.
    SHOULD(ShouldlyString(el.set_visibility(Visibility::Visible)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(0));
}

TEST_CASE("S821: Visibility — exception names the property + the offending value")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Visibility", "UIElement");

    // The thrown InvalidPropertyValueException names the offending property
    // (canonical dotted owner.name form) and carries the offending value in its
    // argument (WPF ArgumentException(InvalidPropertyValue) at DO.cs:653).
    std::string rendered;
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        try {
            (void)el.Set(id, Value::Int32(3));
        } catch (const InvalidPropertyValueException& e) {
            rendered = e.property_name() + "=" + e.value();
            throw;
        }
    }));
    SHOULD(ShouldlyString(rendered).ShouldBe("UIElement.Visibility=3"));
}

TEST_CASE("S120: Visibility — wrong-tag write throws ValueTypeMismatch")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Visibility", "UIElement");

    // Opacity is a Double; writing its payload value to an Int32-typed DP is
    // a wrong-tag write -> ValueTypeMismatch (S030/S037 contract), store
    // unchanged (still Visible).
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)el.Set(id, Value::Double(1.0));
    }));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.visibility())).ShouldBe(0));
}

TEST_CASE("S120: Visibility — NOT inherited (each element's own effective value)")
{
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    // Parent collapses; because Visibility is NOT inherited, the child's own
    // effective value stays its default (Visible) — it does NOT follow.
    (void)parent.set_visibility(Visibility::Collapsed);
    SHOULD(ShouldlyString(static_cast<std::int32_t>(parent.visibility())).ShouldBe(2));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(child.visibility())).ShouldBe(0));
}

TEST_CASE("S120: Opacity — default 1.0; set/get round-trip")
{
    UIElement el;
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(1.0));

    SHOULD(ShouldlyString(el.set_opacity(0.5)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.5));
}

TEST_CASE("S822: Opacity — accepts ANY double (no [0,1] validation, WPF contract)")
{
    UIElement el;

    // Above 1.0 -> ACCEPTED (no ValidateFunc; WPF has none — S822 dep:S821.
    // The S810_1 throw-on-invalid does NOT apply; accept is the WPF contract).
    SHOULD(ShouldlyString(el.set_opacity(1.5)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(1.5));

    // Below 0.0 -> ACCEPTED.
    SHOULD(ShouldlyString(el.set_opacity(-0.5)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(-0.5));

    // NaN -> ACCEPTED (WPF stores it; no throw). Assert via isnan.
    SHOULD(ShouldlyString(el.set_opacity(std::numeric_limits<double>::quiet_NaN()))
                .ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(std::isnan(el.opacity())).ShouldBe(true));
}

TEST_CASE("S120: Opacity — standard Set/Get(PropertyId) path agrees with typed accessor")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Opacity", "UIElement"); // string literal

    SHOULD(ShouldlyString(el.Set(id, Value::Double(0.8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.Get(id).get<double>()).ShouldBe(0.8));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.8));

    // Out-of-range through the generic path is ALSO accepted (no ValidateFunc).
    SHOULD(ShouldlyString(el.Set(id, Value::Double(3.0))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(3.0));
}

TEST_CASE("S822: Opacity — NOT inherited: a child reads its OWN default 1.0, never the parent")
{
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    // Default both 1.0.
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0));

    // Parent sets 0.4 AFTER the child attached: because Opacity is NOT
    // inherited (PropertyFlags::None), the child's OWN opacity stays its own
    // default 1.0 — it never reads the parent's DP value (WPF: no Inherits arm
    // for Opacity). The subtREE alpha is the S208 encoder's ancestor product.
    (void)parent.set_opacity(0.4);
    SHOULD(ShouldlyString(parent.opacity()).ShouldBe(0.4));
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0));

    // A further parent change does NOT touch the child's own value.
    (void)parent.set_opacity(0.2);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0));
}

TEST_CASE("S822: Opacity — a child's own local set is untouched by a parent change (no cascade)")
{
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    // Child declares its OWN local opacity FIRST.
    (void)child.set_opacity(0.9);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.9));

    // Parent changes AFTER: no cascade exists for Opacity, so the child's own
    // value is untouched (no DP inheritance to respect or sweep).
    (void)parent.set_opacity(0.1);
    SHOULD(ShouldlyString(parent.opacity()).ShouldBe(0.1));
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.9));
}

TEST_CASE("S822(S811): a passive child does NOT follow a parent's Opacity (no inheritance pull)")
{
    // S811: there is NO fabricated descendant-push postbox (DO.cs:1704/1732/
    // 2039). For Opacity there is ALSO no inheritance pull at all: a child with
    // no local keeps its OWN default 1.0, regardless of the parent's value —
    // NOT 0.6. (Opacity is PropertyFlags::None; the S208 encoder ancestor
    // product reaches the subtREE alpha, not the child's DP.)
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    SHOULD(ShouldlyString(parent.set_opacity(0.6)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(parent.opacity()).ShouldBe(0.6));

    // The passive child's OWN opacity does NOT follow (no inheritance).
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0));

    // A child with its OWN local opacity is naturally unaffected too.
    UIElement child2;
    child2.set_parent(&parent);
    (void)child2.set_opacity(0.9);
    SHOULD(ShouldlyString(child2.opacity()).ShouldBe(0.9));
    (void)parent.set_opacity(0.2);
    SHOULD(ShouldlyString(child2.opacity()).ShouldBe(0.9));
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0)); // no follow
}

TEST_CASE("S120: Visibility + Opacity resolve globally against plain DependencyObjects too (S043)")
{
    // Both DPs are GLOBALLY registered (self-registration happened during the
    // first UIElement construction in this process), so even a plain
    // DependencyObject can read/write them via the S043 attached-aware global
    // resolution — no per-object registration required.
    DependencyObject plain;

    // Plain object writes Visibility through the global descriptor.
    (void)plain.Set(PropertyId::of("Visibility", "UIElement"), Value::Int32(1));
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Visibility", "UIElement")).get<std::int32_t>()).ShouldBe(1));

    // Plain object writes Opacity; ANY double is ACCEPTED globally (no
    // ValidateFunc — S822).
    (void)plain.Set(PropertyId::of("Opacity", "UIElement"), Value::Double(0.3));
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Opacity", "UIElement")).get<double>()).ShouldBe(0.3));
    SHOULD(ShouldlyString(plain.Set(PropertyId::of("Opacity", "UIElement"), Value::Double(5.0)))
                .ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Opacity", "UIElement")).get<double>()).ShouldBe(5.0));
}
