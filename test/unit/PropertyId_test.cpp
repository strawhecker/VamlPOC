// S033 — PropertyId tests: constexpr-safe static id.
// Scope: PropertyId::of("Width") is a compile-time constant (proven by
// static_assert below); it wraps the property name as a string_view; equality
// is by name; default-id is invalid; std::hash specializes and is consistent
// with equality; and it interconverts with the registry's id_of/try_id.
//
// S800 — COMPOSITE PropertyId (foundation). PropertyId is now the pair
// (name_view, owner_view) where owner is the fully-qualified declaring-type
// name (e.g. "vaml::StackPanel", "::Foo"). Identity is STRICT PAIR: equal IFF
// name AND owner equal; same-name/different-owner pairs are DISTINCT ids;
// std::hash is pair-keyed. The S033 regression (owner "") keeps proving NO
// live semantic change. The composite-identity SET below proves
// pair-equality, same-name/different-owner distinctness, the global-`::` vs
// namespaced-owner distinction, hash difference, and both-compilers
// constexpr-green (the S033 compile-time contract extends to the composite).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>

using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyRegistry;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// --- Compile-time proof: PropertyId::of on a literal is a constant expression.
static_assert(PropertyId::of("Width").name() == "Width", "PropertyId::of must be constexpr-safe");
static_assert(PropertyId::of("Margin").name() == "Margin");
static_assert(PropertyId::of("Width").valid(), "a named id is valid");
static_assert(PropertyId::of("Width") == PropertyId::of("Width"));
static_assert(PropertyId::of("Width") != PropertyId::of("Height"));
static_assert(PropertyId{}.valid() == false, "default id is invalid");

// constexpr equality lives in a constant expression too.
static_assert(PropertyId::of("A") == PropertyId::of("A"));
static_assert(PropertyId::of("A") != PropertyId::of("B"));

// --- S800 COMPOSITE: constexpr pair proofs (both compilers green).
// Pair equality: same (name, owner) equal.
static_assert(PropertyId::of("Orientation", "vaml::StackPanel") ==
              PropertyId::of("Orientation", "vaml::StackPanel"));
// Same name, DIFFERENT owner -> DISTINCT ids (the disambiguation point).
static_assert(PropertyId::of("Orientation", "vaml::StackPanel") !=
              PropertyId::of("Orientation", "vaml::Controls"));
// Global-`::` owner vs namespaced owner are DIFFERENT scopes -> distinct ids.
static_assert(PropertyId::of("Foo", "::Foo") != PropertyId::of("Foo", "vaml::Foo"));
// owner() accessor carries the fully-qualified declaring type.
static_assert(PropertyId::of("Orientation", "vaml::StackPanel").owner() == "vaml::StackPanel");
static_assert(PropertyId::of("Foo", "::Foo").owner() == "::Foo");
// Legacy bridge: bare of(name) carries an EMPTY owner (owner-less compile art).
static_assert(PropertyId::of("Width").owner().empty());
static_assert(PropertyId::of("Width") == PropertyId::of("Width"));
static_assert(PropertyId::of("Width") != PropertyId::of("Height"));
// A composite (owner-carrying) id is a DISTINCT identity from the same-name
// legacy id — same name is NOT identity across different owner scopes.
static_assert(PropertyId::of("Width") != PropertyId::of("Width", "vaml::Button"));
// valid() stays name-driven.
static_assert(PropertyId::of("Orientation", "vaml::StackPanel").valid());

PropertyKey make_key(const std::string& name, ValueType type = ValueType::Int32,
                     const std::string& owner = "Button",
                     PropertyFlags flags = PropertyFlags::None)
{
    return PropertyKey(name, type, owner, flags);
}

} // namespace

TEST_CASE("S033 PropertyId: of() is constexpr-safe and binds the name")
{
    // The compile-time value and the runtime value agree.
    PropertyId id = PropertyId::of("Width");
    SHOULD(ShouldlyString(std::string(id.name())).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(id.valid()).ShouldBe(true));
}

