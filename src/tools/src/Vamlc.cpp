// S280 (dep:S255) — vamlcc compiler skeleton. Reads a .vaml source and writes a
// compiled form. Parses via the S2 scanner (parse_vaml, S085) and resolves
// against the S2 "tables": ElementFactoryRegistry::global() (S087 type table),
// ValueConverterRegistry::global() (S089 value table) and
// DependencyProperty::global() (S032–S039 property table).
//
// S280_1 (dep:S280) — static binding validation is performed inline here via
// validate_bindings (BindingValidator.cpp): each {Binding Path} is validated
// against its static root type through the property registry (Q1=A); a typo /
// wrong hop type becomes a compile-time error naming line/col. Dynamic roots
// (ObservableObject data-context / untyped template item) are NOT validated
// here — they defer to bind-time Class A (S242_1 Strict) and are counted for
// the coverage ratio (S826_1). See BindingValidator.h for the full scope.
//
// S283 (dep:S282) — compile_to_bytes wires the parser → record-emit
// (compile()) → the S281 schema + S282 serializer binary (encode()). The CLI
// (vamlcc_main.cpp / run_vamlcc) writes that binary to the .vbaml output.
//
// Single-threaded + pure (ADR-018: zero locks). CORE layer (ADR-011: no
// Vulkan). Headless-testable via vaml-tools-tests (ADR-021).

#include "vaml/tools/Vamlc.h"
#include "vaml/tools/BindingValidator.h"
#include "vaml/tools/vbaml/VbamlSchema.h"
#include "vaml/tools/vbaml/VbamlSerializer.h"

#include "vaml/core/BindingMarkup.h"       // parse_markup_binding (isBinding detection)
#include "vaml/core/DependencyProperty.h"  // DependencyProperty::global read path
#include "vaml/core/Element.h"             // register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/ParsedElement.h"       // parse_vaml
#include "vaml/core/XmlScanner.h"          // XmlScanException (map to ParseFailed)
#include "vaml/core/ValueConverter.h"      // register_builtin_converters

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml::tools {

namespace {

// Builds the S281 schema (token tables + string table) from a compiled program
// in DOCUMENT order, so the schema is deterministic (insertion order).
vbaml::Schema build_schema(const CompiledProgram& prog)
{
    vbaml::SchemaBuilder builder;
    // Prepopulate the schema-complete event/resource token tables with their
    // display names (stable enum ordinals are the ids; the string table still
    // carries the readable names so the produced .vbaml is human-debuggable).
    (void)builder.event_token("Click");
    (void)builder.event_token("Changed");
    (void)builder.resource_token("Brush");
    (void)builder.resource_token("Color");

    for (const ElementRecord& e : prog.elements)
    {
        (void)builder.type_token(e.typeToken);
        for (const AttributeRecord& a : e.attributes)
        {
            (void)builder.prop_token(a.key);
            (void)builder.intern(a.value);
            if (a.isBinding)
            {
                (void)builder.intern(a.bindingPath);
            }
        }
    }
    return builder.flush();
}

// Resolves + annotates a single attribute on `rec` (the S280 analysis:
// property-table knownProperty, value-table valueType, and binding
// classification via the S252 BindingMarkup parser). `index` is the attribute's
// index (for diagnostics line/col — carried by the element, not per-attribute).
void annotate_attribute(AttributeRecord& ar)
{
    if (ar.isExtension)
    {
        const vaml::BindingMarkup mk = vaml::parse_markup_binding(ar.value);
        if (mk.kind == vaml::MarkupExtensionKind::Binding && !mk.path.empty())
        {
            ar.isBinding = true;
            ar.bindingPath = std::string(mk.path);
        }
    }
    else
    {
        ar.knownProperty = vaml::DependencyProperty::TryGet(ar.key) != nullptr;
        std::string inferred;
        ar.valueType = vaml::ValueConverterRegistry::global()
                           .try_infer_type(ar.value, &inferred)
                       ? inferred : "";
    }
}

// Emits a CompiledProgram from a parsed tree (the S280 record-emit), depth-first
// document order. Each element's id is its index; parent→child edges are wired
// once all ids exist. `parentId` is the parent's index (max for the root).
void emit_elements(const ParsedElement& node, CompiledProgram& prog, std::size_t parentId)
{
    const std::size_t id = prog.elements.size();
    ElementRecord rec;
    rec.id = id;
    rec.line = node.line;
    rec.col = node.col;

    // S088-style resolution key: local_name fallback tagName.
    rec.typeToken = node.local_name.empty() ? std::string(node.tagName)
                                            : std::string(node.local_name);
    if (!ElementFactoryRegistry::global().is_registered(rec.typeToken))
    {
        throw VamlcException(VamlcError::ResolveFailed,
                             "unknown element type '" + rec.typeToken + "'",
                             node.line, node.col);
    }
    // Constructs the type once so its DPs self-register (available to the
    // validator's property-table lookups). The instance is discarded — only
    // the registration side-effects matter.
    (void)ElementFactoryRegistry::global().create(rec.typeToken);

    // Resolve + annotate each attribute.
    rec.attributes.reserve(node.attributes.size());
    for (const ParsedAttribute& pa : node.attributes)
    {
        AttributeRecord ar;
        ar.key = std::string(pa.key);
        ar.value = std::string(pa.value);
        ar.isExtension = pa.isExtension;
        annotate_attribute(ar);
        rec.attributes.push_back(std::move(ar));
    }

    // Stage the element BEFORE recursing so its id is consumed by children.
    prog.elements.push_back(std::move(rec));

    for (const ParsedElement& child : node.children)
    {
        const std::size_t cid = prog.elements.size();
        emit_elements(child, prog, id);
        prog.elements[id].children.push_back(cid);
    }
    (void)parentId;
}

// Counts the aggregate diagnostics on an already-built program.
VamlcCounts tally(const CompiledProgram& prog)
{
    VamlcCounts c;
    std::vector<std::string> distinctTypes;
    for (const ElementRecord& e : prog.elements)
    {
        if (std::find(distinctTypes.begin(), distinctTypes.end(), e.typeToken) ==
            distinctTypes.end())
        {
            distinctTypes.push_back(e.typeToken);
        }
        for (const AttributeRecord& a : e.attributes)
        {
            if (a.knownProperty)
            {
                c.properties += 1;
            }
            if (!a.valueType.empty())
            {
                c.values += 1;
            }
            if (a.isBinding)
            {
                c.bindings += 1;
            }
        }
    }
    c.elements = prog.elements.size();
    c.types = distinctTypes.size();
    return c;
}

} // namespace

VAML_TOOLS_EXPORT
VamlcException::VamlcException(VamlcError kind, std::string what,
                               std::size_t line, std::size_t col)
    : std::runtime_error(std::move(what)), kind_(kind), line_(line), col_(col)
{
}

VAML_TOOLS_EXPORT
CompiledProgram compile(std::string_view source)
{
    // Startup registration (idempotent): the S2 type + value tables.
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Parse the source. The S2 scanner/parser throw their structured
    // exceptions (XmlScanException / XmlParseException) on malformed VAML;
    // map those to the vamlcc ParseFailed category (never let a raw scan/parse
    // exception escape the compile boundary — the CLI reports a typed error).
    ParsedElement root;
    try
    {
        root = vaml::parse_vaml(source);
    }
    catch (const vaml::XmlScanException& e)
    {
        throw VamlcException(VamlcError::ParseFailed, e.what(), e.line(), e.col());
    }
    catch (const vaml::XmlParseException& e)
    {
        throw VamlcException(VamlcError::ParseFailed, e.what(), e.line(), e.col());
    }

    CompiledProgram prog;
    (void)emit_elements(root, prog, std::numeric_limits<std::size_t>::max());

    // S280_1: validate every {Binding Path} against its static root.
    VamlcCounts counts = tally(prog);
    (void)validate_bindings(prog, counts);

    return prog;
}

VAML_TOOLS_EXPORT
CompileResult compile_text(std::string_view source) noexcept
{
    CompileResult r;
    try
    {
        CompiledProgram prog = compile(source);
        r.ok = true;
        r.kind = VamlcError::None;
        r.counts = tally(prog);
        for (const ElementRecord& e : prog.elements)
        {
            for (const AttributeRecord& a : e.attributes)
            {
                if (a.isBinding)
                {
                    const std::size_t dot = a.bindingPath.find('.');
                    const std::string root = (dot == std::string::npos)
                                                 ? "" : a.bindingPath.substr(0, dot);
                    if (!ElementFactoryRegistry::global().is_registered(root))
                    {
                        r.counts.dynamicBindings += 1;
                    }
                }
            }
        }
    }
    catch (const VamlcException& e)
    {
        r.ok = false;
        r.kind = e.kind();
        r.line = e.line();
        r.col = e.col();
        r.detail = e.what();
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.kind = VamlcError::CompileFailed;
        r.detail = e.what();
    }
    return r;
}

VAML_TOOLS_EXPORT
CompileResult try_compile_file(const std::string& path) noexcept
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        CompileResult r;
        r.ok = false;
        r.kind = VamlcError::ReadFailed;
        r.detail = "cannot open '" + path + "'";
        return r;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return compile_text(ss.str());
}

