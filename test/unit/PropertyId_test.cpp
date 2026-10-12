// S033 — PropertyId tests: constexpr-safe static id.
// Scope: PropertyId::of("Width") is a compile-time constant (proven by
// static_assert below); it wraps the property name as a string_view; equality
// is by name; default-id is invalid; std::hash specializes and is consistent
// with equality; and it interconverts with the registry's id_of/try_id.

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
