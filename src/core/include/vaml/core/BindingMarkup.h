#pragma once
// VAML BindingMarkup — the markup-extension parser for {Binding ...} / {Command ...}.
// S252 (dep:S251). Sprint 6.
//
// The S092 reader records an S084-classified isExtension attribute value as a
// DeferredExtension with its ORIGINAL source text retained verbatim (the "S2
// tag", e.g. `{Binding Title}`). THIS step (S252) is the S2 tag's resolution
// point: `parse_markup_binding` parses that source into a structured
// BindingMarkup so the VamlReader (and the app) can bind it.
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — the
// parser + the markup shape are complete and headless-testable on delivery,
// and are kept-and-extended by S253 {Command}).
//
// Grammar (WPF-faithful, minimal for the v1 surface):
//   {Binding Title}
//   {Binding Title, Mode=TwoWay}        (Mode=OneTime | OneWay | TwoWay)
//   {Command DoIt}
//   {StaticResource X}  ->  kind None (an extension the binding layer does NOT
//                           own — left deferred; resolution is S361)
//
// Distinguishable outcomes:
//   - kind==Binding / kind==Command : a live row this layer owns.
//   - kind==None && !malformed      : a non-binding extension (StaticResource,
//                                     x:Null, ...) — NOT ours, leave deferred.
//   - malformed                     : the source is `{Binding ...}` / `{Command ...}`
//                                     but the inner syntax is wrong (empty path,
//                                     a bad Mode= value, unbalanced). A Class A
//                                     AUTHORING error the reader reports loudly
//                                     (never silently left deferred).
//
// Empty/whitespace + un-brace-balanced input returns kind None (not ours) — the
// scanner already classifies only balanced-brace values as isExtension.
//
// ADR-018: ZERO new locks (pure parsing, no shared state).

#include "vaml/core/export.h"

#include <cstdint>
#include <string_view>

namespace vaml {

// Which markup extension kind a {Binding ...} / {Command ...} source denotes.
enum class MarkupExtensionKind : std::uint8_t
{
    None    = 0, // not a binding/command extension (StaticResource, x:Null, ...)
    Binding = 1, // {Binding <path>[, Mode=...]}
    Command = 2, // {Command <name>}  (S253 resolves it against a command registry)
};

// The structured parse of one deferred markup-extension source text.
struct VAML_CORE_EXPORT BindingMarkup
{
    MarkupExtensionKind kind = MarkupExtensionKind::None;
    std::string_view path;    // the Binding path (e.g. "Title") / the Command name (e.g. "DoIt")
    std::string_view mode;    // the Mode= value verbatim when present (e.g. "TwoWay"); empty otherwise
    bool hasMode = false;     // true iff a Mode=<value> token was authored
    bool malformed = false;   // true iff this is a Binding/Command extension with bad inner syntax
};

// Parses a deferred markup-extension source text ("{Binding Title}" / "{Command
// DoIt}" / "{Binding Title, Mode=TwoWay}") into a BindingMarkup. Pure + noexcept
// + headless. See the grammar + outcome table in the header comment.
[[nodiscard]] VAML_CORE_EXPORT BindingMarkup parse_markup_binding(std::string_view source) noexcept;

} // namespace vaml
