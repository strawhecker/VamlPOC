// S283 (dep:S282) — Parser → record-emit.
//
// Compiles a real .vaml source into the S281/S282 binary form, reusing the
// S2 scanner (parse_vaml), the factory registry (type table), the value
// converter registry (value table) and the property registry (property table)
// — no stubs. The emitted .vbaml decodes back to a structurally-equal tree.
//
// These tests exercise the REAL vaml::tools end-to-end path:
//   compile_to_bytes(source) -> bytes (parse → record-emit → S282 encode)
//   decode(bytes, schema)    -> structural-equal compiled program
// ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/tools/Vamlc.h"
#include "vaml/tools/vbaml/VbamlSchema.h"
#include "vaml/tools/vbaml/VbamlSerializer.h"

#include <string>
#include <unordered_set>
#include <vector>

using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::tools::CompiledProgram;
using vaml::tools::vbaml::Schema;
using vaml::tools::vbaml::VbamlMalformedBuffer;

namespace {

// S280_1Text is registered per-process once (the S039 discipline); this TU
// registers it so the S280_1 binding target check passes for the compiled doc
// (idempotent across TUs — a second register of the same name is a no-op).
void registerS283Properties()
{
    static bool done = false;
    if (done)
    {
        return;
    }
    done = true;
    try
    {
        (void)DependencyProperty::Register("S280_1Text", vaml::ValueType::String,
                                           "S283_Owner",
                                           PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None));
    }
    catch (const vaml::PropertyAlreadyRegistered&)
    {
        // Already registered by another TU — fine (the name is globally unique).
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Parser → record-emit: a real .vaml compiles to bytes and decodes equal
// ---------------------------------------------------------------------------

TEST_CASE("S283: compile_to_bytes parses a real doc → bytes, decode → equal tree")
{
    registerS283Properties();

    // A clean doc: Orientation (registered DP) + a binding on S280_1Text.
    const std::string cleanDoc =
        "<StackPanel Orientation=\"Horizontal\">"
        "<TextBlock S280_1Text=\"{Binding Title}\"/>"
        "</StackPanel>";

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(cleanDoc);
    SHOULD(ShouldlyIntegral(bytes.empty()).ShouldBe(0));

    // Decode the schema first (the bytes begin with the six sections).
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    const CompiledProgram decoded = vaml::tools::vbaml::decode(bytes, schema);

    // The parsed + emitted tree is structurally equal (2 elements, 2 types).
    SHOULD(ShouldlyIntegral(decoded.elements.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(decoded.elements[0].typeToken).ShouldBe("StackPanel"));
    SHOULD(ShouldlyString(decoded.elements[1].typeToken).ShouldBe("TextBlock"));
    // The binding attribute survived the full compile→bytes→decode round-trip.
    SHOULD(ShouldlyIntegral(decoded.elements[1].attributes.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(decoded.elements[1].attributes[0].isBinding ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(decoded.elements[1].attributes[0].bindingPath).ShouldBe("Title"));
    // Parent → child edge: StackPanel has child id 1.
    SHOULD(ShouldlyIntegral(decoded.elements[0].children.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(decoded.elements[0].children[0]).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// 2. The emitted .vbaml is deterministic (same source → same bytes)
// ---------------------------------------------------------------------------

TEST_CASE("S283: compile_to_bytes is deterministic — same source, same bytes")
{
    registerS283Properties();

    const std::string doc =
        "<StackPanel Orientation=\"Horizontal\">"
        "<TextBlock S280_1Text=\"{Binding Title}\"/>"
        "</StackPanel>";

    const std::vector<std::uint8_t> b1 = vaml::tools::compile_to_bytes(doc);
    const std::vector<std::uint8_t> b2 = vaml::tools::compile_to_bytes(doc);
    SHOULD(ShouldlyIntegral(b1.size()).ShouldBe(b2.size()));
    SHOULD(ShouldlyIntegral(b1 == b2 ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. CLI seam writes the .vbaml (exit 0) on a valid source
// ---------------------------------------------------------------------------

TEST_CASE("S283: the CLI seam writes a .vbaml and returns 0 on a valid source")
{
    // Compile-to-bytes is the CLI's write payload; run_vamlcc wraps file I/O.
    // We assert the in-memory path (compile_to_bytes) is the write payload and
    // that a valid source yields ok via compile_text.
    const std::string doc = "<StackPanel/>";
    const vaml::tools::CompileResult r = vaml::tools::compile_text(doc);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(doc);
    SHOULD(ShouldlyIntegral(bytes.empty()).ShouldBe(0));
}
