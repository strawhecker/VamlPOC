// S082 (dep:S081) — hand-rolled XML scanner implementation with STRUCTURED
// errors AND NAMESPACE awareness. See XmlScanner.h for the declared scope
// (elements, attributes, self-closing, comments, CDATA, entities, nested tags)
// plus the S082 namespace model (prefix/local split on ':', per-element
// xmlns scope, default-ns for unprefixed elements only, clr-namespace mapping
// tokenized). Every structural failure throws a categorized XmlScanException
// carrying the 1-based line:col where it was detected (guaranteed >= 1 on
// every malformed path — the core promise of S081). S082's MalformedClrNamespace
// category keeps that promise for the clr-namespace throw site.
#include "vaml/core/XmlScanner.h"

#include "vaml/core/Intern.h" // S140: pool-backed string_view token text

#include <algorithm>
#include <cctype>
#include <utility>

#include <cctype>
#include <utility>

namespace vaml {

namespace {

// XML name start: a letter, '_', or ':'. (S080 keeps the pragmatic ASCII
// letter set; full Unicode + the clr-namespace-prefix mapping is S082's step.)
bool isNameStart(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':';
}

// Subsequent name chars extend with digits, '.', and '-' (covers Box.Width /
// Grid.Row attached-style names that the parser will see).
bool isNameChar(char c)
{
    return isNameStart(c) || (c >= '0' && c <= '9') || c == '.' || c == '-';
}

// S083: classify an x-language-directive candidate by its local name. Called
// ONLY when the attribute's resolved namespace_uri == kXamlLanguageNamespace.
// Case-sensitive (XAML is case-sensitive): the four directives are Name,
// Class, Key, Null; any other local in the language namespace maps to None.
XmlDirectiveType classifyXDirective(std::string_view local)
{
    if (local == "Name")
    {
        return XmlDirectiveType::Name;
    }
    if (local == "Class")
    {
        return XmlDirectiveType::Class;
    }
    if (local == "Key")
    {
        return XmlDirectiveType::Key;
    }
    if (local == "Null")
    {
        return XmlDirectiveType::Null;
    }
    return XmlDirectiveType::None;
}

// S084: is the digit-run starting at `i` (view of `v`) a valid exponent
// `[eE][-+]?digits`? Called only when v[i] is 'e' or 'E'. Requires at least one
// digit after the optional sign; otherwise the run is NOT an exponent.
bool scanExponent(std::string_view v, std::size_t& i)
{
    if (i >= v.size() || (v[i] != 'e' && v[i] != 'E'))
    {
        return false;
    }
    ++i; // consume 'e'/'E'
    if (i < v.size() && (v[i] == '+' || v[i] == '-'))
    {
        ++i; // consume optional sign
    }
    const std::size_t digitsStart = i;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9')
    {
        ++i;
    }
    return i > digitsStart; // an exponent MUST have at least one digit
}

// S084: scalar numeric detection — the grammar [-+]?(digits | digits.digits |
// .digits)([eE][-+]?digits)?. This is DETECTION ONLY (S089 converts). The
// comma-list thickness form and all type conversion are S089's converter-domain
// concern — not numerically detected here (recorded NO-GAP, not silent).
bool isScalarNumeric(std::string_view v)
{
    if (v.empty())
    {
        return false;
    }
    std::size_t i = 0;
    if (v[i] == '+' || v[i] == '-')
    {
        ++i; // consume optional sign
        if (i >= v.size())
        {
            return false; // a bare sign is not a number
        }
    }

    std::size_t digitsSeen = 0;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9')
    {
        ++i;
        ++digitsSeen;
    }

    // Optional fractional part: '.' then at least one digit (forms .digits or
    // digits.digits). A trailing '.' with no following digit is NOT numeric
    // ("1.", ".") — we require at least one digit after the '.'.
    if (i < v.size() && v[i] == '.')
    {
        ++i; // consume '.'
        const std::size_t fracStart = i;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9')
        {
            ++i;
            ++digitsSeen;
        }
        if (i == fracStart)
        {
            return false; // '.' with no digit follows, e.g. "1." or "."
        }
    }

    // At least one digit must have appeared in the integer or fraction part
    // (".5", "5", "5.5" all qualify; "e3", "+x", "." do not).
    if (digitsSeen == 0)
    {
        return false;
    }

    // Optional exponent: [eE][-+]?digits, requires at least one digit.
    if (i < v.size() && (v[i] == 'e' || v[i] == 'E'))
    {
        if (!scanExponent(v, i))
        {
            return false;
        }
    }

    return i == v.size(); // the WHOLE value must be consumed
}

} // namespace

XmlScanner::XmlScanner(std::string_view input)
    : input_(input)
{
}

// S082: split on the FIRST ':' only. The '.' (attached-owner) form is NOT a
// namespace separator, so only ':' matters. A name with no ':' has prefix "".
void XmlScanner::splitName(std::string_view name, std::string& prefix, std::string& local)
{
    const std::size_t colon = name.find(':');
    if (colon == std::string::npos)
    {
        prefix.clear();
        local = std::string(name);
        return;
    }
    prefix = std::string(name.substr(0, colon));
    local = std::string(name.substr(colon + 1));
}

// S082: tokenizes a "clr-namespace:" mapping value. The parser-side
// type-resolution (mapping a namespace/assembly to a real vaml type) is S087's
// step — here we only split the value into its structural parts.
//
// Recognized forms:
//   "clr-namespace:Ns.Name"                -> clrNamespace="Ns.Name", assembly=""
//   "clr-namespace:Ns.Name;assembly=Asm"   -> clrNamespace="Ns.Name", assembly="Asm"
// A bare value (no "clr-namespace:" prefix) is returned verbatim as the
// clrNamespace with isClrNamespace=false.
ClrNamespaceReference parse_clr_namespace(std::string_view value, bool* isClrNamespace,
                                          std::size_t line, std::size_t col)
{
    constexpr std::string_view kPrefix = "clr-namespace:";
    ClrNamespaceReference result;

    if (value.compare(0, kPrefix.size(), kPrefix) != 0)
    {
        // Not a clr-namespace mapping — a plain value (e.g. a normal attribute
        // string the consumer also runs through this helper). Interned view.
        result.clrNamespace = intern(value);
        result.assembly = std::string_view();
        if (isClrNamespace)
        {
            *isClrNamespace = false;
        }
        return result;
    }

    if (isClrNamespace)
    {
        *isClrNamespace = true;
    }

    const std::string_view body = value.substr(kPrefix.size());
    const std::size_t semi = body.find(';');
    const std::string_view nsPart = body.substr(0, semi);

    // A clr-namespace mapping MUST have a non-empty namespace part. This is the
    // S082 throw site for the new MalformedClrNamespace category (the S081 1:1
    // taxonomy contract insists every malformed path has its own kind).
    if (nsPart.empty())
    {
        throw XmlScanException(XmlScanError::MalformedClrNamespace,
                               "malformed clr-namespace mapping (empty namespace)", line, col);
    }

    result.clrNamespace = intern(nsPart);
    result.assembly = std::string_view();

    if (semi != std::string_view::npos)
    {
        // Expect ";assembly=Asm" — a ";" suffix that isn't well-formed is also
        // a malformed mapping (same category: this range's dedicated kind).
        const std::string_view tail = body.substr(semi);
        constexpr std::string_view kAssemb = ";assembly=";
        if (tail.compare(0, kAssemb.size(), kAssemb) == 0)
        {
            result.assembly = intern(tail.substr(kAssemb.size()));
        }
        else
        {
            throw XmlScanException(XmlScanError::MalformedClrNamespace,
                                   "malformed clr-namespace mapping (bad ';' suffix)", line, col);
        }
    }
    return result;
}

// S084: classify an attribute value string. See XmlScanner.h. A MarkupExtension
// requires a LEADING '{' AND a BALANCED closing '}' at the very end (nesting
// honored, so `{Binding X}` and `{x:Null}` are extensions while the unbalanced
// `"{abc"` is not). Kept a free function so the S085 parser and any consumer
// can reuse the same classification without re-scanning.
AttributeValueKind classify_attribute_value(std::string_view value, bool* isNumeric)
{
    const bool isExtension =
        value.size() >= 2 && value.front() == '{' && value.back() == '}'
        // Balanced-brace verification ONLY when the first char is '{' and the
        // last is '}' — a plain literal ("{abc") never enters the nesting walk.
        && [](std::string_view v) {
               int depth = 0;
               for (const char c : v)
               {
                   if (c == '{')
                   {
                       ++depth;
                   }
                   else if (c == '}')
                   {
                       --depth;
                       if (depth < 0)
                       {
                           return false; // unbalanced closing before the end
                       }
                   }
               }
               return depth == 0; // fully balanced and net-zero
           }(value);

    if (isNumeric)
    {
        // Numeric applies ONLY to a QuotedLiteral scalar; a markup extension is
        // never numeric at this layer (its contents are resolved by S252/S253).
        *isNumeric = !isExtension && isScalarNumeric(value);
    }
    return isExtension ? AttributeValueKind::MarkupExtension
                       : AttributeValueKind::QuotedLiteral;
}

XmlScanException::XmlScanException(XmlScanError kind, std::string what, std::size_t line, std::size_t col)
    : std::runtime_error(what + " (line " + std::to_string(line) + ", column " + std::to_string(col) + ")")
    , kind_(kind)
    , line_(line)
    , col_(col)
{
}

[[noreturn]] void XmlScanner::fail(XmlScanError kind, const std::string& message) const
{
    throw XmlScanException(kind, message, line_, col_);
}

char XmlScanner::peek(std::size_t offset) const noexcept
{
    const std::size_t i = pos_ + offset;
    if (i >= input_.size())
    {
        return '\0';
    }
    return input_[i];
}

char XmlScanner::advance()
{
    const char c = input_[pos_++];
    if (c == '\n')
    {
        ++line_;
        col_ = 1;
    }
    else
    {
        ++col_;
    }
    return c;
}

bool XmlScanner::startsWith(const char* s) const noexcept
{
    std::size_t i = 0;
    for (; s[i] != '\0'; ++i)
    {
        if (pos_ + i >= input_.size() || input_[pos_ + i] != s[i])
        {
            return false;
        }
    }
    return true;
}

void XmlScanner::skipWhitespace()
{
    while (!atEnd())
    {
        const char c = peek();
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
        {
            break;
        }
        advance();
    }
}

std::string XmlScanner::scanName()
{
    if (atEnd() || !isNameStart(peek()))
    {
        fail(XmlScanError::ExpectedName, "expected a name");
    }
    std::string name;
    while (!atEnd() && isNameChar(peek()))
    {
        name.push_back(advance());
    }
    // Do not consume a trailing ':' or other name-start char that could be a
    // prefix boundary (S082 concern); the loop already stops on it.
    return name;
}

void XmlScanner::appendDecodedEntity(std::string& target)
{
    // peek() == '&' precondition.
    advance(); // consume '&'
    if (atEnd())
    {
        fail(XmlScanError::UnterminatedEntity, "unterminated entity reference");
    }

    if (peek() == '#')
    {
        advance(); // consume '#'
        bool hex = false;
        if (!atEnd() && (peek() == 'x' || peek() == 'X'))
        {
            hex = true;
            advance();
        }
        unsigned long v = 0;
        std::size_t digits = 0;
        while (!atEnd())
        {
            const char c = peek();
            unsigned d;
            if (c >= '0' && c <= '9')
            {
                d = static_cast<unsigned>(c - '0');
            }
            else if (hex && c >= 'a' && c <= 'f')
            {
                d = static_cast<unsigned>(c - 'a' + 10);
            }
            else if (hex && c >= 'A' && c <= 'F')
            {
                d = static_cast<unsigned>(c - 'A' + 10);
            }
            else
            {
                break;
            }
            v = v * (hex ? 16UL : 10UL) + d;
            ++digits;
            advance();
        }
        if (digits == 0)
        {
            fail(XmlScanError::InvalidNumericReference, "invalid numeric character reference");
        }
        if (!atEnd() && peek() == ';')
        {
            advance();
        }
        else
        {
            fail(XmlScanError::UnterminatedNumericReference, "unterminated numeric character reference");
        }
        // Emit as UTF-8. Only code points in the BMP are supported by S080
        // (superset handled by S082+); the ASCII/control-range is what VAML
        // attribute/text actually uses (S089 converters).
        if (v < 0x80UL)
        {
            target.push_back(static_cast<char>(v));
        }
        else if (v < 0x800UL)
        {
            target.push_back(static_cast<char>(0xC0 | (v >> 6)));
            target.push_back(static_cast<char>(0x80 | (v & 0x3F)));
        }
        else if (v <= 0xFFFFUL)
        {
            target.push_back(static_cast<char>(0xE0 | (v >> 12)));
            target.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3F)));
            target.push_back(static_cast<char>(0x80 | (v & 0x3F)));
        }
        else
        {
            fail(XmlScanError::CharacterReferenceOutOfRange, "character reference out of range");
        }
        return;
    }

    // Named predefined entity: consume up to the terminating ';'.
    const char* names[] = {"amp", "lt", "gt", "quot", "apos"};
    const char   values[] = {'&', '<', '>', '"', '\''};
    for (std::size_t k = 0; k < 5; ++k)
    {
        std::size_t j = 0;
        for (; names[k][j] != '\0'; ++j)
        {
            if (atEnd() || peek() != names[k][j])
            {
                break;
            }
            advance();
        }
        if (names[k][j] == '\0' && !atEnd() && peek() == ';')
        {
            advance(); // consume ';'
            target.push_back(values[k]);
            return;
        }
        // Rewind the partially-consumed chars of this candidate.
        // (Only rewind if we consumed some; j is the count consumed.)
        if (j > 0)
        {
            // Re-scan is heavier than rewind: step pos/line/col back j chars.
            // This is a cold path; correctness over speed. Recompute by
            // temporarily decrementing position. Handle multi-line carefully:
            // entity names contain no newlines, so col_ just decrements.
            for (std::size_t r = 0; r < j; ++r)
            {
                --pos_;
                --col_;
            }
        }
    }
    fail(XmlScanError::UnknownEntity, "unknown or malformed entity reference");
}

