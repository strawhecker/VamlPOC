// S033 — PropertyId tests: constexpr-safe static id.
// Scope: PropertyId::of("Width") is a compile-time constant (proven by
// static_assert below); it wraps the property name as a string_view; equality
// is by name; default-id is invalid; std::hash specializes and is consistent
// with equality; and it interconverts with the registry's id_of/try_id.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>

using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyRegistry;
using vaml::ValueType;

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
    // The compile-time value and the runtime value agree. (Compare the name
    // via std::string — doctest stringifies operands with ostream, which a raw
    // std::string_view operand cannot satisfy without <ostream>.)
    PropertyId id = PropertyId::of("Width");
    CHECK(std::string(id.name()) == "Width");
    CHECK(id.valid());
}

TEST_CASE("S033 PropertyId: equality is by name; default id is invalid")
{
    CHECK(PropertyId::of("Width") == PropertyId::of("Width"));
    CHECK(PropertyId::of("Width") != PropertyId::of("Height"));
    CHECK_FALSE(PropertyId::of("Width") == PropertyId::of("Height"));
    CHECK_FALSE(PropertyId::of("Width") != PropertyId::of("Width"));

    // Default-constructed id is invalid and unequal to any named id.
    PropertyId def;
    CHECK_FALSE(def.valid());
    CHECK(def.name().empty());
    CHECK(def != PropertyId::of("Width"));
    CHECK(def == PropertyId{}); // two default ids compare equal
}

TEST_CASE("S033 PropertyId: std::hash is consistent with equality")
{
    const std::hash<PropertyId> h;
    // Equal ids hash equal.
    CHECK(h(PropertyId::of("Width")) == h(PropertyId::of("Width")));
    CHECK(h(PropertyId{}) == h(PropertyId{}));
    // Distinct ids hash (over a sample) to distinct values — and at minimum
    // the same-id invariant holds; equal-inputs-always-equal is the contract.
    CHECK(h(PropertyId::of("Width")) != h(PropertyId::of("Height")));
}

TEST_CASE("S033 PropertyId: registry id_of/try_id round the name-keyed store")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // id_of of a registered name equals the compile-time id of that literal.
    CHECK(reg.id_of("Width") == PropertyId::of("Width"));
    CHECK(reg.id_of("Width").valid());

    // try_id agrees.
    CHECK(reg.try_id("Width") == PropertyId::of("Width"));
    CHECK(reg.try_id("Width").valid());
}

TEST_CASE("S033 PropertyId: id_of/try_id on an unregistered name")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // id_of throws PropertyNotFound; try_id never throws and yields an
    // invalid id.
    CHECK_THROWS_AS((void)reg.id_of("Missing"), vaml::PropertyNotFound);
    CHECK_FALSE(reg.try_id("Missing").valid());
    CHECK(reg.try_id("Missing").name().empty());

    // On an empty registry the same holds.
    PropertyRegistry empty;
    CHECK_THROWS_AS((void)empty.id_of("Anything"), vaml::PropertyNotFound);
    CHECK_FALSE(empty.try_id("Anything").valid());
}
