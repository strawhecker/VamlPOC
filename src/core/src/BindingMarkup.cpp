// S252 — BindingMarkup (dep:S251). Sprint 6.
// Implements parse_markup_binding (see BindingMarkup.h for the grammar + the
// outcome table): structured parse of a deferred {Binding ...} / {Command ...}
// markup-extension source text.
//
// S501 (dep:S500) — the CONVERTER clause: after the path, zero or more
// comma-separated `Mode=<value>` / `Converter=<token>` clauses are parsed (in
// any order). `Converter={StaticResource K}` / `Converter=K` both resolve to
// the key `K`. A bad Mode= value, an empty Converter token, a repeated clause,
// or trailing junk is a Class A MalformedBindingMarkup (never silent).

#include "vaml/core/BindingMarkup.h"

#include <string_view>

namespace vaml {

namespace {

inline bool is_space(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Strips leading whitespace inside [b, e); returns the new begin.
size_t ltrim(std::string_view s, size_t b, size_t e) noexcept
{
    while (b < e && is_space(s[b])) ++b;
    return b;
}

} // namespace

VAML_CORE_EXPORT
BindingMarkup parse_markup_binding(std::string_view source) noexcept
{
    BindingMarkup out;

    const size_t fullB = ltrim(source, 0, source.size());
    const size_t fullE = source.size();
    if (fullB + 2 > source.size() || source[fullB] != '{' || source[fullE - 1] != '}')
        return out; // not a brace-balanced {…} extension (None, not malformed)

    // Inner content between the braces (whitespace-trimmed).
    size_t inB = fullB + 1;
    size_t inE = fullE - 1;
    inB = ltrim(source, inB, inE);
    while (inE > inB && is_space(source[inE - 1])) --inE;
    if (inB >= inE)
        return out; // {} -> None (not owned by us)

    // The leading keyword (Binding / Command / StaticResource / …) — up to the
    // first whitespace or comma.
    size_t kwB = inB;
    size_t kwE = inB;
    while (kwE < inE && !is_space(source[kwE]) && source[kwE] != ',')
        ++kwE;
    const std::string_view kw = source.substr(kwB, kwE - kwB);

    if (kw == "Binding")
        out.kind = MarkupExtensionKind::Binding;
    else if (kw == "Command")
        out.kind = MarkupExtensionKind::Command;
    else if (kw == "x:Static")
        out.kind = MarkupExtensionKind::Static;
    else if (kw == "x:Type")
        out.kind = MarkupExtensionKind::Type;
    else
        return out; // StaticResource / x:Null / other naming extension -> None

    // The remainder after the keyword (whitespace-trimmed).
    size_t rB = ltrim(source, kwE, inE);

    // {x:Static <token>} / {x:Type <name>} — a SINGLE non-empty token, no comma /
    // no whitespace inside (mirrors {Command <name>}).
    if (out.kind == MarkupExtensionKind::Static || out.kind == MarkupExtensionKind::Type)
    {
        if (rB >= inE)
        {
            out.malformed = true; // {x:Static} / {x:Type} with no token
            return out;
        }
        size_t nmE = rB;
        while (nmE < inE && !is_space(source[nmE]) && source[nmE] != ',')
            ++nmE;
        out.path = source.substr(rB, nmE - rB);
        // A trailing comma / extra tokens after the token is malformed.
        size_t tailB = ltrim(source, nmE, inE);
        if (tailB < inE)
        {
            out.malformed = true;
            return out;
        }
        return out;
    }

    // {Command <name>} — a SINGLE non-empty name, no comma / no whitespace inside.
    if (out.kind == MarkupExtensionKind::Command)
    {
        if (rB >= inE)
        {
            out.malformed = true; // {Command} with no name
            return out;
        }
        size_t nmE = rB;
        while (nmE < inE && !is_space(source[nmE]) && source[nmE] != ',')
            ++nmE;
        out.path = source.substr(rB, nmE - rB);
        // A trailing comma / extra tokens after the name is malformed.
        size_t tailB = ltrim(source, nmE, inE);
        if (tailB < inE)
        {
            out.malformed = true;
            return out;
        }
        return out;
    }

    // {Binding <path>[, Mode=<mode>]}
    if (rB >= inE || source[rB] == ',')
    {
        out.malformed = true; // {Binding} with no path, or a leading comma
        return out;
    }

    // The path is ONE token — it extends to the first comma OR whitespace.
    size_t pE = rB;
    while (pE < inE && source[pE] != ',' && !is_space(source[pE]))
        ++pE;
    out.path = source.substr(rB, pE - rB);

    // The remainder after the path (whitespace-trimmed).
    size_t afterP = ltrim(source, pE, inE);
    if (afterP >= inE)
        return out; // plain {Binding Title}

    // S501 — parse zero or more comma-separated `Mode=<value>` / `Converter=<token>`
    // clauses after the path. Each clause is `, <kw>=<value>`. Repeated
    // clauses (a second Mode= or a second Converter=) and unknown keywords are
    // malformed (Class A — never silent).
    size_t pos = afterP;
    while (pos < inE)
    {
        if (source[pos] != ',')
        {
            out.malformed = true; // {Binding Title Extra} — expected a comma clause
            return out;
        }
        const size_t cB = ltrim(source, pos + 1, inE); // clause start (after ',')
        if (cB >= inE)
        {
            out.malformed = true; // trailing comma with nothing after
            return out;
        }
        // The clause keyword ("Mode" / "Converter") — up to '='. Whitespace
        // around the keyword is tolerated (`Mode = TwoWay` == `Mode=TwoWay`),
        // so trim the keyword's trailing whitespace.
        size_t ckwB = cB;
        size_t ckwE = ckwB;
        while (ckwE < inE && source[ckwE] != '=')
        {
            ++ckwE;
        }
        if (ckwE >= inE)
        {
            out.malformed = true; // a clause with no '=' (authoring error)
            return out;
        }
        size_t ckwTrim = ckwE;
        while (ckwTrim > ckwB && is_space(source[ckwTrim - 1])) --ckwTrim;
        const std::string_view clauseKw = source.substr(ckwB, ckwTrim - ckwB);
        if (clauseKw.empty())
        {
            out.malformed = true; // ", =" with no keyword
            return out;
        }
        // The clause's VALUE extends to the next comma (or the end of the
        // inner content), whitespace-trimmed — NOT to the end of the input
        // (a subsequent `, Mode=...` / `, Converter=...` clause follows).
        size_t cbvB = ltrim(source, ckwE + 1, inE);
        if (cbvB >= inE)
        {
            out.malformed = true; // <kw>= with no value
            return out;
        }
        size_t cbvE = cbvB;
        while (cbvE < inE && source[cbvE] != ',')
        {
            ++cbvE;
        }
        while (cbvE > cbvB && is_space(source[cbvE - 1])) --cbvE;

        if (clauseKw == "Mode")
        {
            if (out.hasMode)
            {
                out.malformed = true; // a second Mode= clause (duplicate)
                return out;
            }
            const std::string_view modeVal = source.substr(cbvB, cbvE - cbvB);
            if (modeVal != "OneWay" && modeVal != "OneTime" && modeVal != "TwoWay")
            {
                out.malformed = true; // a bad Mode= value (authoring error)
                return out;
            }
            out.mode = modeVal;
            out.hasMode = true;
        }
        else if (clauseKw == "Converter")
        {
            if (out.hasConverter)
            {
                out.malformed = true; // a second Converter= clause (duplicate)
                return out;
            }
            std::string_view convToken = source.substr(cbvB, cbvE - cbvB);
            // `Converter={StaticResource K}` -> K (strip the braces + keyword);
            // `Converter=K` -> K. A `{DynamicResource ...}` converter token is
            // malformed here (only a StaticResource naming form is a converter
            // key; see the S501 authoring syntax in RENDER_SURFACE.md).
            if (convToken.size() >= 2 && convToken.front() == '{' && convToken.back() == '}')
            {
                const std::string_view inner = convToken.substr(1, convToken.size() - 2);
                // Inner is "<keyword> <key>"; the key is everything after the
                // first whitespace. We require the StaticResource keyword.
                size_t iw = ltrim(inner, 0, inner.size());
                size_t kwEnd = iw;
                while (kwEnd < inner.size() && !is_space(inner[kwEnd])) ++kwEnd;
                const std::string_view innerKw = inner.substr(iw, kwEnd - iw);
                if (innerKw != "StaticResource")
                {
                    out.malformed = true; // only {StaticResource K} is a converter key
                    return out;
                }
                size_t keyB = ltrim(inner, kwEnd, inner.size());
                if (keyB >= inner.size())
                {
                    out.malformed = true; // {StaticResource } with no key
                    return out;
                }
                convToken = inner.substr(keyB);
            }
            if (convToken.empty())
            {
                out.malformed = true; // Converter= with no key
                return out;
            }
            out.converter = convToken;
            out.hasConverter = true;
        }
        else
        {
            out.malformed = true; // an unknown clause keyword (authoring error)
            return out;
        }

        pos = cbvE; // continue after this clause's value
    }
    return out;
}

} // namespace vaml
