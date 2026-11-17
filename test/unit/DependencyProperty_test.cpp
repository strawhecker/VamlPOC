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
//   5. flags() and metadata() round-trip multiple surviving flags + default
//      (S814: InheritOnly retired — round-trip uses the surviving flag set).
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

TEST_CASE("S039/S801: flags() round-trips multiple surviving flags")
{
    const std::string name = "ThemeColor_S039_D";
    // S814: InheritOnly is retired — exercise the round-trip with the
    // surviving multi-flag combination (Inherits | ReadOnly | BindsTwoWayByDefault).
    const auto combined = PropertyFlags::Inherits | PropertyFlags::ReadOnly
                        | PropertyFlags::BindsTwoWayByDefault;
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "Owner",
        PropertyMetadata::WithDefault(Value::Int32(0), combined));

    const DependencyProperty& dp = DependencyProperty::Get(name, "Owner");
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::ReadOnly)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::BindsTwoWayByDefault)).ShouldBe(true));
}

TEST_CASE("S039/S801: flags() round-trips the S818 framework arms")
{
    // S818 (Sprint 11aa): the full WPF FrameworkPropertyMetadataOptions arm set
    // round-trips through Register -> Get -> flags() (and metadata().flags()),
    // byte-faithful ordinals (Option B). Asserts by SYMBOL (has_flag), never by
    // the raw bit number.
    const std::string name = "ThemeColor_S039_K";
    const auto broad = PropertyFlags::AffectsMeasure
                     | PropertyFlags::AffectsArrange
                     | PropertyFlags::AffectsParentMeasure
                     | PropertyFlags::AffectsParentArrange
                     | PropertyFlags::AffectsRender
                     | PropertyFlags::OverridesInheritanceBehavior
                     | PropertyFlags::NotDataBindable
                     | PropertyFlags::Journal
                     | PropertyFlags::SubPropertiesDoNotAffectRender;
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "Owner",
        PropertyMetadata::WithDefault(Value::Int32(0), broad));

    const DependencyProperty& dp = DependencyProperty::Get(name, "Owner");
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::AffectsMeasure)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::AffectsArrange)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::AffectsParentMeasure)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::AffectsParentArrange)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::AffectsRender)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::OverridesInheritanceBehavior)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::NotDataBindable)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::Journal)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(dp.flags(), PropertyFlags::SubPropertiesDoNotAffectRender)).ShouldBe(true));

    // metadata().flags() carries the SAME arm set (the PropertyMetadata pass-
    // through — the S039 flags()/metadata() round-trip arms).
    const auto& mf = dp.metadata().flags();
    SHOULD(ShouldlyString(has_flag(mf, PropertyFlags::AffectsMeasure)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(mf, PropertyFlags::AffectsRender)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(mf, PropertyFlags::SubPropertiesDoNotAffectRender)).ShouldBe(true));
}

// ---- S810_2 — REGISTRATION-TIME DEFAULT VALIDATION (the S034/S039 Register
// ---- gate). WPF `ValidateDefaultValueCommon` (DependencyProperty.cs:376-434)
// ---- validates the DEFAULT against type AND the `ValidateValueCallback` at
// ---- registration, throwing ArgumentException(DefaultValuePropertyTypeMismatch)
// ---- on a wrong-typed default and ArgumentException(DefaultValueInvalid) when
// ---- validateValueCallback(defaultValue) returns false. A false result FAILS
// ---- ATOMICALLY: no GlobalTable entry is installed, no partial state.

TEST_CASE("S810_2: Register rejects a default that fails its ValidateFunc (DefaultValueInvalid)")
{
    const std::string name = "S810_2_InvalidDefault_A";
    // The default 42 is outside the validate range [0,10] — WPF throws
    // ArgumentException(DefaultValueInvalid) at registration for exactly this.
    auto md = PropertyMetadata::WithDefault(Value::Int32(42), PropertyFlags::None)
        .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0 && v.get<std::int32_t>() <= 10; });

    SHOULD(ShouldThrow<vaml::InvalidPropertyValueException>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "S810_2Owner",
            std::move(md));
    }));

    // ATOMICITY: the failed Register installed NO GlobalTable entry. The pair
    // is still free (TryGet == nullptr) so a valid Registrer can later take it.
    SHOULD(ShouldlyString(DependencyProperty::TryGet(name, "S810_2Owner") == nullptr).ShouldBe(true));

    // The same pair registers fine once the default satisfies the func — proof
    // the gate is what refused it, not the pair being reserved.
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "S810_2Owner",
        PropertyMetadata::WithDefault(Value::Int32(5), PropertyFlags::None)
            .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0 && v.get<std::int32_t>() <= 10; }));
    SHOULD(ShouldlyString(DependencyProperty::TryGet(name, "S810_2Owner") != nullptr).ShouldBe(true));
}

TEST_CASE("S810_2: Register rejects a wrong-typed default (DefaultValuePropertyTypeMismatch)")
{
    const std::string name = "S810_2_WrongTypeDefault_A";
    // key type Int32 but the default is a Double (bound type Double) — WPF
    // throws ArgumentException(DefaultValuePropertyTypeMismatch). VAML's
    // type-coherence check (ValueTypeMismatch) is the established analogue.
    SHOULD(ShouldThrow<vaml::ValueTypeMismatch>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "S810_2Owner",
            PropertyMetadata::WithDefault(Value::Double(1.5), PropertyFlags::None));
    }));

    // ATOMICITY: nothing installed, pair not reserved.
    SHOULD(ShouldlyString(DependencyProperty::TryGet(name, "S810_2Owner") == nullptr).ShouldBe(true));
}

TEST_CASE("S810_2: happy-path control — a valid default satisfying its ValidateFunc registers")
{
    const std::string name = "S810_2_ValidDefault_A";
    // Default 5 is inside the range; the gate runs the ValidateFunc and it
    // passes, so registration succeeds normally.
    (void)DependencyProperty::Register(
        name, ValueType::Int32, "S810_2Owner",
        PropertyMetadata::WithDefault(Value::Int32(5), PropertyFlags::None)
            .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0 && v.get<std::int32_t>() <= 10; }));

    const DependencyProperty& dp = DependencyProperty::Get(name, "S810_2Owner");
    SHOULD(ShouldlyString(dp.metadata().defaultValue()).ShouldBe(Value::Int32(5)));
    SHOULD(ShouldlyString(dp.metadata().validate(Value::Int32(5))).ShouldBe(true));
}
