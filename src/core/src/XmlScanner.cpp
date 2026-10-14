// S080 (dep:S049) — hand-rolled XML scanner implementation.
// See XmlScanner.h for the declared scope (elements, attributes, self-closing,
// comments, CDATA, entities, nested tags) and the design rationale.
//
// Structural failures -> XmlScanException (message-only this step); the rich
// structured taxonomy is S081's resolution point (NO-GAP, not a backfill).
#include "vaml/core/XmlScanner.h"

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

} // namespace

XmlScanner::XmlScanner(std::string_view input)
    : input_(input)
{
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
        throw XmlScanException("expected a name");
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
        throw XmlScanException("unterminated entity reference");
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
            throw XmlScanException("invalid numeric character reference");
        }
        if (!atEnd() && peek() == ';')
        {
            advance();
        }
        else
        {
            throw XmlScanException("unterminated numeric character reference");
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
            throw XmlScanException("character reference out of range");
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
    throw XmlScanException("unknown or malformed entity reference");
}

std::string XmlScanner::scanAttributeValue(char quote)
{
    advance(); // consume opening quote
    std::string value;
    while (true)
    {
        if (atEnd())
        {
            throw XmlScanException("unterminated attribute value");
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
            throw XmlScanException("unterminated comment");
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
            throw XmlScanException("unterminated CDATA section");
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
    out.name = scanName();
    out.self_closing = false;

    while (true)
    {
        skipWhitespace();
        if (atEnd())
        {
            throw XmlScanException("unterminated start tag");
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
                throw XmlScanException("malformed self-closing tag");
            }
            advance(); // consume '>'
            out.self_closing = true;
            break;
        }
        // Attribute: name="value" or name='value'.
        if (!isNameStart(c))
        {
            throw XmlScanException("malformed start tag (expected attribute or '>')");
        }
        XmlAttribute attr;
        attr.name = scanName();
        skipWhitespace();
        if (atEnd() || peek() != '=')
        {
            throw XmlScanException("attribute missing '='");
        }
        advance(); // consume '='
        skipWhitespace();
        if (atEnd() || (peek() != '"' && peek() != '\''))
        {
            throw XmlScanException("attribute value must be quoted");
        }
        attr.value = scanAttributeValue(peek());
        out.attributes.push_back(std::move(attr));
    }

    if (!out.self_closing)
    {
        stack_.push_back(out.name);
    }
}

void XmlScanner::scanElementClose(XmlToken& out)
{
    // Precondition: positioned just past "</".
    out.type = XmlTokenType::ElementClose;
    out.name = scanName();
    skipWhitespace();
    if (atEnd() || peek() != '>')
    {
        throw XmlScanException("malformed end tag");
    }
    advance(); // consume '>'

    if (stack_.empty() || stack_.back() != out.name)
    {
        throw XmlScanException("mismatched or stray end tag </" + out.name + ">");
    }
    stack_.pop_back();
}

void XmlScanner::scanText(XmlToken& out)
{
    out.type = XmlTokenType::Text;
    out.value.clear();
    while (!atEnd())
    {
        const char c = peek();
        if (c == '<')
        {
            break;
        }
        if (c == '&')
        {
            appendDecodedEntity(out.value);
            continue;
        }
        out.value.push_back(advance());
    }
    // Whitespace-only text is still a Text token in S080 (the S085 parser
    // decides whether to keep it); no collapsing here.
}

bool XmlScanner::scanMarkup(XmlToken& out)
{
    // Precondition: peek() == '<' and we have not consumed it yet.
    out.attributes.clear();
    out.name.clear();
    out.value.clear();
    out.self_closing = false;

    const std::size_t tokenLine = line();
    const std::size_t tokenCol = col();

    advance(); // consume '<'

    if (atEnd())
    {
        throw XmlScanException("unexpected end of input after '<'");
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
            scanComment(out.value);
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
            scanCData(out.value);
            return true;
        }
        throw XmlScanException("unrecognized '<!' markup");
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
    out.name.clear();
    out.value.clear();
    out.self_closing = false;

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
