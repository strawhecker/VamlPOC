#pragma once
// S280 (dep:S255) — vamlcc compiler skeleton: the `vamlcc` CLI reads a .vaml
// source and writes a compiled form, parsing via the S2 scanner (S080–S085
// XmlScanner + parse_vaml) and resolving against the S2 "tables": the
// ElementFactoryRegistry (S087, the type table), the ValueConverterRegistry
// (S089, the value table) and the DependencyProperty/PropertyRegistry global
// (S032–S039, the property table).
//
// This header owns the shared IN-MEMORY compiled form (CompiledProgram /
// ElementRecord / AttributeRecord) that the whole S280→S283 block refines IN
// PLACE (NO-GAP — never a throwaway, never a silent backfill):
//   * S280 — compile() produces this form from a parsed tree + the three
//     registries; write_program_text() renders a deterministic early form.
//   * S281/S282 — VbamlSchema + VbamlSerializer encode this SAME form to the
//     versioned .vbaml binary record set and decode it back byte-identically.
//   * S283 — compile_to_bytes() wires parser → record-emit → binary (the CLI
//     writes this onward).
//
// The compiler is single-threaded and pure (ADR-018: zero new locks). It
// lives in vaml-tools (the CORE layer, no Vulkan — ADR-011), and is exercised
// headlessly by vaml-tools-tests (ADR-021 consumer pattern; never a spawned
// subprocess in CI).
//
// ERROR MODEL — compile() throws VamlcException (a structured, line/col-carrying
// error) on ANY failure (parse, unknown element type, binding-invalid). The
// non-throwing convenience compile_text()/try_compile_file() catch and map that
// into a CompileResult so the CLI/test seam gets typed outcomes + exit codes
// without try/catch at the call site.
//
// RESOLUTION SEMANTICS (S280): each element's type token is its local_name
// (fallback tagName — the S088 resolution key) validated against the factory
// registry; an unknown type is a compile-time ResolveFailed naming type+line.
// Each non-extension attribute is checked against the property table
// (knownProperty) and its value recognized via the value table (valueType, ""
// when unrecognized). A {Binding}-classified extension is recorded isBinding +
// bindingPath (the S252 BindingMarkup path) and handed to BindingValidator
// (S280_1) for static validation.

#include "vaml/tools/export.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::tools {

// ---------------------------------------------------------------------------
// S280 — the compiler's error taxonomy (contains only what vamlcc can produce).
// ---------------------------------------------------------------------------
enum class VamlcError : std::uint8_t
{
    None,           // no error
    ReadFailed,     // the input .vaml file could not be read
    ParseFailed,    // S085 scanner/parser rejected the source (structured line/col)
    ResolveFailed,  // an element type has no factory in the S087 type table
    BindingInvalid, // S280_1: a static binding failed validation (typo / wrong hop)
    CompileFailed,  // a lower-level compile failure
};

// S280: a structured compiler error carrying the source line:col of the
// offending construct. Parallel to XmlParseException (S085) /
// ElementResolutionException (S088) at the vamlcc layer. what() renders a
// human-readable message naming the offending construct.
class VAML_TOOLS_EXPORT VamlcException : public std::runtime_error
{
public:
    VamlcException(VamlcError kind, std::string what,
                   std::size_t line, std::size_t col);

    [[nodiscard]] VamlcError kind() const noexcept { return kind_; }
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] std::size_t col() const noexcept { return col_; }

private:
    VamlcError  kind_;
    std::size_t line_;
    std::size_t col_;
};

// ---------------------------------------------------------------------------
// S280 — the shared in-memory compiled form (refined in place by S280–S283).
// ---------------------------------------------------------------------------

// One resolved attribute of a compiled element.
struct AttributeRecord
{
    std::string key;         // full attribute name (e.g. "Orientation", "Grid.Row")
    std::string value;       // decoded attribute value string (as authored)
    bool isExtension = false; // S084: value was a MarkupExtension ({Binding ...})
    bool isBinding   = false; // S252: a {Binding Path} extension (kept for compile)
    std::string bindingPath; // S252/S280_1: the parsed path text ("" when not a binding)

    // S280 analysis annotations (NOT structural, NOT serialized by S282):
    bool knownProperty = false; // present in DependencyProperty::global()
    std::string valueType;      // value-table recognized type name ("" if none)
};

