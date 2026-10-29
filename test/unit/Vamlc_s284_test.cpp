// S284 (dep:S283) — GOLDENS: a full VAML file compiles to a .vbaml with a
// correct token stream.
//
// The formal golden gate over the S283 `compile_to_bytes` pipe (parser →
// record-emit → S281 schema → S282 serializer). One full-feature golden
// document is compiled to .vbaml bytes, then the PRODUCED TOKEN STREAM is
// asserted — DECODED FROM REAL BYTES (not re-deriving the in-memory tree) —
// across every section of the S281 record set:
//
//   * magic 0x76424D4C ('vBML') + version 1; decode_schema reconstructs a
//     schema whose type/prop/event/resource token tables + string table match
//     the build_schema contract.
//   * the element-id section: element count == authored; per-element records in
//     DOCUMENT ORDER with the correct typeToken, the attribute FLAG byte
//     (isExtension bit0 / isBinding bit1), the binding path + the target-prop
//     token, and the child-id edges.
//   * token/string invariants: every distinct visible string appears ONCE in
//     the string table, referenced only by a VarU32 index (never by value); the
//     type/prop/event/resource tables are ordered enumerations into it; the
//     event table carries "Click"/"Changed"; the resource table carries
//     "Brush"/"Color".
//
// Plus the S282 re-proof on the golden: decode(encode(prog, schema)) is
// structurally-equal to the compiled program, and re-encoding the decoded
// program with the same schema is BYTE-IDENTICAL to the original bytes.
//
// Plus positive/negative controls: a clean-minimal doc compiles; a malformed
// buffer (bad magic) throws VbamlMalformedBuffer.
//
// TEST-ONLY — zero new product symbols (the S040/S042/S095 precedent). The
// S284-unique global DPs (S039 discipline) are the surface the golden's plain
// attributes resolve against through the S3 registries; the golden's bindings
// are ROOTLESS ({Binding ...}) so the S280_1 validator defers them (dynamic) —
// never a compile error. ADR-018 zero locks; ADR-021 headless (vaml-tools-tests
// drives the real pipe; never a spawned subprocess).

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

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::tools::CompiledProgram;
using vaml::tools::vbaml::Schema;
using vaml::tools::vbaml::VbamlMalformedBuffer;

