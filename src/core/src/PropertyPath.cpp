// S240 — PropertyPath parser + evaluator (dep:S226). Sprint 6 first token.
// Implements the Class A syntax parser (PropertyPath::parse) + the Class B
// value-domain evaluator (evaluate_property_path) + the default
// dependency-object reader. See PropertyPath.h for the full contract.
//
// The parser was REWRITTEN at Sprint 11af to be WPF-faithful against
// E:\GitRepos\wpf\...\MS\Internal\Data\PathParser.cs (the WPF-source-audit
// protocol): the Init → DrillIn → Prop(/Indexer) state machine, a valid empty /
// `.` path producing a `Direct` self-binding, the `/` drill-in operator
// (DrillIn.Always), the `^` indexer escape, and the comma-separated
// IndexerParamInfo (paren/value) list.

#include "vaml/core/PropertyPath.h"

#include "vaml/core/DependencyObject.h" // dependency_object_reader's Get/PropertyId
#include "vaml/core/DependencyProperty.h" // TryGet (unregistered->MissingProperty)
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyRegistry.h" // PropertyNotFound (unregistered->MissingProperty)

#include <cctype>
#include <cstdint>
#include <limits>
#include <string>

namespace vaml {

namespace {

const char kNullChar = static_cast<char>(0);
const char kEscapeChar = '^';
// WPF IsSpecialChar (PathParser.cs:401-405): whitespace is NOT a special char.
bool is_special_char(char c) noexcept
{
    return c == '.' || c == '/' || c == '[' || c == ']';
}

// Human-readable "what" for a PropertyPathException.
std::string build_what(PropertyPathError kind, const std::string& path,
                       const std::string& fragment)
{
    const char* kindName = nullptr;
    switch (kind)
    {
    case PropertyPathError::TrailingDot:         kindName = "malformed path"; break;
    case PropertyPathError::ConsecutiveDot:      kindName = "malformed path"; break;
    case PropertyPathError::UnterminatedIndexer: kindName = "unterminated indexer"; break;
    case PropertyPathError::EmptyIndexer:        kindName = "empty indexer"; break;
    case PropertyPathError::MalformedIndex:      kindName = "malformed index"; break;
    case PropertyPathError::UnclosedQuote:       kindName = "unclosed quote"; break;
    case PropertyPathError::UnmatchedParen:      kindName = "unmatched parenthesis"; break;
    default:                                     kindName = "invalid property path"; break;
    }
    std::string what = "PropertyPath: ";
    what += kindName;
    what += " in '";
    what += path;
    what += "'";
    if (!fragment.empty())
    {
        what += " (at '" + fragment + "')";
    }
    return what;
}

// --- Indexer parsing (the WPF AddIndexer sub-state-machine) -----------------
// A comma-separated list of IndexerParamInfo{parenString, valueString} units;
// `^` escapes `, ( ) ] ^` and white space; nested `[ [ ] ]` is tracked.
struct IndexParseResult
{
    bool             ok = false;
    PropertyIndex    index;
    PropertyPathError kind = PropertyPathError::Empty;
    bool             unterminated = false; // help build a fragment
};

IndexParseResult parse_indexer(const char* openPos, const char* end)
{
    using State = int; // 0 BeginParam, 1 ParenString, 2 ValueString, 3 Done
    IndexParseResult r;
    r.index.kind = PropertyIndex::Kind::Integer; // default; single-int set later

    const char* cur = openPos + 1; // skip '['
    int level = 1;                 // nested [ ]
    bool escaped = false;
    bool trimRight = false;
    std::string paren, value;      // current param's two parts
    std::vector<PropertyIndexerParam>& params = r.index.params;

    State state = 0;
    while (state != 3)
    {
        if (cur >= end)
        {
            r.kind = PropertyPathError::UnterminatedIndexer; // unmatched bracket
            r.ok = false;
            return r;
        }
        const char c = *cur++;

        // Escape char: set the flag for the NEXT character; do not process c.
        if (c == kEscapeChar && !escaped)
        {
            escaped = true;
            continue;
        }

        if (state == 0) // BeginParam: look for optional '(...)'
        {
            if (escaped)
            {
                // no '(', go parse the value
                state = 2;
                // fall through to ValueString handling for this char below
                goto value_string;
            }
            else if (c == '(')
            {
                state = 1; // ParenString
            }
            else
            {
                state = 2;
                goto value_string;
            }
        }
        else if (state == 1) // ParenString: collect until ')'
        {
            if (escaped)
            {
                paren.push_back(c);
            }
            else if (c == ')')
            {
                state = 2; // value begins after ')'
            }
            else if (c == '(')
            {
                paren.push_back(c); // WPF appends an unclosed nesting '(' to paren string
            }
            else
            {
                paren.push_back(c);
            }
        }
        else // state 2: ValueString
        {
        value_string:
            if (escaped)
            {
                value.push_back(c);
                trimRight = false;
            }
            else if (c == '[') // nested indexer level++
            {
                value.push_back(c);
                ++level;
                trimRight = false;
            }
            else if (c == ']') // nested level-- (or the closing one ends the list)
            {
                if (level > 1)
                {
                    value.push_back(c);
                    --level;
                    trimRight = false;
                }
                else
                {
                    // final ']' → end of this param + the whole list
                    if (trimRight)
                    {
                        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                                                  value.back() == '\r' || value.back() == '\n'))
                        {
                            value.pop_back();
                        }
                    }
                    params.push_back(PropertyIndexerParam{paren, value});
                    state = 3;
                }
            }
            else if (c == ',' && level == 1)
            {
                // end of current param; ',' at top level separates params
                if (trimRight)
                {
                    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                                              value.back() == '\r' || value.back() == '\n'))
                    {
                        value.pop_back();
                    }
                }
                params.push_back(PropertyIndexerParam{paren, value});
                paren.clear();
                value.clear();
                trimRight = false;
                state = 0; // next param
            }
            else if (c == ',' && level > 1)
            {
                // a comma INSIDE a nested indexer is literal
                value.push_back(c);
                trimRight = false;
            }
            else if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                value.push_back(c);
                trimRight = true; // trailing whitespace trimmed at the end
            }
            else
            {
                value.push_back(c);
                trimRight = false;
            }
        }

        // After processing each character, clear the escape flag.
        escaped = false;
    }

    // Distill the common single-Integer / single-StringKey forms for the
    // existing reader/accessor surface.
    if (params.size() == 1 && params[0].paren.empty())
    {
        const std::string& v = params[0].value;
        if (!v.empty())
        {
            const char* p = v.data();
            const char* e = v.data() + v.size();
            bool negative = false;
            // trim leading ws
            while (p != e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            {
                ++p;
            }
            if (p != e && *p == '-')
            {
                negative = true;
                ++p;
            }
            bool allDigits = (p != e);
            for (const char* q = p; q != e; ++q)
            {
                if (!std::isdigit(static_cast<unsigned char>(*q)))
                {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits && p != e)
            {
                std::int64_t val = 0;
                bool overflow = false;
                for (const char* q = p; q != e; ++q)
                {
                    const int digit = static_cast<int>(*q - '0');
                    if (val > (std::numeric_limits<std::int64_t>::max() - digit) / 10)
                    {
                        overflow = true;
                        break;
                    }
                    val = val * 10 + digit;
                }
                if (overflow)
                {
                    r.kind = PropertyPathError::MalformedIndex;
                    r.ok = false;
                    return r;
                }
                r.index.kind = PropertyIndex::Kind::Integer;
                r.index.integer = negative ? -val : val;
                r.index.str.clear();
            }
            else
            {
                r.index.kind = PropertyIndex::Kind::StringKey;
                r.index.str = v;
                r.index.integer = 0;
            }
        }
        else
        {
            r.kind = PropertyPathError::EmptyIndexer; // "Items[]"
            r.ok = false;
            return r;
        }
    }
    else if (params.size() == 1)
    {
        // single param with a paren part: treat the value string as the key
        const std::string& v = params[0].value;
        if (v.empty())
        {
            r.kind = PropertyPathError::EmptyIndexer;
            r.ok = false;
            return r;
        }
        r.index.kind = PropertyIndex::Kind::StringKey;
        r.index.str = v;
        r.index.integer = 0;
    }
    else
    {
        // multi-param: record the list; the convenience key is the first's value
        r.index.kind = PropertyIndex::Kind::StringKey;
        r.index.str = params[0].value;
        r.index.integer = 0;
    }

    r.ok = true;
    return r;
}

// Reads a property name starting at `cur` (may include dots for the XLinq form,
// paren-level tracking, and any whitespace — WPF trims). Returns the name and
// the next index; sets `ok=false` + kind on an unbalanced paren.
bool read_property(const char*& cur, const char* end, std::string& name,
                   PropertyPathError& kind)
{
    const char* start = cur;
    int level = 0;
    // WPF AddProperty includes leading dots in the path (for XLinq).
    while (cur != end && *cur == '.')
    {
        ++cur;
    }
    while (cur != end && (level > 0 || !is_special_char(*cur)))
    {
        if (*cur == '(')
        {
            ++level;
        }
        else if (*cur == ')')
        {
            --level;
        }
        ++cur;
    }
    if (level > 0)
    {
        kind = PropertyPathError::UnmatchedParen;
        return false;
    }
    if (level < 0)
    {
        kind = PropertyPathError::UnmatchedParen;
        return false;
    }
    name.assign(start, cur);
    // WPF trims the name.
    std::size_t b = name.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
    {
        name.clear();
    }
    else
    {
        std::size_t e = name.find_last_not_of(" \t\r\n");
        name = name.substr(b, e - b + 1);
    }
    return true;
}

} // namespace

