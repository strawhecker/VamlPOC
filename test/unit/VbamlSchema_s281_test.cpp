// S281 (dep:S280) — Binary record set: the versioned .vbaml schema test.
//
// The schema is the versioned six-section record set (element-id / type-token /
// prop-token / event-token / resource-token / string-table), with the token
// tables enumerated INTO the string table. These tests exercise the REAL
// SchemaBuilder (vaml::tools::vbaml) headlessly via vaml-tools-tests:
//   * intern() dedupes (same string → same index), insertion order stable
//   * type/prop/event/resource token tables enumerate into the string table
//   * flush() yields an immutable Schema snapshot (magic/version/strings/tables)
//   * the same intern order yields the same schema (determinism → byte-identity)
// ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/tools/vbaml/VbamlSchema.h"

#include <string>

using vaml::tools::vbaml::Schema;
using vaml::tools::vbaml::SchemaBuilder;
using vaml::tools::vbaml::kVbamlMagic;
using vaml::tools::vbaml::kVbamlVersion;

// ---------------------------------------------------------------------------
// 1. intern() inserts unique strings in insertion order + dedupes by content
// ---------------------------------------------------------------------------

TEST_CASE("S281: intern dedupes content + preserves insertion order")
{
    SchemaBuilder b;
    const std::uint32_t a = b.intern("StackPanel");
    const std::uint32_t b1 = b.intern("Grid");
    const std::uint32_t a2 = b.intern("StackPanel"); // dup → same index
    const std::uint32_t c = b.intern("Text");

    SHOULD(ShouldlyIntegral(a).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(b1).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(a2).ShouldBe(0u)); // deduped
    SHOULD(ShouldlyIntegral(c).ShouldBe(2u));

    const Schema s = b.flush();
    SHOULD(ShouldlyIntegral(s.strings.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(std::string(s.strings[0])).ShouldBe("StackPanel"));
    SHOULD(ShouldlyString(std::string(s.strings[1])).ShouldBe("Grid"));
    SHOULD(ShouldlyString(std::string(s.strings[2])).ShouldBe("Text"));
}

// ---------------------------------------------------------------------------
// 2. token tables enumerate into the string table (single internment)
// ---------------------------------------------------------------------------

TEST_CASE("S281: type/prop/event/resource token tables enumerate into the string table")
{
    SchemaBuilder b;
    const std::uint32_t typeTok = b.type_token("StackPanel");
    const std::uint32_t propTok = b.prop_token("Orientation");
    const std::uint32_t evTok = b.event_token("Click");
    const std::uint32_t resTok = b.resource_token("Brush");

    const Schema s = b.flush();
    // Each token is a valid index into the shared string table.
    SHOULD(ShouldlyString(std::string(s.strings[typeTok])).ShouldBe("StackPanel"));
    SHOULD(ShouldlyString(std::string(s.strings[propTok])).ShouldBe("Orientation"));
    SHOULD(ShouldlyString(std::string(s.strings[evTok])).ShouldBe("Click"));
    SHOULD(ShouldlyString(std::string(s.strings[resTok])).ShouldBe("Brush"));
    // The semantic lists reference those indexes.
    SHOULD(ShouldlyIntegral(s.types.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(s.types[0]).ShouldBe(typeTok));
    SHOULD(ShouldlyIntegral(s.props.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(s.props[0]).ShouldBe(propTok));
    SHOULD(ShouldlyIntegral(s.events.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(s.events[0]).ShouldBe(evTok));
    SHOULD(ShouldlyIntegral(s.resources.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(s.resources[0]).ShouldBe(resTok));
}

// ---------------------------------------------------------------------------
// 3. flush() snaps an immutable schema with the canonical magic/version
// ---------------------------------------------------------------------------

TEST_CASE("S281: flush() produces a Schema with the canonical magic/version")
{
    SchemaBuilder b;
    (void)b.type_token("StackPanel");
    const Schema s = b.flush();
    SHOULD(ShouldlyIntegral(s.magic).ShouldBe(kVbamlMagic));
    SHOULD(ShouldlyIntegral(s.version).ShouldBe(kVbamlVersion));
    SHOULD(ShouldlyIntegral(static_cast<int>(kVbamlVersion)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. determinism — same intern order → identical schema (byte-identity base)
// ---------------------------------------------------------------------------

TEST_CASE("S281: repeated build yields an identical schema (determinism)")
{
    const auto build = []() {
        SchemaBuilder b;
        (void)b.type_token("StackPanel");
        (void)b.prop_token("Orientation");
        (void)b.intern("Horizontal");
        return b.flush();
    };

    const Schema s1 = build();
    const Schema s2 = build();
    SHOULD(ShouldlyIntegral(s1.strings.size()).ShouldBe(s2.strings.size()));
    for (std::size_t i = 0; i < s1.strings.size(); ++i)
    {
        SHOULD(ShouldlyString(std::string(s1.strings[i])).ShouldBe(std::string(s2.strings[i])));
    }
    SHOULD(ShouldlyIntegral(s1.types.size()).ShouldBe(s2.types.size()));
    SHOULD(ShouldlyIntegral(s1.types[0]).ShouldBe(s2.types[0]));
}
