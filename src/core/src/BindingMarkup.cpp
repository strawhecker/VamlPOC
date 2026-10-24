// S252 — BindingMarkup (dep:S251). Sprint 6.
// Implements parse_markup_binding (see BindingMarkup.h for the grammar + the
// outcome table): structured parse of a deferred {Binding ...} / {Command ...}
// markup-extension source text.

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
    else
        return out; // StaticResource / x:Null / other naming extension -> None

    // The remainder after the keyword (whitespace-trimmed).
    size_t rB = ltrim(source, kwE, inE);

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

    // Expect exactly ", Mode=<value>" after the path.
    if (source[afterP] != ',')
    {
        out.malformed = true; // {Binding Title Extra} / {Binding Title x} — no comma
        return out;
    }
    size_t mB = ltrim(source, afterP + 1, inE);
    constexpr std::string_view kModeKw = "Mode";
    size_t modeKwEnd = mB + kModeKw.size();
    if (modeKwEnd > inE || source.substr(mB, kModeKw.size()) != kModeKw)
    {
        out.malformed = true;
        return out;
    }
    size_t eqB = ltrim(source, modeKwEnd, inE);
    if (eqB >= inE || source[eqB] != '=')
    {
        out.malformed = true;
        return out;
    }
    size_t vB = ltrim(source, eqB + 1, inE);
    if (vB >= inE)
    {
        out.malformed = true; // Mode= with no value
        return out;
    }
    size_t vE = inE;
    while (vE > vB && is_space(source[vE - 1])) --vE;
    const std::string_view modeVal = source.substr(vB, vE - vB);
    if (modeVal != "OneWay" && modeVal != "OneTime" && modeVal != "TwoWay")
    {
        out.malformed = true; // a bad Mode= value (authoring error)
        return out;
    }
    out.mode = modeVal;
    out.hasMode = true;
    return out;
}

} // namespace vaml
