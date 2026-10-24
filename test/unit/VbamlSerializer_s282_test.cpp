// S282 (dep:S281) — Serializer round-trip test.
//
// The serializer writes a sample interpreted tree (a CompiledProgram via the
// S281 schema) to bytes and decodes it back EQUAL (BYTE-IDENTICAL). These tests
// exercise the REAL vaml-tools serializer (encode/decode) headlessly:
//   * encode(prog, schema) -> bytes
//   * decode(bytes, schema) -> prog'  STRUCTURALLY EQUAL to prog
//   * encode(prog', schema) -> the SAME bytes (byte-identical)
//   * round-trip preserves the token/string tables + element records + binding
//     fields
//   * decode rejects a malformed buffer (bad magic) via VbamlMalformedBuffer
// ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/tools/vbaml/VbamlSchema.h"
#include "vaml/tools/vbaml/VbamlSerializer.h"

#include <string>
#include <vector>

using vaml::tools::AttributeRecord;
using vaml::tools::CompiledProgram;
using vaml::tools::ElementRecord;
using vaml::tools::vbaml::Schema;
using vaml::tools::vbaml::SchemaBuilder;
using vaml::tools::vbaml::VbamlMalformedBuffer;

namespace {

// Builds a small but representative sample tree: two elements with a literal
// attribute, an attached property, and a binding extension.
CompiledProgram sample_program()
{
    CompiledProgram prog;
    ElementRecord sp; // id 0
    sp.id = 0;
    sp.typeToken = "StackPanel";
    AttributeRecord a;
    a.key = "S282Text";
    a.value = "Hi";
    a.knownProperty = true;  // (annotation; not serialized by design)
    a.valueType = "String";
    sp.attributes.push_back(a);
    prog.elements.push_back(sp);

    ElementRecord txt; // id 1
    txt.id = 1;
    txt.typeToken = "Text";
    AttributeRecord b;
    b.key = "S282Text";
    b.value = "{Binding Title}";
    b.isExtension = true;
    b.isBinding = true;
    b.bindingPath = "Title";
    txt.attributes.push_back(b);
    prog.elements.push_back(txt);

    // Parent → child edge: StackPanel has child Text (id 1).
    prog.elements[0].children.push_back(1);
    return prog;
}

Schema schema_for(const CompiledProgram& prog)
{
    SchemaBuilder builder;
    for (const ElementRecord& e : prog.elements)
    {
        (void)builder.type_token(e.typeToken);
        for (const AttributeRecord& a : e.attributes)
        {
            (void)builder.prop_token(a.key);
            (void)builder.intern(a.value);
            if (a.isBinding)
            {
                (void)builder.intern(a.bindingPath);
            }
        }
    }
    return builder.flush();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Round-trip: struct-equal + byte-identical
// ---------------------------------------------------------------------------

TEST_CASE("S282: encode→decode round-trips the sample tree structural-equal + byte-identical")
{
    const CompiledProgram prog = sample_program();
    const Schema schema = schema_for(prog);

    const std::vector<std::uint8_t> bytes = vaml::tools::vbaml::encode(prog, schema);
    SHOULD(ShouldlyIntegral(bytes.empty()).ShouldBe(0));

    const CompiledProgram decoded = vaml::tools::vbaml::decode(bytes, schema);
    SHOULD(ShouldlyIntegral(decoded.elements.size()).ShouldBe(2u));
    // Structural equality: types + attributes + bindings.
    SHOULD(ShouldlyString(decoded.elements[0].typeToken).ShouldBe("StackPanel"));
    SHOULD(ShouldlyString(decoded.elements[0].attributes[0].key).ShouldBe("S282Text"));
    SHOULD(ShouldlyString(decoded.elements[0].attributes[0].value).ShouldBe("Hi"));
    SHOULD(ShouldlyString(decoded.elements[1].typeToken).ShouldBe("Text"));
    SHOULD(ShouldlyIntegral(decoded.elements[1].attributes[0].isBinding ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(decoded.elements[1].attributes[0].bindingPath).ShouldBe("Title"));
    SHOULD(ShouldlyIntegral(decoded.elements[0].children.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(decoded.elements[0].children[0]).ShouldBe(1u));

    // BYTE-IDENTICAL: re-encoding the decoded program yields the same bytes.
    const std::vector<std::uint8_t> bytes2 = vaml::tools::vbaml::encode(decoded, schema);
    SHOULD(ShouldlyIntegral(bytes2.size()).ShouldBe(bytes.size()));
    SHOULD(ShouldlyIntegral(bytes2 == bytes ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. decode_schema round-trips the six-section schema
// ---------------------------------------------------------------------------

TEST_CASE("S282: decode_schema round-trips the token/string tables")
{
    const CompiledProgram prog = sample_program();
    const Schema schema = schema_for(prog);

    const std::vector<std::uint8_t> bytes = vaml::tools::vbaml::encode(prog, schema);
    const Schema decoded = vaml::tools::vbaml::decode_schema(bytes);

    SHOULD(ShouldlyIntegral(decoded.magic).ShouldBe(vaml::tools::vbaml::kVbamlMagic));
    SHOULD(ShouldlyIntegral(decoded.version).ShouldBe(vaml::tools::vbaml::kVbamlVersion));
    SHOULD(ShouldlyIntegral(decoded.strings.size()).ShouldBe(schema.strings.size()));
    SHOULD(ShouldlyIntegral(decoded.types.size()).ShouldBe(schema.types.size()));
    SHOULD(ShouldlyIntegral(decoded.props.size()).ShouldBe(schema.props.size()));
    // The same internment order → identical tables.
    for (std::size_t i = 0; i < schema.strings.size(); ++i)
    {
        SHOULD(ShouldlyString(std::string(decoded.strings[i]))
                   .ShouldBe(std::string(schema.strings[i])));
    }
}

// ---------------------------------------------------------------------------
// 3. A malformed buffer (bad magic) throws VbamlMalformedBuffer
// ---------------------------------------------------------------------------

TEST_CASE("S282: decode rejects a bad-magic buffer via VbamlMalformedBuffer")
{
    std::vector<std::uint8_t> bad = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
    bool threw = false;
    try
    {
        (void)vaml::tools::vbaml::decode_schema(bad);
    }
    catch (const VbamlMalformedBuffer&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