void PropertyPath::make_direct(PropertyPath& out, std::string_view text) noexcept
{
    out = PropertyPath();
    out.valid_    = true;
    out.isDirect_ = true;
    out.original_ = std::string(text);
}

PropertyPathException::PropertyPathException(PropertyPathError kind, std::string path,
                                             std::string fragment)
    : std::logic_error(build_what(kind, path, fragment)),
      kind_(kind),
      path_(std::move(path)),
      fragment_(std::move(fragment))
{
}

VAML_CORE_EXPORT
PropertyPath PropertyPath::parse(std::string_view text)
{
    PropertyPath out;
    PropertyPathError err;
    if (try_parse(text, out, &err))
    {
        return out;
    }

    // Build a best-effort fragment for the exception message (authoring aid).
    std::string fragment;
    const char* cur = text.data();
    const char* end = text.data() + text.size();
    std::size_t b = 0;
    while (b < text.size() && (text[b] == ' ' || text[b] == '\t' || text[b] == '\r' || text[b] == '\n'))
    {
        ++b;
    }
    cur = text.data() + b;
    if (cur == end)
    {
        fragment = std::string(text);
    }
    else
    {
        std::string_view rest(cur, end - cur);
        const std::size_t open = rest.find('[');
        if (open != std::string_view::npos)
        {
            const std::size_t close = rest.find(']', open + 1);
            fragment = (close != std::string_view::npos)
                ? std::string(rest.substr(open, close - open + 1))
                : std::string(rest.substr(open));
        }
        else
        {
            fragment = std::string(rest);
        }
    }
    throw PropertyPathException(err, std::string(text), std::move(fragment));
}

