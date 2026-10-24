// S240 — PropertyPath parser + evaluator (dep:S226). Sprint 6 first token.
// Implements the Class A syntax parser (PropertyPath::parse) + the Class B
// value-domain evaluator (evaluate_property_path) + the default
// dependency-object reader. See PropertyPath.h for the full contract.

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

const char* skip_ws(const char* cur, const char* end) noexcept
{
    while (cur != end && (static_cast<int>(*cur) == ' ' || static_cast<int>(*cur) == '\t' ||
                          static_cast<int>(*cur) == '\r' || static_cast<int>(*cur) == '\n' ||
                          static_cast<int>(*cur) == '\f' || static_cast<int>(*cur) == '\v'))
    {
        ++cur;
    }
    return cur;
}

bool is_name_break(char c) noexcept
{
    return c == '.' || c == '[' || c == ']' ||
           (static_cast<int>(c) == ' ') || (static_cast<int>(c) == '\t') ||
           (static_cast<int>(c) == '\r') || (static_cast<int>(c) == '\n') ||
           (static_cast<int>(c) == '\f') || (static_cast<int>(c) == '\v');
}

// Human-readable "what" for a PropertyPathException.
std::string build_what(PropertyPathError kind, const std::string& path,
                       const std::string& fragment)
{
    const char* kindName = nullptr;
    switch (kind)
    {
    case PropertyPathError::Empty:               kindName = "empty property path"; break;
    case PropertyPathError::TrailingDot:         kindName = "trailing dot"; break;
    case PropertyPathError::ConsecutiveDot:      kindName = "consecutive dots"; break;
    case PropertyPathError::UnterminatedIndexer: kindName = "unterminated indexer"; break;
    case PropertyPathError::EmptyIndexer:        kindName = "empty indexer"; break;
    case PropertyPathError::MalformedIndex:      kindName = "malformed index"; break;
    case PropertyPathError::UnclosedQuote:       kindName = "unclosed quote"; break;
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

// Parses the index INSIDE the brackets at [innerCur, innerEnd) into `out`.
// Returns false on a malformed index (kind + *frag set), true on success.
bool parse_index_inner(const char* innerCur, const char* innerEnd,
                       PropertyIndex& out, PropertyPathError& kind,
                       std::string& frag)
{
    innerCur = skip_ws(innerCur, innerEnd);

    if (innerCur == innerEnd)
    {
        kind = PropertyPathError::EmptyIndexer; // "Items[]"
        frag = "[]";
        return false;
    }

    const char c = *innerCur;

    // String key: single or double quotes.
    if (c == '\'' || c == '"')
    {
        const char quote = c;
        std::string key;
        for (const char* p = innerCur + 1; p != innerEnd; ++p)
        {
            if (*p == quote)
            {
                const char* after = skip_ws(p + 1, innerEnd);
                if (after != innerEnd)
                {
                    kind = PropertyPathError::MalformedIndex; // trailing junk after quoted key
                    frag = "[" + std::string(innerCur, innerEnd) + "]";
                    return false;
                }
                out.kind = PropertyIndex::Kind::StringKey;
                out.str = std::move(key);
                return true;
            }
            key.push_back(*p);
        }
        kind = PropertyPathError::UnclosedQuote; // "Items[\"abc]
        frag = "[" + std::string(innerCur, innerEnd) + "]";
        return false;
    }

    // Integer: optional '-', then 1+ digits.
    bool negative = false;
    const char* p = innerCur;
    if (*p == '-')
    {
        negative = true;
        ++p;
    }
    if (p == innerEnd || !std::isdigit(static_cast<unsigned char>(*p)))
    {
        kind = PropertyPathError::MalformedIndex; // "Items[-]" / "Items[abc]"
        frag = "[" + std::string(innerCur, innerEnd) + "]";
        return false;
    }
    std::int64_t value = 0;
    for (; p != innerEnd; ++p)
    {
        if (!std::isdigit(static_cast<unsigned char>(*p)))
        {
            kind = PropertyPathError::MalformedIndex; // "Items[1.5]"
            frag = "[" + std::string(innerCur, innerEnd) + "]";
            return false;
        }
        const int digit = static_cast<int>(*p - '0');
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10)
        {
            kind = PropertyPathError::MalformedIndex; // overflow
            frag = "[" + std::string(innerCur, innerEnd) + "]";
            return false;
        }
        value = value * 10 + digit;
    }
    out.kind = PropertyIndex::Kind::Integer;
    out.integer = negative ? -value : value;
    return true;
}

} // namespace

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
    cur = skip_ws(cur, end);
    if (cur == end)
    {
        fragment = std::string(text);
    }
    else
    {
        std::string_view rest(cur, end - cur);
        const std::size_t open = rest.find('[');
        const std::size_t dot = rest.find('.');
        if (open != std::string_view::npos)
        {
            const std::size_t close = rest.find(']', open + 1);
            fragment = (close != std::string_view::npos)
                ? std::string(rest.substr(open, close - open + 1))
                : std::string(rest.substr(open));
        }
        else if (dot != std::string_view::npos)
        {
            fragment = std::string(rest.substr(dot));
        }
        else
        {
            fragment = std::string(text);
        }
    }
    throw PropertyPathException(err, std::string(text), std::move(fragment));
}

