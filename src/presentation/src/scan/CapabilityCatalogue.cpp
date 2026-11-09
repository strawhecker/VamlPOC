// S642 — CapabilityCatalogue.cpp (the LIB side of the GENERIC CAPABILITY PARITY
// catalogue).
//
// Loads the SINGLE-SOURCE-OF-TRUTH catalogue from its embedded JSON (the exact
// bytes of `tools/capability_catalogue.json` — the SAME file the WPF-emitter C#
// side + the PARITY MATRIX generator read). No runtime file dependency so BOTH CI
// lanes run the identical catalogue. Hand-rolled focused JSON scanner (the S641
// EmitterMetadata precedent — no external JSON dep). PURE + headless.
//
// ZERO LOCKS (ADR-018) + ZERO MINIMAL REAL.

#include <vaml/presentation/scan/CapabilityCatalogue.h>

#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::presentation::scan {

namespace {

// The embedded catalogue bytes — mirrors `tools/capability_catalogue.json` EXACTLY
// (the compile-time single source of truth; regenerate with the repo tooling).
// This raw string is the SAME document the PARITY MATRIX generator + sync check +
// the WPF-emitter read, so integrity is enforced by the headless parity test.
constexpr std::string_view kEmbeddedCatalogue = R"json({
  "version": 1,
  "counts_are_soft_start": true,
  "matrix": "docs/PARITY_MATRIX.md",
  "families": {
    "slot_int":     { "count": 10, "type": "int",     "kind": "Text" , "tolerance": 0.02 },
    "slot_bool":    { "count": 10, "type": "bool",    "kind": "Text" , "tolerance": 0.02 },
    "slot_string":  { "count": 10, "type": "string",  "kind": "Text",  "tolerance": 0.08 },
    "slot_double":  { "count": 10, "type": "double",  "kind": "Text" , "tolerance": 0.02 },
    "icon":         { "count": 10, "type": "icon",    "kind": "Icon",  "tolerance": 0.25 },
    "text":         { "count": 10, "type": "text",    "kind": "Text",  "tolerance": 0.08 }
  },
  "capabilities": [
    { "id": "slot_int_01",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_02",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_03",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_04",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_05",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_06",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_07",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_08",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_09",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_int_10",     "family": "slot_int",    "type": "int",     "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_01",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_02",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_03",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_04",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_05",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_06",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_07",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_08",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_09",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_bool_10",    "family": "slot_bool",   "type": "bool",    "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_01",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_02",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_03",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_04",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_05",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_06",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_07",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_08",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_09",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_string_10",  "family": "slot_string", "type": "string",  "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_01",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_02",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_03",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_04",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_05",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_06",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_07",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_08",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_09",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "slot_double_10",  "family": "slot_double", "type": "double",  "kind": "Text" , "tolerance": 0.02, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_01",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_02",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_03",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_04",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_05",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_06",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_07",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_08",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_09",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "icon_10",         "family": "icon",        "type": "icon",    "kind": "Icon",  "tolerance": 0.25, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_01",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_02",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_03",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_04",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_05",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_06",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_07",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_08",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_09",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" },
    { "id": "text_10",         "family": "text",        "type": "text",    "kind": "Text",  "tolerance": 0.08, "our": "introduced", "wpf": "introduced", "parity": "yes", "gap": "" }
  ]
})json";

// A minimal cursor over the embedded JSON (the S641 EmitterMetadata precedent).
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
        if (more() && s[i] == c) { ++i; return true; }
        return false;
    }
};

bool read_string(Cursor& c, std::string& out)
{
    c.skip_ws();
    if (!c.more() || c.peek() != '"') return false;
    ++c.i;
    out.clear();
    while (c.more())
    {
        const char ch = c.s[c.i];
        if (ch == '"') { ++c.i; return true; }
        if (ch == '\\' && c.i + 1u < c.s.size()) { out.push_back(c.s[c.i + 1u]); c.i += 2u; continue; }
        out.push_back(ch);
        ++c.i;
    }
    return false;
}

