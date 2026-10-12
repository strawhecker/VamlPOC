// S033 — exhaustive PropertyRegistry suite (register / lookup / duplicate-throws).
// Complementary to S032's PropertyKeyRegistry_test.cpp (left untouched). This TU
// is the *exhaustive* sweep S032's roadmap line deferred: bulk register/lookup,
// all duplicate-throws rejection forms leaving the registry unchanged, missing-
// lookup on empty + non-empty registries, and move semantics.

// S036_4: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString + ShouldThrow). The lone REQUIRE (a
// not-null identity gate before deref) is preserved as-is: REQUIRE aborts the
// case when the pointer is null, guarding the subsequent deref — the correct
// severity for a precondition, equivalent to CHECK_NOTHROW -> SHOULD + a
// continue; the null-pointer assert itself is a REQUIRED guard, not an
// assertion we want to continue past. It is the only REQUIRE in the suite.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>
#include <utility>
#include <vector>

using vaml::has_flag;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyNotFound;
using vaml::PropertyRegistry;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

PropertyKey make_key(const std::string& name, ValueType type = ValueType::Int32,
                     const std::string& owner = "Button",
                     PropertyFlags flags = PropertyFlags::None)
{
    return PropertyKey(name, type, owner, flags);
}

} // namespace

TEST_CASE("S033 PropertyRegistry: bulk register across types/flags/owners then lookup each")
{
    PropertyRegistry reg;

    const std::vector<PropertyKey> keys = {
        make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits),
        make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits),
        make_key("Margin", ValueType::Thickness, "Control", PropertyFlags::None),
        make_key("Title", ValueType::String, "Window", PropertyFlags::ReadOnly),
        make_key("IsEnabled", ValueType::Bool, "Control",
                 PropertyFlags::Inherits | PropertyFlags::BindsTwoWayByDefault),
    };

    for (const auto& k : keys)
    {
        (void)reg.register_key(k);
    }
    SHOULD(ShouldlyString(reg.size()).ShouldBe(keys.size()));
    SHOULD(ShouldlyString(reg.empty()).ShouldNotBe(true));

    // Every name -> exact stored key (all fields intact).
    for (const auto& k : keys)
    {
        const PropertyKey& stored = reg.find(k.name());
        SHOULD(ShouldlyString(stored).ShouldBe(k));
        SHOULD(ShouldlyString(reg.try_find(k.name())).ShouldBe(&stored));
    }

    // Sizes stay monotonic; a rejected duplicate does not grow.
    const std::size_t before = reg.size();
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width")); }));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(before));
}

TEST_CASE("S033 PropertyRegistry: duplicate-throws — every rejection form leaves registry unchanged")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button",
                                    PropertyFlags::None));

    // All four rejection forms carry the SAME name; each must throw and leave
    // the registry bit-identical.
    const std::size_t sizeBefore = reg.size();
    const PropertyId idBefore = reg.id_of("Width");
    const PropertyKey* ptrBefore = reg.try_find("Width");

    // (a) same name, different type
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Double, "Button")); }));
    // (b) same name, different owner
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Panel")); }));
    // (c) same name, different flags
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits)); }));
    // (d) same name, byte-identical key object
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Button", PropertyFlags::None)); }));

    // Registry unchanged: size, stored pointer identity, and id all intact.
    SHOULD(ShouldlyString(reg.size()).ShouldBe(sizeBefore));
    SHOULD(ShouldlyString(reg.try_find("Width")).ShouldBe(ptrBefore));
    SHOULD(ShouldlyString(reg.id_of("Width")).ShouldBe(idBefore));
    SHOULD(ShouldlyString(reg.find("Width").type()).ShouldBe(ValueType::Int32)); // original type survived
}

TEST_CASE("S033 PropertyRegistry: missing-lookup — empty and non-empty")
{
    // Empty registry.
    PropertyRegistry empty;
    SHOULD(ShouldlyString(empty.empty()).ShouldBe(true));
    REQUIRE(empty.try_find("Width") == nullptr);  // precondition guard (never throws)
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)empty.find("Width"); }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)empty.id_of("Width"); }));
    SHOULD(ShouldlyString(empty.try_id("Width").valid()).ShouldNotBe(true));

    // Non-empty registry, missing name.
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    REQUIRE(reg.try_find("Missing") == nullptr);
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)reg.find("Missing"); }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)reg.id_of("Missing"); }));
    SHOULD(ShouldlyString(reg.try_id("Missing").valid()).ShouldNotBe(true));

    // The present name still resolves (missing-lookup never corrupts).
    SHOULD(ShouldlyString(reg.try_find("Width") != nullptr).ShouldBe(true));
}

TEST_CASE("S033 PropertyRegistry: move semantics preserve mappings and dedupe")
{
    // Seed a source registry.
    PropertyRegistry src;
    (void)src.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)src.register_key(make_key("Margin", ValueType::Thickness, "Control"));

    // Move-construct.
    PropertyRegistry dst{std::move(src)};
    SHOULD(ShouldlyString(dst.size()).ShouldBe(2));
    SHOULD(ShouldlyString(dst.find("Width").type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(dst.find("Margin").type()).ShouldBe(ValueType::Thickness));
    SHOULD(ShouldlyString(dst.try_id("Width")).ShouldBe(PropertyId::of("Width")));

    // Dedupe behaviour survives the move.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)dst.register_key(make_key("Width")); }));

    // Move-assign onto a fresh registry.
    PropertyRegistry other;
    (void)other.register_key(make_key("Temp", ValueType::Bool, "X"));
    other = std::move(dst);
    SHOULD(ShouldlyString(other.size()).ShouldBe(2));
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)other.register_key(make_key("Margin")); }));
    SHOULD(ShouldlyString(other.try_find("Temp")).ShouldBe(nullptr)); // old content gone
}
