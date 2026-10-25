#pragma once
// VAML ResourceMarkup — the {StaticResource ...} / {DynamicResource ...} markup
// resolution seams. S361 (dep:S360) — Sprint 9.
//
// S252's parse_markup_binding classifies `{StaticResource X}` / `{DynamicResource X}`
// as kind None (a non-binding extension, left deferred to S361). THIS step
// (S361) is that deferral's resolution point: the resource-markup parser turns
// a deferred source into a structured `ResourceMarkup` (Static/Dynamic + the
// resource key), and the VamlReader + a runtime resolver consume it against the
// S360 three-scope lookup.
//
// GRAMMAR (WPF-faithful, minimal for the v1 surface):
//   {StaticResource Key}    — resolved ONCE at apply time (a static reference)
//   {DynamicResource Key}   — a DEFERRED live link, re-resolved when the source
//                             dictionary key changes (S361)
// Outcomes:
//   - name == "StaticResource"  -> ResourceMarkup{ Static, key }
//   - name == "DynamicResource" -> ResourceMarkup{ Dynamic, key }
//   - anything else (a bare-naming extension / {x:Null} / {Binding...}) ->
//     `recognized == false` (NOT this layer's markup; left to its own layer).
//   - malformed (an empty key / unbalanced / no resource name) ->
//     `malformed == true` (a Class A authoring error the reader reports loudly).
//
// ADR-018: ZERO new locks (pure parsing).

#include "vaml/core/export.h"

#include <cstdint>
#include <string_view>

namespace vaml {

// The resource-reference kind a {StaticResource}/{DynamicResource} source denotes.
enum class ResourceReferenceKind : std::uint8_t
{
    None    = 0, // not a resource reference (a bare-naming extension / x:Null / ...)
    Static  = 1, // {StaticResource Key} — resolve once at apply time
    Dynamic = 2, // {DynamicResource Key} — a deferred live link (S361)
};

// The structured parse of one deferred resource-markup source text.
struct VAML_CORE_EXPORT ResourceMarkup
{
    ResourceReferenceKind kind = ResourceReferenceKind::None;
    std::string_view key;    // the resource key (e.g. "MyBrush")
    bool malformed = false;  // true iff this is a {Static/DynamicResource ...} with bad inner syntax
    bool recognized = false; // true iff the source names StaticResource/DynamicResource
};

// Parses a deferred markup source ("{StaticResource X}" / "{DynamicResource X}")
// into a ResourceMarkup. Pure + noexcept + headless.
[[nodiscard]] VAML_CORE_EXPORT ResourceMarkup parse_resource_markup(std::string_view source) noexcept;

// Convenience seam: true iff `source` is a {StaticResource <key>} markup.
[[nodiscard]] VAML_CORE_EXPORT bool static_resource_markup(std::string_view source) noexcept;

// Convenience seam: true iff `source` is a {DynamicResource <key>} markup.
[[nodiscard]] VAML_CORE_EXPORT bool dynamic_resource_markup(std::string_view source) noexcept;

} // namespace vaml
