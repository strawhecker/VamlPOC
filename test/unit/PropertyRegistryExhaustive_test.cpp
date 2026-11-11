// S033 + S801 — exhaustive PropertyRegistry suite (register / lookup /
// duplicate-throws), rewritten S801 for COEXISTENCE semantics. The registry is
// (name, owner) COMPOSITE-keyed (WPF `FromNameKey(name, ownerType)` ground
// truth): a same-name/different-owner pair registers as DISTINCT properties;
// a same-pair re-register throws PropertyAlreadyRegistered and leaves the store
// unchanged. This TU is the exhaustive sweep: bulk register/lookup, all
// same-pair rejection forms leaving the registry unchanged, same-name/
// different-owner coexistence, missing-lookup on empty + non-empty registries,
// and move semantics.

// S036_4: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString + ShouldThrow). The lone REQUIRE (a
// not-null identity gate before deref) is preserved as-is: REQUIRE aborts the
// case when the pointer is null, guarding the subsequent deref — the correct
// severity for a precondition.

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

TEST_CASE("S801 PropertyRegistry: bulk register across types/flags/owners then lookup each")
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

    // Every (name, owner) pair -> exact stored key (all fields intact).
    for (const auto& k : keys)
    {
        const PropertyKey& stored = reg.find(k.name(), k.ownerTypeId());
        SHOULD(ShouldlyString(stored).ShouldBe(k));
        SHOULD(ShouldlyString(reg.try_find(k.name(), k.ownerTypeId())).ShouldBe(&stored));
    }

    // Sizes stay monotonic; a rejected SAME-PAIR duplicate does not grow.
    const std::size_t before = reg.size();
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width")); }));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(before));
}

TEST_CASE("S801 PropertyRegistry: same-name/different-owner COEXISTENCE")
{
    PropertyRegistry reg;

    // The S801 central contract: the SAME name registered under TWO different
    // owners is TWO DISTINCT properties (WPF `(name, ownerType)` identity).
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Panel"));
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Window"));

    SHOULD(ShouldlyString(reg.size()).ShouldBe(3));

    // Both pair-lookups resolve DISTINCTLY (each to its own stored key).
    const PropertyKey* b = reg.try_find("Width", "Button");
    const PropertyKey* p = reg.try_find("Width", "Panel");
    const PropertyKey* w = reg.try_find("Width", "Window");
    REQUIRE(b != nullptr);
    REQUIRE(p != nullptr);
    REQUIRE(w != nullptr);
    SHOULD(ShouldlyString(b != p).ShouldBe(true));
    SHOULD(ShouldlyString(p != w).ShouldBe(true));
    SHOULD(ShouldlyString(b != w).ShouldBe(true));

    // The composite ids resolve distinctly too (pair-keyed).
    const PropertyId idB = reg.id_of("Width", "Button");
    const PropertyId idP = reg.id_of("Width", "Panel");
    const PropertyId idW = reg.id_of("Width", "Window");
    SHOULD(ShouldlyString(idB != idP).ShouldBe(true));
    SHOULD(ShouldlyString(idP != idW).ShouldBe(true));
    SHOULD(ShouldlyString(idB != idW).ShouldBe(true));

    // Coexistence does not make re-register of an EXISTING pair legal.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Button")); }));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(3)); // store unchanged
}