bool read_number(Cursor& c, double& out)
{
    c.skip_ws();
    const std::size_t start = c.i;
    while (c.more() && (std::isdigit(static_cast<unsigned char>(c.s[c.i])) ||
                        c.s[c.i] == '-' || c.s[c.i] == '.' || c.s[c.i] == 'e' ||
                        c.s[c.i] == 'E' || c.s[c.i] == '+'))
        ++c.i;
    if (c.i == start) return false;
    try { out = std::stod(c.s.substr(start, c.i - start)); } catch (...) { return false; }
    return true;
}

// Reads a bare JSON literal (true/false/null) at the cursor; sets `out` for the
// boolean case. Returns false when the cursor is not at a bare literal. Used for
// fields whose JSON value is a boolean (`counts_are_soft_start`).
bool read_literal(Cursor& c, bool* out = nullptr)
{
    c.skip_ws();
    if (!c.more()) return false;
    if (c.peek() == '"')                 return false; // a string, not a literal
    const std::size_t start = c.i;
    while (c.more() && !std::isspace(static_cast<unsigned char>(c.s[c.i])) &&
           c.s[c.i] != ',' && c.s[c.i] != '}' && c.s[c.i] != ']')
        ++c.i;
    if (c.i == start) return false;
    const std::string lit = c.s.substr(start, c.i - start);
    if (out && (lit == "true" || lit == "false"))
        *out = (lit == "true");
    return lit == "true" || lit == "false" || lit == "null";
}

// Consumes a single JSON value of unknown shape (for the fields we skip), handling
// nested objects/arrays with depth counting (the S641 EmitterMetadata precedent).
void skip_value(Cursor& c)
{
    c.skip_ws();
    if (!c.more()) return;
    const char ch = c.peek();
    if (ch == '"')
    {
        std::string t;
        (void)read_string(c, t);
    }
    else if (ch == '{' || ch == '[')
    {
        const char openCh = ch;
        const char closeCh = (ch == '{') ? '}' : ']';
        int depth = 0;
        while (c.more())
        {
            const char cur = c.s[c.i];
            if (cur == '"')
            {
                size_t saved = c.i;
                std::string t;
                if (read_string(c, t)) continue;
                c.i = saved; // not a string open inside -> fall through to normal scan
            }
            if (cur == openCh) ++depth;
            else if (cur == closeCh)
            {
                if (--depth <= 0) { ++c.i; break; }
            }
            ++c.i;
        }
    }
    else
    {
        // a bare literal (true/false/null) or a number — either way, advance past it.
        if (!read_literal(c))
        {
            double t = 0.0;
            (void)read_number(c, t);
        }
    }
}

QuadKind kind_from_name(const std::string& name)
{
    if (name == "Text") return QuadKind::Text;
    if (name == "Icon") return QuadKind::Icon;
    return QuadKind::Solid;
}

CapabilityStatus status_from_name(const std::string& name)
{
    if (name == "done")        return CapabilityStatus::Done;
    if (name == "planned")     return CapabilityStatus::Planned;
    if (name == "out-of-scope") return CapabilityStatus::OutOfScope;
    return CapabilityStatus::Introduced;  // "introduced" default
}

} // namespace

