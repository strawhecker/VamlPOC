// S085 (dep:S084) — ParsedElement/ParsedAttribute assembly. Drives XmlScanner
// (S080-S084) and converts the flat XmlToken stream into a ParsedElement tree.
// See ParsedElement.h for the design rationale (tags+children+content,
// scanner metadata carried forward, authority boundary, error-taxonomy split).
#include "vaml/core/ParsedElement.h"

#include "vaml/core/Intern.h" // S140: pool-backed string_view tree text

#include <string>
#include <utility>
#include <vector>

namespace vaml {

// S085/S140: a live element build-site. `el` is the element being assembled;
// `contentScratch` is the owned accumulation buffer for the element's text —
// interned into `el.content` (string_view) at finalization (ElementClose or a
// self-closing open), because a single content string_view cannot accumulate.
struct ParseFrame
{
    ParsedElement el;
    std::string contentScratch;
};

namespace {

// S085: maps one scanned XmlAttribute into its ParsedAttribute, carrying the
// scanner's already-classified metadata (value_kind/numeric/directive/
// namespace) forward so consumers never re-scan (NO-LAZY-LLM).
ParsedAttribute toParsedAttribute(const XmlAttribute& a)
{
    ParsedAttribute out;
    out.key = a.name; // FULL raw attribute name (Grid.Row, Width, x:Name) — scanner already interned
    out.value = a.value;
    // isExtension: the roadmap-titled bool — true exactly when S084 classified
    // this value a MarkupExtension (e.g. {Binding Title}).
    out.isExtension = (a.value_kind == AttributeValueKind::MarkupExtension);
    // Carry the extended scanner metadata forward (already interned views).
    out.prefix = a.prefix;
    out.local_name = a.local_name;
    out.namespace_uri = a.namespace_uri;
    out.directive = a.directive;
    out.value_kind = a.value_kind;
    out.numeric = a.numeric;
    return out;
}

// S085/S088: copies the element-name metadata from a scanned ElementOpen token
// into a ParsedElement (tagName / ns / prefix / local_name / self_closing)
// plus the S088 source position (token's 1-based line/col) so the resolver /
// reader can name the type + line of an unknown element without re-scanning.
void applyElementIdentity(ParsedElement& el, const XmlToken& tok)
{
    el.tagName = tok.name;       // full element name (StackPanel, local:Button) — scanner interned
    el.ns = tok.namespace_uri;   // S082 resolved namespace URI (empty when none)
    el.prefix = tok.prefix;      // S082 split
    el.local_name = tok.local_name;
    el.self_closing = tok.self_closing;
    el.line = tok.line;          // S088: carried, not re-scanned
    el.col = tok.col;            // S088
}

// S085: installs a FINISHED element `child` either as the root (when the open
// stack is empty) or as a child of the current open element. Returns the open
// stack (may be unchanged or reduced by a self-closing child, which the caller
// handles separately). Sets haveRoot / throws on a second root.
void attach(ParsedElement&& child,
            std::vector<struct ParseFrame>& open,
            ParsedElement& root,
            bool& haveRoot, std::size_t line, std::size_t col)
{
    if (open.empty())
    {
        if (haveRoot)
        {
            // A second root after the first closed: single-root contract (see
            // header). Well-formed markup, but not a single-root document.
            throw XmlParseException(XmlParseError::UnterminatedElement,
                                    "document has more than one root element", line, col);
        }
        root = std::move(child);
        haveRoot = true;
    }
    else
    {
        open.back().el.children.push_back(std::move(child));
    }
}

} // namespace

XmlParseException::XmlParseException(XmlParseError kind, std::string what,
                                     std::size_t line, std::size_t col)
    : std::runtime_error(what + " (line " + std::to_string(line) + ", column " +
                         std::to_string(col) + ")")
    , kind_(kind)
    , line_(line)
    , col_(col)
{
}

ParsedElement parse_vaml(std::string_view source)
{
    // Frame-stack of LIVE build sites. Each non-self-closing ElementOpen pushes
    // a fresh frame; Text accumulates into the current frame's content SCRATCH
    // (S140: content is interned to a string_view at finalization, because a
    // single content view cannot accumulate multiple Text tokens);
    // ElementClose moves the finished frame into its parent (or the single
    // root). The open frames ARE the tree nodes being built — no reference-
    // invalidation hazard, no re-scan.
    std::vector<ParseFrame> open;

    ParsedElement root;
    bool haveRoot = false;

    XmlScanner scanner(source);
    XmlToken tok;
    while (scanner.next(tok))
    {
        switch (tok.type)
        {
            case XmlTokenType::ElementOpen:
            {
                ParseFrame fr;
                applyElementIdentity(fr.el, tok);
                for (const XmlAttribute& a : tok.attributes)
                {
                    fr.el.attributes.push_back(toParsedAttribute(a));
                }
                if (tok.self_closing)
                {
                    // Self-closing: no content, no children, closes immediately.
                    fr.el.content = intern(fr.contentScratch);
                    attach(std::move(fr.el), open, root, haveRoot, tok.line, tok.col);
                }
                else
                {
                    open.push_back(std::move(fr));
                }
                break;
            }

            case XmlTokenType::ElementClose:
            {
                // End-tag MATCHING is enforced by the scanner (it throws
                // XmlScanException::MismatchedEndTag before we ever see a
                // mismatched close); the parser just finalizes its own frame.
                // A defensively-encountered close with an empty stack (cannot
                // happen given the scanner's matching) is ignored.
                if (open.empty())
                {
                    break;
                }
                ParseFrame closed = std::move(open.back());
                open.pop_back();
                closed.el.content = intern(closed.contentScratch);
                attach(std::move(closed.el), open, root, haveRoot, tok.line, tok.col);
                break;
            }

            case XmlTokenType::Text:
                // Text between an element's tokens accumulates into the open
                // element's content SCRATCH, kept AS AUTHORED (whitespace
                // included). Leading/trailing text at top level (open empty) is
                // dropped — it is not element content.
                if (!open.empty())
                {
                    open.back().contentScratch += tok.value;
                }
                break;

            case XmlTokenType::Comment:
            case XmlTokenType::CData:
                // Dropped — S080's hand-off "the S085 parser decides whether
                // to keep it" resolves to: comments and CDATA are not tree
                // nodes and are discarded. (CDATA represents raw character
                // data; whether/how it becomes element content is a later,
                // explicit decision, not this step.)
                break;

            case XmlTokenType::EndOfInput:
                break;
        }
    }

    if (!open.empty())
    {
        // Reached end-of-input while an element was still open — the ONE
        // structural failure the scanner does not detect (it is iterative and
        // returns EndOfInput at EOF). The parser's own error path.
        const std::string unclosed = std::string(open.back().el.tagName);
        // `tok` holds the EndOfInput token (line/col = the EOF position where
        // the still-open element was detected).
        throw XmlParseException(XmlParseError::UnterminatedElement,
                                "unterminated element <" + unclosed + ">",
                                tok.line, tok.col);
    }

    if (!haveRoot)
    {
        // Empty / comment-only / whitespace-only document: no root element. A
        // well-formed VAML/XML document requires a single root. Reported as a
        // structural document-contract failure, not silent.
        throw XmlParseException(XmlParseError::UnterminatedElement,
                                "document has no root element", 1, 1);
    }

    return root;
}

} // namespace vaml