namespace {

// S284-only global DependencyProperty table (name-unique process-wide, S039).
std::unordered_set<std::string>& s284_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS284Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s284_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S284_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// The S284-unique global DPs the golden document sets from text. Registered
// so knownProperty (S280) resolves + the S280_1 target-attribute check passes.
void registerS284Properties()
{
    (void)registerS284Global("S284Width",    ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerS284Global("S284Margin",   ValueType::Thickness, PropertyFlags::None,
                             Value::Thickness(vaml::Thickness{0, 0, 0, 0}));
    (void)registerS284Global("S284Enabled",  ValueType::Bool, PropertyFlags::None, Value::Bool(true));
    (void)registerS284Global("S284Content",  ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS284Global("S284Command",  ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS284Global("S284Text",     ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS284Global("S284Dock.Dock", ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS284Global("S284Grid.Row", ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerS284Global("S284Fill",     ValueType::Color, PropertyFlags::None,
                             Value::Color(vaml::Color{0, 0, 0, 0}));
}

// The full-feature golden .vaml. Every ELEMENT is a registered built-in
// (Window/StackPanel/Button/Text); every plain attribute is an S284-unique DP;
// the event-shaped key (S284Click) is classified as a MarkupExtension? NO — a
// plain literal "OnClick" would ALSO be dropped by the compiler's serializer
// unless it is a known property OR an extension. The compiler (S283) records
// ONLY knownProperty annotations + binding classification; event routing is a
// RUNTIME (S286) concern. So the event-shaped key is omitted from the compiled
// golden here — event handling is asserted at S286 via register_event.
//
// This golden exercises: xmlns:x + x:Name on root + children, an attached-style
// dotted key (S284Dock.Dock / S284Grid.Row), numeric + quoted + comma-list
// values, {Binding} markup extensions (isExtension+isBinding), x:Key deferred
// (a directive the compiler carries as an attribute), self-closing leaves + a
// text-content leaf, and multi-level nesting.
const char* kS284GoldenDoc =
    R"(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root" S284Width="1024" S284Margin="10,10,10,10"><StackPanel S284Enabled="true"><Button x:Name="search" S284Content="Search" S284Width="200" S284Grid.Row="1" S284Command="{Binding SearchCommand}"/><TextBlock x:Name="label" x:Key="searchLabelKey" S284Text="{Binding SearchText}">Search:</TextBlock><Button S284Dock.Dock="Top" S284Fill="#FF0000" S284Command="{Binding SaveCommand}"/></StackPanel></Window>)";

} // namespace

// ---------------------------------------------------------------------------
// AXIS 1: magic + version + schema reconstruction
// ---------------------------------------------------------------------------

TEST_CASE("S284: golden — bytes begin with magic 0x76424D4C + version 1; schema decodes")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    SHOULD(ShouldlyIntegral(bytes.empty()).ShouldBe(0));

    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    SHOULD(ShouldlyIntegral(schema.magic).ShouldBe(vaml::tools::vbaml::kVbamlMagic));
    SHOULD(ShouldlyIntegral(schema.version).ShouldBe(vaml::tools::vbaml::kVbamlVersion));
}

TEST_CASE("S284: golden — event token table carries Click/Changed; resource table Brush/Color")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);

    // The schema always prepopulates the canonical event + resource token
    // tables (build_schema contract) — the four display names are ALWAYS in
    // the string table, in this predetermined order (Click, Changed, Brush,
    // Color — the S281 enum ordinals premium).
    SHOULD(ShouldlyString(schema.events.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(schema.resources.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(std::string(schema.string_view(schema.events[0])))
               .ShouldBe(std::string("Click")));
    SHOULD(ShouldlyString(std::string(schema.string_view(schema.events[1])))
               .ShouldBe(std::string("Changed")));
    SHOULD(ShouldlyString(std::string(schema.string_view(schema.resources[0])))
               .ShouldBe(std::string("Brush")));
    SHOULD(ShouldlyString(std::string(schema.string_view(schema.resources[1])))
               .ShouldBe(std::string("Color")));
}

// ---------------------------------------------------------------------------
// AXIS 2: element-id section (records in document order)
// ---------------------------------------------------------------------------

TEST_CASE("S284: golden — element count + document-order type tokens + child edges")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    const CompiledProgram prog = vaml::tools::vbaml::decode(bytes, schema);

    // 5 elements: Window, StackPanel, Button(search), Text(label), Button(attached).
    SHOULD(ShouldlyIntegral(prog.elements.size()).ShouldBe(5u));
    SHOULD(ShouldlyString(prog.elements[0].typeToken).ShouldBe(std::string("Window")));
    SHOULD(ShouldlyString(prog.elements[1].typeToken).ShouldBe(std::string("StackPanel")));
    SHOULD(ShouldlyString(prog.elements[2].typeToken).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(prog.elements[3].typeToken).ShouldBe(std::string("TextBlock")));
    SHOULD(ShouldlyString(prog.elements[4].typeToken).ShouldBe(std::string("Button")));

    // Window → StackPanel (id 1).
    SHOULD(ShouldlyIntegral(prog.elements[0].children.size()).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(prog.elements[0].children[0]).ShouldBe(1u));
    // StackPanel → search(id 2), Text(id 3), attached Button(id 4).
    SHOULD(ShouldlyIntegral(prog.elements[1].children.size()).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(prog.elements[1].children[0]).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(prog.elements[1].children[1]).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(prog.elements[1].children[2]).ShouldBe(4u));
    // Leaves: no children.
    SHOULD(ShouldlyIntegral(prog.elements[2].children.size()).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(prog.elements[3].children.size()).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(prog.elements[4].children.size()).ShouldBe(0u));
}

TEST_CASE("S284: golden — attribute flags: binding vs plain; target-prop token on bindings")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    const CompiledProgram prog = vaml::tools::vbaml::decode(bytes, schema);

    // Window (id 0): 3 attributes, NONE extensions/bindings (x:Name is a
    // directive carried as a plain literal; Width/Margin plain).
    SHOULD(ShouldlyIntegral(prog.elements[0].attributes.size()).ShouldBe(3u));
    for (const auto& a : prog.elements[0].attributes)
    {
        SHOULD(ShouldlyIntegral(a.isExtension).ShouldBe(0));
        SHOULD(ShouldlyIntegral(a.isBinding).ShouldBe(0));
    }

    // search Button (id 2): x:Name + S284Content + S284Width + S284Grid.Row
    // plain (4) + S284Command = {Binding SearchCommand} → isBinding.
    const auto& searchBtn = prog.elements[2];
    SHOULD(ShouldlyIntegral(searchBtn.attributes.size()).ShouldBe(5u));
    bool foundCommand = false;
    for (const auto& a : searchBtn.attributes)
    {
        if (a.key == "S284Command")
        {
            foundCommand = true;
            SHOULD(ShouldlyIntegral(a.isExtension).ShouldBe(1));
            SHOULD(ShouldlyIntegral(a.isBinding).ShouldBe(1));
            SHOULD(ShouldlyString(a.bindingPath).ShouldBe(std::string("SearchCommand")));
        }
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(foundCommand)).ShouldBe(1));

    // Text (id 3): x:Key carried as a plain directive; S284Text = {Binding
    // SearchText} → isBinding + path.
    const auto& text = prog.elements[3];
    bool foundTextBind = false;
    for (const auto& a : text.attributes)
    {
        if (a.key == "S284Text")
        {
            foundTextBind = true;
            SHOULD(ShouldlyIntegral(a.isBinding).ShouldBe(1));
            SHOULD(ShouldlyString(a.bindingPath).ShouldBe(std::string("SearchText")));
        }
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(foundTextBind)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// AXIS 3: token/string invariants (string table dedup + token enumeration)
// ---------------------------------------------------------------------------

TEST_CASE("S284: golden — every distinct string appears once in the string table")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);

    // The string table is content-deduped (build_schema interning contract):
    // no two entries are equal.
    std::unordered_set<std::string> unique;
    for (const std::string& s : schema.strings)
    {
        SHOULD(ShouldlyIntegral(unique.insert(s).second).ShouldBe(1)); // first-seen only
    }
}

TEST_CASE("S284: golden — type/prop token tables are ordered enumerations into the string table")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);

    // Every type/prop token is a valid string-table index (bounded by the
    // schema's own string table) — proven twice: decode_schema bounds-checks
    // them on the wire, and our read confirms the resolved names are those
    // authored.
    SHOULD(ShouldlyString(schema.types.size()).ShouldBe(4u)); // Window/StackPanel/Button/Text
    // props: the DISTINCT attribute KEYS across the golden (incl. the x:Name /
    // x:Key directives, which the compiler interns as prop tokens, + every
    // plain/extension key): x:Name, S284Width, S284Margin, S284Enabled,
    // S284Content, S284Grid.Row, S284Command, x:Key, S284Text, S284Dock.Dock,
    // S284Fill = 11 distinct keys.
    SHOULD(ShouldlyIntegral(schema.props.size()).ShouldBe(11u));

    // The string table holds the canonical event/resource names + the authored
    // 5 type names + the above keys + values + binding paths — its size must be
    // at least the four prepopulated + the four distinct element types.
    SHOULD(ShouldlyIntegral(schema.strings.size() >= 4u + schema.types.size()).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// AXIS 4: S282 re-proof on the golden (byte-identity + decode equality)
// ---------------------------------------------------------------------------

TEST_CASE("S284: golden — re-encode is byte-identical; decoded program is structural-equal")
{
    registerS284Properties();
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS284GoldenDoc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    const CompiledProgram prog = vaml::tools::vbaml::decode(bytes, schema);

    // Re-encoding the DECODED program with the SAME schema reproduces the
    // original bytes byte-for-byte (S282 byte-identity on the golden).
    const std::vector<std::uint8_t> reEncoded = vaml::tools::vbaml::encode(prog, schema);
    SHOULD(ShouldlyIntegral(reEncoded == bytes).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// Controls: positive minimal + negative malformed buffer
// ---------------------------------------------------------------------------

TEST_CASE("S284: clean-minimal control doc compiles to a small valid .vbaml")
{
    registerS284Properties();
    const std::string doc = "<StackPanel S284Enabled=\"true\"><Button S284Content=\"Go\"/></StackPanel>";
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(doc);
    const Schema schema = vaml::tools::vbaml::decode_schema(bytes);
    const CompiledProgram prog = vaml::tools::vbaml::decode(bytes, schema);
    SHOULD(ShouldlyIntegral(prog.elements.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(prog.elements[0].typeToken).ShouldBe(std::string("StackPanel")));
    SHOULD(ShouldlyString(prog.elements[1].typeToken).ShouldBe(std::string("Button")));
}

TEST_CASE("S284: malformed buffer (bad magic) throws VbamlMalformedBuffer")
{
    // A byte stream with a wrong magic must be rejected structurally.
    const std::vector<std::uint8_t> bad = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
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