CapabilityCatalogue parse_capability_catalogue(const std::string& jsonText)
{
    CapabilityCatalogue cat;
    Cursor c(jsonText);
    if (!c.consume('{')) return cat;   // not an object -> empty (never silently claimed)

    while (true)
    {
        if (c.consume('}')) break;
        std::string key;
        if (!read_string(c, key) || !c.consume(':')) break;
        if (key == "version")
        {
            double v = 0.0;
            if (!read_number(c, v)) break;
            cat.version = static_cast<int>(v);
        }
        else if (key == "counts_are_soft_start")
        {
            // the value is a bare boolean literal (true/false), not a JSON number.
            if (!read_literal(c, &cat.counts_are_soft_start))
                break;
        }
        else if (key == "capabilities")
        {
            if (!c.consume('[')) break;
            while (true)
            {
                if (c.consume(']')) break;
                if (!c.consume('{')) return cat;
                Capability cap;
                bool malformed = false;
                while (true)
                {
                    if (c.consume('}'))
                    {
                        // complete a capability only when its id is non-empty
                        if (!cap.id.empty())
                            cat.capabilities.push_back(std::move(cap));
                        break;
                    }
                    std::string k;
                    if (!read_string(c, k) || !c.consume(':')) { malformed = true; break; }
                    if (k == "id")        { if (!read_string(c, cap.id)) { malformed = true; break; } }
                    else if (k == "family") { if (!read_string(c, cap.family)) { malformed = true; break; } }
                    else if (k == "type") { if (!read_string(c, cap.type)) { malformed = true; break; } }
                    else if (k == "kind") { std::string kn; if (!read_string(c, kn)) { malformed = true; break; } cap.kind = kind_from_name(kn); }
                    else if (k == "tolerance") { if (!read_number(c, cap.tolerance)) { malformed = true; break; } }
                    else if (k == "our")  { std::string st; if (!read_string(c, st)) { malformed = true; break; } cap.our = status_from_name(st); }
                    else if (k == "wpf")  { std::string st; if (!read_string(c, st)) { malformed = true; break; } cap.wpf = status_from_name(st); }
                    else if (k == "parity") { std::string p; if (!read_string(c, p)) { malformed = true; break; } cap.parity_yes = (p == "yes"); }
                    else if (k == "gap")  { if (!read_string(c, cap.gap)) { malformed = true; break; } }
                    else { skip_value(c); }
                    c.skip_ws();
                    bool comma = c.consume(',');
                    (void)comma;
                }
                if (malformed) return cat;  // never silently incomplete
                c.skip_ws();
                bool comma = c.consume(',');
                (void)comma;
            }
        }
        else
        {
            c.skip_ws();
            if (c.peek() == '"')
            {
                std::string t;
                if (!read_string(c, t)) break;
            }
            else if (c.peek() == '{' || c.peek() == '[')
            {
                skip_value(c);
            }
            else
            {
                // a bare literal (true/false/null) or a number.
                if (!read_literal(c))
                {
                    double t = 0.0;
                    if (!read_number(c, t)) break;
                }
            }
        }
        c.skip_ws();
        if (c.consume('}')) break;
        if (!c.consume(',')) break;
    }
    return cat;
}

bool capability_catalogue_loaded() noexcept
{
    return !capability_catalogue().capabilities.empty();
}

const CapabilityCatalogue& capability_catalogue() noexcept
{
    // The embedded catalogue is parsed ONCE at first use (function-local Meyers
    // static — ADR-018 zero locks; single-threaded startup/read).
    static const CapabilityCatalogue cat =
        parse_capability_catalogue(std::string(kEmbeddedCatalogue));
    return cat;
}

const Capability* find_capability(const CapabilityCatalogue& cat, const std::string& id) noexcept
{
    for (const auto& cap : cat.capabilities)
        if (cap.id == id)
            return &cap;
    return nullptr;
}

double catalogue_tolerance_for(const Capability& cap) noexcept
{
    // The catalogue's declared per-capability tolerance wins; the built-in kind
    // default (the S641 scaffold) is the fallback for an un-catalogued capability.
    if (cap.tolerance > 0.0)
        return cap.tolerance;
    return default_tolerance_for(cap.kind);
}

std::size_t family_count(const CapabilityCatalogue& cat, const std::string& family) noexcept
{
    std::size_t n = 0;
    for (const auto& cap : cat.capabilities)
        if (cap.family == family)
            ++n;
    return n;
}

CapabilitySummary summarize(const CapabilityCatalogue& cat) noexcept
{
    CapabilitySummary s;
    s.total = cat.capabilities.size();
    for (const auto& cap : cat.capabilities)
    {
        if (cap.type == "int")         ++s.ints;
        else if (cap.type == "bool")   ++s.bools;
        else if (cap.type == "string") ++s.strings;
        else if (cap.type == "double") ++s.doubles;
        else if (cap.type == "icon")   ++s.icons;
        else if (cap.type == "text")   ++s.texts;
    }
    return s;
}

} // namespace vaml::presentation::scan
