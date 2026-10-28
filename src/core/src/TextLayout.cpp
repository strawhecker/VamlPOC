// S400 — TextLayout implementation (vaml::core). The pure CPU text-layout +
// wrapping seam: see TextLayout.h for the split, the wrap rule, and the
// metrics-provider seam. This TU is headless + deterministic (no Vulkan, no
// device, no I/O) so the core tests prove the wrap/advance math exactly.

#include "vaml/core/TextLayout.h"

#include "vaml/core/BindingAccess.h"       // S250_1 view diagnostics channel (never silent)
#include "vaml/core/Brush.h"               // SolidColorBrush (the resolved brush object)
#include "vaml/core/ResourceDictionary.h"  // S360 find_for_element (three-scope walk)
#include "vaml/core/UIElement.h"           // UIElement (the resolution anchor)
#include "vaml/core/Value.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace vaml {

TextGlyphMetric default_text_metric(std::uint32_t codepoint, float fontSize) noexcept
{
    // Bounded ASCII estimate (see header): printable 32..126 advance 0.6em /
    // paint; space advances 0.3em / no paint; out-of-range advances 0 / no
    // paint. fontSize clamps >= 0 so a degenerate size never yields negative
    // advance.
    const float fs = std::max(0.0f, fontSize);
    if (codepoint >= 32u && codepoint <= 126u)
    {
        if (codepoint == 32u) // space: advances, does NOT paint
            return TextGlyphMetric{ fs * 0.3f, false };
        return TextGlyphMetric{ fs * 0.6f, true };
    }
    return TextGlyphMetric{ 0.0f, false };
}

// S465 — GRADIENT FILL RESOLUTION (RESOLVES the S460 LinearGradientBrush seed).
// See TextLayout.h. Samples the authored gradient at the normalized position `t`
// along its Stop sequence.
Color resolve_gradient_fill(const UIElement& element, std::int32_t brushId, double t) noexcept
{
    // Default 0 -> opaque black (the default un-authored fill), no diagnostic.
    if (brushId == 0)
    {
        return Color{ 0, 0, 0, 255 };
    }

    const std::string key = "Brush:" + std::to_string(brushId);
    const ResourceValue* rv = vaml::find_for_element(element, key);
    if (rv && rv->kind() == ResourceValue::Kind::Object && rv->object())
    {
        if (const auto* grad = dynamic_cast<const LinearGradientBrush*>(rv->object()))
        {
            const auto& stops = grad->stops();
            // <2 stops -> representative (scalar) color (can't interpolate a
            // gradient from 0/1 stops); the honest boundary the S465 sampler
            // reports rather than inventing one.
            if (stops.size() < 2)
            {
                return grad->representative_color();
            }
            // Clamp t to [0,1], then interpolate across the ordered stops by
            // their Offset (the linear ramp between consecutive stops).
            const double tt = std::clamp(t, 0.0, 1.0);
            if (tt <= stops.front().offset)
            {
                return stops.front().color;
            }
            if (tt >= stops.back().offset)
            {
                return stops.back().color;
            }
            for (std::size_t i = 0; i + 1 < stops.size(); ++i)
            {
                const double a = stops[i].offset;
                const double b = stops[i + 1].offset;
                if (tt < a || tt > b)
                {
                    continue;
                }
                const double span = (b - a) == 0.0 ? 1.0 : (b - a);
                const double f = std::clamp((tt - a) / span, 0.0, 1.0);
                const Color ca = stops[i].color;
                const Color cb = stops[i + 1].color;
                auto lerp = [&](std::uint8_t x, std::uint8_t y) {
                    return static_cast<std::uint8_t>(
                        static_cast<unsigned>(x) * (1.0 - f) +
                        static_cast<unsigned>(y) * f + 0.5);
                };
                return Color{ lerp(ca.r, cb.r), lerp(ca.g, cb.g), lerp(ca.b, cb.b), 255u };
            }
            return stops.back().color;
        }
    }

    // Unresolved non-zero brushId -> opaque black + a structured diagnostic
    // (the S250_1 channel; NO-silent, mirrors resolve_foreground_color).
    vaml::BindingDiagnostic d;
    d.reasonCode = vaml::BindingReasonCode::MissingProperty;
    d.targetProp = "Background";
    d.sourcePath = key;
    d.detail     = "gradient BrushId " + std::to_string(brushId) +
                   " did not resolve to an authored LinearGradientBrush; defaulted to opaque black";
    vaml::view_binding_diagnostics().emit(d);
    return Color{ 0, 0, 0, 255 };
}

