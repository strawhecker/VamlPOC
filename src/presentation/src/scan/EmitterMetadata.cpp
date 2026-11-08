// S641 — EmitterMetadata.cpp (forward-compatible sidecar JSON reader).
//
// Reads the S640b emitter's `out.json` sidecar into the typed EmitterMetadata.
// A tiny hand-rolled JSON object/array/string/number walker is enough for the
// fixed schema; unknown/extra fields are IGNORED so a later metadata shape is
// never fatal (the forward-compat contract). Pure + headless (both CI lanes).

#include <vaml/presentation/scan/EmitterMetadata.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace vaml::presentation::scan {

namespace {

// A minimal cursor over the JSON text.
struct Cursor
{
    const std::string& s;
    std::size_t i = 0;

    explicit Cursor(const std::string& str) : s(str) {}
    void skip_ws()
    {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
            ++i;
    }
    bool more() const noexcept { return i < s.size(); }
    char peek() const noexcept { return more() ? s[i] : '\0'; }
    bool consume(char c)
    {
        skip_ws();
        if (more() && s[i] == c)
        {
            ++i;
            return true;
        }
        return false;
    }
    bool fail(const std::string& detail, MetadataResult& out)
    {
        out.ok = false;
        out.error = MetadataError::Truncated;
        out.detail = detail;
        return false;
    }
};

// Reads a JSON string (incl. escaped quotes/basic escapes); false on malformed.
bool read_string(Cursor& c, std::string& out, MetadataResult& res)
{
    c.skip_ws();
    if (!c.more() || c.peek() != '"')
    {
        res.ok = false;
        res.error = MetadataError::Truncated;
        res.detail = "expected a JSON string";
        return false;
    }
    ++c.i;
    out.clear();
    while (c.more())
    {
        const char ch = c.s[c.i];
        if (ch == '"')
        {
            ++c.i;
            return true;
        }
        if (ch == '\\')
        {
            if (c.i + 1u >= c.s.size())
            {
                res.error = MetadataError::Truncated;
                res.detail = "unterminated escape";
                res.ok = false;
                return false;
            }
            out.push_back(c.s[c.i + 1u]);
            c.i += 2u;
            continue;
        }
        out.push_back(ch);
        ++c.i;
    }
    res.ok = false;
    res.error = MetadataError::Truncated;
    res.detail = "unterminated string";
    return false;
}

// Reads a JSON number as double; false on malformed.
bool read_number(Cursor& c, double& out, MetadataResult& res)
{
    c.skip_ws();
    std::size_t start = c.i;
    while (c.more() && (std::isdigit(static_cast<unsigned char>(c.s[c.i])) ||
                        c.s[c.i] == '-' || c.s[c.i] == '.' || c.s[c.i] == 'e' ||
                        c.s[c.i] == 'E' || c.s[c.i] == '+'))
        ++c.i;
    if (c.i == start)
    {
        res.ok = false;
        res.error = MetadataError::BadNumber;
        res.detail = "expected a JSON number";
        return false;
    }
    try
    {
        out = std::stod(c.s.substr(start, c.i - start));
    }
    catch (...)
    {
        res.ok = false;
        res.error = MetadataError::BadNumber;
        res.detail = "malformed JSON number";
        return false;
    }
    return true;
}

QuadKind kind_from_string(const std::string& s)
{
    if (s == "Text") return QuadKind::Text;
    if (s == "Icon") return QuadKind::Icon;
    return QuadKind::Solid;
}

} // namespace

