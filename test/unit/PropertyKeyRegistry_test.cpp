// S032 — PropertyKey + PropertyRegistry + PropertyFlags tests.
// Scope: PropertyKey carries the exact (name, type, owner-type id, flags)
// tuple; equality distinguishes on each field; PropertyFlags OR/has_flag
// combine independent bits; PropertyRegistry maps name->key, lookup works, and
// register_key throws PropertyAlreadyRegistered on a name collision (dedupe)
// leaving the registry unchanged. The exhaustive register/lookup/dedupe-throws
// suite + constexpr-safe static id is S033 — this TU is the build-verifying
// smoke surface for the real S032 types.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

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
    CHECK(has_flag(combo, PropertyFlags::Inherits));
    CHECK(has_flag(combo, PropertyFlags::ReadOnly));
    CHECK_FALSE(has_flag(combo, PropertyFlags::BindsTwoWayByDefault));

    // has_flag does not require absent unrelated bits.
    CHECK(has_flag(combo, PropertyFlags::Inherits));
    CHECK(has_flag(combo, PropertyFlags::ReadOnly));

    // None tests false for every bit.
    CHECK_FALSE(has_flag(PropertyFlags::None, PropertyFlags::Inherits));
    CHECK_FALSE(has_flag(PropertyFlags::None, PropertyFlags::BindsTwoWayByDefault));

    // Three-way OR still composes.
    const PropertyFlags all3 = PropertyFlags::Inherits | PropertyFlags::ReadOnly
                             | PropertyFlags::BindsTwoWayByDefault;
    CHECK(has_flag(all3, PropertyFlags::Inherits));
    CHECK(has_flag(all3, PropertyFlags::ReadOnly));
    CHECK(has_flag(all3, PropertyFlags::BindsTwoWayByDefault));
}

TEST_CASE("S032 PropertyKey: constructor + getters return the exact tuple")
{
    const PropertyKey key = make_key("Width", ValueType::Int32, "Button",
                                     PropertyFlags::Inherits | PropertyFlags::ReadOnly);

    CHECK(key.name() == "Width");
    CHECK(key.type() == ValueType::Int32);
    CHECK(key.ownerTypeId() == "Button");
    CHECK(has_flag(key.flags(), PropertyFlags::Inherits));
    CHECK(has_flag(key.flags(), PropertyFlags::ReadOnly));
    CHECK_FALSE(has_flag(key.flags(), PropertyFlags::BindsTwoWayByDefault));
}

TEST_CASE("S032 PropertyKey: equality distinguishes on every field")
{
    const PropertyKey base = make_key("Width", ValueType::Int32, "Button",
                                      PropertyFlags::Inherits);

    // Identical -> equal.
    CHECK(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits) == base);
    // Each single-field difference breaks equality.
    CHECK(make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits) != base); // name
    CHECK(make_key("Width", ValueType::Double, "Button", PropertyFlags::Inherits) != base); // type
    CHECK(make_key("Width", ValueType::Int32, "Panel", PropertyFlags::Inherits) != base);   // owner
    CHECK(make_key("Width", ValueType::Int32, "Button", PropertyFlags::None) != base);      // flags

    // operator!= mirrors operator==.
    CHECK(make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits) != make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits));
    CHECK_FALSE(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits) == make_key("Height", ValueType::Int32, "Button", PropertyFlags::Inherits));
}

TEST_CASE("S032 PropertyKey: different ValueType tags from S030 are usable")
{
    // Smoke: Int32/Thickness/String all construct fine as a property type.
    const PropertyKey intKey  = make_key("Number", ValueType::Int32, "Control");
    const PropertyKey thick   = make_key("Margin", ValueType::Thickness, "Control");
    const PropertyKey str     = make_key("Title", ValueType::String, "Control");

    CHECK(intKey.type() == ValueType::Int32);
    CHECK(thick.type() == ValueType::Thickness);
    CHECK(str.type() == ValueType::String);
}

TEST_CASE("S032 PropertyKey: describe() renders a human-readable line")
{
    const PropertyKey key = make_key("Width", ValueType::Int32, "Button",
                                     PropertyFlags::Inherits | PropertyFlags::ReadOnly);
    const std::string d = key.describe();
    CHECK(d.find("Width") != std::string::npos);
    CHECK(d.find("Int32") != std::string::npos);
    CHECK(d.find("Button") != std::string::npos);
    CHECK(d.find("Inherits") != std::string::npos);
    CHECK(d.find("ReadOnly") != std::string::npos);
}

TEST_CASE("S032 PropertyRegistry: register then find/try_find/size")
{
    PropertyRegistry reg;
    CHECK(reg.empty());

    const PropertyKey& stored = reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    CHECK(reg.size() == 1);
    CHECK_FALSE(reg.empty());

    // register_key returns a live reference to the stored key.
    CHECK(stored.name() == "Width");

    // try_find finds by name; find throws nothing.
    const PropertyKey* p = reg.try_find("Width");
    REQUIRE(p != nullptr);
    CHECK(p->ownerTypeId() == "Button");
    CHECK(reg.find("Width").type() == ValueType::Int32);

    // Missing names: try_find -> nullptr, find -> throws PropertyNotFound.
    CHECK(reg.try_find("Missing") == nullptr);
    CHECK_THROWS_AS((void)reg.find("Missing"), PropertyNotFound);
}

TEST_CASE("S032 PropertyRegistry: dedupe — duplicate name throws and leaves map unchanged")
{
    PropertyRegistry reg;
    const PropertyKey& first = reg.register_key(make_key("Width", ValueType::Int32, "Button"));

    // Same name, even with a different owner/type/flags -> rejection on NAME.
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width", ValueType::Double, "Panel")),
                    PropertyAlreadyRegistered);

    // Registry unchanged: size stable and the original key still findable.
    CHECK(reg.size() == 1);
    CHECK(reg.try_find("Width") == &first);
    CHECK(reg.find("Width").type() == ValueType::Int32);
}

TEST_CASE("S032 PropertyRegistry: multiple distinct names coexist")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)reg.register_key(make_key("Height", ValueType::Int32, "Button"));
    (void)reg.register_key(make_key("Margin", ValueType::Thickness, "Control"));

    CHECK(reg.size() == 3);
    CHECK(reg.find("Width").ownerTypeId() == "Button");
    CHECK(reg.find("Margin").type() == ValueType::Thickness);
}
