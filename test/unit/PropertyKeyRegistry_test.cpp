// S032 — PropertyKey + PropertyRegistry + PropertyFlags tests.
// Scope: PropertyKey carries the exact (name, type, owner-type id, flags)
// tuple; equality distinguishes on each field; PropertyFlags OR/has_flag
// combine independent bits; PropertyRegistry maps name->key, lookup works, and
// register_key throws PropertyAlreadyRegistered on a name collision (dedupe)
// leaving the registry unchanged. The exhaustive register/lookup/dedupe-throws
// suite + constexpr-safe static id is S033 — this TU is the build-verifying
// smoke surface for the real S032 types.

// S036_4: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString + ShouldThrow). The lone REQUIRE (a
// not-null identity gate before deref) is preserved: it is a precondition
// guard, and aborting the case on a null is the correct severity.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>

using vaml::has_flag;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
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

TEST_CASE("S032 PropertyFlags: OR combines independent bits; has_flag is per-bit")
{
    const PropertyFlags combo = PropertyFlags::Inherits | PropertyFlags::ReadOnly;

    // OR result carries both bits.
    SHOULD(ShouldlyString(has_flag(combo, PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(combo, PropertyFlags::ReadOnly)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(combo, PropertyFlags::BindsTwoWayByDefault)).ShouldNotBe(true));

    // has_flag does not require absent unrelated bits.
    SHOULD(ShouldlyString(has_flag(combo, PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(combo, PropertyFlags::ReadOnly)).ShouldBe(true));

    // None tests false for every bit.
    SHOULD(ShouldlyString(has_flag(PropertyFlags::None, PropertyFlags::Inherits)).ShouldNotBe(true));
    SHOULD(ShouldlyString(has_flag(PropertyFlags::None, PropertyFlags::BindsTwoWayByDefault)).ShouldNotBe(true));

    // Three-way OR still composes.
    const PropertyFlags all3 = PropertyFlags::Inherits | PropertyFlags::ReadOnly
                             | PropertyFlags::BindsTwoWayByDefault;
    SHOULD(ShouldlyString(has_flag(all3, PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(all3, PropertyFlags::ReadOnly)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(all3, PropertyFlags::BindsTwoWayByDefault)).ShouldBe(true));
}

TEST_CASE("S032 PropertyKey: constructor + getters return the exact tuple")
{
    const PropertyKey key = make_key("Width", ValueType::Int32, "Button",
                                     PropertyFlags::Inherits | PropertyFlags::ReadOnly);

    SHOULD(ShouldlyString(std::string(key.name())).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(key.type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(std::string(key.ownerTypeId())).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(has_flag(key.flags(), PropertyFlags::Inherits)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(key.flags(), PropertyFlags::ReadOnly)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(key.flags(), PropertyFlags::BindsTwoWayByDefault)).ShouldNotBe(true));
}

TEST_CASE("S032 PropertyKey: equality distinguishes on every field")
{
    const PropertyKey base = make_key("Width", ValueType::Int32, "Button",
                                      PropertyFlags::Inherits);

    // Identical -> equal.
    SHOULD(ShouldlyString(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits)).ShouldBe(base));
    // Each single-field difference breaks equality.
    SHOULD(ShouldlyString(make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits)).ShouldNotBe(base)); // name
    SHOULD(ShouldlyString(make_key("Width", ValueType::Double, "Button", PropertyFlags::Inherits)).ShouldNotBe(base)); // type
    SHOULD(ShouldlyString(make_key("Width", ValueType::Int32, "Panel", PropertyFlags::Inherits)).ShouldNotBe(base));   // owner
    SHOULD(ShouldlyString(make_key("Width", ValueType::Int32, "Button", PropertyFlags::None)).ShouldNotBe(base));      // flags

    // operator!= mirrors operator==.
    SHOULD(ShouldlyString(make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits)).ShouldNotBe(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits)));
    SHOULD(ShouldlyString(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits)).ShouldNotBe(make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits)));
}

TEST_CASE("S032 PropertyKey: different ValueType tags from S030 are usable")
{
    // Smoke: Int32/Thickness/String all construct fine as a property type.
    const PropertyKey intKey  = make_key("Number", ValueType::Int32, "Control");
    const PropertyKey thick   = make_key("Margin", ValueType::Thickness, "Control");
    const PropertyKey str     = make_key("Title", ValueType::String, "Control");

    SHOULD(ShouldlyString(intKey.type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(thick.type()).ShouldBe(ValueType::Thickness));
    SHOULD(ShouldlyString(str.type()).ShouldBe(ValueType::String));
}

TEST_CASE("S032 PropertyKey: describe() renders a human-readable line")
{
    const PropertyKey key = make_key("Width", ValueType::Int32, "Button",
                                     PropertyFlags::Inherits | PropertyFlags::ReadOnly);
    const std::string d = key.describe();
    SHOULD(ShouldlyString(d.find("Width") != std::string::npos).ShouldBe(true));
    SHOULD(ShouldlyString(d.find("Int32") != std::string::npos).ShouldBe(true));
    SHOULD(ShouldlyString(d.find("Button") != std::string::npos).ShouldBe(true));
    SHOULD(ShouldlyString(d.find("Inherits") != std::string::npos).ShouldBe(true));
    SHOULD(ShouldlyString(d.find("ReadOnly") != std::string::npos).ShouldBe(true));
}

TEST_CASE("S032 PropertyRegistry: register then find/try_find/size")
{
    PropertyRegistry reg;
    SHOULD(ShouldlyString(reg.empty()).ShouldBe(true));

    const PropertyKey& stored = reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(1));
    SHOULD(ShouldlyString(reg.empty()).ShouldNotBe(true));

    // register_key returns a live reference to the stored key.
    SHOULD(ShouldlyString(std::string(stored.name())).ShouldBe(std::string("Width")));

    // try_find finds by name; find throws nothing.
    const PropertyKey* p = reg.try_find("Width");
    REQUIRE(p != nullptr); // precondition guard before deref
    SHOULD(ShouldlyString(std::string(p->ownerTypeId())).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(reg.find("Width").type()).ShouldBe(ValueType::Int32));

    // Missing names: try_find -> nullptr, find -> throws PropertyNotFound.
    SHOULD(ShouldlyString(reg.try_find("Missing") == nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)reg.find("Missing"); }));
}

TEST_CASE("S032 PropertyRegistry: dedupe — duplicate name throws and leaves map unchanged")
{
    PropertyRegistry reg;
    const PropertyKey& first = reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // Same name, even with a different owner/type/flags -> rejection on NAME.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Double, "Panel")); }));

    // Registry unchanged: size stable and the original key still findable.
    SHOULD(ShouldlyString(reg.size()).ShouldBe(1));
    SHOULD(ShouldlyString(reg.try_find("Width")).ShouldBe(&first));
    SHOULD(ShouldlyString(reg.find("Width").type()).ShouldBe(ValueType::Int32));
}

TEST_CASE("S032 PropertyRegistry: multiple distinct names coexist")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)reg.register_key(make_key("Height", ValueType::Int32, "Button"));
    (void)reg.register_key(make_key("Margin", ValueType::Thickness, "Control"));

    SHOULD(ShouldlyString(reg.size()).ShouldBe(3));
    SHOULD(ShouldlyString(std::string(reg.find("Width").ownerTypeId())).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(reg.find("Margin").type()).ShouldBe(ValueType::Thickness));
}
