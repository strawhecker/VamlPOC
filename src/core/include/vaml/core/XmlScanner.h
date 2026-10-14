#pragma once
// S080 (dep:S049) — hand-rolled XML scanner, Sprint 2 first feature.
//
// Tokenizes the six constructs the S080 title names — elements, attributes,
// self-closing tags, comments, CDATA, entities — plus nested tags, all over a
// single std::string_view, with a coarse (markup-level) token model:
//
//   ElementOpen  — tag name + decoded attribute list + self_closing flag
//                  (handles <a>, <a b="c">, and <a/>)      [elements, attrs, self-closing]
//   ElementClose — </name> (name kept for nesting checks)   [nested tags]
//   Text         — character data between elements, entities decoded inline  [entities]
//   Comment      — <!-- ... -->, content NOT entity-decoded, multi-line       [comments]
//   CData        — <![CDATA[ ... ]]>, raw content, no entity expansion        [CDATA]
//   EndOfInput   — the terminal token (next() returns false and emits it)
//
// DECODING RULES (declared scope):
//   * The 5 predefined entities (&amp; &lt; &gt; &quot; &apos;) plus numeric
//     refs (&#65; decimal, &#x41; hex) are decoded inline into BOTH Text
//     content and AttributeValue. There is NO separate EntityRef token —
//     attributes are single-value, so entities must expand inside the value
//     anyway; a distinct token would be redundant parser surface (NO-LAZY-LLM).
//   * Comment and CData content is returned RAW (never entity-decoded) —
//     XML semantics.
//
// NESTING: the scanner is iterative, but it tracks a depth stack of open tag
// names so a stray/mismatched end tag is a REAL error (you cannot scan nested
// tags honestly while ignoring a mismatched close). Any structural failure the
// scanner must detect (unterminated comment/CDATA/attribute quote/entity,
// malformed tag, unmatched end tag) throws XmlScanException — a STRUCTURED,
// categorized exception carrying the 1-based line:col where the malformed
// input was detected. The taxonomy (XmlScanError) and guaranteed line/col were
// formalized by S081, consuming S080's documented NO-GAP hand-off (S080
// delivered message-only XmlScanException + tokens already carrying line:col).
// Every malformed path has a distinct XmlScanError kind (list below) and a
// line()/col() >= 1 — the core promise of S081.
//
// MINIMAL REAL? No. XmlScanner is delivered complete for its declared scope
// (all six listed constructs real + tested), not a seed. S081 formalized the
// structured error taxonomy; later steps (S082 namespaces, S083 x: directives,
// S084 attribute grammar, S086 goldens) extend this type by normal feature
// progression — no sentinel, nothing left for silent backfill.
//
// S082 (dep:S081) — NAMESPACES. The scanner became namespace-aware:
//   * Names split on the FIRST ':' only into (prefix, local_name). The '.'
//     (attached-owner "Grid.Row") is NOT a namespace separator — only ':' is.
//   * `name` (and XmlAttribute::name) remain the FULL raw text, so S080's
//     assertions hold unchanged; prefix/local_name/namespace_uri are ADDED.
//   * A per-element namespace SCOPE is tracked (default `xmlns="uri"` plus
//     prefixed `xmlns:p="uri"`), pushed on non-self-closing ElementOpen and
//     popped on the matching ElementClose. Unprefixed ELEMENTS resolve to the
//     default namespace; the default does NOT apply to ATTRIBUTES (XML rule),
//     so an unprefixed attribute has no namespace_uri.
//   * `xmlns` / `xmlns:p` are NAMESPACE DECLARATIONS, not ordinary attributes:
//     they are collected into XmlToken::xmlns and applied to scope, never
//     emitted in `attributes`.
//   * A WPF-style "clr-namespace:Ns.Name;assembly=Asm" mapping value is
//     TOKENIZED into ClrNamespaceReference via parse_clr_namespace (parts
//     extracted; TYPE resolution is S087's step). A malformed clr-namespace
//     value is a categorized XmlScanError::MalformedClrNamespace (keeps the
//     S081 1:1 taxonomy contract honest).
// Unmapped prefixes / unknown namespaces are NOT scan errors at S082 — the
// structured "unknown namespace"/type-resolution authority is the S085/S087
// parser (recorded NO-GAP hand-off, not a silent omission).
//
// S083 (dep:S082) — X-LANGUAGE DIRECTIVES. The scanner recognizes the four XAML
// x-language directives — Name, Class, Key, Null — as a DISTINCT, structured
// token category, layering on S082's namespace model (prefix/local split +
// resolved namespace_uri).
//   * Classification is keyed on the resolved NAMESPACE URI, not the literal
//     "x" prefix. The x-language namespace is defined by its canonical URI
//     (kXamlLanguageNamespace below); ANY prefix mapped to that URI (via
//     xmlns:p="<xaml-uri>") is the x-language — so `x:Name` AND `foo:Name`
//     (when foo maps to the xaml URI) are both Name directives. Conversely, a
//     prefix bound to a NON-xaml URI (e.g. S082's xmlns:x="urn:vm:ns:x") is
//     NOT a directive — the URI, not the letter, is authoritative.
//   * Case-sensitive local matching: Name / Class / Key / Null (XAML is
//     case-sensitive); any OTHER local in the xaml namespace maps to
//     XmlDirectiveType::None.
//   * A directive is tagged ON the XmlAttribute as `directive`, keeping the
//     single ordered `attributes` list intact (value stays in `value`); the
//     S085 parser checks `directive != None`. Deliberate over a separate
//     directives vector (which would carry values and require re-merging with
//     ordinary attributes, losing ordering — NO-LAZY-LLM: classify-in-place is
//     the minimal faithful option).
//   * TOKENIZATION ONLY — semantic application is later steps (recorded
//     NO-GAP, not silent): x:Name name-registry -> S092; x:Class root-owner
//     binding -> S092/S089; x:Key resource-key -> S361; x:Null null-set (as
//     markup-extension {x:Null} -> S084 tagging). Element-level directives are
//     NOT special-cased here (an element already in the language ns surfaces
//     via S082's namespace_uri and is parsed by S085 — recorded).
//
// NOTE on the terminal token name: "EndOfInput" (not "EOF") so the enum avoids
// the <cstdio> EOF macro — a real trap, not cosmetic.

