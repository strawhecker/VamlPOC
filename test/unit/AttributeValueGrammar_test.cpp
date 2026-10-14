// S084 (dep:S083) — ATTRIBUTE VALUE GRAMMAR tests.
//
// Proves the S084 value classification at BOTH layers (see XmlScanner.h):
//   * classify_attribute_value (the free classifier): quoted literal vs
//     markup-extension {…} tagged; numeric detection flag.
//   * scanner integration: every scanned XmlAttribute carries value_kind +
//     numeric, including S083 x: directive values and nested/extended values.
//
// Grammar under test:
//   * QuotedLiteral — the default; a plain decoded quoted string.
//   * MarkupExtension — value begins with '{' and ends with a BALANCED '}'
//     (nesting honored): {Binding X} -> MarkupExtension; "{abc" (unbalanced) ->
//     NOT -> QuotedLiteral. Tagged so S085 sets isExtension=true.
//   * Numeric — a QuotedLiteral whose scalar grammar parses as a number:
//     [-+]?(digits | digits.digits | .digits)[eE][-+]?digits. "100", "-45",
//     "3.14", "+2.5E-3" -> numeric; "Hello", "auto" -> not.
//
// Boundary (recorded NO-GAP, not silent): scalar detection only — the
// comma-list thickness form ("10,20") is S089's converter concern, NOT flagged
// numeric here, and is not silently claimed.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/XmlScanner.h"

#include <string>
#include <vector>

using vaml::AttributeValueKind;
using vaml::classify_attribute_value;
using vaml::XmlScanner;
using vaml::XmlToken;
using vaml::kXamlLanguageNamespace;

namespace {

// Drain a scanner into a vector of XmlToken (keeps S084 value_kind/numeric
// fields directly visible).
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
// classify_attribute_value: quoted literal vs markup extension
// ---------------------------------------------------------------------------

TEST_CASE("S084: plain quoted literal classifies as QuotedLiteral")
{
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("Hello")))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("auto")))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("")))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
}

TEST_CASE("S084: {Binding X} classifies as MarkupExtension")
{
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("{Binding Title}")))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("{Binding}")))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
}

TEST_CASE("S084: a leading-{ without a balanced closing } is NOT an extension")
{
    // "{abc" is unbalanced (no closing brace) -> QuotedLiteral, per plan.
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("{abc")))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    // A lone "{" alone (too short) is also not an extension.
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("{")))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
}