std::string XmlScanner::scanAttributeValue(char quote)
{
    advance(); // consume opening quote
    std::string value;
    while (true)
    {
        if (atEnd())
        {
            fail(XmlScanError::UnterminatedAttributeValue, "unterminated attribute value");
        }
        const char c = peek();
        if (c == quote)
        {
            advance();
            break;
        }
        if (c == '&')
        {
            appendDecodedEntity(value);
            continue;
        }
        value.push_back(advance());
    }
    return value;
}

void XmlScanner::scanComment(std::string& content)
{
    // Precondition: we are positioned just past "<!--".
    while (true)
    {
        if (atEnd())
        {
            fail(XmlScanError::UnterminatedComment, "unterminated comment");
        }
        if (startsWith("-->"))
        {
            advance();
            advance();
            advance();
            return;
        }
        content.push_back(advance());
    }
}

void XmlScanner::scanCData(std::string& content)
{
    // Precondition: positioned just past "<![CDATA[".
    while (true)
    {
        if (atEnd())
        {
            fail(XmlScanError::UnterminatedCData, "unterminated CDATA section");
        }
        if (startsWith("]]>"))
        {
            advance();
            advance();
            advance();
            return;
        }
        content.push_back(advance());
    }
}

void XmlScanner::scanElementOpen(XmlToken& out)
{
    out.type = XmlTokenType::ElementOpen;
    out.name = intern(scanName()); // buffered name -> interned view
    out.self_closing = false;

    // Attribute / xmlns loop. xmlns and xmlns:p are NAMESPACE DECLARATIONS
    // (S082): collected into out.xmlns, and NOT emitted as ordinary attributes.
    // Every other attribute is collected (name/value) but its namespace is
    // resolved AFTER the whole tag is read, once ALL xmlns declarations are
    // known — XML declarations apply to the whole element regardless of where
    // they appear in the tag (order-independent self-application).
    while (true)
    {
        skipWhitespace();
        if (atEnd())
        {
            fail(XmlScanError::UnterminatedStartTag, "unterminated start tag");
        }
        const char c = peek();
        if (c == '>')
        {
            advance();
            break;
        }
        if (c == '/')
        {
            advance();
            if (atEnd() || peek() != '>')
            {
                fail(XmlScanError::MalformedSelfClosing, "malformed self-closing tag");
            }
            advance(); // consume '>'
            out.self_closing = true;
            break;
        }
        // Attribute: name="value" or name='value'.
        if (!isNameStart(c))
        {
            fail(XmlScanError::MalformedStartTag, "malformed start tag (expected attribute or '>')");
        }
        XmlAttribute attr;
        std::string attrNameBuf = scanName(); // buffered; interned below
        skipWhitespace();
        if (atEnd() || peek() != '=')
        {
            fail(XmlScanError::AttributeMissingEquals, "attribute missing '='");
        }
        advance(); // consume '='
        skipWhitespace();
        if (atEnd() || (peek() != '"' && peek() != '\''))
        {
            fail(XmlScanError::AttributeValueNotQuoted, "attribute value must be quoted");
        }
        std::string attrValueBuf = scanAttributeValue(peek());
        attr.name = intern(attrNameBuf);
        attr.value = intern(attrValueBuf);

        // S082: classify. "xmlns" sets the DEFAULT namespace; "xmlns:p" sets a
        // prefixed namespace. Both are declarations, not attributes.
        if (attrNameBuf == "xmlns")
        {
            out.xmlns.emplace_back(std::string_view(), attr.value);
            continue;
        }
        if (attrNameBuf.compare(0, 6, "xmlns:") == 0)
        {
            out.xmlns.emplace_back(intern(std::string_view(attrNameBuf).substr(6)), attr.value);
            continue;
        }
        out.attributes.push_back(std::move(attr));
    }

    // S082: push a namespace scope frame for THIS element (inherits the
    // enclosing scope) and apply its own xmlns declarations (self-applying).
    // Pushed for BOTH self-closing and non-self-closing elements so the
    // element's own name + attributes see its own declarations; popped below
    // for self-closing, kept on the stack for non-self-closing (children
    // inherit it) and popped on the matching ElementClose.
    nsStack_.push_back(nsStack_.empty() ? std::map<std::string, std::string>()
                                        : nsStack_.back());
    for (const auto& decl : out.xmlns)
    {
        // nsStack_ is owned map<string,string> (scope mutation); copy the
        // interned decl views into it.
        nsStack_.back()[std::string(decl.first)] = std::string(decl.second);
    }

    // Resolve each ordinary attribute's namespace against the applied scope.
    for (XmlAttribute& attr : out.attributes)
    {
        std::string attrPrefix;
        std::string attrLocal;
        splitName(attr.name, attrPrefix, attrLocal);
        attr.prefix = intern(attrPrefix);
        attr.local_name = intern(attrLocal);
        if (attr.prefix.empty())
        {
            // The default namespace does NOT apply to attributes (XML rule).
            attr.namespace_uri = std::string_view();
        }
        else
        {
            attr.namespace_uri = intern(resolvePrefix(std::string(attr.prefix)));
        }
        // S083: classify an x-language directive when this attribute's
        // RESOLVED namespace is the canonical XAML language URI. Keyed on the
        // URI, not the literal "x" prefix (see kXamlLanguageNamespace).
        attr.directive = (attr.namespace_uri == kXamlLanguageNamespace)
                             ? classifyXDirective(attr.local_name)
                             : XmlDirectiveType::None;
        // S084: classify the VALUE grammar (quoted literal vs markup extension)
        // + the scalar-numeric detection flag. Uniform across ordinary props AND
        // S083 x: directive values ({x:Null} -> MarkupExtension, x:Name="foo" ->
        // quoted literal).
        attr.value_kind = classify_attribute_value(attr.value, &attr.numeric);
    }

    // Split + resolve the element's own name against the applied scope.
    std::string elemPrefix;
    std::string elemLocal;
    splitName(out.name, elemPrefix, elemLocal);
    out.prefix = intern(elemPrefix);
    out.local_name = intern(elemLocal);
    out.namespace_uri = intern(out.prefix.empty() ? resolvePrefix("")
                                                  : resolvePrefix(std::string(out.prefix)));

    if (out.self_closing)
    {
        // No children: the temporary frame is discarded immediately.
        nsStack_.pop_back();
    }
    else
    {
        stack_.push_back(std::string(out.name));
    }
}

