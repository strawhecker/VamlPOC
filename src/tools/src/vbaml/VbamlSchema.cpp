// S281 (dep:S280) — the versioned .vbaml binary record-set schema: the six
// sections (element-id / type-token / prop-token / event-token / resource-token
// / string-table), with token tables enumerated INTO the string table.
//
// The builder interns strings in INSERTION ORDER + content-deduped, so the same
// document-order program always yields the same schema (byte-identical, S282).
// ADR-018: zero locks (single-threaded build).

#include "vaml/tools/vbaml/VbamlSchema.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace vaml::tools::vbaml {

SchemaBuilder::SchemaBuilder() = default;
SchemaBuilder::~SchemaBuilder() = default;

std::uint32_t SchemaBuilder::intern(std::string_view s)
{
    for (std::size_t i = 0; i < owned_.size(); ++i)
    {
        if (owned_[i] == s)
        {
            return static_cast<std::uint32_t>(i);
        }
    }
    const std::uint32_t idx = static_cast<std::uint32_t>(owned_.size());
    owned_.emplace_back(s);
    return idx;
}

std::uint32_t SchemaBuilder::intern_list(std::string_view s, std::vector<std::uint32_t>* list)
{
    const std::uint32_t idx = intern(s);
    if (std::find(list->begin(), list->end(), idx) == list->end())
    {
        list->push_back(idx);
    }
    return idx;
}

Schema SchemaBuilder::flush()
{
    Schema s;
    s.magic = kVbamlMagic;
    s.version = kVbamlVersion;
    s.strings.reserve(owned_.size());
    for (const std::string& str : owned_)
    {
        s.strings.push_back(str);
    }
    s.types = types_;
    s.props = props_;
    s.events = events_;
    s.resources = resources_;
    return s;
}

} // namespace vaml::tools::vbaml