#include "vaml/core/export.h"

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// S083: the canonical XAML x-language namespace URI. Any attribute whose
// RESOLVED namespace_uri equals this is examined for a directive (Name/Class/
// Key/Null). Exposed as a named constant so scan/parse/resource code speaks
// the URI, not the letter "x" (which, per S082, is just one possible binding).
inline constexpr std::string_view kXamlLanguageNamespace =
    "http://schemas.microsoft.com/winfx/2006/xaml";

// S083: the x-language directive type of an attribute. None = NOT one of the
// four x-language directives (the default). The four are Name, Class, Key,
// Null — matched CASE-SENSITIVELY against local_name when the attribute's
// resolved namespace_uri == kXamlLanguageNamespace.
enum class XmlDirectiveType
{
    None,  // not an x-language directive (ordinary attribute, or unknown local in the xaml ns)
    Name,  // x:Name — name-registry binding (applied S092)
    Class, // x:Class — root-owner class binding (applied S092/S089)
    Key,   // x:Key — resource-dictionary key (applied S361)
    Null,  // x:Null — explicit null value (markup-extension form {x:Null} tagged S084)
};

// Coarse XML token kind. Order is arbitrary (S080's model is coarse by
// design — the S085 ParsedElement consumes these).
enum class XmlTokenType
{
    ElementOpen,
    ElementClose,
    Text,
    Comment,
    CData,
    EndOfInput,
};

// One decoded attribute on an ElementOpen: (name, decoded value) plus the
// S082 namespace split (prefix / local_name / resolved namespace_uri). `name`
// stays the FULL raw text; prefix/local are the split on the first ':'.
// `directive` (S083) is the x-language directive type when the resolved
// namespace_uri == kXamlLanguageNamespace (None otherwise).
struct XmlAttribute
{
    std::string name;
    std::string value;
    std::string prefix;      // "" if no ':' — S082
    std::string local_name;  // full name sans the "prefix:" part — S082
    std::string namespace_uri; // resolved from the (non-default) prefix — S082
    XmlDirectiveType directive = XmlDirectiveType::None; // S083
};