// S082: resolves a (split) prefix to a namespace URI from the innermost scope.
// An "" prefix resolves to the current DEFAULT namespace (the `xmlns` frame
// entry keyed ""); a prefixed name resolves to its mapped URI. An unmapped
// prefix resolves to "" — unknown namespaces are NOT a scan error at S082 (the
// structured authority is S085/S087, recorded NO-GAP).
const std::string& XmlScanner::resolvePrefix(const std::string& prefix) const
{
    if (nsStack_.empty())
    {
        // No namespace scope at all: default and any prefixed name are empty.
        static const std::string kEmpty;
        return kEmpty;
    }
    const auto& frame = nsStack_.back();
    const auto it = frame.find(prefix);
    if (it == frame.end())
    {
        static const std::string kEmpty;
        return kEmpty;
    }
    return it->second;
}

void XmlScanner::scanElementClose(XmlToken& out)
{
    // Precondition: positioned just past "</".
    out.type = XmlTokenType::ElementClose;
    std::string nameBuf = scanName();
    out.name = intern(nameBuf);
    skipWhitespace();
    if (atEnd() || peek() != '>')
    {
        fail(XmlScanError::MalformedEndTag, "malformed end tag");
    }
    advance(); // consume '>'

    if (stack_.empty() || stack_.back() != std::string(out.name))
    {
        fail(XmlScanError::MismatchedEndTag, "mismatched or stray end tag </" + std::string(out.name) + ">");
    }
    stack_.pop_back();
    // S082: pop the matching namespace scope frame (pushed on this ElementOpen).
    if (!nsStack_.empty())
    {
        nsStack_.pop_back();
    }
}