VAML_TOOLS_EXPORT
std::string write_program_text(const CompiledProgram& prog)
{
    std::string out;
    out += "vamlc compiled program\n";
    for (const ElementRecord& e : prog.elements)
    {
        out += "#" + std::to_string(e.id) + " " + e.typeToken + " {";
        for (std::size_t i = 0; i < e.attributes.size(); ++i)
        {
            const AttributeRecord& a = e.attributes[i];
            if (i)
            {
                out += ", ";
            }
            out += a.key;
            if (a.isBinding)
            {
                out += "={Binding " + a.bindingPath + "}";
            }
            else
            {
                out += "=\"" + a.value + "\"";
            }
        }
        out += "}\n";
    }
    return out;
}

namespace {

bool read_file(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

} // namespace

VAML_TOOLS_EXPORT
std::vector<std::uint8_t> compile_to_bytes(std::string_view source)
{
    CompiledProgram prog = compile(source);
    const vbaml::Schema schema = build_schema(prog);
    return vbaml::encode(prog, schema);
}

VAML_TOOLS_EXPORT
int run_vamlcc(const std::vector<std::string>& args)
{
    // args = {progName, inPath, outPath}; the program name is args[0] (ignored).
    if (args.size() < 3)
    {
        return 1; // usage
    }
    const std::string& inPath = args[1];
    const std::string& outPath = args[2];

    std::string source;
    if (!read_file(inPath, source))
    {
        return 2; // input unreadable
    }

    CompileResult r = compile_text(source);
    if (!r.ok)
    {
        return 3; // parse / resolve / binding / compile failure
    }

    std::vector<std::uint8_t> bytes = compile_to_bytes(source);
    if (!write_file(outPath, bytes))
    {
        return 2; // output unwritable
    }
    return 0; // success
}

} // namespace vaml::tools