TEST_CASE("S801 PropertyRegistry: duplicate-throws — every same-pair rejection form leaves registry unchanged")
{
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button",
                                    PropertyFlags::None));

    // All rejection forms carry the SAME (name, owner) pair; each must throw
    // and leave the registry byte-identical. type/flags are NOT part of the key
    // identity (they follow the owner-defined key) — so a same-pair re-register
    // with different type/flags is STILL a conflict (WPF `(name, ownerType)`
    // identity).
    const std::size_t sizeBefore = reg.size();
    const PropertyId idBefore = reg.id_of("Width", "Button");
    const PropertyKey* ptrBefore = reg.try_find("Width", "Button");

    // (a) same pair, different type
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Double, "Button")); }));
    // (b) same pair (same owner), same type — exact duplicate key object
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Button")); }));
    // (c) same pair, different flags
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)reg.register_key(make_key("Width", ValueType::Int32, "Button", PropertyFlags::Inherits)); }));

    // Registry unchanged: size, stored pointer identity, and id all intact.
    SHOULD(ShouldlyString(reg.size()).ShouldBe(sizeBefore));
    SHOULD(ShouldlyString(reg.try_find("Width", "Button")).ShouldBe(ptrBefore));
    SHOULD(ShouldlyString(reg.id_of("Width", "Button")).ShouldBe(idBefore));
    SHOULD(ShouldlyString(reg.find("Width", "Button").type()).ShouldBe(ValueType::Int32)); // original type survived
}

TEST_CASE("S801 PropertyRegistry: missing-lookup — empty and non-empty")
{
    // Empty registry.
    PropertyRegistry empty;
    SHOULD(ShouldlyString(empty.empty()).ShouldBe(true));
    REQUIRE(empty.try_find("Width", "Button") == nullptr);  // precondition guard (never throws)
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)empty.find("Width", "Button"); }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)empty.id_of("Width", "Button"); }));
    SHOULD(ShouldlyString(empty.try_id("Width", "Button").valid()).ShouldNotBe(true));

    // Non-empty registry, missing pair.
    PropertyRegistry reg;
    (void)reg.register_key(make_key("Width", ValueType::Int32, "Button"));
    REQUIRE(reg.try_find("Missing", "Button") == nullptr);
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)reg.find("Missing", "Button"); }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)reg.id_of("Missing", "Button"); }));
    SHOULD(ShouldlyString(reg.try_id("Missing", "Button").valid()).ShouldNotBe(true));

    // A registered NAME under a DIFFERENT owner is a distinct lookup (absent).
    REQUIRE(reg.try_find("Width", "Panel") == nullptr); // same name, other owner -> absent

    // The present pair still resolves (missing-lookup never corrupts).
    SHOULD(ShouldlyString(reg.try_find("Width", "Button") != nullptr).ShouldBe(true));
}

TEST_CASE("S801 PropertyRegistry: move semantics preserve mappings and dedupe")
{
    // Seed a source registry.
    PropertyRegistry src;
    (void)src.register_key(make_key("Width", ValueType::Int32, "Button"));
    (void)src.register_key(make_key("Width", ValueType::Int32, "Panel"));
    (void)src.register_key(make_key("Margin", ValueType::Thickness, "Control"));

    // Move-construct.
    PropertyRegistry dst{std::move(src)};
    SHOULD(ShouldlyString(dst.size()).ShouldBe(3));
    SHOULD(ShouldlyString(dst.find("Width", "Button").type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(dst.find("Width", "Panel").type()).ShouldBe(ValueType::Int32));
    SHOULD(ShouldlyString(dst.find("Margin", "Control").type()).ShouldBe(ValueType::Thickness));
    SHOULD(ShouldlyString(dst.try_id("Width", "Button")).ShouldBe(PropertyId::of("Width", "Button")));

    // Dedupe behaviour survives the move: same-pair throws, coexistence holds.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)dst.register_key(make_key("Width", ValueType::Int32, "Button")); }));
    (void)dst.register_key(make_key("Width", ValueType::Int32, "Window")); // new owner, coexists
    SHOULD(ShouldlyString(dst.size()).ShouldBe(4));

    // Move-assign onto a fresh registry.
    PropertyRegistry other;
    (void)other.register_key(make_key("Temp", ValueType::Bool, "X"));
    other = std::move(dst);
    SHOULD(ShouldlyString(other.size()).ShouldBe(4));
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] { (void)other.register_key(make_key("Margin", ValueType::Thickness, "Control")); }));
    SHOULD(ShouldlyString(other.try_find("Temp", "X")).ShouldBe(nullptr)); // old content gone
}
