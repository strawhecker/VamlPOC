#pragma once
// VAML IconGlyphPlan — the S400_1 RENDER-side pure seam (vaml::vulkan::icon):
// turns the Icon control's DP-driven inputs into the S206 draw item the
// classifier emits, resolving through the S213_1 best-fit + the cached icon
// provider, with the deliberate fallback glyph + structured diagnostic for an
// unresolved asset (ADR-015 NO-GAP — NEVER silent, NEVER a crash).
//
// S400_1 SPLIT (ADR-011 / the S400 TextGlyphPlan precedent): the Icon CONTROL
// (the DP surface Source/IconKey/TintMode/Foreground/Width/Height/Stretch +
// the resolution contract) lives in CORE (vaml::core::Icon); this vulkan
// surface is the RENDER half — it maps the resolved icon ASSET (via
// vaml::core::resolve_icon, S360_1 -> the S202_1 raster) + the S213_1 BEST-FIT
// (logical size × host.last_framebuffer_scale -> the chosen atlas tile + a
// resample decision) into ONE S206 DrawItem. It is PURE + HEADLESS-TESTABLE:
// no Vulkan runtime, only the S202/IconAtlas slot data + the S213_1
// select_best_fit decision + the DI TintMode->kind mapping.
//
// TINT MODE -> SHADER KIND (the S400_1 title mapping, recorded NO-GAP in
// S205_1):
//   * TintMode::Preserve  -> the multicolor SDF/quad path (DrawItem sampling
//     the icon's full-color atlas tile). `icon_tint_kind` returns the DrawItem
//     kind for the sampled atlas quad.
//   * TintMode::Monochrome -> UiShaderKind::Monochrome (the S205_1 single-
//     channel alpha-mask, tinted by Foreground in-shader). `icon_tint_kind`
//     returns the monochrome path; the emitted item carries the Foreground tint
//     color (resolve_foreground_color) so the S205_1 monochrome fragment
//     applies it entirely in-shader.
//
// FALLBACK GLYPH + STRUCTURED DIAGNOSTIC (the S400_1 headline, ADR-015 NO-GAP):
// when the IconKey does NOT resolve to a populated asset slot (missing key,
// out-of-set, not-yet-rasterized, unsupported) OR the asset has no best-fit
// tile, `build_icon_draw_item` (a) returns a DELIBERATE fallback glyph DrawItem
// (a recognizable filled square — the "broken icon" marker, NOT a crash, NOT a
// silent blank) and (b) emits a STRUCTURED diagnostic through the S250_1 view
// diagnostics channel (vaml::view_binding_diagnostics(), the same channel the
// binding engine uses — mirrors binding diagnostics, ADR-016 observation-only).
// Resolution is observed, never control flow.
//
// NO-GAP (recorded, not silent): the LIVE device-backed IconAtlas upload +
// the S209 texture-binding of the icon atlas descriptor set + the REAL
// last_framebuffer_scale() sampling are the live [s400_1-live] path (both apps,
// not CI). This seam PROVES the decision math headlessly (best-fit, tint, UV,
// fallback + diagnostic); the live path feeds the REAL IconAtlas candidates +
// the REAL host scale. The S409_1 viewer growth renders the produced quad
// through the generic renderer. NOT a stub / NOT MINIMAL REAL — real,
// permanent, kept-and-extended surface.

#include <vaml/vulkan/quad/TextureAtlas.h>    // AtlasLayout (the S202 exact-grid UV source)
#include <vaml/vulkan/icon/IconAtlas.h>       // IconAtlas (candidates_for / has_slot)
#include <vaml/vulkan/icon/IconBestFit.h>     // BestFitResult / select_best_fit / physical_n_for
#include <vaml/vulkan/ui/UiScene.h>           // DrawItem, solid_rect / text_glyph (the emitted quad)

#include <vaml/core/Color.h>                  // Color (the resolved tint)
#include <vaml/core/Element.h>                // Icon (the control), IconTintMode / IconStretch

