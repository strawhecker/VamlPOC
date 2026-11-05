// S640 — ImageGlyphPlan implementation (vaml::vulkan::icon). The RENDER-side
// pure seam that turns the Image control's DP-driven inputs into the S206 draw
// item via the S213_1 best-fit + the cached icon provider, with the fallback
// glyph + structured diagnostic for an unresolved asset. See ImageGlyphPlan.h
// for the split + NO-GAP. Mirror of build_icon_draw_item (S400_1), specialized
// for the Image control (full-color Preserve path, no Monochrome tint). Pure +
// headless-testable.

#include <vaml/vulkan/icon/ImageGlyphPlan.h>

#include <vaml/core/BindingAccess.h>   // vaml::view_binding_diagnostics() (S250_1 channel)
#include <vaml/core/Geometry.h>        // Rect (the logical box)
#include <vaml/vulkan/ui/TextGlyphPlan.h> // ui::text_glyph_uv (the S202 exact-grid tile->UV)
#include <vaml/vulkan/ui/UiScene.h>    // solid_rect (the emitted quad)

namespace vaml::vulkan::icon {

namespace {

// The DELIBERATE fallback glyph: a recognizable filled square at the image's
// logical box. NOT a crash, NOT a silent blank — a visible "broken image"
// marker (ADR-015 NO-GAP). The straight-alpha color is a neutral, clearly-
// synthetic gray so it reads as a placeholder, distinct from a real asset.
ui::DrawItem makeImageFallbackGlyph(const vaml::Rect& box) noexcept
{
    return ui::solid_rect(box.x, box.y, box.width, box.height,
                          ui::Color4{ 0.80f, 0.80f, 0.80f, 0.55f });
}

// Emits a STRUCTURED diagnostic through the S250_1 view channel (mirrors
// binding diagnostics; ADR-016 observation-only, NEVER control flow, NEVER a
// crash). `reason` names the failure; `detail` carries the image source context.
void emitImageDiagnostic(std::string_view reason, std::string detail) noexcept
{
    vaml::BindingDiagnostic d;
    d.reasonCode  = vaml::BindingReasonCode::MissingProperty; // shared Class B tag (an unresolved asset resolve)
    d.targetProp  = "Source";
    d.sourceType  = "ImageAsset";
    d.detail      = std::string(reason) + ": " + detail;
    vaml::view_binding_diagnostics().emit(d);
}

} // namespace

VAML_VULKAN_EXPORT
ImageDrawResult
build_image_draw_item(const ImageGlyphInput& in,
                      const std::vector<IconCandidate>& candidates,
                      bool hasSlot,
                      IconBestFitCache& bestFit,
                      const quad::AtlasLayout& layout) noexcept
{
    ImageDrawResult result;

    // --- Unresolved asset -> fallback glyph + structured diagnostic ---------
    // An unresolvable image (out-of-set / missing / not-yet-rasterized /
    // unsupported) MUST NOT crash and MUST NOT be silent (ADR-015 / the S202_1
    // NO-GAP fallback is the image control's job). We emit the deliberate
    // fallback glyph + a structured diagnostic, then return. The asset slot is
    // the decision: if the caller said unresolved OR there is no populated
    // candidate for the key, there is nothing to sample.
    if (!in.assetResolved || !hasSlot)
    {
        result.ok = false;
        result.isFallback = true;
        result.item = makeImageFallbackGlyph(in.logicalBox);
        emitImageDiagnostic("image-asset-unresolved",
                           "Image control has no resolvable asset slot (source='" +
                               in.source + "', key=" + std::to_string(in.assetKey) +
                               "); rendered the fallback glyph");
        return result;
    }

    // --- Resolved + a populated slot -> the S213_1 best-fit ----------------
    // The logical image size (the smaller extent is the honest side; images
    // rasterize square through the S202_1 icon family, so the best-fit axis is
    // the logical side). The physical N = logical side x dpiScale (S213_1).
    const float side = (in.logicalBox.width < in.logicalBox.height)
                           ? in.logicalBox.width
                           : in.logicalBox.height;
    if (side <= 0.0f)
    {
        // A degenerate (zero/negative) logical image box -> the fallback glyph +
        // a diagnostic (a zero box has no renderable quad). This is still a
        // "not resolvable to a visible draw" outcome (ADR-015 NO-silent).
        result.ok = false;
        result.isFallback = true;
        result.item = makeImageFallbackGlyph(in.logicalBox);
        emitImageDiagnostic("image-degenerate-box", "Image logical box is zero/negative; no quad to render");
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
        result.item = makeImageFallbackGlyph(in.logicalBox);
        emitImageDiagnostic("image-no-best-fit", "no S213_1 best-fit candidate resolved for the image");
        return result;
    }
    const auto best = bestFit.resolve(in.assetKey, static_cast<std::uint32_t>(side), in.dpiScale,
                                     candidates);
    if (!best || best->candidateIndex < 0 ||
        static_cast<std::size_t>(best->candidateIndex) >= candidates.size())
    {
        result.ok = false;
        result.isFallback = true;
        result.item = makeImageFallbackGlyph(in.logicalBox);
        emitImageDiagnostic("image-no-best-fit", "no S213_1 best-fit candidate resolved for the image");
        return result;
    }
    const icon::IconCandidate& chosen = candidates[static_cast<std::size_t>(best->candidateIndex)];
    result.slotTile = chosen.tileIndex;
    result.targetN  = best->resampleToN ? best->targetN : chosen.side();

    // The quad UVs from the chosen tile via the S202 exact-grid layout. Image
    // always samples the FULL-COLOR asset tile (the S400_1 Preserve path — there
    // is no Monochrome tint for Image, recorded NO-GAP in ImageGlyphPlan.h). For
    // S640 (headless) the item is the positioned box; the texture-select is the
    // live [s640-live] path (NO-GAP, recorded not silent).
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    if (!ui::text_glyph_uv(layout, chosen.tileIndex, u0, v0, u1, v1))
    {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
    }
    (void)u0; (void)v0; (void)u1; (void)v1; // UVs select the atlas sample on the live path

    // The draw item: the image box, sampling the tile's UVs. An opaque white
    // tint — the asset's own RGB shows through (full-color Preserve; no tint).
    // The quad's alpha (1.0) is the image's coverage; the S593 single-opacity
    // fold applies the element opacity at render.
    result.item = ui::solid_rect(in.logicalBox.x, in.logicalBox.y,
                                 in.logicalBox.width, in.logicalBox.height,
                                 ui::Color4{ 1.0f, 1.0f, 1.0f, 1.0f });
    result.ok = true;
    result.isFallback = false;
    return result;
}

} // namespace vaml::vulkan::icon
