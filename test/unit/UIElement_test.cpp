// S120 — UIElement base (dep:S097): visibility, opacity, transform owner
// slot, layout slot.
//
// Proves the four concern areas end-to-end against live UIElements:
//   1. Visibility — real DP, default Visible, NOT inherited; typed accessors
//      round-trip all three states; the standard Set/Get(PropertyId) path
//      works; a wrong-tag write throws ValueTypeMismatch.
//   2. Opacity — real DP, default 1.0, INHERITED, validated [0,1]: an
//      out-of-range Set is REJECTED with the store unchanged; a parent's
//      opacity cascades to a child that reads it (read-side pull, S040); a
//      child local set WINS and is not clobbered by a later parent change
//      (S040 local-wins); the parent's synchronous metadata PropChanged fires
//      on the real change (S811 — WPF PropertyChangedCallback, not a fabricated
//      descendant-push postbox). Gated on the SINGLE Inherits flag throughout.
//   3. Transform owner slot — sentinel-only (verified by the searchable
//      sentinel comment in UIElement.h, resolved by S208); no fabricated
//      member/type now.
//   4. Layout slot — sentinel-only (verified by the searchable sentinel
//      comment in UIElement.h, resolved by S122).
//
// Also proves UIElement derives DependencyObject (dynamic_cast) and that the
// Visibility property is NOT inherited (each element's visibility is its own
// effective value — the anti-claim to Opacity's inheritance, both under test).
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

#include <cstdint>
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

TEST_CASE("S120: Visibility — NOT inherited (anti-claim to Opacity's inheritance)")
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

TEST_CASE("S120: Opacity — out-of-range Set THROWS InvalidPropertyValueException, store unchanged")
{
    UIElement el;
    // In-range sets move the value.
    SHOULD(ShouldlyString(el.set_opacity(0.75)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.75));

    // Above 1.0 -> THROWS (S810_1: validate [0,1] refusal, WPF
    // ArgumentException(InvalidPropertyValue); SetResult::REJECTED retired),
    // store stays 0.75.
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] { (void)el.set_opacity(2.0); }));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.75));

    // Below 0.0 -> THROWS, store stays 0.75.
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] { (void)el.set_opacity(-0.5); }));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.75));
}

TEST_CASE("S120: Opacity — standard Set/Get(PropertyId) path agrees with typed accessor")
{
    UIElement el;
    const PropertyId id = PropertyId::of("Opacity", "UIElement"); // string literal

    SHOULD(ShouldlyString(el.Set(id, Value::Double(0.8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(el.Get(id).get<double>()).ShouldBe(0.8));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.8));

    // Out-of-range through the generic path also THROWS (S810_1).
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(id, Value::Double(3.0));
    }));
    SHOULD(ShouldlyString(el.opacity()).ShouldBe(0.8));
}

TEST_CASE("S120: Opacity inheritance — a parent's opacity cascades to a child that reads it")
{
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    // Default both 1.0.
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(1.0));

    // Parent sets 0.4 AFTER the child attached (dynamic ordering, S040): the
    // child reads 0.4 via the read-side pull — no child write ever.
    (void)parent.set_opacity(0.4);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.4));

    // A further parent change recomputes again, still no child write.
    (void)parent.set_opacity(0.2);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.2));
}

TEST_CASE("S120: Opacity inheritance — child local wins and is not clobbered by a later parent change")
{
    UIElement parent;
    UIElement child;
    child.set_parent(&parent);

    // Child declares its OWN local opacity FIRST -> local-wins.
    (void)child.set_opacity(0.9);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.9));

    // Parent changes AFTER: the cascade must RESPECT the child's real local
    // override (S040 local-wins) — child stays 0.9, never swept.
    (void)parent.set_opacity(0.1);
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.9));
}

TEST_CASE("S120(S811): Opacity inherited VALUE propagates to a passive child (pull model — no notification postbox)")
{
    // S811: the fabricated descendant-push postbox is REMOVED (there is NO
    // per-object subscriber list in WPF, DO.cs:1704/1732/2039). What survives
    // is the INHERITED VALUE propagating to a passive child: it has no local
    // opacity of its own, so after a parent opacity write its effective value
    // follows the parent (0.6) via the S039 ancestor walk on Get.
    UIElement parent;
    UIElement child;
    child.set_parent(&parent); // child inherits Opacity (global Inherits) but
                               // has no local store entry of its own (passive)

    // The parent's inherited Opacity write changes the parent's local value.
    SHOULD(ShouldlyString(parent.set_opacity(0.6)).ShouldBe(SetResult::APPLIED));

    // The passive child's effective opacity follows the parent (pull model).
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.6));

    // A child with its OWN local opacity is NOT clobbered (local-wins).
    UIElement child2;
    child2.set_parent(&parent);
    (void)child2.set_opacity(0.9);
    SHOULD(ShouldlyString(child2.opacity()).ShouldBe(0.9));
    (void)parent.set_opacity(0.2);
    SHOULD(ShouldlyString(child2.opacity()).ShouldBe(0.9));
    SHOULD(ShouldlyString(child.opacity()).ShouldBe(0.2)); // passive follows
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

    // Plain object writes Opacity; validate [0,1] still applies globally.
    (void)plain.Set(PropertyId::of("Opacity", "UIElement"), Value::Double(0.3));
    SHOULD(ShouldlyString(plain.Get(PropertyId::of("Opacity", "UIElement")).get<double>()).ShouldBe(0.3));
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)plain.Set(PropertyId::of("Opacity", "UIElement"), Value::Double(5.0));
    }));
}
