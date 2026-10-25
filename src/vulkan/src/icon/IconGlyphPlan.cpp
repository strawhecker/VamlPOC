// S400_1 — IconGlyphPlan implementation (vaml::vulkan::icon). The RENDER-side
// pure seam that turns the Icon control's DP-driven inputs into the S206 draw
// item via the S213_1 best-fit + the cached icon provider, with the fallback
// glyph + structured diagnostic for an unresolved asset. See IconGlyphPlan.h
// for the split + NO-GAP. Pure + headless-testable.

#include <vaml/vulkan/icon/IconGlyphPlan.h>

#include <vaml/core/BindingAccess.h>   // vaml::view_binding_diagnostics() (S250_1 channel)
#include <vaml/core/Geometry.h>        // Rect (the logical box)
#include <vaml/vulkan/ui/TextGlyphPlan.h> // ui::text_glyph_uv (the S202 exact-grid tile->UV)
#include <vaml/vulkan/ui/UiScene.h>    // solid_rect (the emitted quad)

namespace vaml::vulkan::icon {

VAML_VULKAN_EXPORT
IconRenderPath icon_tint_to_path(vaml::IconTintMode mode) noexcept
{
    // TintMode::Preserve -> the full-color atlas sample (the existing quad/
    // SDF-text multicolor path). TintMode::Monochrome -> the S205_1 single-
    // channel alpha-mask, tinted by Foreground in-shader. Recorded NO-GAP in
    // S205_1 (the TintMode identity is S400_1's; the monochrome fragment is
    // S205_1's).
    return (mode == vaml::IconTintMode::Monochrome) ? IconRenderPath::Monochrome
                                                    : IconRenderPath::PreserveQuad;
}

namespace {

// The DELIBERATE fallback glyph: a recognizable filled square at the icon's
// logical box. NOT a crash, NOT a silent blank — a visible "broken icon"
// marker (ADR-015 NO-GAP). The straight-alpha color is a neutral, clearly-
// synthetic gray so it reads as a placeholder, distinct from a real asset.
ui::DrawItem makeFallbackGlyph(const vaml::Rect& box) noexcept
{
    return ui::solid_rect(box.x, box.y, box.width, box.height,
                          ui::Color4{ 0.80f, 0.80f, 0.80f, 0.55f });
}

// Emits a STRUCTURED diagnostic through the S250_1 view channel (mirrors
// binding diagnostics; ADR-016 observation-only, NEVER control flow, NEVER a
// crash). `reason` names the failure; `detail` carries the icon key context.
void emitIconDiagnostic(std::string_view reason, std::string detail) noexcept
{
    vaml::BindingDiagnostic d;
    d.reasonCode  = vaml::BindingReasonCode::MissingProperty; // shared Class B tag (an unresolved asset resolve)
    d.targetProp  = "IconKey";
    d.sourceType  = "IconAsset";
    d.detail      = std::string(reason) + ": " + detail;
    vaml::view_binding_diagnostics().emit(d);
}

} // namespace

VAML_VULKAN_EXPORT
IconDrawResult
build_icon_draw_item(const IconGlyphInput& in,
                     const std::vector<IconCandidate>& candidates,
                     bool hasSlot,
                     IconBestFitCache& bestFit,
                     const quad::AtlasLayout& layout,
                     const vaml::Color& resolvedForeground) noexcept
{
    IconDrawResult result;

    // --- Unresolved asset -> fallback glyph + structured diagnostic ---------
    // An unresolvable key (out-of-set / missing / not-yet-rasterized /
    // unsupported) MUST NOT crash and MUST NOT be silent (ADR-015 / the S202_1
    // NO-GAP fallback is S400_1's job). We emit the deliberate fallback glyph +
    // a structured diagnostic, then return. The asset slot is the decision: if
    // the caller said unresolved OR there is no populated candidate for the
    // key, there is nothing to sample.
    if (!in.assetResolved || !hasSlot)
    {
        result.ok = false;
        result.isFallback = true;
        result.item = makeFallbackGlyph(in.logicalBox);
        emitIconDiagnostic("icon-asset-unresolved",
                           "Icon control has no resolvable asset slot (key=" +
                               std::to_string(in.assetKey) + "); rendered the fallback glyph");
        return result;
    }

    // --- Resolved + a populated slot -> the S213_1 best-fit ----------------
    // The logical icon size (the box's smaller extent is the honest side; icons
    // rasterize square, so the best-fit axis is the logical side). The physical
    // N = logical side × dpiScale (S213_1 N_physical).
    const float side = (in.logicalBox.width < in.logicalBox.height)
                           ? in.logicalBox.width
                           : in.logicalBox.height;
    if (side <= 0.0f)
    {
        // A degenerate (zero/negative) logical icon box -> the fallback glyph +
        // a diagnostic (a zero box has no renderable quad). This is still a
        // "not resolvable to a visible draw" outcome (ADR-015 NO-silent).
        result.ok = false;
        result.isFallback = true;
        result.item = makeFallbackGlyph(in.logicalBox);
        emitIconDiagnostic("icon-degenerate-box", "Icon logical box is zero/negative; no glyph to render");
        return result;
    }
    const std::uint32_t nPhysical = physical_n_for(side, in.dpiScale);
    (void)nPhysical;

    // The S213_1 cached decision over the given size family. An empty candidate
    // family (hasSlot true but candidates empty — a transient/guarded state)
    // still falls back.
    if (candidates.empty())
    {
        result.ok = false;
        result.isFallback = true;
        result.item = makeFallbackGlyph(in.logicalBox);
        emitIconDiagnostic("icon-no-best-fit", "no S213_1 best-fit candidate resolved for the icon");
        return result;
    }
    const auto best = bestFit.resolve(in.assetKey, static_cast<std::uint32_t>(side), in.dpiScale,
                                     candidates);
    if (!best || best->candidateIndex < 0 ||
        static_cast<std::size_t>(best->candidateIndex) >= candidates.size())
    {
        result.ok = false;
        result.isFallback = true;
        result.item = makeFallbackGlyph(in.logicalBox);
        emitIconDiagnostic("icon-no-best-fit", "no S213_1 best-fit candidate resolved for the icon");
        return result;
    }
    const icon::IconCandidate& chosen = candidates[static_cast<std::size_t>(best->candidateIndex)];
    result.slotTile = chosen.tileIndex;
    result.targetN  = best->resampleToN ? best->targetN : chosen.side();

    // The quad UVs from the chosen tile via the S202 exact-grid layout. Both
    // Tint paths emit a box quad; the difference is which atlas sample the
    // renderer binds (the S209 per-texture binding selects the icon atlas set)
    // and whether the Monochrome tint is applied (the S205_1 fragment). For
    // S400_1 (headless) the item is the positioned box; the texture-select +
    // the monochrome in-shader tint are the live [s400_1-live] path (NO-GAP,
    // recorded not silent).
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    if (!ui::text_glyph_uv(layout, chosen.tileIndex, u0, v0, u1, v1))
    {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
    }
    (void)u0; (void)v0; (void)u1; (void)v1; // UVs select the atlas sample on the live path

    // The draw item: the icon box, sampling the tile's UVs. For Monochrome the
    // resolved FOREGROUND tints (the S205_1 shader applies it in-shader; here
    // we carry the resolved Color so the item's straight-alpha color is the tint
    // — the exact value the S205_1 monochrome fragment multiplies).
    const float fr = static_cast<float>(resolvedForeground.r) / 255.0f;
    const float fg = static_cast<float>(resolvedForeground.g) / 255.0f;
    const float fb = static_cast<float>(resolvedForeground.b) / 255.0f;
    const float fa = static_cast<float>(resolvedForeground.a) / 255.0f;
    // Tint overrides the sampled RGB when Monochrome; Preserve samples the
    // asset's own RGB (we still carry an opaque tint so the quad's alpha is
    // the icon's coverage).
    const bool mono = (icon_tint_to_path(in.tint) == IconRenderPath::Monochrome);
    const float cr = mono ? fr : 1.0f;
    const float cg = mono ? fg : 1.0f;
    const float cb = mono ? fb : 1.0f;
    const float ca = mono ? fa : 1.0f;

    result.item = ui::solid_rect(in.logicalBox.x, in.logicalBox.y,
                                 in.logicalBox.width, in.logicalBox.height,
                                 ui::Color4{ cr, cg, cb, ca });
    result.ok = true;
    result.isFallback = false;
    return result;
}

} // namespace vaml::vulkan::icon