void XmlScanner::scanText(XmlToken& out)
{
    out.type = XmlTokenType::Text;
    std::string buf;
    while (!atEnd())
    {
        const char c = peek();
        if (c == '<')
        {
            break;
        }
        if (c == '&')
        {
            appendDecodedEntity(buf);
            continue;
        }
        buf.push_back(advance());
    }
    out.value = intern(buf);
    // Whitespace-only text is still a Text token in S080 (the S085 parser
    // decides whether to keep it); no collapsing here.
}

bool XmlScanner::scanMarkup(XmlToken& out)
{
    // Precondition: peek() == '<' and we have not consumed it yet.
    out.attributes.clear();
    out.name = std::string_view();
    out.value = std::string_view();
    out.self_closing = false;
    out.xmlns.clear();
    out.prefix = std::string_view();
    out.local_name = std::string_view();
    out.namespace_uri = std::string_view();

    const std::size_t tokenLine = line();
    const std::size_t tokenCol = col();

    advance(); // consume '<'

    if (atEnd())
    {
        fail(XmlScanError::UnexpectedEndAfterLessThan, "unexpected end of input after '<'");
    }

    if (peek() == '!')
    {
        advance(); // consume '!'
        if (startsWith("--"))
        {
            advance();
            advance(); // consume "--" (now just past "<!--")
            out.type = XmlTokenType::Comment;
            out.line = tokenLine;
            out.col = tokenCol;
            std::string commentBuf;
            scanComment(commentBuf);
            out.value = intern(commentBuf);
            return true;
        }
        if (startsWith("[CDATA["))
        {
            for (int i = 0; i < 7; ++i)
            {
                advance(); // consume "[CDATA["
            }
            out.type = XmlTokenType::CData;
            out.line = tokenLine;
            out.col = tokenCol;
            std::string cdataBuf;
            scanCData(cdataBuf);
            out.value = intern(cdataBuf);
            return true;
        }
        fail(XmlScanError::UnrecognizedMarkup, "unrecognized '<!' markup");
    }

    if (peek() == '/')
    {
        advance(); // consume '/'
        out.type = XmlTokenType::ElementClose;
        out.line = tokenLine;
        out.col = tokenCol;
        scanElementClose(out);
        return true;
    }

    out.type = XmlTokenType::ElementOpen;
    out.line = tokenLine;
    out.col = tokenCol;
    scanElementOpen(out);
    return true;
}

bool XmlScanner::next(XmlToken& out)
{
    out.attributes.clear();
    out.name = std::string_view();
    out.value = std::string_view();
    out.self_closing = false;
    out.xmlns.clear();
    out.prefix = std::string_view();
    out.local_name = std::string_view();
    out.namespace_uri = std::string_view();

    // Whitespace between tokens is skipped except inside Text (scanText reads
    // until '<' including whitespace, so nothing to skip here when text is
    // the next token — the loop below only skips when we see '<'/end).
    while (!atEnd())
    {
        if (peek() != '<')
        {
            const std::size_t tokenLine = line();
            const std::size_t tokenCol = col();
            out.type = XmlTokenType::Text;
            out.line = tokenLine;
            out.col = tokenCol;
            scanText(out);
            return true;
        }
        if (scanMarkup(out))
        {
            return true;
        }
        // scanMarkup always returns true (or throws); unreachable, kept pure.
    }

    out.type = XmlTokenType::EndOfInput;
    out.line = line();
    out.col = col();
    return false;
}

} // namespace vaml
