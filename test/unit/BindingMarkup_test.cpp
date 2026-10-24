// S252 (dep:S251) — BindingMarkup: the {Binding ...} / {Command ...} markup
// parser (the S2-tag resolution point). Pure value-domain — registers no
// property names. Shouldly vocabulary. ADR-018: zero locks.
//
// Covers the parse grammar + the outcome table of parse_markup_binding
// (BindingMarkup.h): Binding path extraction, Mode= variants, Command kind,
// StaticResource -> kind None, malformed {Binding}/{Command} -> structured
// malformed flag. The S252 reader-level binding (VamlReader::BindExtensions
// wiring the parsed markup to a live BindingExpression) lives in
// VamlReader_binding_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString (the SHOULD seam)

#include "vaml/core/BindingMarkup.h"

#include <string_view>

using vaml::BindingMarkup;
using vaml::MarkupExtensionKind;
using vaml::parse_markup_binding;

namespace {

int kind_i(MarkupExtensionKind k) { return static_cast<int>(k); }

} // namespace

// ---------------------------------------------------------------------------
// 1. {Binding <path>} — the headline: path extracted, kind Binding, no mode
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: {Binding Title} parses to a Binding path, no mode")
{
    const BindingMarkup m = parse_markup_binding("{Binding Title}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(m.hasMode ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyString(std::string(m.path)).ShouldBe(std::string("Title")));
}

TEST_CASE("S252 markup: {Binding } with trimmed path still parses")
{
    const BindingMarkup m = parse_markup_binding("{Binding Name}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(std::string(m.path)).ShouldBe(std::string("Name")));
}

// ---------------------------------------------------------------------------
// 2. {Binding <path>, Mode=...} — Mode variants
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: {Binding Title, Mode=TwoWay} parses the mode")
{
    const BindingMarkup m = parse_markup_binding("{Binding Title, Mode=TwoWay}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(std::string(m.path)).ShouldBe(std::string("Title")));
    SHOULD(ShouldlyIntegral(m.hasMode ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(m.mode)).ShouldBe(std::string("TwoWay")));
}

TEST_CASE("S252 markup: {Binding Title, Mode=OneTime} + OneWay parse")
{
    const BindingMarkup m1 = parse_markup_binding("{Binding Title, Mode=OneTime}");
    SHOULD(ShouldlyString(kind_i(m1.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(std::string(m1.mode)).ShouldBe(std::string("OneTime")));

    const BindingMarkup m2 = parse_markup_binding("{Binding Title, Mode=OneWay}");
    SHOULD(ShouldlyString(kind_i(m2.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(std::string(m2.mode)).ShouldBe(std::string("OneWay")));
}

TEST_CASE("S252 markup: whitespace inside the braces is tolerated")
{
    const BindingMarkup m = parse_markup_binding("{ Binding  Title ,  Mode = TwoWay }");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(std::string(m.path)).ShouldBe(std::string("Title")));
    SHOULD(ShouldlyIntegral(m.hasMode ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(m.mode)).ShouldBe(std::string("TwoWay")));
}

// ---------------------------------------------------------------------------
// 3. {Command <name>} — the S253 command extension
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: {Command DoIt} parses to a Command name")
{
    const BindingMarkup m = parse_markup_binding("{Command DoIt}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Command)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyString(std::string(m.path)).ShouldBe(std::string("DoIt")));
}

// ---------------------------------------------------------------------------
// 4. Non-binding naming extensions -> kind None (StaticResource, ...)
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: {StaticResource X} is kind None (not ours)")
{
    const BindingMarkup m = parse_markup_binding("{StaticResource SomeBrush}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::None)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(0));
}

TEST_CASE("S252 markup: {x:Null} is kind None")
{
    const BindingMarkup m = parse_markup_binding("{x:Null}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::None)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// 5. Malformed {Binding}/{Command} -> the malformed flag (Class A authoring)
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: {Binding} with no path is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Binding}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S252 markup: {Binding , Mode=TwoWay} (leading comma) is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Binding , Mode=TwoWay}");
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S252 markup: {Binding Title, Mode=Bogus} (bad Mode value) is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Binding Title, Mode=Bogus}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S252 markup: {Binding Title Extra} (no comma) is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Binding Title Extra}");
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S252 markup: {Binding Title,Mode} (Mode with no =) is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Binding Title,Mode}");
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S252 markup: {Command} with no name is malformed")
{
    const BindingMarkup m = parse_markup_binding("{Command}");
    SHOULD(ShouldlyString(kind_i(m.kind)).ShouldBe(kind_i(MarkupExtensionKind::Command)));
    SHOULD(ShouldlyIntegral(m.malformed ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 6. Non-extension inputs -> kind None (not owned by the binding layer)
// ---------------------------------------------------------------------------

TEST_CASE("S252 markup: empty / unbalanced input is kind None")
{
    SHOULD(ShouldlyString(kind_i(parse_markup_binding("").kind))
               .ShouldBe(kind_i(MarkupExtensionKind::None)));
    SHOULD(ShouldlyString(kind_i(parse_markup_binding("{}").kind))
               .ShouldBe(kind_i(MarkupExtensionKind::None)));
    SHOULD(ShouldlyString(kind_i(parse_markup_binding("plain").kind))
               .ShouldBe(kind_i(MarkupExtensionKind::None)));
}
