// S083 (dep:S082) — XML scanner X-LANGUAGE DIRECTIVE tests.
//
// Proves the S083 x-directive classification on the scanner (see XmlScanner.h):
//   * the four XAML x-language directives — Name, Class, Key, Null — are
//     recognized as a distinct XmlDirectiveType on the XmlAttribute when the
//     attribute's RESOLVED namespace_uri == kXamlLanguageNamespace
//     ("http://schemas.microsoft.com/winfx/2006/xaml");
//   * classification is keyed on the NAMESPACE URI, not the literal "x"
//     prefix: any prefix bound to the xaml URI is the x-language (foo:Name is
//     a Name directive), and a prefix bound to a NON-xaml URI (S082's fake
//     xmlns:x="urn:vm:ns:x") stays None (not a directive) — the URI, not the
//     letter, is authoritative;
//   * matching is CASE-SENSITIVE; an unknown local in the xaml ns -> None;
//   * `name` stays the FULL raw text (S080 contract); directives coexist with
//     ordinary attributes (order/content preserved);
//   * ordinary / dotted-attached / non-language-prefixed attributes are NOT
//     directives.
//
// Uses the standing Shouldly-for-doctest vocabulary. Existing S080/S081/S082
// tests remain green unchanged (they use non-xaml / fake URIs, so their
// attributes are never directives).

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/XmlScanner.h"

#include <string>
#include <vector>

using vaml::XmlDirectiveType;
using vaml::XmlScanner;
using vaml::XmlToken;
using vaml::kXamlLanguageNamespace;

namespace {

// Drain a scanner into a vector of XmlToken (keeps the S083-exposed directive
// field directly visible).
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
// The four directives classified under the real xaml namespace URI
// ---------------------------------------------------------------------------

TEST_CASE("S083: x:Name / x:Class / x:Key / x:Null each classify to the right directive")
{
    // x maps to the REAL xaml language URI -> all four are directives.
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) +
        "\" x:Class=\"App.Main\" x:Name=\"root\" x:Key=\"k\" x:Null=\"ignored\"/>");

    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(t.attributes.size()).ShouldBe(static_cast<std::size_t>(4)));

    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("x:Class")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Class)));

    SHOULD(ShouldlyString(t.attributes[1].name).ShouldBe(std::string("x:Name")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));

    SHOULD(ShouldlyString(t.attributes[2].name).ShouldBe(std::string("x:Key")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[2].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Key)));

    SHOULD(ShouldlyString(t.attributes[3].name).ShouldBe(std::string("x:Null")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[3].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Null)));
}

TEST_CASE("S083: directive attribute keeps full raw name + prefix/local + resolved uri (S080/S082 intact)")
{
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Name=\"theName\"/>");
    const XmlToken& t = seq[0];

    // S080 contract: name is the FULL raw text; S082 split fields coexist.
    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("x:Name")));
    SHOULD(ShouldlyString(t.attributes[0].prefix).ShouldBe(std::string("x")));
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Name")));
    SHOULD(ShouldlyString(t.attributes[0].namespace_uri)
               .ShouldBe(std::string(kXamlLanguageNamespace)));
    // The directive VALUE is carried in `value` unchanged (S085 applies it).
    SHOULD(ShouldlyString(t.attributes[0].value).ShouldBe(std::string("theName")));
}

// ---------------------------------------------------------------------------
// URI-keyed, NOT prefix-keyed
// ---------------------------------------------------------------------------

TEST_CASE("S083: a non-'x' prefix bound to the xaml URI is ALSO a directive")
{
    // XAML treats the URI as authoritative: any prefix mapped to the canonical
    // xaml namespace is the x-language, so foo:Name IS a Name directive.
    const auto seq = drainTokens(
        "<Root xmlns:foo=\"" + std::string(kXamlLanguageNamespace) + "\" foo:Name=\"big\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Name")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));
}