VAML_CORE_EXPORT
bool PropertyPath::try_parse(std::string_view text, PropertyPath& out,
                             PropertyPathError* err) noexcept
{
    // Trim exactly as WPF does (PathParser.cs:82).
    std::size_t b = 0;
    while (b < text.size() && (text[b] == ' ' || text[b] == '\t' || text[b] == '\r' || text[b] == '\n'))
    {
        ++b;
    }
    std::size_t e = text.size();
    while (e > b && (text[e - 1] == ' ' || text[e - 1] == '\t' || text[e - 1] == '\r' || text[e - 1] == '\n'))
    {
        --e;
    }

    const char* cur = text.data() + b;
    const char* end = text.data() + e;

    // Empty path -> Direct self-binding (WPF PathParser.cs:83-89).
    if (cur == end)
    {
        make_direct(out, text);
        if (err != nullptr) { *err = PropertyPathError::Empty; /* no-error/ok tag */ }
        return true;
    }

    std::vector<PropertySegment> segments;
    DrillIn drillIn = DrillIn::IfNeeded; // first-level default (PathParser.cs:95)
    bool done = false;

    while (!done)
    {
        // The WPF top loop skips whitespace between levels (PathParser.cs:105-110).
        char c = (cur < end) ? *cur : kNullChar;
        if (c != kNullChar && (c == ' ' || c == '\t' || c == '\r' || c == '\n'))
        {
            ++cur;
            continue;
        }

        // State.Init / State.DrillIn transition (PathParser.cs:117-155).
        if (c == '/' || c == kNullChar || c == '.')
        {
            // DrillIn state: a separator selects the drill-in mode for the
            // FOLLOWING property/indexer step.
            if (c == '/')
            {
                ++cur;
                // skip whitespace
                while (cur < end && (*cur == ' ' || *cur == '\t' || *cur == '\r' || *cur == '\n'))
                {
                    ++cur;
                }
                if (cur >= end)
                {
                    // trailing '/' -> malformed (no step follows)
                    if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
                    return false;
                }
                if (*cur == '/')
                {
                    if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
                    return false;
                }
                drillIn = DrillIn::Always;
                continue;
            }
            else if (c == '.')
            {
                ++cur;
                drillIn = DrillIn::Never;
                continue;
            }
            else
            {
                // NullChar -> end
                done = true;
                break;
            }
        }
        else if (c == '[')
        {
            // --- Indexer step ------------------------------------------------
            const char* openPos = cur;
            // find the matching close '[' at the top level of this indexer
            IndexParseResult ipr = parse_indexer(openPos, end);
            if (!ipr.ok)
            {
                if (err != nullptr) { *err = ipr.kind; }
                return false;
            }
            // advance `cur` past the closing ']'
            {
                int lvl = 1;
                cur = openPos + 1;
                while (cur < end && lvl > 0)
                {
                    if (*cur == '[')
                    {
                        ++lvl;
                    }
                    else if (*cur == ']')
                    {
                        --lvl;
                        if (lvl == 0)
                        {
                            ++cur;
                            break;
                        }
                    }
                    ++cur;
                }
            }
            PropertySegment seg;
            seg.kind = PropertySegmentKind::Index;
            seg.drillIn = drillIn;
            seg.index = std::move(ipr.index);
            seg.name = (seg.index.kind == PropertyIndex::Kind::StringKey) ? seg.index.str : std::string();
            seg.source.assign(openPos, cur);
            segments.push_back(std::move(seg));
            // StartNewLevel: next level's drill-in defaults to Never
            drillIn = DrillIn::Never;
            continue;
        }
        else
        {
            // --- Property step (or a leading-dot already consumed) ------------
            // WPF AddProperty: read the name until a special char (respecting
            // paren-level), including any internal whitespace.
            PropertyPathError pk;
            std::string name;
            if (!read_property(cur, end, name, pk))
            {
                if (err != nullptr) { *err = pk; }
                return false;
            }
            if (name.empty())
            {
                // The property name is empty at this position -> we produced a
                // Direct step (WPF: name.Length==0 -> SourceValueType.Direct).
                // This happens for a bare trailing '.' handled by the loop above
                // reaching end; guard deterministically here.
                if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
                return false;
            }
            PropertySegment seg;
            seg.kind = PropertySegmentKind::Property;
            seg.drillIn = drillIn;
            seg.name = std::move(name);
            seg.source.assign(seg.name);
            // Preserve a leading-dot source prefix for round-trip by tracking
            // separators is complex; the parser records the bare name (the WPF
            // SourceValueInfo name field). Diagnostics use the original text.
            segments.push_back(std::move(seg));
            drillIn = DrillIn::Never;
            continue;
        }
    }

    if (segments.empty())
    {
        // A path that reduced to nothing but separators (e.g. ".") is Direct.
        make_direct(out, text);
        if (err != nullptr) { *err = PropertyPathError::Empty; /* no-error/ok tag */ }
        return true;
    }

    out = PropertyPath();
    out.valid_    = true;
    out.isDirect_ = false;
    out.original_ = std::string(text);
    out.segments_ = std::move(segments);
    if (err != nullptr) { *err = PropertyPathError::Empty; /* no-error/ok tag */ }
    return true;
}

