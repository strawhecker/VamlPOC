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
    std::string_view key;        // full raw attribute name (Grid.Row, Width, x:Name) — S140 interned
    std::string_view value;      // decoded value string — S140 interned
    bool isExtension = false; // true when value classified MarkupExtension (S084)

    // Extended (carried forward from the scanner, no re-scan); S140 interned:
    std::string_view prefix;                         // S082: "" when no ':' split
    std::string_view local_name;                     // S082: full name sans "prefix:"
    std::string_view namespace_uri;                  // S082: resolved (non-default) ns
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
    std::string_view tagName;     // full element name (StackPanel, local:Button) — S140 interned
    std::string_view ns;          // resolved namespace URI (empty when none) — S140 interned
    std::vector<ParsedAttribute> attributes;
    std::vector<ParsedElement> children;
    std::string content;     // text between the element's bounding tokens (as authored) — S140 interned

    // Extended (carried forward from the scanner); S140 interned:
    std::string_view prefix;      // S082: "" when no ':' split
    std::string_view local_name;  // S082: element name sans "prefix:"
    bool self_closing = false; // S080: <a/> versus <a></a>
    // S088: 1-based source position of the element's opening '<'. Carried from
    // the scanner's ElementOpen token (which already computed them — S085's
    // "carried, not re-scanned" discipline) so the S088 resolver / S091 reader
    // can report "unknown element type '<name>' (line N, column M)" without
    // re-deriving a line map. Default 1,1 matches the scanner's own start.
    std::size_t line = 1;
    std::size_t col = 1;
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
//
// S451 — the DOCUMENT/NAMESPACE/COMMENT grammar (the S450_1 .xaml acceptance
// elevated to a documented contract). parse_vaml is the single DOCUMENT-LEVEL
// choke point for both readers (S2 text + vamlcc's compiled recompile, which
// parses the same source through this function), and it accepts a real
// WPF-shaped document:
//   * ROOT IDENTITY — exactly ONE root element; a second root (post-close) or
//     NO root throws XmlParseException(UterterminatedElement). BOM + optional
//     `<?xml ...?>` prolog + leading whitespace are stripped at the document
//     level before scanning (never a tree node).
//   * NAMESPACE presentation — `xmlns` (default) + `xmlns:p` (prefixed) are
//     S082 namespace DECLARATIONS (not attributes), applied self-per-element;
//     an unprefixed element name resolves the DEFAULT ns, a prefixed one its
//     mapped URI; an unprefixed ATTRIBUTE is in NO namespace (XML rule);
//     `clr-namespace:Ns[;assembly=A]` is tokenized as a mapping (S082).
//   * x: DIRECTIVE namespace — an attribute whose RESOLVED URI is the canonical
//     XAML language namespace (kXamlLanguageNamespace) is classified as an
//     x: directive (Name/Class/Key/Null, else None) by S083. `x:Name` /
//     `x:Key` bookkeeping lives at the S2 reader (S092) / S7 compiler (S286).
//   * COMMENT discard — `<!-- -->` AND CDATA tokens are NOT tree nodes; both
//     paths (parse_vaml -> text reader; vamlcc -> compiled reader via the same
//     parse_vaml) drop them consistently, so a comment never appears in the
//     children/content of the built tree on EITHER reader. This confirms S080's
//     "S085 decides" hand-off as: dropped, identically, on both paths.
VAML_CORE_EXPORT ParsedElement parse_vaml(std::string_view source);

// ---------------------------------------------------------------------------
// S452 — the PROPERTY-ELEMENT grammar seam (the S366-recorded NO-GAP hand-off,
// RESOLVED here). A property-element is a child element whose tag is
// `Owner.Property` where Owner is the PARENT element's local type name; it sets
// the named Property onto the parent as a DP VALUE (the S043 attached-aware
// DP path), NOT as a visual child. This is the syntactic seam S453 (Resources /
// x:Key) and S454 (Style/Setter/Trigger) ride.
//
// Two pure, headless-testable helpers:
//   - `is_property_element(tagName)`  — true when the tag is a dotted name
//     `<Owner.Property>` (a '.' in the tag, which per S082 is NOT a namespace
//     separator). The element's `local_name` (sans any `prefix:`) is what is
//     inspected; a prefixed `<local:Owner.Property>` has local_name
//     "Owner.Property", which is still a property-element candidate resolved
//     against its local owner name.
//   - `split_property_element(tagName) -> (owner, prop)` — the owner (the
//     portion before the FIRST '.') and the property (the portion after).
//     A tag with no '.' is not a property-element (owner empty, prop empty).
//
// These are the shared, single authority the S2 text reader (VamlReader::build)
// AND the S7 compiler (vamlcc + VBamlReader) both use, so both parsers agree on
// exactly which dotted tags are property-elements (parity, not duplication).
[[nodiscard]] VAML_CORE_EXPORT bool is_property_element(std::string_view tagName) noexcept;
// Splits at the FIRST '.': <Owner.Property> -> (Owner, Property). When the tag
// has no '.', both views are empty (not a property-element). `tagName` should
// be the element's LOCAL name (sans any namespace prefix) so the owner matches
// the parent's local type name.
[[nodiscard]] VAML_CORE_EXPORT std::pair<std::string_view, std::string_view>
split_property_element(std::string_view tagName) noexcept;

} // namespace vaml