VAML_CORE_EXPORT
bool PropertyPath::try_parse(std::string_view text, PropertyPath& out,
                             PropertyPathError* err) noexcept
{
    const char* cur = text.data();
    const char* end = text.data() + text.size();

    // Empty or whitespace-only path.
    const char* first = skip_ws(cur, end);
    if (first == end)
    {
        if (err != nullptr) { *err = PropertyPathError::Empty; }
        return false;
    }

    std::vector<PropertySegment> segments;
    cur = first;

    while (true)
    {
        cur = skip_ws(cur, end);
        if (cur == end)
        {
            break;
        }

        const char c = *cur;

        // --- Leading '.' — route step to a property name.
        if (c == '.')
        {
            if (segments.empty())
            {
                // A path may not START with '.' (WPF-faithful).
                if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
                return false;
            }
            const char* after = skip_ws(cur + 1, end);
            if (after == end)
            {
                if (err != nullptr) { *err = PropertyPathError::TrailingDot; } // "Address."
                return false;
            }
            if (*after == '.')
            {
                if (err != nullptr) { *err = PropertyPathError::ConsecutiveDot; } // "Address.."
                return false;
            }
            if (*after == '[')
            {
                if (err != nullptr) { *err = PropertyPathError::TrailingDot; } // ".[" malformed
                return false;
            }

            PropertySegment seg;
            seg.kind = PropertySegmentKind::Property;
            const char* p = after;
            while (p != end && !is_name_break(*p))
            {
                ++p;
            }
            if (p == after)
            {
                if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
                return false;
            }
            seg.name.assign(after, p);
            seg.source = "." + seg.name;
            segments.push_back(std::move(seg));
            cur = p;
            continue;
        }

        // --- Leading '[' — an index step.
        if (c == '[')
        {
            const char* openPos = cur;
            const char* close = nullptr;
            for (const char* p = openPos + 1; p != end; ++p)
            {
                if (*p == ']')
                {
                    close = p;
                    break;
                }
            }
            if (close == nullptr)
            {
                if (err != nullptr) { *err = PropertyPathError::UnterminatedIndexer; } // "Items[0"
                return false;
            }
            PropertyIndex idx;
            PropertyPathError innerErr;
            std::string frag;
            if (!parse_index_inner(openPos + 1, close, idx, innerErr, frag))
            {
                if (err != nullptr) { *err = innerErr; }
                return false;
            }
            PropertySegment seg;
            seg.kind = PropertySegmentKind::Index;
            seg.index = std::move(idx);
            seg.name = (seg.index.kind == PropertyIndex::Kind::StringKey) ? seg.index.str : std::string();
            seg.source.assign(openPos, close + 1);
            segments.push_back(std::move(seg));
            cur = close + 1;
            continue;
        }

        // --- A bare token: allowed ONLY as the FIRST step.
        if (segments.empty())
        {
            PropertySegment seg;
            seg.kind = PropertySegmentKind::Property;
            const char* p = cur;
            while (p != end && !is_name_break(*p))
            {
                ++p;
            }
            if (p == cur)
            {
                if (err != nullptr) { *err = PropertyPathError::Empty; }
                return false;
            }
            seg.name.assign(cur, p);
            seg.source = seg.name;
            segments.push_back(std::move(seg));
            cur = p;
            continue;
        }

        // A bare token mid-path ("Address Street") — the only chars that
        // continue a path after a step are '.' and '['. Stable diagnostic.
        if (err != nullptr) { *err = PropertyPathError::TrailingDot; }
        return false;
    }

    out = PropertyPath();
    out.valid_ = true;
    out.original_ = std::string(text);
    out.segments_ = std::move(segments);
    return true;
}

VAML_CORE_EXPORT
PropertyRead evaluate_property_path(const Value& root, const PropertyPath& path,
                                    const PropertyReader& reader)
{
    if (!path.valid() || path.segment_count() == 0)
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

VAML_CORE_EXPORT
PropertyReader dependency_object_reader()
{
    return [](const Value& node, const PropertySegment& segment) -> PropertyRead {
        // Only a DependencyObject behind UntypedPtr is a readable object node
        // for the default reader.
        if (node.type() != ValueType::UntypedPtr)
        {
            return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
        }
        void* raw = node.get<void*>();
        DependencyObject* obj = static_cast<DependencyObject*>(raw);
        if (obj == nullptr)
        {
            return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
        }

        if (segment.kind == PropertySegmentKind::Index)
        {
            // A plain DependencyObject is not indexable. Honest Class B answer;
            // collection readers land with collections (NO-GAP, recorded).
            return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
        }

        // Property segment: read through the DP store. The unregistered case
        // becomes MissingProperty (never a thrown raw PropertyNotFound). We use
        // the object's own Get and map the unregistered throw.
        try
        {
            return PropertyRead{true, PropertyPathError::Empty,
                                obj->Get(PropertyId::of(segment.name)), 0};
        }
        catch (const PropertyNotFound&)
        {
            return PropertyRead{false, PropertyPathError::MissingProperty, Value::Null(), 0};
        }
    };
}

} // namespace vaml