TEST_CASE("S033 PropertyId: equality is by name; default id is invalid")
{
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldBe(PropertyId::of("Width")));
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldNotBe(PropertyId::of("Height")));
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldNotBe(PropertyId::of("Height")));
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldBe(PropertyId::of("Width")));

    // Default-constructed id is invalid and unequal to any named id.
    PropertyId def;
    SHOULD(ShouldlyString(def.valid()).ShouldNotBe(true));
    SHOULD(ShouldlyString(def.name().empty()).ShouldBe(true));
    SHOULD(ShouldlyString(def).ShouldNotBe(PropertyId::of("Width")));
    SHOULD(ShouldlyString(def).ShouldBe(PropertyId{})); // two default ids compare equal
}

TEST_CASE("S033 PropertyId: std::hash is consistent with equality")
{
    const std::hash<PropertyId> h;
    // Equal ids hash equal.
    SHOULD(ShouldlyString(h(PropertyId::of("Width"))).ShouldBe(h(PropertyId::of("Width"))));
    SHOULD(ShouldlyString(h(PropertyId{})).ShouldBe(h(PropertyId{})));
    // Distinct ids hash (over a sample) to distinct values — equal-inputs-
    // always-equal is the contract.
    SHOULD(ShouldlyString(h(PropertyId::of("Width"))).ShouldNotBe(h(PropertyId::of("Height"))));
}

TEST_CASE("S033 PropertyId: registry id_of/try_id round the name-keyed store")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // id_of of a registered name equals the compile-time id of that literal.
    SHOULD(ShouldlyString(reg.id_of("Width")).ShouldBe(PropertyId::of("Width")));
    SHOULD(ShouldlyString(reg.id_of("Width").valid()).ShouldBe(true));

    // try_id agrees.
    SHOULD(ShouldlyString(reg.try_id("Width")).ShouldBe(PropertyId::of("Width")));
    SHOULD(ShouldlyString(reg.try_id("Width").valid()).ShouldBe(true));
}

TEST_CASE("S033 PropertyId: id_of/try_id on an unregistered name")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // id_of throws PropertyNotFound; try_id never throws and yields an
    // invalid id. (ShouldThrow OUT-EXPOSES doctest: wrong/missing throw
    // reports the actual type + message.)
    SHOULD(ShouldThrow<vaml::PropertyNotFound>([&] { (void)reg.id_of("Missing"); }));
    SHOULD(ShouldlyString(reg.try_id("Missing").valid()).ShouldNotBe(true));
    SHOULD(ShouldlyString(reg.try_id("Missing").name().empty()).ShouldBe(true));

    // On an empty registry the same holds.
    PropertyRegistry empty;
    SHOULD(ShouldThrow<vaml::PropertyNotFound>([&] { (void)empty.id_of("Anything"); }));
    SHOULD(ShouldlyString(empty.try_id("Anything").valid()).ShouldNotBe(true));
}

// ===========================================================================
// S800 — COMPOSITE PropertyId (foundation, pure type change, no behavior
// change yet). The type is now the pair (name_view, owner_view); identity is
// STRICT PAIR (name AND owner); std::hash is PAIR-KEYED. Same-name /
// different-owner pairs are DISTINCT ids — the WPF `(name, Type)` disambiguation.
// The S033 registry cases above (owner "" via the legacy bridge) stay GREEN,
// proving no live semantic change this step.
// ===========================================================================

TEST_CASE("S800 PropertyId: pair-equality of the composite (name, owner)")
{
    // Same (name, owner) -> equal.
    SHOULD(ShouldlyString(PropertyId::of("Orientation", "vaml::StackPanel"))
               .ShouldBe(PropertyId::of("Orientation", "vaml::StackPanel")));
    // Same name, different owner -> DISTINCT (the disambiguation point).
    SHOULD(ShouldlyString(PropertyId::of("Orientation", "vaml::StackPanel"))
               .ShouldNotBe(PropertyId::of("Orientation", "vaml::Controls")));
    // owner() accessor carries the fully-qualified declaring type.
    SHOULD(ShouldlyString(std::string(PropertyId::of("Orientation", "vaml::StackPanel").owner()))
               .ShouldBe(std::string("vaml::StackPanel")));
    SHOULD(ShouldlyString(PropertyId::of("Orientation", "vaml::StackPanel").valid()).ShouldBe(true));
}

