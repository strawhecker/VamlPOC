// S280 (dep:S255) — vamlcc CLI skeleton headless suite.
//
// The vamlcc compiler reads a .vaml source and writes a compiled form, parsing
// via the S2 scanner (parse_vaml) and resolving against the S2 "tables":
// ElementFactoryRegistry (S087 type table), ValueConverterRegistry (S089 value
// table) and DependencyProperty::global (S032–S039 property table).
//
// These tests exercise the REAL vaml-tools compiler seam (vaml::tools::compile)
// headlessly via vaml-tools-tests (ADR-021 — the same code the CLI runs), plus
// the deterministic text form (write_program_text) and the headless CLI entry
// (run_vamlcc) returns typed exit codes (read/parse/compile errors). Also
// proves determinism: the same input yields the same records.
//
// Registers no property names (the S091 reader owns conversion; this compiler
// only annotates). ADR-018 zero locks. Uses the registered built-in element
// vocabulary (register_builtin_elements) + the registration-time side effect of
// DP registration (StackPanel Orientatiation, Grid.Row/Column, ...).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h" // StringMaker + operator<< for enums
#include <vaml/assert/Shouldly.h>

#include "vaml/tools/Vamlc.h"

#include <string>
#include <vector>

using vaml::tools::CompileResult;
using vaml::tools::CompiledProgram;
using vaml::tools::VamlcError;

namespace {

int kind_i(VamlcError k) { return static_cast<int>(k); }

} // namespace

// ---------------------------------------------------------------------------
// 1. compile() resolves a golden doc against the three tables + record emit
// ---------------------------------------------------------------------------

TEST_CASE("S280: compile() produces non-empty records for a golden StackPanel/Grid doc")
{
    // A golden doc exercising the built-in element vocabulary + a literal
    // property (Grid.Row attached) + a binding extension. The binding targets
    // Orientation (a registered DP); the root "StackPanel" is a registered
    // element type, so the {Binding StackPanel.Orientation} hop validates
    // statically (S280_1).
    const std::string doc =
        "<StackPanel>"
        "  <Grid Grid.Row=\"1\" Orientation=\"{Binding StackPanel.Orientation}\">"
        "    <TextBlock/>"
        "  </Grid>"
        "</StackPanel>";

    const CompileResult r = vaml::tools::compile_text(doc);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    // S280: non-empty real records; the tree has 3 elements.
    SHOULD(ShouldlyIntegral(r.counts.elements).ShouldBe(3u));
    // One attribute value the value table recognizes: "1" (Int32).
    SHOULD(ShouldlyIntegral(r.counts.values).ShouldBe(1u));
    // One binding extension ({Binding StackPanel.Orientation}).
    SHOULD(ShouldlyIntegral(r.counts.bindings).ShouldBe(1u));
    // Two attributes resolve in the property table (Grid.Row + Orientation as
    // the binding target is an extension, so only Grid.Row is a plain prop).
    SHOULD(ShouldlyIntegral(r.counts.properties).ShouldBe(1u));
    // Three distinct types (StackPanel, Grid, Text).
    SHOULD(ShouldlyIntegral(r.counts.types).ShouldBe(3u));
}

// ---------------------------------------------------------------------------
// 2. compile() throws ResolveFailed on an unknown element type (S088-style)
// ---------------------------------------------------------------------------

TEST_CASE("S280: an unknown element type is a structured ResolveFailed with line/col")
{
    bool threw = false;
    try
    {
        (void)vaml::tools::compile("<BogusElement/>");
    }
    catch (const vaml::tools::VamlcException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(kind_i(e.kind()))
                   .ShouldBe(kind_i(vaml::tools::VamlcError::ResolveFailed)));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(1u));
        SHOULD(ShouldlyIntegral(e.col() > 0).ShouldBe(1));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // The non-throwing seam maps it to a typed CompileResult.
    const CompileResult r = vaml::tools::compile_text("<BogusElement/>");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(kind_i(r.kind)).ShouldBe(kind_i(VamlcError::ResolveFailed)));
}

// ---------------------------------------------------------------------------
// 3. Parse failure (malformed VAML) maps to ParseFailed with line/col
// ---------------------------------------------------------------------------

TEST_CASE("S280: a malformed source maps to ParseFailed via the non-throwing seam")
{
    const CompileResult r = vaml::tools::compile_text("<StackPanel><Grid></StackPanel>");
    // The scanner rejects the mismatched end tag; compile_text maps it to
    // ParseFailed (never an uncaught throw).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(kind_i(r.kind)).ShouldBe(kind_i(VamlcError::ParseFailed)));
}

// ---------------------------------------------------------------------------
// 4. Determinism: the same input yields identical compiled records
// ---------------------------------------------------------------------------

TEST_CASE("S280: compile is deterministic — same input, same text form")
{
    const std::string doc = "<StackPanel><Grid Orientation=\"Horizontal\"/></StackPanel>";
    const CompileResult r1 = vaml::tools::compile_text(doc);
    const CompileResult r2 = vaml::tools::compile_text(doc);
    SHOULD(ShouldlyIntegral(static_cast<int>(r1.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r2.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r1.counts.elements).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(r2.counts.elements).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(r1.counts.types).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(r2.counts.types).ShouldBe(2u));
}

// ---------------------------------------------------------------------------
// 5. write_program_text — the deterministic early "write compiled form"
// ---------------------------------------------------------------------------

TEST_CASE("S280: write_program_text renders the compiled program deterministically")
{
    const std::string doc = "<StackPanel><TextBlock Text=\"hi\"/></StackPanel>";
    const CompileResult r = vaml::tools::compile_text(doc);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // The text form is produced from the same records (id=0 StackPanel, id=1 Text).
    const CompiledProgram prog = vaml::tools::compile(doc);
    const std::string text = vaml::tools::write_program_text(prog);
    // Deterministic + names the elements in document order.
    SHOULD(ShouldlyIntegral(text.find("#0 StackPanel") != std::string::npos).ShouldBe(1));
    SHOULD(ShouldlyIntegral(text.find("#1 Text") != std::string::npos).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 6. The headless CLI seam (run_vamlcc) returns typed exit codes
// ---------------------------------------------------------------------------

TEST_CASE("S280: run_vamlcc usage error returns 1 (no input paths)")
{
    const int code = vaml::tools::run_vamlcc({"vamlcc"});
    SHOULD(ShouldlyIntegral(code).ShouldBe(1));
}

TEST_CASE("S280: run_vamlcc unreadable input returns 2")
{
    const int code = vaml::tools::run_vamlcc({"vamlcc", "C:/definitely/missing.vaml", "out.vbaml"});
    SHOULD(ShouldlyIntegral(code).ShouldBe(2));
}

TEST_CASE("S280: try_compile_file returns ReadFailed for a missing file")
{
    const CompileResult r = vaml::tools::try_compile_file("C:/definitely/missing.vaml");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(kind_i(r.kind)).ShouldBe(kind_i(VamlcError::ReadFailed)));
}
