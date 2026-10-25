#pragma once
// VAML TextLayout — the PURE CPU text-layout + wrapping seam for the S400 Text
// control (vaml::core). This is the S400 "CPU text-layout engine (measure +
// wrap)" headline, kept IN core per ADR-011 (control logic lives in core;
// rendering — the SDF glyph UV mapping — is a vaml::vulkan concern consumed via
// S400's build_text_glyph_items).
//
// The seam is deliberately METRICS-PROVIDER-INDEPENDENT: `text_layout_plan`
// queries a `TextGlyphMetrics` provider for each codepoint's horizontal advance
// (and whether it paints). This makes the seam fully HEADLESS + deterministic:
//   - a REAL S205/SDF SdfAtlas/FontAtlas supplies real advances at RENDER time
//     (the vulkan build_text_glyph_items feeds the provider from the atlas),
//   - the S400 Text::MeasureOverride feeds `default_text_metric` (a bounded
//     estimate, fontSize-based) so measuring needs no device,
//   - the core tests drive a TOY provider to prove wrap/advance correctness.
//
// WRAP RULE (the S123 wrap rule applied at the glyph-advance level, WPF-
// faithful): text flows left-to-right within `availableWidth`; a WORD (a run
// of non-space glyphs) that would push the cursor past `availableWidth` WRAPS
// to a new line; a single word LONGER than the full `availableWidth` still gets
// its own line (it overflows rather than wrapping mid-word) — the S134
// "a child wider than the whole boundary still gets its own row" precedent
// applied to words. SPACES advance the cursor + participate in wrap but render
// nothing (`render=false`); leading spaces of a wrapped line are dropped.
//
// NOT a stub / NOT MINIMAL REAL — this is the real, permanent, kept-and-
// extended text layout engine the S400 Text control + the S402 TextBox
// (multiline) + the S409_1 viewer growth all consume. NO-GAP: glyph UV/atlas
// mapping is S400-render (vulkan), NOT backfilled here (ADR-011 / ADR-021).

#include "vaml/core/Color.h"      // Color (the resolved foreground)
#include "vaml/core/export.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace vaml {

class UIElement; // forward — resolve_foreground_color anchors resolution at an element

// One glyph's horizontal metric the layout uses. `render=false` for a codepoint
// that occupies advance width but paints nothing (e.g. a space) — it still
// advances the cursor + participates in the wrap decision.
struct TextGlyphMetric
{
    float advance = 0.0f;
    bool  render  = true;
};

// The metrics provider the layout queries per codepoint. A real provider reads
// a glyph atlas (vulkan render); a toy provider drives the core tests; the Text
// control's Measure uses default_text_metric (a bounded estimate).
using TextGlyphMetrics = std::function<TextGlyphMetric(std::uint32_t codepoint)>;

// One placed glyph: codepoint + top-left (x,y) relative to the layout origin +
// its advance width. `render` is carried so the renderer emits only paint-able
// glyphs (a space advances but never emits a quad).
struct TextGlyphPlacement
{
    std::uint32_t codepoint = 0;
    float         x         = 0.0f;
    float         y         = 0.0f;
    float         advance   = 0.0f;
    bool          render    = true;
};

// One wrapped line: its top Y (line index × lineHeight), its height (the
// layout's line height) + the glyphs placed on it (left-to-right). `width` =
// the last glyph's right edge (the cursor).
struct TextLayoutLine
{
    float y          = 0.0f;   // top Y of the line (line index × lineHeight)
    float lineHeight = 0.0f;
    float width      = 0.0f;
    std::vector<TextGlyphPlacement> glyphs;
};

// The result of text_layout_plan: the wrapped lines (top-to-bottom) + the
// measured extent (the DesiredSize the Text control reports).
struct TextLayoutPlan
{
    std::vector<TextLayoutLine> lines;
    float lineHeight     = 0.0f;
    float measuredWidth  = 0.0f;
    float measuredHeight = 0.0f;
};

// The DEFAULT metrics estimate the Text control's MeasureOverride uses so
// measuring needs no device: a printable ASCII codepoint (32..126) advances
// `fontSize * 0.6` and renders; a space advances `fontSize * 0.3` but does NOT
// paint; every other codepoint advances 0 + does NOT render (it contributes
// nothing to layout). This is a BOUNDED ESTIMATE (the render-time glyph emitter
// feeds the REAL atlas advances; this is only the headless-friendly layout
// basis). Pure + headless-testable.
VAML_CORE_EXPORT TextGlyphMetric default_text_metric(std::uint32_t codepoint,
                                                     float fontSize) noexcept;

// Resolves a Text.Foreground BrushId to a Color, WPF-faithful + NO-silent:
//   * brushId == 0 (the default) -> opaque black (the Text default foreground).
//   * a non-zero brushId maps to a registered SolidColorBrush resource looked up
//     by the interned key `Brush:<brushId>` through the S360 three-scope walk
//     anchored at `element` (element -> parent chain -> app -> system). When
//     found, its Color is returned.
//   * an UNRESOLVED non-zero brushId -> opaque black (the deliberate fallback)
//     AND a structured diagnostic emitted through the S250_1 view diagnostics
//     channel (mirrors binding diagnostics; NEVER silent, NEVER a crash).
// Pure + headless-testable.
VAML_CORE_EXPORT Color resolve_foreground_color(const UIElement& element,
                                                std::int32_t brushId) noexcept;

// The WRAP ENGINE. Flows `content` (ASCII 32..126; out-of-range codepoints use
// the provider, which usually returns {0,false}) into wrapped lines within
// `availableWidth`, using `metrics` for the per-glyph advance + render flag and
// `lineHeightEm` (default 1.2) × `fontSize` for line height. Returns the lines
// (top-to-bottom, y = i × lineHeight) + the measured extent {measuredWidth,
// measuredHeight}. Empty content -> ZERO lines/height 0; whitespace-only ->
// ONE line (lineHeight tall) with the spaces' combined advance width. A blank
// string never escapes a non-finite width; availableWidth is clamped >= 0.
// Pure + headless-testable (no Vulkan, no device, no I/O).
VAML_CORE_EXPORT TextLayoutPlan text_layout_plan(std::string_view content,
                                                 float fontSize,
                                                 float availableWidth,
                                                 const TextGlyphMetrics& metrics,
                                                 float lineHeightEm = 1.2f);

} // namespace vaml
