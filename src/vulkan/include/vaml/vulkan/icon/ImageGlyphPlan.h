#pragma once
// VAML ImageGlyphPlan — the S640 RENDER-side pure seam (vaml::vulkan::icon):
// turns the Image control's DP-driven inputs into the S206 draw item the
// classifier emits, resolving through the S213_1 best-fit + the cached icon
// provider, with the deliberate fallback glyph + structured diagnostic for an
// unresolved asset (ADR-015 NO-GAP — NEVER silent, NEVER a crash).
//
// S640 SPLIT (ADR-011 / the S400_1 IconGlyphPlan precedent): the Image CONTROL
// (the DP surface Source/IconKey/Stretch/Width/Height + the resolution
// contract) lives in CORE (vaml::core::Image, S483); this vulkan surface is
// the RENDER half — it maps the resolved image ASSET (via the S400_1 icon
// machinery, S483 "rides the S400_1/S202_1 icon-asset pipeline") + the S213_1
// BEST-FIT (logical size x host.last_framebuffer_scale -> the chosen atlas tile)
// into ONE S206 DrawItem. It is PURE + HEADLESS-TESTABLE: no Vulkan runtime,
// only the S202/IconAtlas slot data + the S213_1 select_best_fit decision.
//
// RELATION TO ICON (S400_1, recorded NO-GAP in S483): Image rides the EXACT
// S400_1/S202_1 icon asset pipeline — the same packaged IconSourceSet, the same
// rasterize-once (PNG/SVG -> RGBA8 atlas tile) and the same S213_1 best-fit.
// The ONLY differences from Icon: (1) Image has NO TintMode — it always samples
// the asset's full-color tile (the Preserve path; there is no Monochrome
// alpha-mask tint for Image), and (2) Image's headline DP is the BARE `Source`
// (S640 registered the bare name so `Source="..."` resolves). So this seam is
// the S400_1 PreserveQuad path specialized for the Image control's input set.
//
// FALLBACK GLYPH + STRUCTURED DIAGNOSTIC (the S640 headline, ADR-015 NO-GAP):
// when the Image's Source/IconKey does NOT resolve to a populated asset slot
// (missing key, out-of-set, not-yet-rasterized, unsupported) OR the asset has
// no best-fit tile, `build_image_draw_item` (a) returns a DELIBERATE fallback
// glyph DrawItem (a recognizable filled square — the "broken image" marker,
// NOT a crash, NOT a silent blank) and (b) emits a STRUCTURED diagnostic through
// the S250_1 view diagnostics channel (vaml::view_binding_diagnostics(), the
// same channel the binding engine uses — mirrors binding diagnostics, ADR-016
// observation-only). Resolution is observed, never control flow.
//
// NO-GAP (recorded, not silent): the LIVE device-backed IconAtlas upload + the
// S209 texture-binding of the icon atlas descriptor set + the REAL
// last_framebuffer_scale() sampling are the live [s640-live] path (both apps,
// not CI). This seam PROVES the decision math headlessly (best-fit, UV, fallback
// + diagnostic); the live path feeds the REAL IconAtlas candidates + the REAL
// host scale. The S641 scanner renders the produced quad through the generic
// renderer and confirms the image's rect + non-default pixels. NOT a stub /
// NOT MINIMAL REAL — real, permanent, kept-and-extended surface.

#include <vaml/vulkan/quad/TextureAtlas.h>    // AtlasLayout (the S202 exact-grid UV source)
#include <vaml/vulkan/icon/IconAtlas.h>       // IconAtlas (candidates_for / has_slot)
#include <vaml/vulkan/icon/IconBestFit.h>     // BestFitResult / select_best_fit / physical_n_for
#include <vaml/vulkan/ui/UiScene.h>           // DrawItem, solid_rect / text_glyph (the emitted quad)

#include <vaml/core/Element.h>                // Image, IconStretch

#include <cstdint>
#include <string>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::icon {

// The S640 best-fit INPUT: the resolved image's asset DISPOSITION at the
// caller's arranged logical box + the real dpiScale (host.last_framebuffer_scale
// in a live render; 1.0 in the headless default) + the bare Source string (for
// the structured diagnostic).
struct ImageGlyphInput
{
    vaml::Rect logicalBox{};      // the image's arranged final box (logical px)
    float      dpiScale = 1.0f;   // the host framebuffer scale (S213_1 N = size x scale)
    vaml::IconStretch stretch = vaml::IconStretch::None;
    std::string  source;          // the bare Source string (diagnostic context)
    std::uint32_t assetKey = 0;   // the resolved S202_1 atlas key (0 = unresolved)
    bool         assetResolved = false;
};

// The S640 RENDER decision for a resolved image: the chosen atlas slot (via the
// S213_1 best-fit) + the DrawItem the classifier appends. `ok=false` marks an
// UNRESOLVED asset -> the caller emits the fallback glyph + the diagnostic. When
// `resampledTo` is non-null it reports the physical side the sprite renders at.
struct ImageDrawResult
{
    bool          ok = false;         // false = unresolved -> fallback + diagnostic
    vaml::vulkan::ui::DrawItem item{}; // the quad to append (valid when ok || fallback)
    bool          isFallback = false; // true = the fallback glyph (ok may still be false)
    std::uint32_t slotTile  = 0;     // the chosen atlas tile (for UV mapping)
    std::uint32_t targetN   = 0;     // the physical side (S213_1)
};

// The headline S640 render seam. Given the Image control's DP-driven inputs +
// the image's size-family candidate list + `hasSlot` (whether a populated atlas
// slot exists for the key) + the S213_1 best-fit cache + the atlas layout,
// returns the DrawItem quad for the image.
//
// Resolution outcome:
//   * assetResolved==false OR hasSlot==false (out-of-set / not-yet-rasterized /
//     unsupported) -> `ok=false`, `isFallback=true`, the item = the DELIBERATE
//     fallback glyph (a recognizable filled square at the logical box), AND a
//     structured diagnostic is emitted through the S250_1 view channel (mirrors
//     binding diagnostics). NEVER silent, NEVER a crash.
//   * resolved + a best-fit slot -> `ok=true`, `isFallback=false`, the item =
//     the full-color image quad sampled from the best-fit tile's UVs.
// Pure + headless-testable (no Vulkan runtime; the cache is S213_1; the
// candidate family is supplied by the caller — the S213_1/IconAtlas convention
// the live [s640-live] path feeds from a real device atlas).
VAML_VULKAN_EXPORT ImageDrawResult
build_image_draw_item(const ImageGlyphInput& in,
                      const std::vector<IconCandidate>& candidates,
                      bool hasSlot,
                      IconBestFitCache& bestFit,
                      const quad::AtlasLayout& layout) noexcept;

} // namespace vaml::vulkan::icon
