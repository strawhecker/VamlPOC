#pragma once
// S281 (dep:S280) — Binary record set: the versioned .vbaml schema.
//
// The compiled program (S280's CompiledProgram) is serialized to a compact,
// VERSIONED binary record set with six sections — EXACTLY the roadmap title:
//     element-id / type-token / prop-token / event-token / resource-token / string-table.
//
//   * element-id     — the element-id SECTION: the total element count + each
//                      element's implicit id (== its document-order index).
//   * type-token     — the table of distinct element TYPE names. Each element
//                      references one via a TOKEN (its string-table index).
//   * prop-token     — the table of distinct PROPERTY KEYS. Each property
//                      attribute references one via a token.
//   * event-token    — the (schema-complete) EVENT token table. The S092/S322
//                      reader event surface owns GENERATION; this schema delivers
//                      the stable, enum-based event-token table so the record
//                      set is forward-complete from the start.
//   * resource-token — the (schema-complete) RESOURCE token table. Resource
//                      authoring is S361; the token table is present and
//                      enum-based so resource references compile to stable ids
//                      before that sprint lands (NO-GAP, not silent).
//   * string-table   — the string TABLE: every distinct string (type names,
//                      property keys, attribute values, binding paths) lives
//                      here ONCE; every record references the table by a
//                      VarU32 index — never by value.
//
// The five string-bearing tables are all enumerations INTO the single string
// table ("token tables enumerated into the string table"). The type/prop/
// event/resource tables are each an ORDERED LIST of string-table indexes; the
// string table is the authoritative store of the unique UTF-8 strings.
//
// DESIGN FOR BYTE-IDENTITY (S282): internment is INSERTION-ORDER + content
// deduped (never a hash map that perturbs byte-for-byte output). The schema is
// built by an explicit document-order pass over the compiled program (see
// build_schema), so the same program always yields the same schema and the
// same bytes. Encode/decode are PURE functions of the in-memory model
// (ADR-018: zero locks; no timestamps, no metadata).

#include "vaml/tools/export.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::tools::vbaml {

// The canonical .vbaml magic (4 bytes, big-endian) — "vBML".
inline constexpr std::uint32_t kVbamlMagic  = 0x76424D4C; // 'v' 'B' 'M' 'L'
inline constexpr std::uint16_t kVbamlVersion = 1;        // schema version

// The built-in, schema-complete EVENT token names (enum-stable ordinals).
// The registered-event surface (S092/S322) appends its own names at the RUNTIME
// side; this schema-level table ships the canonical core set so event records
// compile to stable, forward-contractual ids before Sprint 8.
enum EventToken : std::uint8_t
{
    EventNone = 0,
    EventClick = 1,   // control click (S320 routed event)
    EventChanged = 2, // value/selection changed
    EventCustom = 99, // user-authored handler name (deferred; S322 resolves)
};

// The built-in, schema-complete RESOURCE token names (enum-stable ordinals).
// Resource authoring is S361; the token table is present so resource references
// compile to stable ids now (NO-GAP, not silent).
enum ResourceToken : std::uint8_t
{
    ResourceNone = 0,
    ResourceBrush = 1,  // a Brush resource
    ResourceColor = 2,  // a Color resource
};

// The IMMUTABLE flushed schema. `strings` is the complete string table
// (insertion order) — the schema OWNS its strings (plain std::string, so a
// flushed Schema remains valid after the builder is destroyed). `types`/`props`/
// `events`/`resources` are ORDERED LISTS OF STRING-TABLE INDEXES — the semantic
// token tables, each enumerating which string-table entries belong to that
// section. A token for a type/prop/event/resource is simply its string-table
// index.
struct VAML_TOOLS_EXPORT Schema
{
    std::uint32_t magic   = kVbamlMagic;
    std::uint16_t version = kVbamlVersion;

    // OWNED string table. Use string_table_view(i) / string_table(i) helpers to
    // read entries as views or copies without tripping on ownership.
    std::vector<std::string> strings;      // the full string table (unique, insertion order)
    std::vector<std::uint32_t>    types;     // type-token table: indexes into strings
    std::vector<std::uint32_t>    props;     // prop-token table: indexes into strings
    std::vector<std::uint32_t>    events;    // event-token table: indexes into strings
    std::vector<std::uint32_t>    resources; // resource-token table: indexes into strings

    // Read entry `i` as a view (valid for the schema's lifetime).
    [[nodiscard]] std::string_view string_view(std::size_t i) const noexcept
    {
        return i < strings.size() ? std::string_view(strings[i]) : std::string_view{};
    }
};

// The MUTABLE schema builder. Open until flush(); intern() is insertion-order +
// content-deduped. flush() snaps an immutable Schema (the builder's owned
// stable string storage outlives the returned Schema).
class VAML_TOOLS_EXPORT SchemaBuilder
{
public:
    SchemaBuilder();
    ~SchemaBuilder();

    SchemaBuilder(const SchemaBuilder&) = delete;
    SchemaBuilder& operator=(const SchemaBuilder&) = delete;

    // Interns `s`: returns its stable string-table index (0 for empty).
    [[nodiscard]] std::uint32_t intern(std::string_view s);
    // Public type/prop/event/resource token accessors — each records the index
    // in its semantic token table AND interning the name into the string table.
    [[nodiscard]] std::uint32_t type_token(std::string_view name) { return intern_list(name, &types_); }
    [[nodiscard]] std::uint32_t prop_token(std::string_view name) { return intern_list(name, &props_); }
    [[nodiscard]] std::uint32_t event_token(std::string_view name) { return intern_list(name, &events_); }
    [[nodiscard]] std::uint32_t resource_token(std::string_view name) { return intern_list(name, &resources_); }

    // Finalizes the builder into an immutable Schema snapshot. After flush the
    // builder is inert (use the returned Schema).
    [[nodiscard]] VAML_TOOLS_EXPORT Schema flush();

private:
    [[nodiscard]] std::uint32_t intern_list(std::string_view s, std::vector<std::uint32_t>* list);
    // The builder's OWNED stable string storage (content-deduped, insertion
    // order). Vectors are the members (not a pimpl) — this is an internal tool
    // type, not a stable ABI surface, so there is no pimpl to defend (ADR-021).
    std::vector<std::string> owned_;
    std::vector<std::uint32_t> types_;     // type-token indexes (insertion order)
    std::vector<std::uint32_t> props_;     // prop-token indexes
    std::vector<std::uint32_t> events_;    // event-token indexes
    std::vector<std::uint32_t> resources_; // resource-token indexes
};

} // namespace vaml::tools::vbaml
