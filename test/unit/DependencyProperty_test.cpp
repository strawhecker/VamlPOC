// S039 + S801 — DependencyProperty tests (dep:S038). The S036-re-scope landing
// site. S801: the global table + read path are (name, owner) COMPOSITE-keyed
// (WPF `FromNameKey(name, ownerType)` ground truth).
//
// Verifies:
//   1. Register() installs the canonical (key, metadata) definition in the
//      GLOBAL registry (PropertyRegistry::global) + companion table, and
//      returns a process-lifetime const reference.
//   2. (name, owner) dedupe: re-registering the SAME pair throws
//      PropertyAlreadyRegistered and leaves the existing registration
//      unchanged; a same-name/different-owner pair COEXISTS as a distinct DP.
//   3. Type coherence: Register() refuses a key type != metadata bound type
//      (ValueTypeMismatch).
//   4. TryGet(name, owner) / Get(name, owner) look the definition up by its
//      composite pair (null / throw).
//   5. flags() and metadata() round-trip the InheritOnly flag + default.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyProperty.h"

#include <string>

using vaml::DependencyProperty;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::test::ShouldThrow;

TEST_CASE("S039/S801: DependencyProperty::Register installs a global property; Get/TryGet retrieve it")
{
    // Fresh, unique name to avoid cross-TEST_CASE interference in the shared
    // global table.
    const std::string name = "ThemeColor_S039_A";
    const DependencyProperty& dp = DependencyProperty::Register(
        name, ValueType::Int32, "TestOwner",
        PropertyMetadata::WithDefault(Value::Int32(3), PropertyFlags::Inherits));

    SHOULD(ShouldlyString(dp.property_id().name()).ShouldBe(name));
    SHOULD(ShouldlyString(dp.flags() == PropertyFlags::Inherits).ShouldBe(true));
    SHOULD(ShouldlyString(dp.metadata().defaultValue()).ShouldBe(Value::Int32(3)));

    // Read path: the SAME definition is reachable via the composite pair.
    const DependencyProperty* got = DependencyProperty::TryGet(name, "TestOwner");
    SHOULD(ShouldlyString(got != nullptr).ShouldBe(true));
    if (got != nullptr)
    {
        SHOULD(ShouldlyString(got->metadata().defaultValue()).ShouldBe(Value::Int32(3)));
    }
    const DependencyProperty& viaGet = DependencyProperty::Get(name, "TestOwner");
    SHOULD(ShouldlyString(viaGet.property_id().name()).ShouldBe(name));
}

TEST_CASE("S039/S801: Register is (name, owner)-unique — same-pair throws, coexistence holds")
{
    const std::string name = "ThemeColor_S039_B";
    // First registration with a distinct default.
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "OwnerOne",
        PropertyMetadata::WithDefault(Value::Int32(11), PropertyFlags::Inherits));

    // Re-registering the SAME (name, owner) PAIR (even with different metadata)
    // throws PropertyAlreadyRegistered and leaves the original intact.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "OwnerOne",
            PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    }));

    const DependencyProperty& dp = DependencyProperty::Get(name, "OwnerOne");
    SHOULD(ShouldlyString(dp.metadata().defaultValue()).ShouldBe(Value::Int32(11))); // original kept

    // A same-name DIFFERENT-owner pair COEXISTS as a distinct DP (S801).
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "OwnerTwo",
        PropertyMetadata::WithDefault(Value::Int32(42), PropertyFlags::None));
    SHOULD(ShouldlyString(DependencyProperty::Get(name, "OwnerTwo")
                              .metadata().defaultValue())
              .ShouldBe(Value::Int32(42)));
}

TEST_CASE("S039/S801: Register enforces type coherence between key type and metadata bound type")
{
    const std::string name = "ThemeColor_S039_C";
    // key type Int32 but metadata bound to Double -> ValueTypeMismatch, nothing stored.
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "Owner",
            PropertyMetadata::WithDefault(Value::Double(1.5), PropertyFlags::None));
    }));

    // Nothing was stored (the failed Register did not reserve the pair).
    SHOULD(ShouldlyString(DependencyProperty::TryGet(name, "Owner") == nullptr).ShouldBe(true));
}

TEST_CASE("S039/S801: TryGet returns null / Get throws for an unregistered pair")
{
    SHOULD(ShouldlyString(DependencyProperty::TryGet("NoSuchProperty_S039", "Owner") == nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)DependencyProperty::Get("NoSuchProperty_S039", "Owner");
    }));
}

TEST_CASE("S039/S801: flags() round-trips InheritOnly + multiple flags")
{
    const std::string name = "ThemeColor_S039_D";
    const auto combined = PropertyFlags::Inherits | PropertyFlags::InheritOnly;
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "Owner",
        PropertyMetadata::WithDefault(Value::Int32(0), combined));

    const DependencyProperty& dp = DependencyProperty::Get(name, "Owner");
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::InheritOnly)).ShouldBe(true));
}
