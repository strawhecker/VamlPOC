// S080 (dep:S049) — XML scanner (hand-rolled) tests.
//
// Proves the six constructs the S080 title names — elements, attributes,
// self-closing tags, comments, CDATA, entities — plus nested tags, via the
// standing Shouldly-for-doctest vocabulary (S036_1..S036_5): no CHECK / no
// CHECK_THROWS_AS; typed-exception assertions use ShouldThrow (S036_3).
//
// Token-model notes exercised here (see XmlScanner.h for the full rationale):
//   * attributes ride on ElementOpen as (name, decoded value) list;
//   * self-closing is a flag, not a separate token kind;
//   * entities are DECODED inline into Text and AttributeValue (no EntityRef
//     token) — proven by round-trip through the decoded output;
//   * Comment + CData content is returned RAW (never entity-decoded);
//   * a stray/mismatched end tag is a REAL error (XmlScanException) — the
//     scanner cannot honestly scan nested tags while ignoring a mismatched close.
//
// S081 is the explicit structured-error step; S080 only needs message-only
// XmlScanException, assembled here via ShouldThrow.

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/XmlScanner.h"

#include <string>
#include <vector>

using vaml::XmlAttribute;
using vaml::XmlScanException;
using vaml::XmlScanner;
using vaml::XmlToken;
using vaml::XmlTokenType;
using vaml::test::ShouldThrow;

namespace {

// Drain a scanner, grouping tokens into one struct for whole-sequence
// assertions. Reading a multi-line source is easier than eyeballing tokens.
struct TokenRec
{
    XmlTokenType type;
    std::string name;      // tag name (open/close); empty otherwise
    std::string value;     // Text / Comment / CData content
    bool self_closing;
    std::vector<XmlAttribute> attributes;
    std::size_t line;
    std::size_t col;
};

std::vector<TokenRec> drain(const std::string& src)
{
    XmlScanner scanner(src);
    std::vector<TokenRec> out;
    XmlToken tok;
    while (scanner.next(tok))
    {
        TokenRec r;
        r.type = tok.type;
        r.name = tok.name;
        r.value = tok.value;
        r.self_closing = tok.self_closing;
        r.attributes = tok.attributes;
        r.line = tok.line;
        r.col = tok.col;
        out.push_back(std::move(r));
    }
    return out;
}

std::string typeName(XmlTokenType t)
{
    switch (t)
    {
        case XmlTokenType::ElementOpen:  return "ElementOpen";
        case XmlTokenType::ElementClose: return "ElementClose";
        case XmlTokenType::Text:         return "Text";
        case XmlTokenType::Comment:      return "Comment";
        case XmlTokenType::CData:        return "CData";
        case XmlTokenType::EndOfInput:   return "EndOfInput";
    }
    return "?";
}

// Flattens a token (type|name|value|selfClosing|attrs) into one readable line.
std::string describe(const TokenRec& r)
{
    std::string s = typeName(r.type);
    if (!r.name.empty())
    {
        s += " name=" + r.name;
    }
    if (!r.value.empty())
    {
        s += " val=" + r.value;
    }
    if (r.self_closing)
    {
        s += " self";
    }
    for (const XmlAttribute& a : r.attributes)
    {
        s += " @" + a.name + "=" + a.value;
    }
    s += " L" + std::to_string(r.line) + "C" + std::to_string(r.col);
    return s;
}

std::string joinSeq(const std::vector<TokenRec>& seq)
{
    std::string s;
    for (const TokenRec& r : seq)
    {
        if (!s.empty())
        {
            s += " | ";
        }
        s += describe(r);
    }
    return s;
}

std::string attrValue(const TokenRec& r, const std::string& name)
{
    for (const XmlAttribute& a : r.attributes)
    {
        if (a.name == name)
        {
            return a.value;
        }
    }
    return "<missing:" + name + ">";
}

} // namespace

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------

TEST_CASE("S080: plain element emits open + close with the right names")
{
    const auto seq = drain("<Button>text</Button>");

    SHOULD(ShouldlyString(joinSeq(seq)).ShouldBe(
        "ElementOpen name=Button L1C1 | Text val=text L1C9 | ElementClose name=Button L1C13"));
}