TEST_CASE("S084: nested braces inside a markup extension are honored (balanced)")
{
    // The grammar balances nested {}: {x:Null} and a CommandParameter with a
    // brace-pair inside both end on a balanced }.
    SHOULD(ShouldlyIntegral(static_cast<int>(classify_attribute_value("{x:Null}")))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
    SHOULD(ShouldlyIntegral(static_cast<int>(
                               classify_attribute_value("{StaticResource {x:Type Button}}")))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
}

// ---------------------------------------------------------------------------
// classify_attribute_value: numeric detection
// ---------------------------------------------------------------------------

TEST_CASE("S084: numeric detection — integral forms")
{
    bool num = false;
    classify_attribute_value("100", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
    num = false;
    classify_attribute_value("-45", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
    num = false;
    classify_attribute_value("+7", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
}

TEST_CASE("S084: numeric detection — fractional + exponent forms")
{
    bool num = false;
    classify_attribute_value("3.14", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
    num = false;
    classify_attribute_value(".5", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
    num = false;
    classify_attribute_value("+2.5E-3", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
    num = false;
    classify_attribute_value("1e6", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(1));
}

TEST_CASE("S084: numeric detection — non-numeric literals are not numeric")
{
    bool num = true;
    classify_attribute_value("Hello", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
    num = true;
    classify_attribute_value("auto", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
    // Comma-list thickness form is NOT scalar-numeric (S089 concern).
    num = true;
    classify_attribute_value("10,20", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
}

TEST_CASE("S084: numeric edge cases — trailing dot and bare sign are not numeric")
{
    bool num = true;
    classify_attribute_value("1.", &num); // trailing '.' with no digit
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
    num = true;
    classify_attribute_value("-", &num); // bare sign
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
    num = true;
    classify_attribute_value("", &num); // empty
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
}

TEST_CASE("S084: a markup extension is never numeric at the classifier layer")
{
    // {Binding X} is NOT numeric (its contents resolve later — S252/S253).
    bool num = true;
    classify_attribute_value("{Binding Width}", &num);
    SHOULD(ShouldlyIntegral(static_cast<int>(num)).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// Scanner integration
// ---------------------------------------------------------------------------

TEST_CASE("S084: scanned quoted literal carries QuotedLiteral + numeric flag")
{
    const auto seq = drainTokens("<Button Width=\"100\" Text=\"Hello\"/>");
    const XmlToken& t = seq[0];

    // Width="100" — quoted literal, numeric detected.
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].numeric)).ShouldBe(1));

    // Text="Hello" — quoted literal, NOT numeric.
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].numeric)).ShouldBe(0));
}

TEST_CASE("S084: scanned markup extension {Binding Title} is tagged MarkupExtension")
{
    const auto seq = drainTokens("<Text Text=\"{Binding Title}\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyString(t.attributes[0].value).ShouldBe(std::string("{Binding Title}")));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
    // Not numeric (extension contents resolve later).
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].numeric)).ShouldBe(0));
}

TEST_CASE("S084: scanned nested-brace extension is tagged MarkupExtension")
{
    const auto seq = drainTokens("<Button CommandParameter=\"{StaticResource {x:Type Button}}\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
}

TEST_CASE("S084: scanned unbalanced leading-{ stays QuotedLiteral")
{
    // "{abc" — unbalanced, no closing brace -> quoted literal, not numeric.
    const auto seq = drainTokens("<Text Text=\"{abc\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].numeric)).ShouldBe(0));
}

TEST_CASE("S084: float extension value + sign/exponent numeric on the scanner")
{
    const auto seq = drainTokens("<Root X=\"-3.5\" Y=\"+2.5E-3\"/>");
    const XmlToken& t = seq[0];
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].numeric)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[1].numeric)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
}

// ---------------------------------------------------------------------------
// Uniformity across S083 x: directive values
// ---------------------------------------------------------------------------

TEST_CASE("S084: x: directive value {x:Null} is tagged MarkupExtension")
{
    // x maps to the real xaml URI -> the value {x:Null} is recognized as a
    // markup extension (S084), even though S083 tags directive=Null.
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Null=\"{x:Null}\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(vaml::XmlDirectiveType::Null)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
}

TEST_CASE("S084: x: directive value plain text is a QuotedLiteral")
{
    // x:Name="foo" is a quoted literal (S083 directive + S084 QuotedLiteral).
    const auto seq = drainTokens(
        "<Root xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Name=\"foo\"/>");
    const XmlToken& t = seq[0];

    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].directive))
               .ShouldBe(static_cast<int>(vaml::XmlDirectiveType::Name)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    SHOULD(ShouldlyIntegral(static_cast<int>(t.attributes[0].numeric)).ShouldBe(0));
}

TEST_CASE("S084: value_kind and numeric are per-token (no leakage across siblings)")
{
    const auto seq = drainTokens("<a><b W=\"5\"/><c T=\"{Binding X}\"/></a>");

    // seq[1] = <b W="5"/> — numeric, quoted literal.
    SHOULD(ShouldlyIntegral(seq[1].attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[1].attributes[0].numeric)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[1].attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));

    // seq[2] = <c T="{Binding X}"/> — extension, not numeric.
    SHOULD(ShouldlyIntegral(seq[2].attributes.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[2].attributes[0].value_kind))
               .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seq[2].attributes[0].numeric)).ShouldBe(0));
}
