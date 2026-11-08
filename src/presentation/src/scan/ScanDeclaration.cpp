// S641 — in-XAML `SCAN DECLARATION` parser (hand-rolled comment-block reader).
//
// Reads the authored `.xaml`'s `<!-- SCAN DECLARATION ... END SCAN DECLARATION
// -->` comment block and produces a typed ScanDeclaration. Pure, headless,
// no regex/XML dependency. See ScanDeclaration.h for the grammar.
//
// ADR-018: zero locks (single-threaded pure parse). ZERO MINIMAL REAL.

#include <vaml/presentation/scan/ScanDeclaration.h>

#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace vaml::presentation::scan {

namespace {

// A single trimmed token line yields its space-separated words (quoted strings
// are kept as one token with the quotes intact for the caller to unquote).
std::vector<std::string> tokenize_line(const std::string& line)
{
    std::vector<std::string> words;
    std::string cur;
    bool inQuote = false;
    for (char c : line)
    {
        if (c == '"')
        {
            cur.push_back(c);
            inQuote = !inQuote;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)) && !inQuote)
        {
            if (!cur.empty())
            {
                words.push_back(cur);
                cur.clear();
            }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty())
        words.push_back(cur);
    return words;
}

// Parses a non-negative float token; false on any non-numeric content.
bool parse_number(const std::string& tok, double& out)
{
    if (tok.empty())
        return false;
    std::size_t used = 0;
    try
    {
        out = std::stod(tok, &used);
    }
    catch (...)
    {
        return false;
    }
    return used == tok.size() && out >= 0.0 && std::isfinite(out);
}

// Parses a `two`-int token pair (frame / rect components). Returns false on a
// non-numeric or negative component.
bool parse_u32(const std::string& tok, std::uint32_t& out)
{
    double d = 0.0;
    if (!parse_number(tok, d))
        return false;
    out = static_cast<std::uint32_t>(d);
    return true;
}

