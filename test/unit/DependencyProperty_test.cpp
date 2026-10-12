// S039 — DependencyProperty tests (dep:S038). The S036-re-scope landing site.
//
// Verifies the GLOBAL property table + DependencyProperty::Register surface
// authored at S039 (the element of the S036 re-scope that makes effective-value
// resolution a real consumer of a global property table):
//   1. Register() installs the canonical (key, metadata) definition in the
//      GLOBAL registry (PropertyRegistry::global) + companion table, and
//      returns a process-lifetime const reference.
//   2. Name dedupe: re-registering a name throws PropertyAlreadyRegistered and
//      leaves the existing registration unchanged (WPF-style global).
//   3. Type coherence: Register() refuses a key type != metadata bound type
//      (ValueTypeMismatch).
//   4. TryGet() / Get() look the definition up by name (null / throw).
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

TEST_CASE("S039: DependencyProperty::Register installs a global property; Get/TryGet retrieve it")
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

    // Read path: the SAME definition is reachable via the registry.
    const DependencyProperty* got = DependencyProperty::TryGet(name);
    SHOULD(ShouldlyString(got != nullptr).ShouldBe(true));
    if (got != nullptr)
    {
        SHOULD(ShouldlyString(got->metadata().defaultValue()).ShouldBe(Value::Int32(3)));
    }
    const DependencyProperty& viaGet = DependencyProperty::Get(name);
    SHOULD(ShouldlyString(viaGet.property_id().name()).ShouldBe(name));
}

TEST_CASE("S039: Register is name-unique globally — re-register throws, existing unchanged")
{
    const std::string name = "ThemeColor_S039_B";
    // First registration with a distinct default.
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "OwnerOne",
        PropertyMetadata::WithDefault(Value::Int32(11), PropertyFlags::Inherits));

    // Re-registering the SAME name (even with different metadata/owner) throws
    // PropertyAlreadyRegistered and leaves the original definition intact.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "OwnerTwo",
            PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    }));

    const DependencyProperty& dp = DependencyProperty::Get(name);
    SHOULD(ShouldlyString(dp.metadata().defaultValue()).ShouldBe(Value::Int32(11))); // original kept
}

TEST_CASE("S039: Register enforces type coherence between key type and metadata bound type")
{
    const std::string name = "ThemeColor_S039_C";
    // key type Int32 but metadata bound to Double -> ValueTypeMismatch, nothing stored.
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "Owner",
            PropertyMetadata::WithDefault(Value::Double(1.5), PropertyFlags::None));
    }));

    // Nothing was stored (the failed Register did not reserve the name).
    SHOULD(ShouldlyString(DependencyProperty::TryGet(name) == nullptr).ShouldBe(true));
}

TEST_CASE("S039: TryGet returns null / Get throws for an unregistered name")
{
    SHOULD(ShouldlyString(DependencyProperty::TryGet("NoSuchProperty_S039") == nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)DependencyProperty::Get("NoSuchProperty_S039");
    }));
}

TEST_CASE("S039: flags() round-trips InheritOnly + multiple flags")
{
    const std::string name = "ThemeColor_S039_D";
    const auto combined = PropertyFlags::Inherits | PropertyFlags::InheritOnly;
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "Owner",
        PropertyMetadata::WithDefault(Value::Int32(0), combined));

    const DependencyProperty& dp = DependencyProperty::Get(name);
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::InheritOnly)).ShouldBe(true));
}