// A WPF-style "clr-namespace:Ns.Name[;assembly=Asm]" mapping, TOKENIZED into
// parts (S082). The parts are extracted here; resolving them to a real type is
// S087's step (recorded NO-GAP, not a silent omission).
struct ClrNamespaceReference
{
    std::string clrNamespace; // e.g. "Ns.Name" (may be empty if malformed)
    std::string assembly;     // e.g. "Asm" (empty when no ';assembly=' present)
};

// A single scanned token. `name` is the tag name (open/close); `value` carries
// Text / Comment / CData content; `attributes` is only populated for
// ElementOpen (namespace-declaration xmlns/xmlns:p excluded — they live in
// `xmlns`); `self_closing` distinguishes <a/> from <a></a>. line/col are
// the 1-based position of the token's first character in the source.
struct XmlToken
{
    XmlTokenType type = XmlTokenType::EndOfInput;
    std::string name;
    std::string value;
    bool self_closing = false;
    std::vector<XmlAttribute> attributes;
    // S082: namespace declarations (xmlns / xmlns:prefix) seen on this
    // ElementOpen, as (declaredPrefix, uri) pairs — prefix "" = default.
    std::vector<std::pair<std::string, std::string>> xmlns;
    // S082: `name` split on the first ':' (prefix "" when none). `.` is NOT a
    // namespace separator. Prefixed element -> namespace_uri resolved from the
    // prefixed scope; unprefixed -> the DEFAULT namespace (may be "").
    std::string prefix;
    std::string local_name;
    std::string namespace_uri;
    std::size_t line = 0;
    std::size_t col = 0;
};

// Structured scan-error category. Each value maps 1:1 to a distinct malformed
// scan path (S081 formalized taxonomy). A catch site can switch on kind()
// without string-matching the human message. Guaranteed 1:1 with the throw
// sites in XmlScanner.cpp — no silent/uncovered malformed path (NO-GAP).
enum class XmlScanError
{
    ExpectedName,               // '<' then end/illegal name start
    UnterminatedEntity,         // '&' at EOF with no body
    InvalidNumericReference,    // '&#'/'&#x' with no digits
    UnterminatedNumericReference, // numeric ref missing ';'
    CharacterReferenceOutOfRange, // code point > 0xFFFF (S080 UTF-8 supports BMP)
    UnknownEntity,              // named entity not one of amp/lt/gt/quot/apos
    UnterminatedAttributeValue, // attribute quote never closed
    UnterminatedComment,        // '<!--' never '-->'
    UnterminatedCData,          // '<![CDATA[' never ']]>'
    UnterminatedStartTag,       // start tag never '>' nor '/>'
    MalformedSelfClosing,       // '/' not followed by '>'
    MalformedStartTag,          // illegal char where attr or '>' expected
    AttributeMissingEquals,     // attr name not followed by '='
    AttributeValueNotQuoted,    // after '=' value not "..." or '...'
    MalformedEndTag,            // '</name' not followed by '>'
    MismatchedEndTag,           // '</name>' doesn't match top of open stack
    UnexpectedEndAfterLessThan, // '<' at EOF (no markup follows)
    UnrecognizedMarkup,         // '<!' not '<!--' nor '<![CDATA['
    MalformedClrNamespace,      // 'clr-namespace:' with no namespace/assembly body (S082)
};

// Tokenizes a "clr-namespace:" mapping value into ClrNamespaceReference.
// Recognizes `clr-namespace:Ns.Name` and `clr-namespace:Ns.Name;assembly=Asm`.
// `isClrNamespace` (optional out) is true iff `value` carried the
// "clr-namespace:" prefix (a plain value yields clrNamespace == value with
// isClrNamespace == false). A MALFORMED clr-namespace body (empty namespace,
// or a ";" suffix that isn't a well-formed ";assembly=Asm") throws
// XmlScanException with kind MalformedClrNamespace — the S082 throw site that
// keeps the S081 1:1 taxonomy contract honest. `line`/`col` (default 1,1) are
// the reported position; the scanner passes its real cursor. Extraction only —
// resolving a mapping to a real type is S087 (recorded NO-GAP).
VAML_CORE_EXPORT ClrNamespaceReference parse_clr_namespace(std::string_view value,
                                                           bool* isClrNamespace = nullptr,
                                                           std::size_t line = 1,
                                                           std::size_t col = 1);