VAML_CORE_EXPORT
PropertyRead evaluate_property_path(const Value& root, const PropertyPath& path,
                                    const PropertyReader& reader)
{
    if (!path.valid())
    {
        return PropertyRead{false, PropertyPathError::Empty, Value::Null(), 0};
    }
    if (path.is_direct())
    {
        // Empty / '.' path -> the whole object (WPF SourceValueInfo(Direct)).
        return PropertyRead{true, PropertyPathError::Empty, root, 0};
    }
    if (path.segment_count() == 0)
    {
        return PropertyRead{false, PropertyPathError::Empty, Value::Null(), 0};
    }

    Value node = root;
    const auto& segs = path.segments();
    for (std::size_t i = 0; i < segs.size(); ++i)
    {
        const PropertySegment& seg = segs[i];
        PropertyRead r = reader(node, seg);
        if (!r.ok)
        {
            // Propagate the reader failure with the failing segment index.
            return PropertyRead{false, r.error, Value::Null(), i};
        }
        if (i + 1 < segs.size() && r.value.isNull())
        {
            // A null on a NON-final segment: null-safe stop. The caller owns
            // the null-result policy.
            return PropertyRead{false, PropertyPathError::NullIntermediate, Value::Null(), i};
        }
        node = r.value;
    }
    // Final segment resolved (value may be Value::Null — a real read).
    return PropertyRead{true, PropertyPathError::Empty, node, segs.size() - 1};
}

} // namespace vaml
