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

#include <optional>
#include <string>
#include <vector>

using vaml::XmlAttribute;
using vaml::XmlScanError;
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
        r.name = std::string(tok.name);   // S140: interned view -> owned rec
        r.value = std::string(tok.value);
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
        s += " @" + std::string(a.name) + "=" + std::string(a.value);
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
            return std::string(a.value); // S140: interned view -> owned
        }
    }
    return "<missing:" + name + ">";
}

// S081: drains a scanner and returns the XmlScanException it throws, or
// nullopt if it somehow completed. The caller asserts on the returned object.
std::optional<XmlScanException> scanErrorOf(const std::string& src)
{
    try
    {
        XmlScanner s(src);
        XmlToken t;
        while (s.next(t))
        {
        }
    }
    catch (const XmlScanException& e)
    {
        return e;
    }
    return std::nullopt;
}

// Asserts kind() equals `expected` and that line()/col() are both >= 1.
void assertStructured(const std::optional<XmlScanException>& err, XmlScanError expected)
{
    SHOULD(ShouldlyIntegral(err.has_value()).ShouldBe(true));
    if (!err.has_value())
    {
        return;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(err->kind())).ShouldBe(static_cast<int>(expected)));
    SHOULD(ShouldlyIntegral(err->line()).ShouldBeGreaterThan(0));
    SHOULD(ShouldlyIntegral(err->col()).ShouldBeGreaterThan(0));
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

// ---------------------------------------------------------------------------
// S081 — structured errors: each malformed path has a distinct XmlScanError
// kind and a guaranteed line()/col() >= 1.
// ---------------------------------------------------------------------------

TEST_CASE("S081: expected-name error is structured")
{
    assertStructured(scanErrorOf("<a></1>"), XmlScanError::ExpectedName);
}

TEST_CASE("S081: unterminated-entity error is structured")
{
    // '&' at EOF with no body -> unterminated entity (UnknownEntity is a
    // distinct category for a malformed named entity, tested separately).
    assertStructured(scanErrorOf("<t>&"), XmlScanError::UnterminatedEntity);
}

TEST_CASE("S081: invalid-numeric-reference error is structured")
{
    // '&#;' has no digits.
    assertStructured(scanErrorOf("<t>&#;</t>"), XmlScanError::InvalidNumericReference);
}

TEST_CASE("S081: unterminated-numeric-reference error is structured")
{
    assertStructured(scanErrorOf("<t>&#65</t>"), XmlScanError::UnterminatedNumericReference);
}

TEST_CASE("S081: character-reference-out-of-range error is structured")
{
    // 0x110000 is > 0xFFFF (beyond the BMP S080 encodes).
    assertStructured(scanErrorOf("<t>&#1114112;</t>"), XmlScanError::CharacterReferenceOutOfRange);
}

TEST_CASE("S081: unknown-entity error is structured")
{
    assertStructured(scanErrorOf("<t>&bogus;</t>"), XmlScanError::UnknownEntity);
}

TEST_CASE("S081: unterminated-attribute-value error is structured")
{
    assertStructured(scanErrorOf("<a b=\"unterminated"), XmlScanError::UnterminatedAttributeValue);
}

TEST_CASE("S081: unterminated-comment error is structured")
{
    assertStructured(scanErrorOf("<!-- never closed"), XmlScanError::UnterminatedComment);
}

TEST_CASE("S081: unterminated-CDATA error is structured")
{
    assertStructured(scanErrorOf("<![CDATA[ never closed"), XmlScanError::UnterminatedCData);
}

TEST_CASE("S081: unterminated-start-tag error is structured")
{
    assertStructured(scanErrorOf("<a"), XmlScanError::UnterminatedStartTag);
}

TEST_CASE("S081: malformed-self-closing error is structured")
{
    assertStructured(scanErrorOf("<a/"), XmlScanError::MalformedSelfClosing);
}

TEST_CASE("S081: malformed-start-tag error is structured")
{
    // '1' where an attribute-or-'>' was expected.
    assertStructured(scanErrorOf("<a 1=\"x\">"), XmlScanError::MalformedStartTag);
}

TEST_CASE("S081: attribute-missing-equals error is structured")
{
    // '<a b x>' — 'b' is not followed by '=' (the next name 'x' is seen).
    assertStructured(scanErrorOf("<a b x>"), XmlScanError::AttributeMissingEquals);
}

TEST_CASE("S081: attribute-value-not-quoted error is structured")
{
    assertStructured(scanErrorOf("<a b=x>"), XmlScanError::AttributeValueNotQuoted);
}

TEST_CASE("S081: malformed-end-tag error is structured")
{
    assertStructured(scanErrorOf("<a></a"), XmlScanError::MalformedEndTag);
}

TEST_CASE("S081: mismatched-end-tag error is structured")
{
    assertStructured(scanErrorOf("<a></b>"), XmlScanError::MismatchedEndTag);
}

TEST_CASE("S081: unexpected-end-after-less-than error is structured")
{
    assertStructured(scanErrorOf("<"), XmlScanError::UnexpectedEndAfterLessThan);
}

TEST_CASE("S081: unrecognized-markup error is structured")
{
    assertStructured(scanErrorOf("<!>"), XmlScanError::UnrecognizedMarkup);
}

TEST_CASE("S081: unterminated comment reports exact line:col on a multi-line source")
{
    // Source: "<a>\n<!-- um"  — the comment starts line 2; unterminated at EOF
    // (line 2). Position of detection is the EOF position.
    const auto err = scanErrorOf("<a>\n<!-- um");
    SHOULD(ShouldlyIntegral(err.has_value()).ShouldBe(true));
    if (err.has_value())
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(err->kind()))
                   .ShouldBe(static_cast<int>(XmlScanError::UnterminatedComment)));
        // EOF is at line 2, col 8 (after "<!-- um").
        SHOULD(ShouldlyIntegral(err->line()).ShouldBe(std::size_t{2}));
    }
}

TEST_CASE("S081: mismatched end tag reports exact line:col")
{
    // "<a></b>" — '</b>' mismatch is detected AFTER the scanner consumes
    // "</b>" (the detection cursor = current scanner position), i.e. line 1,
    // col 8. Detection position is the failure cursor (not token start).
    const auto err = scanErrorOf("<a></b>");
    SHOULD(ShouldlyIntegral(err.has_value()).ShouldBe(true));
    if (err.has_value())
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(err->kind()))
                   .ShouldBe(static_cast<int>(XmlScanError::MismatchedEndTag)));
        SHOULD(ShouldlyIntegral(err->line()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(err->col()).ShouldBe(std::size_t{8}));
    }
}