// Thrown on any structural scan failure. Structurally categorized (kind()) with
// the 1-based line:col where the malformed input was detected (line()/col(),
// both >= 1). what() appends the position for human-readable diagnostics.
// Base type is std::runtime_error, so a bare `catch (const std::runtime_error&)`
// still works. S081 replaced S080's message-only exception with this taxonomy.
class XmlScanException : public std::runtime_error
{
public:
    XmlScanException(XmlScanError kind, std::string what, std::size_t line, std::size_t col);

    XmlScanError kind() const noexcept { return kind_; }
    // 1-based position where the malformed input was detected (>= 1 always).
    std::size_t line() const noexcept { return line_; }
    std::size_t col() const noexcept { return col_; }

private:
    XmlScanError kind_;
    std::size_t line_;
    std::size_t col_;
};

// Hand-rolled XML scanner over a std::string_view. NOT pimpl'd: its state is
// three POD scalars + an owned depth stack, and it is a cold (non-hot-path)
// tokenizer, so a hidden impl would be ceremony without benefit.
class VAML_CORE_EXPORT XmlScanner
{
public:
    explicit XmlScanner(std::string_view input);

    // Scans the next token into `out`. Returns true while a real token was
    // produced; on reaching the end of input it emits XmlTokenType::EndOfInput
    // into `out` and returns false. Throws XmlScanException on a structural
    // error.
    bool next(XmlToken& out);

private:
    // Returns the current 1-based line/col of the NEXT unconsumed character.
    std::size_t line() const noexcept { return line_; }
    std::size_t col() const noexcept { return col_; }

    bool atEnd() const noexcept { return pos_ >= input_.size(); }
    char peek(std::size_t offset = 0) const noexcept;
    // Consumes one character, updating line/col. '\n' is the line terminator.
    char advance();

    void skipWhitespace();
    bool startsWith(const char* s) const noexcept;

    // Scans a name (letters / digits / _ - : .). First char must be a name
    // start letter or one of the allowed punctuation.
    std::string scanName();

    // Scans markup after the '(' consumed. Returns true (a token was emitted);
    // never reaches EOF without a token or an exception.
    bool scanMarkup(XmlToken& out);

    void scanComment(std::string& content);
    void scanCData(std::string& content);
    void scanElementOpen(XmlToken& out);
    void scanElementClose(XmlToken& out);
    void scanText(XmlToken& out);

    // Entity-decode at a '&' (peek() == '&' precondition) into `target`.
    void appendDecodedEntity(std::string& target);
    // Attribute value up to the matching `quote` (entities decoded inline).
    std::string scanAttributeValue(char quote);

    // Centralized structured-throw helper (S081). Captures the current
    // line()/col() and throws XmlScanException with the given kind + message.
    // [[noreturn]] so the compiler knows throw sites never fall through.
    [[noreturn]] void fail(XmlScanError kind, const std::string& message) const;

    // S082: assigns `name` split on the first ':' into (prefix, local). The '.'
    // attached-owner form ("Grid.Row") is NOT split — only ':' is; prefix is ""
    // when there is no ':'.
    static void splitName(const std::string& name, std::string& prefix, std::string& local);
    // S082: resolves a (already split) prefix to a namespace URI from the
    // innermost scope. "" prefix -> the DEFAULT namespace (unprefixed element);
    // unmapped prefix -> "" (unknown-ns authority is S085/S087, recorded).
    const std::string& resolvePrefix(const std::string& prefix) const;

    std::string_view input_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t col_ = 1;
    // Open-tag depth stack (non-self-closing names) for end-tag matching.
    std::vector<std::string> stack_;
    // S082: per-element namespace scope stack, one frame per open (non-
    // self-closing) element. Each frame maps declared prefix -> uri, including
    // "" for the DEFAULT namespace. Mirrors stack_ depth (push on ElementOpen,
    // pop on matching ElementClose).
    std::vector<std::map<std::string, std::string>> nsStack_;
};

} // namespace vaml