TEST_CASE("S083: an 'x' prefix bound to a NON-xaml URI is NOT a directive (S082 fake URI)")
{
    // S082's tests bind xmlns:x="urn:vm:ns:x" — those x: attrs must stay
    // PLAIN attributes (URI-keyed classification). This is the S082 backward
    // compat guarantee: existing S082 tests remain green unchanged.
    const auto seq = drainTokens("<Root xmlns:x=\"urn:vm:ns:x\" x:Name=\"n\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Name")));
    SHOULD(ShouldlyString(t.attributes[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:x")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

// ---------------------------------------------------------------------------
// Ordinary attributes are never directives
// ---------------------------------------------------------------------------

TEST_CASE("S083: unprefixed ordinary attribute is not a directive")
{
    const auto seq = drainTokens("<Button Width=\"100\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

TEST_CASE("S083: dotted attached-owner attribute is not a directive")
{
    const auto seq = drainTokens("<Grid Grid.Row=\"2\"/>");
    const XmlToken& t = seq[0];
    // The '.' attached-owner form is NOT split by S082's splitName (only ':' is).
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Grid.Row")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

TEST_CASE("S083: non-language prefixed attribute is not a directive")
{
    const auto seq = drainTokens("<Root xmlns:q=\"urn:vm:ns:other\" q:Width=\"5\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(t.attributes[0].namespace_uri).ShouldBe(std::string("urn:vm:ns:other")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

// ---------------------------------------------------------------------------
// Unknown local in the language namespace -> None (only the four are directives)
// ---------------------------------------------------------------------------

TEST_CASE("S083: unknown local in the xaml namespace is not a directive")
{
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Bogus=\"1\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("Bogus")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

TEST_CASE("S083: directive matching is case-sensitive")
{
    // "name" (lowercase) is NOT a directive — XAML is case-sensitive.
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:name=\"n\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyString(t.attributes[0].local_name).ShouldBe(std::string("name")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

// ---------------------------------------------------------------------------
// Directives coexist with ordinary attributes; multiple directives per element
// ---------------------------------------------------------------------------

TEST_CASE("S083: directives coexist with ordinary attrs, order/content preserved")
{
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) +
        "\" Width=\"100\" x:Name=\"root\" Grid.Row=\"1\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(t.attributes.size()).ShouldBe(static_cast<std::size_t>(3)));

    // Ordinary attr first — not a directive.
    SHOULD(ShouldlyString(t.attributes[0].name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));

    // Directive in the middle.
    SHOULD(ShouldlyString(t.attributes[1].name).ShouldBe(std::string("x:Name")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));

    // Ordinary dotted attr last — not a directive.
    SHOULD(ShouldlyString(t.attributes[2].name).ShouldBe(std::string("Grid.Row")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[2].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}

TEST_CASE("S083: multiple directives on one element are tagged independently")
{
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) +
        "\" x:Class=\"App.Main\" x:Name=\"root\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Class)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));
}

// ---------------------------------------------------------------------------
// Directive state is per-token (no leakage across tokens)
// ---------------------------------------------------------------------------

TEST_CASE("S083: directive does not leak across sibling tokens")
{
    // First element has an x:Name directive; second has an ordinary x-free attr.
    const auto seq = drainTokens(
        "<a xmlns:x=\"" + std::string(kXamlLanguageNamespace) +
        "\"><b x:Name=\"n\"/><c Width=\"1\"/></a>");

    // seq[0] = <a> open (xmlns declared here, no x attrs).
    SHOULD(ShouldlyIntegral(seq[0].attributes.size()).ShouldBe(static_cast<std::size_t>(0)));

    // seq[1] = <b x:Name="n"/> — the directive.
    SHOULD(ShouldlyIntegral(seq[1].attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[1].attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));

    // seq[2] = <c Width="1"/> — inherits the xaml ns scope but Width is local
    // "Width", not a directive.
    SHOULD(ShouldlyIntegral(seq[2].attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyString(seq[2].attributes[0].local_name).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[2].attributes[0].directive))
               .ShouldBe(static_cast<int>(XmlDirectiveType::None)));
}
