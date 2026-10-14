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
// NOTE on the terminal token name: "EndOfInput" (not "EOF") so the enum avoids
// the <cstdio> EOF macro — a real trap, not cosmetic.

#include "vaml/core/export.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

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

// One decoded attribute on an ElementOpen: (name, decoded value).
struct XmlAttribute
{
    std::string name;
    std::string value;
};

// A single scanned token. `name` is the tag name (open/close); `value` carries
// Text / Comment / CData content; `attributes` is only populated for
// ElementOpen; `self_closing` distinguishes <a/> from <a></a>. line/col are
// the 1-based position of the token's first character in the source.
struct XmlToken
{
    XmlTokenType type = XmlTokenType::EndOfInput;
    std::string name;
    std::string value;
    bool self_closing = false;
    std::vector<XmlAttribute> attributes;
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
};

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

    std::string_view input_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t col_ = 1;
    // Open-tag depth stack (non-self-closing names) for end-tag matching.
    std::vector<std::string> stack_;
};

} // namespace vaml
