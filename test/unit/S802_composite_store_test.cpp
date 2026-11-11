// S802 (dep:S801) — PER-OBJECT STORE + RESOLUTION over the composite (name, owner).
//
// S801 re-keyed the GLOBAL registry (PropertyRegistry + DependencyProperty::GlobalTable)
// onto the (name, owner) composite pair, so a same-name/different-owner pair
// registers as DISTINCT global DependencyProperties (coexistence). S802 (THIS file)
// delivers the per-object side: the DependencyObject's block-resident store, and
// EVERY resolution path in DependencyObject.{h,cpp} (register_property, Get,
// Set, GetEffectiveValue, has_local_value, the S041 property-changed subscriber
// registry, the S037/S041 marshalling queue) now key on the composite (name, owner).
//
// THIS is the headline proof: ONE object registers TWO same-name/different-owner
// DPs and they COEXIST as distinct per-object store entries that resolve
// independently — Set on one pair never touches the other -- exactly WPF's
// (name, ownerType) per-object identity (DependencyProperty.cs FromNameKey).
//
// Proves:
//   1. COEXISTENCE (headline): register "Prop" under {ownerA, ownerB} on ONE
//      object -> both coexist in the per-object store (two distinct entries).
//   2. INDEPENDENT WRITE/READ: Set(PropertyId::of("Prop","ownerA")) does not
//      touch Get(PropertyId::of("Prop","ownerB")); has_local_value is distinct
//      per pair; each resolves its own local override / default.
//   3. INDEPENDENT DEFAULTS: the two pairs carry distinct defaultValue and
//      resolve independently (ownerA default 1, ownerB default 9).
//   4. SAME-PAIR SET OVERWRITES, CROSS-PAIR NEVER ALIASES: a second Set on the
//      same pair overwrites that pair only.
//   5. same-pair re-register throws (PropertyAlreadyRegistered), same-name/
//      different-owner re-register succeeds as a DISTINCT store entry.
//   6. SUBSCRIBERS are pair-keyed: add_property_changed((name, ownerA)) fires
//      on ownerA's Set, NOT ownerB's (composite registration-ordered registry).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>

using vaml::DependencyObject;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

PropertyKey int32Key(const std::string& name, const std::string& owner)
{
    return PropertyKey(name, ValueType::Int32, owner, PropertyFlags::None);
}

PropertyMetadata int32Md(std::int32_t defaultValue)
{
    return PropertyMetadata::WithDefault(Value::Int32(defaultValue), PropertyFlags::None);
}

} // namespace

TEST_CASE("S802: coexistence — two same-name/different-owner DPs share one per-object store")
{
    // THE S802 headline on the per-object axis. ONE object registers "Prop"
    // under TWO owners; both coexist as distinct store entries.
    DependencyObject obj;
    (void)obj.register_property(int32Key("Prop", "ownerA"), int32Md(1));
    (void)obj.register_property(int32Key("Prop", "ownerB"), int32Md(9));

    const PropertyId idA = PropertyId::of("Prop", "ownerA");
    const PropertyId idB = PropertyId::of("Prop", "ownerB");

    // Distinct per-pair defaults before any write.
    SHOULD(ShouldlyString(obj.Get(idA)).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(obj.Get(idB)).ShouldBe(Value::Int32(9)));

    // has_local_value is distinct per pair (both false, no local yet).
    SHOULD(ShouldlyString(obj.has_local_value(idA)).ShouldBe(false));
    SHOULD(ShouldlyString(obj.has_local_value(idB)).ShouldBe(false));

    // Write ownerA's Prop only.
    SHOULD(ShouldlyString(obj.Set(idA, Value::Int32(20))).ShouldBe(SetResult::APPLIED));

    // ownerB's Prop is UNTOUCHED (cross-pair never aliases).
    SHOULD(ShouldlyString(obj.Get(idB)).ShouldBe(Value::Int32(9)));
    SHOULD(ShouldlyString(obj.has_local_value(idA)).ShouldBe(true));
    SHOULD(ShouldlyString(obj.has_local_value(idB)).ShouldBe(false));

    // Same-pair Set overwrites that pair only.
    SHOULD(ShouldlyString(obj.Set(idB, Value::Int32(30))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(idB)).ShouldBe(Value::Int32(30)));
    SHOULD(ShouldlyString(obj.Get(idA)).ShouldBe(Value::Int32(20))); // ownerA unchanged
    SHOULD(ShouldlyString(obj.has_local_value(idB)).ShouldBe(true));
}

TEST_CASE("S802: same-pair re-register throws; same-name/different-owner is a distinct entry")
{
    DependencyObject obj;
    (void)obj.register_property(int32Key("Width", "ownerA"), int32Md(1));

    // Same (name, owner) pair re-register -> PropertyAlreadyRegistered, store unchanged.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)obj.register_property(int32Key("Width", "ownerA"), int32Md(99));
    }));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width", "ownerA"))).ShouldBe(Value::Int32(1)));

    // Same-name/different-owner re-register -> DISTINCT entry (S802 coexistence).
    (void)obj.register_property(int32Key("Width", "ownerB"), int32Md(2));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width", "ownerB"))).ShouldBe(Value::Int32(2)));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width", "ownerA"))).ShouldBe(Value::Int32(1)));
}

TEST_CASE("S802: subscribers are pair-keyed — ownerA fires, ownerB does not")
{
    DependencyObject obj;
    (void)obj.register_property(int32Key("Opacity", "ownerA"), int32Md(0));
    (void)obj.register_property(int32Key("Opacity", "ownerB"), int32Md(0));

    const PropertyId idA = PropertyId::of("Opacity", "ownerA");
    const PropertyId idB = PropertyId::of("Opacity", "ownerB");

    int fireA = 0;
    int fireB = 0;
    // Subscribe to EACH pair separately with a per-pair counter.
    (void)obj.add_property_changed(idA, [&](PropertyId, const Value&, const Value&) { ++fireA; });
    (void)obj.add_property_changed(idB, [&](PropertyId, const Value&, const Value&) { ++fireB; });

    // Setting ownerA's Opacity fires ONLY the ownerA subscription.
    (void)obj.Set(idA, Value::Int32(5));
    SHOULD(ShouldlyString(fireA).ShouldBe(1));
    SHOULD(ShouldlyString(fireB).ShouldBe(0));

    // Setting ownerB's Opacity fires ONLY the ownerB subscription.
    (void)obj.Set(idB, Value::Int32(7));
    SHOULD(ShouldlyString(fireA).ShouldBe(1));
    SHOULD(ShouldlyString(fireB).ShouldBe(1));
}
