// S085 (dep:S084) — ParsedElement + ParsedAttribute tree tests.
//
// Proves the S085 tree assembly from the S080-S084 token stream (see
// ParsedElement.h for the full design rationale):
//   * parse_vaml turns a realistic nested document into a ParsedElement tree;
//   * children are appended in document order at the correct nesting depth;
//   * every attribute maps to ParsedAttribute {key,value,isExtension}, with the
//     extended scanner metadata (value_kind/numeric/directive/namespace)
//     carried forward — isExtension is true exactly for a S084 MarkupExtension;
//   * element text accumulates into the parent element's `content` (tags +
//     children + content, NOT a pseudo-element);
//   * self-closing elements close immediately (no ElementClose token);
//   * comments and CDATA are dropped (not tree nodes);
//   * structural failures — the parser's OWN path (the scanner does not detect
//     these): unterminated element (EOF with an open element) and missing
//     root — throw XmlParseException with XmlParseError::UnterminatedElement;
//   * scanner-level malformed markup (e.g. mismatched end tag) still propagates
//     as XmlScanException — the authority split is preserved.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ParsedElement.h"
#include "vaml/core/XmlScanner.h"

#include <string>
#include <vector>

using vaml::AttributeValueKind;
using vaml::ParsedAttribute;
using vaml::ParsedElement;
using vaml::parse_vaml;
using vaml::XmlDirectiveType;
using vaml::XmlParseError;
using vaml::XmlParseException;
using vaml::XmlScanException;
using vaml::kXamlLanguageNamespace;