TEST_CASE("S080: element with namespaced and dotted names tokenizes the name intact")
{
    // Attached-style names (Grid.Row) and prefixed names are preserved whole
    // in S080; their interpretation is S082's step.
    const auto seq = drain("<Grid.Row>1</Grid.Row>");
    SHOULD(ShouldlyString(seq[0].name).ShouldBe(std::string("Grid.Row")));
    SHOULD(ShouldlyString(seq[2].name).ShouldBe(std::string("Grid.Row")));
    SHOULD(ShouldlyString(typeName(seq[0].type)).ShouldBe(std::string("ElementOpen")));
    SHOULD(ShouldlyString(typeName(seq[2].type)).ShouldBe(std::string("ElementClose")));
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------

TEST_CASE("S080: attributes with double quotes are captured name + value")
{
    const auto seq = drain("<Button Width=\"100\" Height=\"30\">x</Button>");
    const TokenRec& open = seq[0];

    SHOULD(ShouldlyString(typeName(open.type)).ShouldBe(std::string("ElementOpen")));
    SHOULD(ShouldlyString(open.name).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(attrValue(open, "Width")).ShouldBe(std::string("100")));
    SHOULD(ShouldlyString(attrValue(open, "Height")).ShouldBe(std::string("30")));
}

TEST_CASE("S080: attributes with single quotes are captured name + value")
{
    const auto seq = drain("<Text Content='hello' />");
    const TokenRec& open = seq[0];

    SHOULD(ShouldlyString(typeName(open.type)).ShouldBe(std::string("ElementOpen")));
    SHOULD(ShouldlyString(attrValue(open, "Content")).ShouldBe(std::string("hello")));
    SHOULD(ShouldlyIntegral(open.self_closing).ShouldBe(true));
}

TEST_CASE("S080: entity references in attribute values are decoded inline")
{
    // Entities must expand inside a single-value attribute (the reason S080
    // has no separate EntityRef token).
    const auto seq = drain("<Text T=\"a&amp;b&lt;c&gt;d&quot;e&apos;f\"/>");
    const TokenRec& open = seq[0];
    SHOULD(ShouldlyString(attrValue(open, "T"))
               .ShouldBe(std::string("a&b<c>d\"e'f")));
}

// ---------------------------------------------------------------------------
// Self-closing
// ---------------------------------------------------------------------------

TEST_CASE("S080: self-closing tag is a flag, not a separate token")
{
    const auto seq = drain("<empty/>");
    SHOULD(ShouldlyIntegral(seq.size()).ShouldBe(static_cast<std::size_t>(1))); // no ElementClose
    SHOULD(ShouldlyString(typeName(seq[0].type)).ShouldBe(std::string("ElementOpen")));
    SHOULD(ShouldlyString(seq[0].name).ShouldBe(std::string("empty")));
    SHOULD(ShouldlyIntegral(seq[0].self_closing).ShouldBe(true));
}

TEST_CASE("S080: non-self-closing closed element leaves self_closing false and emits close")
{
    const auto seq = drain("<a></a>");
    SHOULD(ShouldlyIntegral(seq[0].self_closing).ShouldBe(false));
    SHOULD(ShouldlyString(typeName(seq[1].type)).ShouldBe(std::string("ElementClose")));
    SHOULD(ShouldlyString(seq[1].name).ShouldBe(std::string("a")));
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST_CASE("S080: comment content is raw (never entity-decoded), even across lines")
{
    // '&amp;' inside a comment must NOT become '&' — XML comment semantics.
    const auto seq = drain("<!-- note &amp; more\nline 2 --><a/>");
    SHOULD(ShouldlyString(typeName(seq[0].type)).ShouldBe(std::string("Comment")));
    SHOULD(ShouldlyString(seq[0].value).ShouldBe(std::string(" note &amp; more\nline 2 ")));
    SHOULD(ShouldlyString(typeName(seq[1].type)).ShouldBe(std::string("ElementOpen")));
}

// ---------------------------------------------------------------------------
// CDATA
// ---------------------------------------------------------------------------

TEST_CASE("S080: CDATA content is returned raw, terminating at ]]>" )
{
    const auto seq = drain("<![CDATA[ a < b && c ]]>");
    SHOULD(ShouldlyString(typeName(seq[0].type)).ShouldBe(std::string("CData")));
    SHOULD(ShouldlyString(seq[0].value).ShouldBe(std::string(" a < b && c ")));
}

TEST_CASE("S080: CDATA boundary does not trigger on an interior ']' followed by non-'>>'")
{
    // ']]' then 'x' is NOT a terminator; only ']]>' closes.
    const auto seq = drain("<![CDATA[ a]]x ]]>");
    SHOULD(ShouldlyString(typeName(seq[0].type)).ShouldBe(std::string("CData")));
    SHOULD(ShouldlyString(seq[0].value).ShouldBe(std::string(" a]]x ")));
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

TEST_CASE("S080: all five predefined entities decode in text")
{
    const auto seq = drain("<t>&amp;&lt;&gt;&quot;&apos;</t>");
    SHOULD(ShouldlyString(typeName(seq[1].type)).ShouldBe(std::string("Text")));
    SHOULD(ShouldlyString(seq[1].value).ShouldBe(std::string("&<>\"'")));
}

TEST_CASE("S080: numeric character references decode (decimal + hex)")
{
    const auto seq = drain("<t>&#65;&#x41;</t>");
    SHOULD(ShouldlyString(seq[1].value).ShouldBe(std::string("AA")));
}

TEST_CASE("S080: entity round-trips through raw text source")
{
    // '&lt;Button/&gt;' as text should decode so the scanner reports Text "<Button/>".
    const auto seq = drain("<t>&lt;Button/&gt;</t>");
    SHOULD(ShouldlyString(seq[1].value).ShouldBe(std::string("<Button/>")));
}

// ---------------------------------------------------------------------------
// Nested tags
// ---------------------------------------------------------------------------

TEST_CASE("S080: deep nesting interleaves open/close correctly")
{
    const auto seq = drain("<a><b><c>x</c></b></a>");
    SHOULD(ShouldlyString(joinSeq(seq)).ShouldBe(
        "ElementOpen name=a L1C1 | ElementOpen name=b L1C4 | ElementOpen name=c L1C7 "
        "| Text val=x L1C10 | ElementClose name=c L1C11 | ElementClose name=b L1C15 "
        "| ElementClose name=a L1C19"));
}

TEST_CASE("S080: sibling elements + interleaved text")
{
    const auto seq = drain("<a>one<b/>two</a>");
    SHOULD(ShouldlyString(joinSeq(seq)).ShouldBe(
        "ElementOpen name=a L1C1 | Text val=one L1C4 | ElementOpen name=b self L1C7 "
        "| Text val=two L1C11 | ElementClose name=a L1C14"));
}

TEST_CASE("S080: sibling open elements at the same depth don't confuse the stack")
{
    const auto seq = drain("<a/><b/><a/><b/>");
    SHOULD(ShouldlyIntegral(seq.size()).ShouldBe(static_cast<std::size_t>(4)));
    SHOULD(ShouldlyString(seq[0].name).ShouldBe(std::string("a")));
    SHOULD(ShouldlyString(seq[1].name).ShouldBe(std::string("b")));
    SHOULD(ShouldlyString(seq[2].name).ShouldBe(std::string("a")));
    SHOULD(ShouldlyString(seq[3].name).ShouldBe(std::string("b")));
}

// ---------------------------------------------------------------------------
// Line / col
// ---------------------------------------------------------------------------

TEST_CASE("S080: tokens report their 1-based source line and column")
{
    const auto seq = drain("<a>\n\n  <b x=\"1\"/></a>");
    // seq[0]=ElementOpen a L1C1; seq[1]=Text "\n\n  " L1C4; seq[2]=ElementOpen b L3C3
    SHOULD(ShouldlyIntegral(seq[0].line).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(seq[0].col).ShouldBe(static_cast<std::size_t>(1)));
    // seq[1] is the Text whitespace token starting at L1C4.
    SHOULD(ShouldlyIntegral(seq[1].line).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(seq[1].col).ShouldBe(static_cast<std::size_t>(4)));
    SHOULD(ShouldlyIntegral(seq[2].name == "b").ShouldBe(true));
    SHOULD(ShouldlyIntegral(seq[2].line).ShouldBe(static_cast<std::size_t>(3)));
    SHOULD(ShouldlyIntegral(seq[2].col).ShouldBe(static_cast<std::size_t>(3)));
}

// ---------------------------------------------------------------------------
// EOF
// ---------------------------------------------------------------------------

TEST_CASE("S080: next() returns false and emits EndOfInput once exhausted")
{
    XmlScanner scanner("<a/>");
    XmlToken tok;
    bool more = scanner.next(tok);
    SHOULD(ShouldlyString(more).ShouldBe(true));
    more = scanner.next(tok);
    SHOULD(ShouldlyString(more).ShouldBe(false));
    SHOULD(ShouldlyString(typeName(tok.type)).ShouldBe(std::string("EndOfInput")));
}

// ---------------------------------------------------------------------------
// Malformed -> XmlScanException
// ---------------------------------------------------------------------------

TEST_CASE("S080: unterminated comment throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<!-- never closed");
        XmlToken t;
        (void)s.next(t);
    }));
}

TEST_CASE("S080: unterminated CDATA throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<![CDATA[ never closed");
        XmlToken t;
        (void)s.next(t);
    }));
}

TEST_CASE("S080: unterminated attribute quote throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<a b=\"unterminated");
        XmlToken t;
        (void)s.next(t);
    }));
}

TEST_CASE("S080: stray end tag throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("</a>");
        XmlToken t;
        (void)s.next(t);
    }));
}

TEST_CASE("S080: mismatched end tag throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<a></b>");
        XmlToken t;
        while (s.next(t))
        {
        }
    }));
}

TEST_CASE("S080: unknown entity ref throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<t>&bogus;</t>");
        XmlToken t;
        while (s.next(t))
        {
        }
    }));
}

TEST_CASE("S080: unterminated numeric character reference throws XmlScanException")
{
    SHOULD(ShouldThrow<XmlScanException>([] {
        XmlScanner s("<t>&#65</t>"); // missing ';'
        XmlToken t;
        while (s.next(t))
        {
        }
    }));
}
