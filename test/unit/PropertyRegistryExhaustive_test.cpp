// S033 — exhaustive PropertyRegistry suite (register / lookup / duplicate-throws).
// Complementary to S032's PropertyKeyRegistry_test.cpp (left untouched). This TU
// is the *exhaustive* sweep S032's roadmap line deferred: bulk register/lookup,
// all duplicate-throws rejection forms leaving the registry unchanged, missing-
// lookup on empty + non-empty registries, and move semantics.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

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
        CHECK_NOTHROW((void)reg.register_key(k));
    }
    CHECK(reg.size() == keys.size());
    CHECK_FALSE(reg.empty());

    // Every name -> exact stored key (all fields intact).
    for (const auto& k : keys)
    {
        const PropertyKey& stored = reg.find(k.name());
        CHECK(stored == k);
        CHECK(reg.try_find(k.name()) == &stored);
    }

    // Sizes stay monotonic; a rejected duplicate does not grow.
    const std::size_t before = reg.size();
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width")), PropertyAlreadyRegistered);
    CHECK(reg.size() == before);
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
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width", ValueType::Double, "Button")),
                    PropertyAlreadyRegistered);
    // (b) same name, different owner
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width", ValueType::Int32, "Panel")),
                    PropertyAlreadyRegistered);
    // (c) same name, different flags
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width", ValueType::Int32, "Button",
                                                    PropertyFlags::Inherits)),
                    PropertyAlreadyRegistered);
    // (d) same name, byte-identical key object
    CHECK_THROWS_AS((void)reg.register_key(make_key("Width", ValueType::Int32, "Button",
                                                    PropertyFlags::None)),
                    PropertyAlreadyRegistered);

    // Registry unchanged: size, stored pointer identity, and id all intact.
    CHECK(reg.size() == sizeBefore);
    CHECK(reg.try_find("Width") == ptrBefore);
    CHECK(reg.id_of("Width") == idBefore);
    CHECK(reg.find("Width").type() == ValueType::Int32); // original type survived
}

TEST_CASE("S033 PropertyRegistry: missing-lookup — empty and non-empty")
{
    // Empty registry.
    PropertyRegistry empty;
    CHECK(empty.empty());
    REQUIRE(empty.try_find("Width") == nullptr);  // never throws
    CHECK_THROWS_AS((void)empty.find("Width"), PropertyNotFound);
    CHECK_THROWS_AS((void)empty.id_of("Width"), PropertyNotFound);
    CHECK_FALSE(empty.try_id("Width").valid());

    // Non-empty registry, missing name.
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    REQUIRE(reg.try_find("Missing") == nullptr);
    CHECK_THROWS_AS((void)reg.find("Missing"), PropertyNotFound);
    CHECK_THROWS_AS((void)reg.id_of("Missing"), PropertyNotFound);
    CHECK_FALSE(reg.try_id("Missing").valid());

    // The present name still resolves (missing-lookup never corrupts).
    CHECK(reg.try_find("Width") != nullptr);
}

TEST_CASE("S033 PropertyRegistry: move semantics preserve mappings and dedupe")
{
    // Seed a source registry.
    PropertyRegistry src;
    (void)src.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)src.register_key(make_key("Margin", ValueType::Thickness, "Control"));

    // Move-construct.
    PropertyRegistry dst{std::move(src)};
    CHECK(dst.size() == 2);
    CHECK(dst.find("Width").type() == ValueType::Int32);
    CHECK(dst.find("Margin").type() == ValueType::Thickness);
    CHECK(dst.try_id("Width") == PropertyId::of("Width"));

    // Dedupe behaviour survives the move.
    CHECK_THROWS_AS((void)dst.register_key(make_key("Width")), PropertyAlreadyRegistered);

    // Move-assign onto a fresh registry.
    PropertyRegistry other;
    (void)other.register_key(make_key("Temp", ValueType::Bool, "X"));
    other = std::move(dst);
    CHECK(other.size() == 2);
    CHECK_THROWS_AS((void)other.register_key(make_key("Margin")), PropertyAlreadyRegistered);
    CHECK(other.try_find("Temp") == nullptr); // old content gone
}