MetadataResult parse_emitter_metadata(const std::string& jsonText)
{
    MetadataResult res;
    Cursor c(jsonText);

    // Root must be an object.
    if (!c.consume('{'))
    {
        res.ok = false;
        res.error = MetadataError::NotObject;
        res.detail = "sidecar root is not a JSON object";
        return res;
    }

    // The object key : value list. Unknown keys are skipped (forward-compat).
    while (true)
    {
        c.skip_ws();
        if (c.consume('}'))
            break;
        std::string key;
        if (!read_string(c, key, res))
            return res;
        if (!c.consume(':'))
        {
            res.ok = false;
            res.error = MetadataError::Truncated;
            res.detail = "missing ':' after sidecar key '" + key + "'";
            return res;
        }

        c.skip_ws();
        if (key == "Width")
        {
            double d = 0.0;
            if (!read_number(c, d, res)) return res;
            res.metadata.width = d >= 0 ? static_cast<std::uint32_t>(d) : 0u;
        }
        else if (key == "Height")
        {
            double d = 0.0;
            if (!read_number(c, d, res)) return res;
            res.metadata.height = d >= 0 ? static_cast<std::uint32_t>(d) : 0u;
        }
        else if (key == "Source")
        {
            std::string s;
            if (!read_string(c, s, res)) return res;
            res.metadata.source = s;
        }
        else if (key == "Quads")
        {
            if (!c.consume('['))
            {
                res.ok = false;
                res.error = MetadataError::BadArray;
                res.detail = "'Quads' is not a JSON array";
                return res;
            }
            while (true)
            {
                c.skip_ws();
                if (c.consume(']'))
                    break;
                if (!c.consume('{'))
                {
                    res.ok = false;
                    res.error = MetadataError::BadQuad;
                    res.detail = "'Quads' entry is not an object";
                    return res;
                }
                MetadataQuad q;
                while (true)
                {
                    c.skip_ws();
                    if (c.consume('}'))
                        break;
                    std::string qk;
                    if (!read_string(c, qk, res)) return res;
                    if (!c.consume(':'))
                    {
                        res.ok = false;
                        res.error = MetadataError::Truncated;
                        res.detail = "missing ':' in quad";
                        return res;
                    }
                    c.skip_ws();
                    if (qk == "Name")
                    {
                        if (!read_string(c, q.name, res)) return res;
                    }
                    else if (qk == "Kind")
                    {
                        std::string ks;
                        if (!read_string(c, ks, res)) return res;
                        q.kind = kind_from_string(ks);
                        q.kind_known = true;
                    }
                    else if (qk == "X" || qk == "Y" || qk == "Width" || qk == "Height")
                    {
                        double d = 0.0;
                        if (!read_number(c, d, res)) return res;
                        const int v = d >= 0 ? static_cast<int>(d) : 0;
                        if (qk == "X") q.x = v;
                        else if (qk == "Y") q.y = v;
                        else if (qk == "Width") q.w = v;
                        else q.h = v;
                    }
                    else
                    {
                        // Unknown quad field: skip its value (forward-compat).
                        if (c.peek() == '"')
                        {
                            std::string tmp;
                            if (!read_string(c, tmp, res)) return res;
                        }
                        else
                        {
                            double tmp = 0.0;
                            if (!read_number(c, tmp, res)) return res;
                        }
                    }
                    // consume the ',' between fields (loop handles it)
                    c.skip_ws();
                    bool comma = c.consume(',');
                    (void)comma;
                }
                res.metadata.quads.push_back(std::move(q));
                c.skip_ws();
                bool comma = c.consume(',');
                (void)comma;
            }
        }
        else
        {
            // Unknown top-level key: skip its value (forward-compat).
            c.skip_ws();
            if (c.peek() == '"')
            {
                std::string tmp;
                if (!read_string(c, tmp, res)) return res;
            }
            else if (c.peek() == '[')
            {
                // skip an unknown array value
                int depth = 0;
                while (c.more())
                {
                    char ch = c.s[c.i];
                    if (ch == '[' || ch == '{') ++depth;
                    else if (ch == ']' || ch == '}') { if (--depth <= 0) { ++c.i; break; } }
                    ++c.i;
                }
            }
            else if (c.peek() == '{')
            {
                int depth = 0;
                while (c.more())
                {
                    char ch = c.s[c.i];
                    if (ch == '{') ++depth;
                    else if (ch == '}') { if (--depth <= 0) { ++c.i; break; } }
                    ++c.i;
                }
            }
            else
            {
                double tmp = 0.0;
                if (!read_number(c, tmp, res)) return res;
            }
        }

        c.skip_ws();
        // consume the ',' between top-level members, or finish at '}'.
        if (c.consume('}'))
            break;
        if (!c.consume(','))
        {
            res.ok = false;
            res.error = MetadataError::Truncated;
            res.detail = "missing ',' between sidecar members";
            return res;
        }
    }

    res.ok = true;
    return res;
}

MetadataResult read_emitter_metadata_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        MetadataResult res;
        res.ok = false;
        res.error = MetadataError::NotObject;
        res.detail = "could not open sidecar file: " + path;
        return res;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_emitter_metadata(ss.str());
}

} // namespace vaml::presentation::scan
