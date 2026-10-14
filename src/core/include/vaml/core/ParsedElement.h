#pragma once
// S085 (dep:S084) — ParsedElement + ParsedAttribute: the tree that consumes the
// S080-S084 XML token stream into a lossless, readable hierarchy. This is the
// first Sprint-2 step that goes beyond a linear token list — it assembles the
// flat XmlToken sequence (elements, attributes, self-closing, comments, CDATA,
// text) into a ParsedElement tree.
//
// DESIGN — why the tree is "tags + children + content" (NOT tags + children):
//   Element body text between the bounding tokens is a real, authored thing in
//   XML ("<Text>Hello</Text>"). Smuggling it in as a pseudo-element (an empty
//   tagName sentinel inside `children`) would CONFLATE two genuinely different
//   kinds of node and force every consumer to type-dispatch on an ambiguous
//   sentinel. Instead the type system separates them: `children` holds
//   ParsedElements, `content` holds the element's own text. Readers get the
//   distinction for free, no sentinel inspection — the readable, non-
//   misunderstandable structure. Conscience note (honest, not silent): a
//   single `content` string plus an ordered `children` vector does not capture
//   INTERLEAVED text/child ORDERING (e.g. "<p>Hello <b>world</b> again</p>").
//   That is a consequence of the finalized tags+children+content shape; pure
//   in-order text is what S085's scope operates on, and full mixed-content
//   ordering (if ever needed) is a later, explicit decision — not this step.
//
// DESIGN — why the extended scanner metadata is CARRIED, not re-scanned:
//   The scanner (S082-S084) already classifies namespaces (prefix/local/uri),
//   x-language directives (S083), value grammar + numeric (S084). The tree
//   carries that metadata forward so S086/S087/S091 consume a complete,
//   non-lossy tree without re-scanning (NO-LAZY-LLM: don't discard computed
//   structure only to recompute later). The roadmap-titled fields
//   (tagName/ns/attributes/children/content and key/value/isExtension) are the
//   stable CONTRACT; the extended fields ride along as the scanner computed
//   them.
//
// AUTHORITY BOUNDARY (kept): unknown/unmapped prefix -> empty `ns` is NOT a
// parse error here (S082 semantics); namespace/type resolution authority is
// S087. S085 only reports STRUCTURAL tree failures, which are distinct from the
// scanner's malformed-markup failures.
//
// ERROR TAXONOMY — how S085 splits with the scanner:
//   The scanner (S081) already validates malformed MARKUP and throws
//   categorized XmlScanException (incl. XmlScanError::MismatchedEndTag). The
//   PARSER owns exactly the structural failure the scanner does NOT detect:
//   reaching end-of-input while an element is still open (`<a>` with no
//   `</a>`) -> XmlParseException with XmlParseError::UnterminatedElement.
//   (The scanner is iterative and returns EndOfInput at EOF without checking
//   its open-tag stack, so "unterminated element" is genuinely the parser's
//   path.) A parser-side XmlParseError::MismatchedEndTag is deliberately NOT
//   added: the scanner already enforces end-tag matching and throws first, so
//   such a value would be DEAD surface (S048 refactor-gate discipline) — the
//   taxonomy contains only what the parser itself can actually produce.
//
// MINIMAL REAL? No. ParsedElement/ParsedAttribute/parse_vaml are delivered
// complete for their declared scope and tested; S086 (round-trip) and later
// steps extend by normal feature progression — no sentinel, no silent backfill.

#include "vaml/core/export.h"
#include "vaml/core/XmlScanner.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// S085: parse-level structural error category — the PARSER's taxonomy,
// distinct from the scanner's XmlScanError (malformed markup). 1:1 per
// parser-malformed path, mirroring the S081 scanner contract at the next layer.
enum class XmlParseError
{
    None,                // no structural error
    UnterminatedElement, // end-of-input reached while an element was still open
};

// S085: one parsed attribute on a ParsedElement. `key` is the FULL raw attribute
// name (e.g. Grid.Row, Width, x:Name), `value` the decoded value string, and
// `isExtension` is true when the value was classified a MarkupExtension by S084
// (e.g. {Binding Title}). The extended fields carry the scanner's already-
// classified metadata so consumers never re-scan.
struct ParsedAttribute
{
    std::string key;        // full raw attribute name (Grid.Row, Width, x:Name)
    std::string value;      // decoded value string
    bool isExtension = false; // true when value classified MarkupExtension (S084)

    // Extended (carried forward from the scanner, no re-scan):
    std::string prefix;                         // S082: "" when no ':' split
    std::string local_name;                     // S082: full name sans "prefix:"
    std::string namespace_uri;                  // S082: resolved (non-default) ns
    XmlDirectiveType directive = XmlDirectiveType::None; // S083: x-language directive
    AttributeValueKind value_kind = AttributeValueKind::QuotedLiteral; // S084
    bool numeric = false;                       // S084: scalar numeric detected
};

// S085: one element of the parsed tree. `tagName` is the FULL element name
// (e.g. StackPanel, local:Button), `ns` the resolved namespace URI (empty when
// none), `attributes` the ordered attribute list, `children` the ordered child
// elements, and `content` the element's own text between its bounding tokens
// (kept as authored, incl. whitespace). Extended fields: the S082 prefix/local
// split of the element name + the self-closing flag.
struct ParsedElement
{
    std::string tagName;     // full element name (StackPanel, local:Button)
    std::string ns;          // resolved namespace URI (empty when none)
    std::vector<ParsedAttribute> attributes;
    std::vector<ParsedElement> children;
    std::string content;     // text between the element's bounding tokens (as authored)

    // Extended (carried forward from the scanner):
    std::string prefix;      // S082: "" when no ':' split
    std::string local_name;  // S082: element name sans "prefix:"
    bool self_closing = false; // S080: <a/> versus <a></a>
};

// S085: parse-level exception carrying an XmlParseError kind + the 1-based
// line:col where the structural failure was detected. Parallel to
// XmlScanException (S081) at the parser layer. Not exported (mirrors
// XmlScanException, whose class type is likewise not exported).
class XmlParseException : public std::runtime_error
{
public:
    XmlParseException(XmlParseError kind, std::string what, std::size_t line, std::size_t col);

    XmlParseError kind() const noexcept { return kind_; }
    std::size_t line() const noexcept { return line_; }
    std::size_t col() const noexcept { return col_; }

private:
    XmlParseError kind_;
    std::size_t line_;
    std::size_t col_;
};

// S085: parses a single-root VAML/XML source string into a ParsedElement tree.
// Drives XmlScanner (S080-S084) and assembles the tokens into a hierarchy:
//   * ElementOpen -> a ParsedElement carrying the scanner's resolved fields
//     (tagName, ns, prefix, local_name, self_closing) + its XmlAttributes
//     mapped into ParsedAttributes (key/value/isExtension + extended metadata);
//   * Text between an element's tokens accumulates into that element's
//     `content` (kept as authored);
//   * ElementClose closes the current element (end-tag matching is enforced by
//     the scanner, which throws XmlScanException on a mismatch);
//   * Comment and CData tokens are DROPPED (not tree nodes — the S080 hand-off
//     "the S085 parser decides whether to keep it" resolves to: drop them);
//   * EndOfInput while an element is still open -> XmlParseException with
//     XmlParseError::UnterminatedElement.
// Scanner-level malformed markup propagates as XmlScanException (S081). The
// returned element is the single root; a later root element is out of scope
// (VAML/XML require a single root — that is the documented contract here).
VAML_CORE_EXPORT ParsedElement parse_vaml(std::string_view source);

} // namespace vaml
