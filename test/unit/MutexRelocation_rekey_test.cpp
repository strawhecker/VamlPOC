// S143 — mutex relocation + token re-key tests (dep:S142).
//
// S811 RETRACTS S143's Part A (the ChangeQueue value-write/notification
// postbox + the S037 wrong-thread write-QUEUING). The fabricated
// ChangeQueue/ChangeProcessor/ChangeRecorder pipeline is removed (there is NO
// per-object multi-subscriber list in WPF, DO.cs:1704/1732/2039), and a
// non-affine Set/Get now THROWS. What survives and is proven HERE is S143's
// Part B: the S140-token + S141-store content-keyed re-keys that REMAIN —
//   - PropertyRegistry::register_key/find/try_find/id_of/try_id by (name,
//     owner) string_view CONTENT (no transient std::string); dedupe throws;
//     returned-key stability across store relocation;
//   - DependencyProperty::Register/TryGet/Get content lookup; duplicate-pair
//     throws + stores nothing; property_id()/owner_type_id() views remain valid.
//
// (The S811-corrected wrong-thread THROW contract for DependencyObject is
// proven in DependencyObject_marshalling_test.cpp.)
//
// ADR-018: zero new locks.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>
#include <string_view>
#include <vector>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::PropertyRegistry;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "S143Owner", PropertyFlags::None);
}

} // namespace

TEST_CASE("S143/S801 PartB: PropertyRegistry content lookup — no transient std::string, dedupe unchanged, key stability")
{
    PropertyRegistry reg;
    const PropertyKey& w = reg.register_key(int32Key("S143_Width"));
    const PropertyKey& m = reg.register_key(int32Key("S143_Margin"));

    // Lookup by (name, owner) string_view content — no transient std::string.
    SHOULD(ShouldlyString(reg.try_find(std::string_view("S143_Width"), std::string_view("S143Owner"))).ShouldBe(&w));
    SHOULD(ShouldlyString(reg.find(std::string_view("S143_Margin"), std::string_view("S143Owner")).name()).ShouldBe(std::string("S143_Margin")));
    SHOULD(ShouldlyString(reg.id_of("S143_Width", "S143Owner")).ShouldBe(PropertyId::of("S143_Width", "S143Owner")));
    SHOULD(ShouldlyString(reg.try_id("S143_Width", "S143Owner").valid()).ShouldBe(true));
    SHOULD(ShouldlyString(reg.try_id("S143_Missing", "S143Owner").valid()).ShouldBe(false));

    // Dedupe throws + leaves the registry unchanged.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)reg.register_key(int32Key("S143_Width"));
    }));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(2u));

    // Returned key reference remains stable (the NameTable owns the key node).
    SHOULD(ShouldlyString(reg.try_find("S143_Width", "S143Owner")).ShouldBe(&w));
    (void)m;

    // Register many to force the node-vector relocation; stored key refs stay valid.
    {
        PropertyRegistry big;
        std::vector<const PropertyKey*> refs;
        const int n = 200;
        for (int i = 0; i < n; ++i)
        {
            const PropertyKey& k = big.register_key(int32Key("S143_Bulk_" + std::to_string(i)));
            refs.push_back(&k);
        }
        for (int i = 0; i < n; ++i)
        {
            // Every earlier reference is still valid + content-resolvable.
            SHOULD(ShouldlyString(big.try_find("S143_Bulk_" + std::to_string(i), "S143Owner")).ShouldBe(refs[i]));
        }
    }
}

TEST_CASE("S143/S801 PartB: DependencyProperty content lookup — Register/TryGet/Get, dedupe throws, views stay valid")
{
    // Unique names to avoid cross-TEST_CASE interference in the global table.
    const std::string name = "S143_Dp_ThemeColor";
    const DependencyProperty& dp = DependencyProperty::Register(
        name, ValueType::Int32, "S143Owner",
        PropertyMetadata::WithDefault(Value::Int32(3), PropertyFlags::Inherits));

    SHOULD(ShouldlyString(dp.property_id().name()).ShouldBe(name));
    // property_id()/owner_type_id() views remain valid after content lookup.
    const std::string& owner = dp.owner_type_id();
    SHOULD(ShouldlyString(owner).ShouldBe(std::string("S143Owner")));

    // TryGet/Get by composite (name, owner) — no transient std::string.
    const DependencyProperty* got = DependencyProperty::TryGet(std::string_view(name), std::string_view("S143Owner"));
    SHOULD(ShouldlyString(got != nullptr).ShouldBe(true));
    if (got != nullptr)
    {
        SHOULD(ShouldlyString(got->metadata().defaultValue()).ShouldBe(Value::Int32(3)));
    }
    SHOULD(ShouldlyString(DependencyProperty::Get(name, "S143Owner").property_id().name()).ShouldBe(name));

    // Duplicate (name, owner) PAIR Register throws + stores nothing (existing kept).
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "S143Owner",
            PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    }));
    const DependencyProperty& still = DependencyProperty::Get(name, "S143Owner");
    SHOULD(ShouldlyString(still.metadata().defaultValue()).ShouldBe(Value::Int32(3))); // original kept

    // Missing pair -> null / throw.
    SHOULD(ShouldlyString(DependencyProperty::TryGet("S143_NoSuch", "S143Owner") == nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)DependencyProperty::Get("S143_NoSuch", "S143Owner");
    }));
}