#include <cstdint>
#include <optional>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::icon {

// The S400_1 decision: which shader/render path a TintMode selects.
enum class IconRenderPath
{
    PreserveQuad = 0, // sample the icon's full-color atlas tile (TintMode=Preserve)
    Monochrome   = 1, // single-channel alpha-mask, tinted by Foreground (Monochrome)
};

// Maps an IconTintMode to its render path (the S400_1 title mapping, recorded
// NO-GAP in S205_1). Pure + headless-testable.
VAML_VULKAN_EXPORT IconRenderPath
icon_tint_to_path(vaml::IconTintMode mode) noexcept;

// The S400_1 best-fit INPUT: the resolved icon's asset DISPOSITION at the
// caller's arranged logical box + the real dpiScale (host.last_framebuffer_scale
// in a live render; 1.0 in the headless default).
struct IconGlyphInput
{
    vaml::Rect logicalBox{};      // the icon's arranged final box (logical px)
    float      dpiScale = 1.0f;   // the host framebuffer scale (S213_1 N = size×scale)
    vaml::IconTintMode tint = vaml::IconTintMode::Preserve;
    std::int32_t foregroundBrushId = 0; // Monochrome tint (WPF-faithful brush read)
    IconStretch      stretch = IconStretch::None;
    std::uint32_t    assetKey = 0;      // the resolved S202_1 atlas key (0 = unresolved)
    bool             assetResolved = false;
};

// The S400_1 RENDER decision for a resolved icon: the chosen atlas slot (via
// the S213_1 best-fit) + the DrawItem the classifier appends. `ok=false` marks
// an UNRESOLVED asset -> the caller emits the fallback glyph + the diagnostic
// (see build_icon_draw_item). When `resampledTo` is non-null it reports the
// physical side the sprite renders at (the S213_1 targetN / candidate side).
struct IconDrawResult
{
    bool         ok = false;         // false = unresolved -> fallback + diagnostic
    vaml::vulkan::ui::DrawItem item{}; // the quad to append (valid when ok || fallback)
    bool         isFallback = false; // true = the fallback glyph (ok may still be false)
    std::uint32_t slotTile  = 0;     // the chosen atlas tile (for UV mapping)
    std::uint32_t targetN   = 0;     // the physical side (S213_1)
};

// The headline S400_1 render seam. Given the Icon control's DP-driven inputs +
// the icon's size-family candidate list + `hasSlot` (whether a populated atlas
// slot exists for the key) + the resolved foreground color (via
// vaml::core::resolve_foreground_color against the control) + the S213_1
// best-fit cache + the atlas layout, returns the DrawItem quad for the icon.
//
// Resolution outcome:
//   * assetResolved==false OR hasSlot==false (out-of-set / not-yet-rasterized /
//     unsupported) -> `ok=false`, `isFallback=true`, the item = the DELIBERATE
//     fallback glyph (a recognizable filled square at the logical box), AND a
//     structured diagnostic is emitted through the S250_1 view channel (mirrors
//     binding diagnostics). NEVER silent, NEVER a crash.
//   * resolved + a best-fit slot -> `ok=true`, `isFallback=false`, the item =
//     the icon quad (Monochrome path -> tinted by the resolved foreground;
//     Preserve path -> the full-color atlas sample), sampled from the best-fit
//     tile's UVs.
// Pure + headless-testable (no Vulkan runtime; the cache is S213_1; the
// candidate family is supplied by the caller — the S213_1/IconAtlas convention
// the live [s400_1-live] path feeds from a real device atlas).
VAML_VULKAN_EXPORT IconDrawResult
build_icon_draw_item(const IconGlyphInput& in,
                     const std::vector<IconCandidate>& candidates,
                     bool hasSlot,
                     IconBestFitCache& bestFit,
                     const quad::AtlasLayout& layout,
                     const vaml::Color& resolvedForeground) noexcept;

} // namespace vaml::vulkan::icon