// One resolved element of the compiled program. `id` is the element's
// 0-based document-order index; `typeToken` is its resolved element type name;
// `children` holds child element ids (document order). `line`/`col` carry the
// 1-based source position (S085) so S280_1's compile-time errors can name the
// offending element's location.
struct ElementRecord
{
    std::size_t id = 0;
    std::string typeToken;
    std::vector<AttributeRecord> attributes;
    std::vector<std::size_t> children;
    std::size_t line = 1; // 1-based source line of the element's '<'
    std::size_t col  = 1; // 1-based source column
};

// The complete compiled program: every element in document order, `id` == index.
struct CompiledProgram
{
    std::vector<ElementRecord> elements;
};

// S280: aggregate compile counters (assert-heavy tests + CLI reporting).
struct VamlcCounts
{
    std::size_t elements        = 0; // element records
    std::size_t types           = 0; // DISTINCT resolved element type names
    std::size_t properties      = 0; // attributes that resolved in the property table
    std::size_t values          = 0; // attributes whose value the value table recognized
    std::size_t bindings        = 0; // {Binding Path} extension attributes
    std::size_t dynamicBindings = 0; // binding extensions deferred at a DYNAMIC root (S826_1)
};

// S280: the non-throwing compile outcome (what compile_text() returns).
struct VAML_TOOLS_EXPORT CompileResult
{
    bool        ok = false;
    VamlcError  kind = VamlcError::None;
    std::size_t line = 0; // 1-based, when kind != None (else 0)
    std::size_t col  = 0;
    std::string detail; // human-readable when kind != None
    VamlcCounts counts;
};

// ---------------------------------------------------------------------------
// S280 — the compile entry points (single-threaded; ADR-018 zero locks).
// ---------------------------------------------------------------------------

// Parses `source` (S085 parse_vaml) and resolves it against the three S2
// registries into a CompiledProgram. Populates the registries as needed
// (register_builtin_elements + register_builtin_converters, and constructs
// each distinct element type once so its DPs self-register for validation).
// Throws VamlcException on any failure (parse / unknown type / invalid static
// binding).
[[nodiscard]] VAML_TOOLS_EXPORT CompiledProgram compile(std::string_view source);

// Non-throwing convenience: compiles `source` and returns a CompileResult
// (ok/counts on success; kind+line+col+detail on failure). Never throws.
[[nodiscard]] VAML_TOOLS_EXPORT CompileResult compile_text(std::string_view source) noexcept;

// Non-throwing file seam: reads the .vaml at `path` and compiles it. A read
// failure maps to ReadFailed; otherwise delegates to compile_text. The CLI
// layer builds on this. Never throws.
[[nodiscard]] VAML_TOOLS_EXPORT CompileResult try_compile_file(const std::string& path) noexcept;

// S280: the deterministic text rendering of the compiled program (the
// early-but-real "write compiled form"). Same input → identical output; ids and
// attributes are in document order. Refined to the binary form at S282/S283.
[[nodiscard]] VAML_TOOLS_EXPORT std::string write_program_text(const CompiledProgram& prog);

// S283 (dep:S282): Parser → record-emit. Compiles `source` (reusing the S2
// scanner + factory/value/property tables, no stubs) and emits the versioned
// .vbaml binary record set (S281 schema + S282 serializer). Throws
// VamlcException on failure (including the serializer's malformed-buffer
// guard — see VbamlSerializer).
[[nodiscard]] VAML_TOOLS_EXPORT std::vector<std::uint8_t> compile_to_bytes(std::string_view source);

// ---------------------------------------------------------------------------
// S280 — the headless CLI seam. args = {progName, inPath, outPath} (args[0]
// unused). Reads inPath, compiles, writes the .vbaml to outPath and returns:
//   0 = success
//   1 = usage (wrong argument count)
//   2 = input could not be read (or output could not be written)
//   3 = parse / resolve / binding / compile failure (reported to stderr)
// Pure and testable headlessly (ADR-021 — vaml-tools-tests drives it directly).
[[nodiscard]] VAML_TOOLS_EXPORT int run_vamlcc(const std::vector<std::string>& args);

} // namespace vaml::tools