namespace {

// Convenience: find the attribute with `key` inside `el`, or null when absent.
const ParsedAttribute* findAttr(const ParsedElement& el, const std::string& key)
{
    for (const ParsedAttribute& a : el.attributes)
    {
        if (a.key == key)
        {
            return &a;
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Tree assembly — nested document
// ---------------------------------------------------------------------------

TEST_CASE("S085: nested document assembles into a correctly-ordered tree")
{
    const ParsedElement root = parse_vaml(
        "<StackPanel Width=\"300\">"
        "<Text Content=\"Hello\"/>"
        "<Button Height=\"40\">Click</Button>"
        "</StackPanel>");

    // Root: tagName + ns(empty) + attributes.
    SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("StackPanel")));
    SHOULD(ShouldlyString(root.ns).ShouldBe(std::string("")));

    // Root attribute Width="300" — quoted literal, numeric detected.
    SHOULD(ShouldlyIntegral(root.attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    const ParsedAttribute* w = findAttr(root, "Width");
    SHOULD(ShouldlyIntegral(w != nullptr).ShouldBe(true));
    if (w)
    {
        SHOULD(ShouldlyString(w->value).ShouldBe(std::string("300")));
        SHOULD(ShouldlyIntegral(static_cast<int>(w->isExtension)).ShouldBe(0));
        SHOULD(ShouldlyIntegral(static_cast<int>(w->numeric)).ShouldBe(1));
        SHOULD(ShouldlyIntegral(static_cast<int>(w->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    }

    // Two children in document order.
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(2)));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("Text")));
    SHOULD(ShouldlyString(root.children[1].tagName).ShouldBe(std::string("Button")));

    // <Text .../> self-closing: no children, no content.
    SHOULD(ShouldlyIntegral(static_cast<int>(root.children[0].self_closing)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(root.children[0].content.empty()).ShouldBe(true));

    // <Button Height="40">Click</Button> carries body text in `content`.
    SHOULD(ShouldlyString(root.children[1].content).ShouldBe(std::string("Click")));
    const ParsedAttribute* h = findAttr(root.children[1], "Height");
    SHOULD(ShouldlyIntegral(h != nullptr).ShouldBe(true));
    if (h)
    {
        SHOULD(ShouldlyString(h->value).ShouldBe(std::string("40")));
        SHOULD(ShouldlyIntegral(static_cast<int>(h->numeric)).ShouldBe(1));
    }
}

TEST_CASE("S085: deeper nesting preserves depth")
{
    const ParsedElement root = parse_vaml("<a><b><c>x</c></b></a>");

    SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("a")));
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("b")));
    SHOULD(ShouldlyIntegral(root.children[0].children.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(root.children[0].children[0].tagName).ShouldBe(std::string("c")));
    // Leaf content at the deepest level.
    SHOULD(ShouldlyString(root.children[0].children[0].content).ShouldBe(std::string("x")));
    // Intermediate elements have no own content.
    SHOULD(ShouldlyIntegral(root.children[0].content.empty()).ShouldBe(true));
}

TEST_CASE("S085: sibling elements + interleaved text land at correct depth and content")
{
    const ParsedElement root = parse_vaml("<a>one<b/>two</a>");

    // "one" and "two" both accumulate into <a>'s content; <b/> is a child.
    SHOULD(ShouldlyString(root.content).ShouldBe(std::string("onetwo")));
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("b")));
    // Self-closing sibling: closed immediately, no ElementClose needed.
    SHOULD(ShouldlyIntegral(static_cast<int>(root.children[0].self_closing)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// isExtension / extended attribute metadata
// ---------------------------------------------------------------------------

TEST_CASE("S085: markup-extension value sets isExtension; extended metadata carried")
{
    const ParsedElement root = parse_vaml(
        "<Text Text=\"{Binding Title}\" Width=\"120\"/>");

    SHOULD(ShouldlyIntegral(root.attributes.size()).ShouldBe(static_cast<std::size_t>(2)));

    const ParsedAttribute* bind = findAttr(root, "Text");
    SHOULD(ShouldlyIntegral(bind != nullptr).ShouldBe(true));
    if (bind)
    {
        // Roadmap-titled bool: isExtension true for a S084 MarkupExtension.
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->isExtension)).ShouldBe(1));
        SHOULD(ShouldlyString(bind->value).ShouldBe(std::string("{Binding Title}")));
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
        // An extension is never numeric at S084.
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->numeric)).ShouldBe(0));
    }

    const ParsedAttribute* width = findAttr(root, "Width");
    SHOULD(ShouldlyIntegral(width != nullptr).ShouldBe(true));
    if (width)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(width->isExtension)).ShouldBe(0));
        SHOULD(ShouldlyIntegral(static_cast<int>(width->numeric)).ShouldBe(1));
        SHOULD(ShouldlyIntegral(static_cast<int>(width->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    }
}

TEST_CASE("S085: x: directive + namespace metadata carried into ParsedAttribute")
{
    // x maps to the real xaml URI: x:Name="foo" -> directive=Name on the
    // ParsedAttribute, isExtension stays false (quoted literal).
    const ParsedElement root = parse_vaml(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Name=\"foo\"/>");

    const ParsedAttribute* name = findAttr(root, "x:Name");
    SHOULD(ShouldlyIntegral(name != nullptr).ShouldBe(true));
    if (name)
    {
        SHOULD(ShouldlyString(name->key).ShouldBe(std::string("x:Name")));
        SHOULD(ShouldlyString(name->local_name).ShouldBe(std::string("Name")));
        SHOULD(ShouldlyString(name->prefix).ShouldBe(std::string("x")));
        SHOULD(ShouldlyString(name->namespace_uri).ShouldBe(std::string(kXamlLanguageNamespace)));
        SHOULD(ShouldlyIntegral(static_cast<int>(name->directive))
                   .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));
        SHOULD(ShouldlyIntegral(static_cast<int>(name->isExtension)).ShouldBe(0));
        SHOULD(ShouldlyIntegral(static_cast<int>(name->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    }
}

TEST_CASE("S085: element name namespace (ns + prefix + local_name) carried from scanner")
{
    const ParsedElement root = parse_vaml(
        "<Window xmlns:local=\"urn:vm:ns:ui\"><local:Button Width=\"10\"/></Window>");

    // Root element unprefixed -> empty ns, empty prefix, local_name == tagName.
    SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("Window")));
    SHOULD(ShouldlyString(root.ns).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(root.prefix).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(root.local_name).ShouldBe(std::string("Window")));

    // Child is prefixed local:Button -> ns resolved to the mapping, prefix kept.
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    const ParsedElement& child = root.children[0];
    SHOULD(ShouldlyString(child.tagName).ShouldBe(std::string("local:Button")));
    SHOULD(ShouldlyString(child.ns).ShouldBe(std::string("urn:vm:ns:ui")));
    SHOULD(ShouldlyString(child.prefix).ShouldBe(std::string("local")));
    SHOULD(ShouldlyString(child.local_name).ShouldBe(std::string("Button")));
}

// ---------------------------------------------------------------------------
// Comments / CDATA are dropped
// ---------------------------------------------------------------------------

TEST_CASE("S085: comments and CDATA are dropped (not tree nodes)")
{
    const ParsedElement root =
        parse_vaml("<a><!-- note --><![CDATA[ raw ]]><b/></a>");

    // Only <b/> survives as a child; the comment/CDATA add nothing.
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("b")));
    SHOULD(ShouldlyIntegral(root.content.empty()).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// Structural errors — the parser's OWN path (XmlParseException)
// ---------------------------------------------------------------------------

TEST_CASE("S085: unterminated element throws XmlParseException::UnterminatedElement")
{
    // The scanner is iterative and returns EndOfInput at EOF, so "<a>" with no
    // "</a>" is the parser's OWN structural failure.
    bool threw = false;
    try
    {
        (void)parse_vaml("<a>");
    }
    catch (const XmlParseException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(XmlParseError::UnterminatedElement)));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBeGreaterThan(0));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBeGreaterThan(0));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S085: unterminated nested element is reported")
{
    bool threw = false;
    try
    {
        (void)parse_vaml("<a><b></a>"); // <b> never closed; </a> mismatches anyway
    }
    catch (const XmlScanException&)
    {
        // The scanner flags the mismatched end tag </a> BEFORE the parser
        // could report <b> unterminated — that authority split is expected.
        threw = true;
    }
    catch (const XmlParseException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S085: missing root (empty / whitespace-only document) is a structural error")
{
    bool threw = false;
    try
    {
        (void)parse_vaml("   ");
    }
    catch (const XmlParseException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(XmlParseError::UnterminatedElement)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// Scanner-level malformed markup still propagates as XmlScanException
// ---------------------------------------------------------------------------

TEST_CASE("S085: scanner-level mismatched end tag propagates as XmlScanException")
{
    // End-tag matching is the scanner's job; the parser does not mask it.
    bool threw = false;
    try
    {
        (void)parse_vaml("<a></b>");
    }
    catch (const XmlScanException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(vaml::XmlScanError::MismatchedEndTag)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S085: scanner-level malformed markup (unterminated quote) propagates")
{
    bool threw = false;
    try
    {
        (void)parse_vaml("<a b=\"unterminated");
    }
    catch (const XmlScanException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