// Splits a comma-joined rect "X,Y,W,H" into up to 4 numeric tokens; returns the
// token count (0 when the token is not comma-joined at all — a caller then treats
// it as the first of space-separated tokens). Supports the grammar's `rect
// 32,32,128,128` AND `rect 32 32 128 128` forms.
std::size_t split_rect_token(const std::string& tok, std::uint32_t out[4])
{
    std::size_t count = 0;
    std::string cur;
    bool hasComma = false;
    for (char c : tok)
    {
        if (c == ',')
        {
            hasComma = true;
            if (count < 4u && !cur.empty())
            {
                std::uint32_t v = 0;
                if (parse_u32(cur, v))
                {
                    out[count] = v;
                    ++count;
                }
            }
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (hasComma)
    {
        if (count < 4u && !cur.empty())
        {
            std::uint32_t v = 0;
            if (parse_u32(cur, v))
            {
                out[count] = v;
                ++count;
            }
        }
        return count;
    }
    return 0;  // not comma-joined
}

DeclarationResult fail(DeclarationError err, const std::string& detail, int line)
{
    DeclarationResult r;
    r.ok = false;
    r.error = err;
    r.detail = detail;
    r.line = line;
    return r;
}

} // namespace

double default_tolerance_for(QuadKind kind) noexcept
{
    // The tolerance scaffold's built-in per-kind floor. Solid geometry is tight;
    // Icon (centroid/presence) is loose; Text (auto-locate band) is intermediate.
    switch (kind)
    {
        case QuadKind::Solid: return 0.02;
        case QuadKind::Text:  return 0.08;
        case QuadKind::Icon:  return 0.25;
    }
    return 0.02;
}

DeclarationResult parse_scan_declaration(const std::string& xamlText)
{
    // --- 1) find the comment block `<!-- SCAN DECLARATION ... END SCAN DECLARATION -->`
    const std::string open = "<!-- SCAN DECLARATION";
    const std::string end  = "END SCAN DECLARATION";
    const std::string close= "-->";

    std::size_t blockStart = xamlText.find(open);
    if (blockStart == std::string::npos)
        return fail(DeclarationError::NoBlock,
                    "no '<!-- SCAN DECLARATION' comment block found in the XAML", 0);

    // The block's body starts just after the opening marker's newline.
    std::size_t bodyStart = xamlText.find('\n', blockStart);
    std::size_t endMarker = xamlText.find(end, blockStart);
    if (endMarker == std::string::npos)
        return fail(DeclarationError::MissingEnd,
                    "the SCAN DECLARATION block never reached 'END SCAN DECLARATION'", 0);

    // The END marker must be followed by a comment close on the same line.
    std::size_t lineEnd = xamlText.find('\n', endMarker);
    if (lineEnd == std::string::npos)
        lineEnd = xamlText.size();
    std::string tail = xamlText.substr(endMarker, lineEnd - endMarker);
    if (tail.find(close) == std::string::npos)
        return fail(DeclarationError::MissingEnd,
                    "the SCAN DECLARATION block's END line lacks the '-->' comment close", 0);

    // --- 2) tokenize the body lines (between bodyStart and endMarker) ---
    ScanDeclaration decl;
    std::istringstream body(xamlText.substr(bodyStart, endMarker - bodyStart));
    std::string line;
    int lineno = 0;
    while (std::getline(body, line))
    {
        ++lineno;
        // Trim whitespace at the line ends.
        std::size_t b = line.find_first_not_of(" \t\r");
        std::size_t e = line.find_last_not_of(" \t\r");
        if (b == std::string::npos)
            continue;   // blank line
        std::string trimmed = line.substr(b, e - b + 1);
        if (trimmed.empty())
            continue;

        // A line that starts with ';' is a comment line — skip.
        if (trimmed[0] == ';')
            continue;

        const std::vector<std::string> toks = tokenize_line(trimmed);
        if (toks.empty())
            continue;
        const std::string& cmd = toks[0];

        if (cmd == "frame")
        {
            if (toks.size() != 3u)
                return fail(DeclarationError::MalformedLine,
                            "frame expects 'frame W H'", lineno + 1);
            if (!parse_u32(toks[1], decl.width) || !parse_u32(toks[2], decl.height))
                return fail(DeclarationError::BadNumber,
                            "frame dims must be non-negative integers", lineno + 1);
            if (decl.width == 0u || decl.height == 0u)
                return fail(DeclarationError::BadNumber,
                            "frame dims must be non-zero", lineno + 1);
            continue;
        }

        if (cmd == "quad")
        {
            if (toks.size() < 3u)
                return fail(DeclarationError::MalformedLine,
                            "quad expects 'quad NAME KIND ...'", lineno + 1);
            ScanQuad q;
            q.name = toks[1];

            // KIND
            if (toks[2] == "solid")
                q.kind = QuadKind::Solid;
            else if (toks[2] == "text")
                q.kind = QuadKind::Text;
            else if (toks[2] == "icon")
                q.kind = QuadKind::Icon;
            else
                return fail(DeclarationError::BadKind,
                            "quad kind must be solid|text|icon (got '" + toks[2] + "')", lineno + 1);

            // locating: `rect X,Y,W,H` (explicit) vs `auto` (auto-locate)
            std::size_t i = 3u;
            if (i < toks.size() && toks[i] == "rect")
            {
                q.located = true;
                std::uint32_t v[4];
                // Support BOTH `rect 32,32,128,128` (one comma-joined token) and
                // `rect 32 32 128 128` (four space-separated tokens).
                std::size_t got = 0;
                if (i + 1u < toks.size())
                    got = split_rect_token(toks[i + 1u], v);
                std::size_t k = 0;
                if (got == 0u)
                {
                    // space-separated form: consume 4 subsequent tokens
                    if (toks.size() < i + 5u)
                        return fail(DeclarationError::BadRect,
                                    "rect expects 'rect X,Y,W,H'", lineno + 1);
                    for (k = 0; k < 4u; ++k)
                    {
                        if (!parse_u32(toks[i + 1u + k], v[k]))
                            return fail(DeclarationError::BadNumber,
                                        "rect component must be a non-negative integer", lineno + 1);
                    }
                }
                else
                {
                    if (got != 4u)
                        return fail(DeclarationError::BadRect,
                                    "rect expects 4 components X,Y,W,H", lineno + 1);
                    k = 4u;
                }
                q.x = static_cast<float>(v[0]);
                q.y = static_cast<float>(v[1]);
                q.w = static_cast<float>(v[2]);
                q.h = static_cast<float>(v[3]);
                if (q.w <= 0.0f || q.h <= 0.0f)
                    return fail(DeclarationError::BadRect,
                                "rect width/height must be positive", lineno + 1);
                // when comma-joined, the rect consumed 1 token; else 4 tokens
                i += (got != 0u) ? 2u : 5u;
            }
            else if (i < toks.size() && toks[i] == "auto")
            {
                q.located = false;
                i += 1u;
            }
            else
            {
                return fail(DeclarationError::MalformedLine,
                            "quad must be 'rect X,Y,W,H' or 'auto' to locate", lineno + 1);
            }

            // remaining option tokens
            for (; i < toks.size(); ++i)
            {
                const std::string& t = toks[i];
                if (t == "tolerance")
                {
                    if (i + 1u >= toks.size())
                        return fail(DeclarationError::BadNumber,
                                    "tolerance expects a 0..1 fraction", lineno + 1);
                    double tol = 0.0;
                    if (!parse_number(toks[i + 1u], tol) || tol < 0.0 || tol > 1.0)
                        return fail(DeclarationError::BadNumber,
                                    "tolerance must be a 0..1 fraction", lineno + 1);
                    q.has_tolerance = true;
                    q.tolerance = tol;
                    ++i;
                }
                else if (t == "band")
                {
                    if (i + 1u >= toks.size())
                        return fail(DeclarationError::BadNumber, "band expects a count", lineno + 1);
                    std::uint32_t bandVal = 0;
                    if (!parse_u32(toks[i + 1u], bandVal))
                        return fail(DeclarationError::BadNumber, "band must be a non-negative integer", lineno + 1);
                    q.band = static_cast<int>(bandVal);
                    ++i;
                }
                else if (t == "expect-literal")
                {
                    if (i + 1u >= toks.size())
                        return fail(DeclarationError::MalformedLine,
                                    "expect-literal expects a quoted string", lineno + 1);
                    std::string lit = toks[i + 1u];
                    if (lit.size() >= 2u && lit.front() == '"' && lit.back() == '"')
                        q.expect_literal = lit.substr(1u, lit.size() - 2u);
                    else
                        q.expect_literal = lit;
                    ++i;
                }
                else if (t == "centroid")
                {
                    q.require_centroid = true;
                }
                else if (t == "presence")
                {
                    q.require_presence = true;
                }
                else
                {
                    return fail(DeclarationError::UnknownKeyword,
                                "unknown quad option '" + t + "'", lineno + 1);
                }
            }

            decl.quads.push_back(std::move(q));
            continue;
        }

        return fail(DeclarationError::UnknownKeyword,
                    "unknown SCAN DECLARATION directive '" + cmd + "'", lineno + 1);
    }

    // A declaration with no frame is malformed (the scan needs a render size).
    if (decl.width == 0u || decl.height == 0u)
        return fail(DeclarationError::MalformedLine,
                    "SCAN DECLARATION requires a 'frame W H' line", 0);

    DeclarationResult r;
    r.ok = true;
    r.declaration = std::move(decl);
    return r;
}

} // namespace vaml::presentation::scan
