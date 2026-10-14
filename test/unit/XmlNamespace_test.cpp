// S082 (dep:S081) — XML scanner NAMESPACE tests.
//
// Proves the S082 namespace model on the scanner (see XmlScanner.h):
//   * names split on the FIRST ':' into (prefix, local_name) — the '.'
//     attached-owner form ("Grid.Row") is NOT a namespace separator;
//   * a per-element xmlns scope is tracked (default `xmlns="uri"` + prefixed
//     `xmlns:p="uri"`), pushed on non-self-closing ElementOpen and popped on
//     the matching ElementClose;
//   * unprefixed ELEMENTS resolve to the default namespace; the default does
//     NOT apply to ATTRIBUTES (XML rule);
//   * `xmlns` / `xmlns:p` are NAMESPACE DECLARATIONS (collected in XmlToken::
//     xmlns), NOT ordinary attributes;
//   * a WPF-style "clr-namespace:Ns.Name;assembly=Asm" value is TOKENIZED into
//     ClrNamespaceReference (parts extracted; type resolution is S087).
//
// Uses the standing Shouldly-for-doctest vocabulary. S082-unique xmlns URIs
// ("urn:vm:ns:*") avoid collisions with the S080/S081 TUs. Existing
// XmlScanner_test.cpp cases must remain green unchanged (name stays the FULL
// raw text).

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/XmlScanner.h"

#include <optional>
#include <string>
#include <vector>

using vaml::ClrNamespaceReference;
using vaml::XmlAttribute;
using vaml::XmlScanner;
using vaml::XmlToken;
using vaml::XmlTokenType;
using vaml::parse_clr_namespace;