TEST_CASE("S800 PropertyId: distinctness of same-name/different-owner (WPF semantics)")
{
    // The headline composite case: two owner types declare the SAME property
    // name -> two DISTINCT ids. Mirrors WPF's Slider.Orientation vs
    // StackPanel.Orientation being different DPs.
    const PropertyId sliderOrient = PropertyId::of("Orientation", "vaml::Slider");
    const PropertyId panelOrient  = PropertyId::of("Orientation", "vaml::StackPanel");
    SHOULD(ShouldlyString(sliderOrient).ShouldNotBe(panelOrient));
    SHOULD(ShouldlyString(panelOrient).ShouldNotBe(sliderOrient));
    // Same name same owner is ALWAYS the same id (dedupe contract).
    SHOULD(ShouldlyString(sliderOrient).ShouldBe(PropertyId::of("Orientation", "vaml::Slider")));
}

TEST_CASE("S800 PropertyId: global-`::` owner vs namespaced owner are distinct")
{
    // C++ has a genuine global namespace (C# does not). `::Foo` is a real,
    // valid ownership scope for an app type OUTSIDE any namespace; it is a
    // DIFFERENT scope from `vaml::Foo`.
    SHOULD(ShouldlyString(PropertyId::of("Foo", "::Foo"))
               .ShouldNotBe(PropertyId::of("Foo", "vaml::Foo")));
    SHOULD(ShouldlyString(std::string(PropertyId::of("Foo", "::Foo").owner()))
               .ShouldBe(std::string("::Foo")));
    // Same owner scope -> same name is the same id.
    SHOULD(ShouldlyString(PropertyId::of("Foo", "::Foo"))
               .ShouldBe(PropertyId::of("Foo", "::Foo")));
}

TEST_CASE("S800 PropertyId: pair-keyed std::hash — equal pairs equal, diff owners differ")
{
    const std::hash<PropertyId> h;
    // Equal pairs hash equal.
    SHOULD(ShouldlyString(h(PropertyId::of("Orientation", "vaml::StackPanel")))
               .ShouldBe(h(PropertyId::of("Orientation", "vaml::StackPanel"))));
    // Same name, different owner -> (over a sample) DIFFERENT hash: the
    // pair-keyed hash distinguishes the composite ids.
    SHOULD(ShouldlyString(h(PropertyId::of("Orientation", "vaml::Slider")))
               .ShouldNotBe(h(PropertyId::of("Orientation", "vaml::StackPanel"))));
    // Global-`::` vs namespaced owner hash to different values (sample).
    SHOULD(ShouldlyString(h(PropertyId::of("Foo", "::Foo")))
               .ShouldNotBe(h(PropertyId::of("Foo", "vaml::Foo"))));
    // Legacy pair (owner "") still hashes consistently with its equality.
    SHOULD(ShouldlyString(h(PropertyId::of("Width"))).ShouldBe(h(PropertyId::of("Width"))));
    SHOULD(ShouldlyString(h(PropertyId::of("Width"))).ShouldNotBe(h(PropertyId::of("Height"))));
}

TEST_CASE("S800 PropertyId: legacy bridge (owner \"\") — no behavior change regression")
{
    // The S033 regression: every existing runtime consumer constructs ids via
    // the bare of(name) bridge with owner "". Strict pair equality among them
    // reduces to today's name-only behavior — NO live semantic change.
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldBe(PropertyId::of("Width")));
    SHOULD(ShouldlyString(PropertyId::of("Width")).ShouldNotBe(PropertyId::of("Height")));
    // A composite (owner-carrying) id is a DISTINCT identity from the same-name
    // legacy id — same name is NOT identity across different owner scopes, and
    // the legacy (owner "") form is the deprecated path slated for removal.
    SHOULD(ShouldlyString(PropertyId::of("Width"))
               .ShouldNotBe(PropertyId::of("Width", "vaml::Button")));
}