Color resolve_foreground_color(const UIElement& element, std::int32_t brushId) noexcept
{
    // Default (0) -> opaque black (the Text default foreground). No lookup, no
    // diagnostic — this is the honest default, not a failure.
    if (brushId == 0)
    {
        return Color{ 0, 0, 0, 255 };
    }

    // A non-zero BrushId resolves a registered SolidColorBrush via the S360
    // three-scope walk anchored at the element. The key convention is the
    // interned string "Brush:<id>" (S144 discipline; a stable, searchable key).
    const std::string key = "Brush:" + std::to_string(brushId);
    const ResourceValue* rv = vaml::find_for_element(element, key);
    if (rv && rv->kind() == ResourceValue::Kind::Object && rv->object())
    {
        if (const auto* brush = dynamic_cast<const SolidColorBrush*>(rv->object()))
        {
            return brush->color();
        }
        // S460 (BRUSH AUTHORING): an authored LinearGradientBrush also resolves
        // by id — its scalar representative color feeds the same scalar-foreground
        // path (S403-note: S465 upgrades this to the real gradient render).
        if (const auto* grad = dynamic_cast<const LinearGradientBrush*>(rv->object()))
        {
            return grad->representative_color();
        }
    }

    // Unresolved non-zero BrushId -> opaque black (the deliberate fallback) +
    // a STRUCTURED diagnostic through the S250_1 view diagnostics channel
    // (mirrors binding diagnostics; NEVER silent, NEVER a crash — ADR-016
    // observation, never control flow). The diagnostic reuses the binding
    // record + sink surface (one channel, two consumers — S250_1).
    vaml::BindingDiagnostic d;
    d.reasonCode = vaml::BindingReasonCode::MissingProperty;
    d.targetProp = "Foreground";
    d.sourcePath = key;
    d.detail     = "Text foreground BrushId " + std::to_string(brushId) +
                   " did not resolve to a registered SolidColorBrush; defaulted to opaque black";
    vaml::view_binding_diagnostics().emit(d);
    return Color{ 0, 0, 0, 255 };
}