namespace {

// Drain a scanner into a vector of XmlToken (keeps the S082-exposed fields
// directly visible).
std::vector<XmlToken> drainTokens(const std::string& src)
{
    XmlScanner scanner(src);
    std::vector<XmlToken> out;
    XmlToken tok;
    while (scanner.next(tok))
    {
        out.push_back(std::move(tok));
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Prefix / local split on ':'; '.' is not a namespace separator
// ---------------------------------------------------------------------------

TEST_CASE("S082: prefixed element splits into prefix + local on the first colon")
{
    const auto seq = drainTokens("<x:Button xmlns:x=\"urn:vm:ns:core\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyString(t.name).ShouldBe(std::string("x:Button"))); // name intact
    SHOULD(ShouldlyString(t.prefix).ShouldBe(std::string("x")));
    SHOULD(ShouldlyString(t.local_name).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(t.namespace_uri).ShouldBe(std::string("urn:vm:ns:core")));
}

TEST_CASE("S082: dotted attached-owner name is NOT split on '.'")
{
    // "Grid.Row" has a '.' but no ':' — it is a single name (attached-owner
    // form the S085 parser interprets), NOT a prefix split.
    const auto seq = drainTokens("<Grid.Row>1</Grid.Row>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.prefix).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(t.local_name).ShouldBe(std::string("Grid.Row")));
}

TEST_CASE("S082: unprefixed element has empty prefix + full local")
{
    const auto seq = drainTokens("<Button/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.prefix).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(t.local_name).ShouldBe(std::string("Button")));
}

// ---------------------------------------------------------------------------
// Default namespace applies to unprefixed ELEMENTS (not attributes)
// ---------------------------------------------------------------------------

TEST_CASE("S082: unprefixed element under default xmlns resolves to that namespace")
{
    const auto seq = drainTokens("<Panel xmlns=\"urn:vm:ns:controls\"><Text/></Panel>");
    SHOULD(ShouldlyString(seq[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:controls")));
    // Child inherits the parent's default namespace scope.
    SHOULD(ShouldlyString(seq[1].namespace_uri).ShouldBe(std::string("urn:vm:ns:controls")));
}

TEST_CASE("S082: unprefixed element with no default xmlns has empty namespace")
{
    const auto seq = drainTokens("<Button/>");
    SHOULD(ShouldlyString(seq[0].namespace_uri).ShouldBe(std::string("")));
}

TEST_CASE("S082: default namespace does NOT apply to attributes")
{
    const auto seq = drainTokens("<Button xmlns=\"urn:vm:ns:controls\" Width=\"100\"/>");
    const XmlToken& t = seq[0];
    // Element gets the default namespace...
    SHOULD(ShouldlyString(t.namespace_uri).ShouldBe(std::string("urn:vm:ns:controls")));
    // ...but its unprefixed attribute does NOT (XML rule).
    SHOULD(ShouldlyIntegral(t.attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(t.attributes[0].prefix).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(t.attributes[0].namespace_uri).ShouldBe(std::string("")));
}

// ---------------------------------------------------------------------------
// Prefixed attributes resolve via the prefixed scope
// ---------------------------------------------------------------------------

TEST_CASE("S082: prefixed attribute resolves its namespace via the xmlns:p mapping")
{
    const auto seq = drainTokens(
        "<Button xmlns:x=\"urn:vm:ns:x\"><x:Something x:Name=\"foo\"/></Button>");
    // seq[1] is the x:Something ElementOpen whose x:Name attribute is prefixed.
    const XmlToken& t = seq[1];
    SHOULD(ShouldlyString(t.prefix).ShouldBe(std::string("x")));
    SHOULD(ShouldlyString(t.namespace_uri).ShouldBe(std::string("urn:vm:ns:x")));
    SHOULD(ShouldlyIntegral(t.attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("x:Name")));
    SHOULD(ShouldlyString(t.attributes[0].prefix).ShouldBe(std::string("x")));
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Name")));
    SHOULD(ShouldlyString(t.attributes[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:x")));
}

TEST_CASE("S082: unmapped prefix resolves to empty namespace (not a scan error)")
{
    // Unknown prefix is NOT a scan error at S082 — the structured authority is
    // the S085/S087 parser (recorded NO-GAP). Here it just resolves to "".
    const auto seq = drainTokens("<q:Unknown/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.prefix).ShouldBe(std::string("q")));
    SHOULD(ShouldlyString(t.local_name).ShouldBe(std::string("Unknown")));
    SHOULD(ShouldlyString(t.namespace_uri).ShouldBe(std::string("")));
}

// ---------------------------------------------------------------------------
// xmlns scope: push/pop, nesting, shadowing
// ---------------------------------------------------------------------------

TEST_CASE("S082: xmlns declarations are NOT emitted as ordinary attributes")
{
    const auto seq = drainTokens("<a xmlns=\"urn:vm:ns:a\" xmlns:p=\"urn:vm:ns:p\" w=\"1\"/>");
    const XmlToken& t = seq[0];
    // Only the real attribute 'w' is an attribute; xmlns/xmlns:p are in `xmlns`.
    SHOULD(ShouldlyIntegral(t.attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("w")));
    // The two declarations are collected separately.
    SHOULD(ShouldlyIntegral(t.xmlns.size()).ShouldBe(static_cast<std::size_t>(2)));
    SHOULD(ShouldlyString(t.xmlns[0].first).ShouldBe(std::string("")));     // default
    SHOULD(ShouldlyString(t.xmlns[0].second).ShouldBe(std::string("urn:vm:ns:a")));
    SHOULD(ShouldlyString(t.xmlns[1].first).ShouldBe(std::string("p")));    // prefixed
    SHOULD(ShouldlyString(t.xmlns[1].second).ShouldBe(std::string("urn:vm:ns:p")));
}

TEST_CASE("S082: namespace scope pops so a sibling does not inherit an inner default")
{
    // Token order: <a> open, <b> open, <c/> open, </b> close, <d/> open, </a> close.
    const auto seq = drainTokens(
        "<a xmlns=\"urn:vm:ns:outer\"><b xmlns=\"urn:vm:ns:inner\"><c/></b><d/></a>");
    // a: outer; b + c: inner; d (a sibling of b) is back to outer.
    SHOULD(ShouldlyString(seq[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:outer")));
    SHOULD(ShouldlyString(seq[1].namespace_uri).ShouldBe(std::string("urn:vm:ns:inner")));
    SHOULD(ShouldlyString(seq[2].namespace_uri).ShouldBe(std::string("urn:vm:ns:inner")));
    // seq[3] is </b> (an ElementClose — no namespace_uri). d is seq[4].
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[3].type))
               .ShouldBe(static_cast<int>(XmlTokenType::ElementClose)));
    SHOULD(ShouldlyString(seq[4].namespace_uri).ShouldBe(std::string("urn:vm:ns:outer")));
}

TEST_CASE("S082: prefixed scope is inherited by descendants and shadowed by an inner redefinition")
{
    const auto seq = drainTokens(
        "<r xmlns:p=\"urn:vm:ns:first\"><a p:K=\"1\"/><b xmlns:p=\"urn:vm:ns:second\" p:K=\"2\"/></r>");
    // a inherits p->first; b redefines p->second for itself.
    SHOULD(ShouldlyString(seq[1].attributes[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:first")));
    SHOULD(ShouldlyString(seq[2].attributes[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:second")));
}

TEST_CASE("S082: self-closing element sees its own xmlns declarations (self-applying)")
{
    // A self-closing element declares its namespace ON the tag, so its own
    // unprefixed name must resolve against THAT declaration.
    const auto seq = drainTokens("<Self xmlns=\"urn:vm:ns:self\"/>");
    SHOULD(ShouldlyString(seq[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:self")));
}

// ---------------------------------------------------------------------------
// clr-namespace mapping tokenized
// ---------------------------------------------------------------------------

TEST_CASE("S082: clr-namespace with assembly tokenizes into parts")
{
    bool isClr = false;
    const ClrNamespaceReference ref =
        parse_clr_namespace("clr-namespace:System.Windows.Controls;assembly=PresentationFramework", &isClr);

    SHOULD(ShouldlyString(isClr).ShouldBe(true));
    SHOULD(ShouldlyString(ref.clrNamespace).ShouldBe(std::string("System.Windows.Controls")));
    SHOULD(ShouldlyString(ref.assembly).ShouldBe(std::string("PresentationFramework")));
}

TEST_CASE("S082: clr-namespace without assembly yields empty assembly")
{
    bool isClr = false;
    const ClrNamespaceReference ref = parse_clr_namespace("clr-namespace:Vaml.Hmi", &isClr);

    SHOULD(ShouldlyString(isClr).ShouldBe(true));
    SHOULD(ShouldlyString(ref.clrNamespace).ShouldBe(std::string("Vaml.Hmi")));
    SHOULD(ShouldlyString(ref.assembly).ShouldBe(std::string("")));
}

TEST_CASE("S082: a plain value (no clr-namespace prefix) is returned verbatim, not flagged")
{
    bool isClr = true; // should be reset to false
    const ClrNamespaceReference ref = parse_clr_namespace("SomeRegularValue", &isClr);

    SHOULD(ShouldlyString(isClr).ShouldBe(false));
    SHOULD(ShouldlyString(ref.clrNamespace).ShouldBe(std::string("SomeRegularValue")));
    SHOULD(ShouldlyString(ref.assembly).ShouldBe(std::string("")));
}

TEST_CASE("S082: malformed clr-namespace (empty namespace) is a categorized throw")
{
    using vaml::XmlScanError;
    using vaml::XmlScanException;
    using vaml::test::ShouldThrow;

    SHOULD(ShouldThrow<XmlScanException>([] {
        bool isClr = false;
        (void)parse_clr_namespace("clr-namespace:", &isClr);
    }));

    // Verify the kind is the S082 MalformedClrNamespace category (keeps the
    // S081 1:1 taxonomy contract honest).
    try
    {
        bool isClr = false;
        (void)parse_clr_namespace("clr-namespace:", &isClr);
    }
    catch (const XmlScanException& e)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(XmlScanError::MalformedClrNamespace)));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBeGreaterThan(0));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBeGreaterThan(0));
    }
}

TEST_CASE("S082: malformed clr-namespace (bad ';' suffix) is a categorized throw")
{
    using vaml::XmlScanError;
    using vaml::XmlScanException;

    bool isClr = false;
    bool caught = false;
    try
    {
        (void)parse_clr_namespace("clr-namespace:Vaml.Hmi;bogus=foo", &isClr);
    }
    catch (const XmlScanException& e)
    {
        caught = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(XmlScanError::MalformedClrNamespace)));
        SHOULD(ShouldlyString(isClr).ShouldBe(true));
    }
    SHOULD(ShouldlyIntegral(caught).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// S080/S081 backward compatibility — name field stays the FULL raw text
// ---------------------------------------------------------------------------

TEST_CASE("S082: name field remains the full raw text (S080 contract intact)")
{
    // The S080 prefixed/dotted-name test asserted name == "Grid.Row"; S082 must
    // not alter `name`. Here we also confirm a prefixed name keeps the prefix.
    const auto seq = drainTokens("<x:Button xmlns:x=\"urn:vm:ns:core\"/>");
    SHOULD(ShouldlyString(seq[0].name).ShouldBe(std::string("x:Button")));
}