TextLayoutPlan text_layout_plan(std::string_view content,
                                float fontSize,
                                float availableWidth,
                                const TextGlyphMetrics& metrics,
                                float lineHeightEm)
{
    const float fs = std::max(0.0f, fontSize);
    const float lh = std::max(0.0f, lineHeightEm) * fs;
    const float avail = std::max(0.0f, availableWidth);

    TextLayoutPlan plan;
    plan.lineHeight = lh;

    if (content.empty())
    {
        // Empty content: ZERO lines, height 0 (a Text with no content measures
        // to 0x0 — it takes no box).
        return plan;
    }

    // Cream the content into a codepoint array (the provider keys on codepoints;
    // byte iterating over ASCII is safe here, but we read through uint8 to be
    // explicit). VAML ASCII scope: content is printable ASCII (32..126).
    std::vector<std::uint32_t> cps;
    cps.reserve(content.size());
    for (char ch : content)
    {
        const std::uint32_t cp = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        cps.push_back(cp);
    }

    auto metricFor = [&](std::uint32_t cp) -> TextGlyphMetric {
        return metrics ? metrics(cp) : default_text_metric(cp, fs);
    };

    // Line assembly state. We build one line's glyphs then emit it on a wrap /
    // at the end. `lineWidth` = current cursor width; `wordWidth` = the current
    // (non-space) run's accumulated advance, measured from the run's first
    // glyph.
    std::vector<TextGlyphPlacement> cur;
    float cursorX = 0.0f;
    float maxW    = 0.0f;

    // Emits the current line (if it has any glyphs) into `plan`, then resets.
    auto flush = [&]() {
        if (cur.empty())
            return;
        // DROP TRAILING spaces (they would overhang the line's measured width;
        // a space before a wrap becomes the leading dropped space of the next
        // line — it inflates THIS line's width otherwise). The cursor already
        // advanced past them, but the line width is recomputed from the kept,
        // non-trailing-space glyphs only.
        while (!cur.empty() && !cur.back().render)
        {
            cur.pop_back();
        }
        if (cur.empty())
            return; // nothing renderable on this line (a lone trailing space)
        TextLayoutLine line;
        line.y          = static_cast<float>(plan.lines.size()) * lh;
        line.lineHeight = lh;
        line.glyphs     = std::move(cur);
        float w = 0.0f;
        for (const auto& g : line.glyphs)
            w = std::max(w, g.x + g.advance);
        line.width = w;
        maxW       = std::max(maxW, w);
        plan.lines.push_back(std::move(line));
        cur.clear();
    };

    // Tokenize into words (runs of non-space) + the interleaved spaces, so the
    // wrap decision is WORD-LEVEL (a word wraps as a unit; spaces advance).
    std::size_t i = 0;
    while (i < cps.size())
    {
        // Skip spaces: they advance the line cursor (and a leading fallback to
        // a new line drops them). A sequence of spaces just advances cursorX,
        // but if the NEXT word would wrap, the trailing spaces are NOT carried
        // (they become the leading dropped spaces).
        if (cps[i] == 32u)
        {
            const auto m = metricFor(32u);
            cursorX += m.advance;
            cur.push_back(TextGlyphPlacement{ 32u, cursorX - m.advance, plan.lines.size() * lh,
                                              m.advance, false });
            ++i;
            continue;
        }

        // A word: accumulate its placement across glyphs, then decide the wrap.
        float wordStart = cursorX; // where the word begins on the CURRENT line
        std::vector<TextGlyphPlacement> word;
        while (i < cps.size() && cps[i] != 32u)
        {
            const std::uint32_t cp = cps[i];
            const auto m = metricFor(cp);
            word.push_back(TextGlyphPlacement{ cp, 0.0f, plan.lines.size() * lh, m.advance, m.render });
            ++i;
        }
        // Anchor the word's glyphs (x = wordStart + sum of preceding advances).
        float runX = wordStart;
        float wordW = 0.0f;
        for (auto& g : word)
        {
            g.x = runX;
            runX += g.advance;
            wordW += g.advance;
        }

        // WRAP DECISION (the S123/S134 word-wrap rule): if the word does NOT fit
        // on the current line (wordStart + wordW > avail) AND the line already
        // has content (wordStart > 0), wrap -> flush the current line, start a
        // new one at cursor 0. A word LONGER than the full avail still gets its
        // own line (overflows rather than wraps mid-word).
        if (wordStart > 0.0f && wordStart + wordW > avail)
        {
            flush();
            // Re-anchor the word onto the new line at cursor 0 (the leading
            // dropped space already advanced the OLD line only).
            float nx = 0.0f;
            for (auto& g : word)
            {
                g.y = plan.lines.size() * lh;
                g.x = nx;
                nx += g.advance;
            }
            wordStart = 0.0f;
        }
        // Append the (possibly re-anchored) word to the current line.
        for (auto& g : word)
        {
            cursorX = g.x + g.advance; // keep cursor current
            cur.push_back(std::move(g));
        }
    }

    flush();
    if (plan.lines.empty())
    {
        // Whitespace-only input -> ONE line (lineHeight tall) with the spaces'
        // combined width; never zero lines for a non-empty blank string.
        TextLayoutLine line;
        line.lineHeight = lh;
        line.width      = cursorX;
        plan.lines.push_back(std::move(line));
        maxW = cursorX;
    }

    plan.measuredWidth  = maxW;
    plan.measuredHeight = static_cast<float>(plan.lines.size()) * lh;
    return plan;
}

} // namespace vaml
