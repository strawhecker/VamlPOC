#pragma once
// S207 (dep:S206) — the Scene→command-buffer encoder (vaml::vulkan::ui):
// walks the REAL core visual tree (vaml::core::UIElement) and produces the
// S206 DrawItem list the UiScene batcher consumes — draw list BY MATERIAL,
// z-order VIA TREE ORDER.
//
// S206 is the named consumer's BATcher: it takes a flat std::vector<DrawItem>,
// groups CONTIGUOUS same-key runs via batch_plan (one vkCmdDrawIndexed each),
// then records the indexed draws. S207 is the missing FEED: it walks the core
// visual tree and emits DrawItem items in the exact order UiScene::set_items
// expects:
//
//   * Z-ORDER VIA TREE ORDER (the S207 headline): a depth-first PRE-ORDER walk
//     (this element, then its children in index/visual order) emits later
//     children AFTER earlier ones — so a later sibling, drawn later, paints ON
//     TOP of an earlier one. The emitted list is already z-ordered; no sorting
//     pass, no z-index field.
//
//   * DRAW LIST BY MATERIAL: each visited element classifies to a MaterialKind
//     (the material vocabulary below), and only NON-None visuals are appended.
//     Because the walk is in tree order, same-material elements that are
//     ADJACENT in the tree collapse into CONTIGUOUS runs — exactly the input
//     batch_plan turns into a single draw per run. An element whose material
//     differs from its neighbor starts a NEW run (separate draw), preserving
//     z/insertion order (the batching never reorders — the S206 guarantee).
//
// SPLIT (the S162/S200/S206 precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime) —
//       MaterialKind { None, Solid, Border, Text, Monochrome }
//       ElementVisual  (MaterialKind + the DrawItem it contributes)
//       classify_element(UIElement&)  (default classifier -> None for base)
//       MaterialGroupCount(items)     (contiguous same-material-run count)
//       VisualWalkStep / visual_walk_steps()  (the fixed ordered walk contract)
//     These are the S207 proof surface proven in ctest WITHOUT a live runtime.
//   * encode_visual_tree(...) — the headline encoder: a depth-first pre-order
//     walk over the REAL core tree that appends each visited element's DrawItem
//     (via the caller's `visualOf` → ElementVisual) into `out`, honoring the
//     skip rules (None material, degenerate geometry, Collapsed visibility).
//     It is a PURE WALKER over a BORROWED tree — no RAII owner, no resource.
//
// S207 does NOT own a second "render model": it BORROWS the core tree (const
// UIElement&) and emits S206 DrawItems directly. No per-element instancing/
// secondary buffers here — S209 GPU instanced/indirect batching is recorded
// NO-GAP, not silent. ADR-018: zero new locks (single-threaded walk + encode,
// the Sprint-5 per-frame CPU model stated in S206/S201). ADR-011 maintained:
// the encoder lives in vaml-vulkan (core gains no render surface); vaml-vulkan
// links vaml-core (a legitimate Vulkan→core edge, matching the PUBLIC
// glfw/vulkan/vma/image edges).
//
// NO-GAP (recorded, not silent):
//   - S208 (dep:S207) honors Opacity + Clip + RenderTransform + composes the
//     per-element Model into UiPushConstants.mvp. S207 delivers the structural
//     walk + material grouping BASE; transforms/clip/opacity are S208's named
//     step. The transform-owner sentinel in UIElement.h resolves at S208.
//   - S400 (Text control) / S364 (templates) supply REAL per-control materials
//     + glyph UVs; until then classify_element's default returns None for base
//     elements (the forward seam real controls replace, kept-and-extended).
//   - TextGlyph UV assignment (glyph atlas sub-rects per glyph) is S400; S207
//     emits positioned rects only.
//   - Monochrome MaterialKind is CARRIED (mirrors UiShaderKind::Monochrome) but
//     not exercised here — S400_1/S528_1 own it (recorded).

#include <vaml/vulkan/ui/UiScene.h>   // DrawItem, solid_rect/border_rect/text_glyph, ui_item_degenerate
#include <vaml/vulkan/ui/UiPipeline.h> // Color4

#include <vaml/core/UIElement.h>      // vaml::UIElement, Visibility (the REAL core visual tree)

#include <cstdint>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The material vocabulary for S207's "draw list by material" grouping. Mirrors
// UiShaderKind (Solid/Border/Text/Monochrome) plus None for a visited element
// that contributes NO visual (a base/container element with no control render).
// Two ADJACENT elements of the SAME material collapse into one contiguous run
// (one batched draw via S206); a material CHANGE starts a new run (new draw,
// z/insertion order preserved). Monochrome is CARRIED for S205_1/S400_1 but
// deliberately not exercised by S207's default classifier (recorded NO-GAP).
enum class MaterialKind
{
    None = 0,        // contributes no draw (base/container; no control render yet)
    Solid = 1,       // a flat straight-alpha rectangle      -> DrawItemKind::SolidRect
    Border = 2,      // a rectangular stroke                  -> DrawItemKind::BorderRect
    Text = 3,        // a positioned glyph quad               -> DrawItemKind::TextGlyph
    Monochrome = 4,  // monochrome-tint mask (S205_1; carried, unexercised here)
};

// One visited element's contribution to the draw list: the material the element
// classifies to + the S206 DrawItem that material renders. `kind == None` means
// "contribute nothing" (the element still walks its children — a container
// contributes no draw but its subtree does).
struct ElementVisual
{
    MaterialKind kind = MaterialKind::None;
    DrawItem     item{};   // meaningful only when kind != None
};

// The DEFAULT, forward-looking classifier: maps a core UIElement to its render
// material. The base UIElement / a container contributes NO visual yet (no real
// control material exists until S400+ / S364), so the base classification is
// None. This is the S207 seam real controls (Text→S400, Button→S401, templates
// →S364) REPLACE in place — kept-and-extended, never silently backfilled, and
// never turned into a core virtual (ADR-011: "how to render" stays out of core;
// the render state Visibility/Opacity is already neutral core data). Pure +
// headless-testable.
VAML_VULKAN_EXPORT MaterialKind classify_element(const vaml::UIElement& element) noexcept;

// Counts the number of CONTIGUOUS same-material runs in a draw list — the
// "draw list by material" proof (and the exact number of batched draws S206's
// batch_plan will produce for a fully encoded list, since each contiguous
// same-material run batches into one draw). Degenerate items are SKIPPED
// (mirroring batch_plan, so the count matches the scene's batch_count()).
// Emits in LIST ORDER, never reorders. Pure + headless-testable.
VAML_VULKAN_EXPORT std::uint32_t MaterialGroupCount(std::span<const DrawItem> items) noexcept;

// The S207 record/walk contract (the S166/S201/S206 record-plan precedent):
// the fixed ordered steps encode_visual_tree runs per visited element.
enum class VisualWalkStep
{
    Classify,        // classify the element -> MaterialKind (classify_element default)
    SkipNone,        // MaterialKind::None -> contribute nothing (children still walk)
    SkipCollapsed,   // Visibility::Collapsed -> contribute nothing (children still walk)
    SkipDegenerate,  // zero/negative extent -> ui_item_degenerate skip (children still walk)
    Append,          // else append the element's ElementVisual.item to the draw list
};

VAML_VULKAN_EXPORT const std::vector<VisualWalkStep>& visual_walk_steps() noexcept;

// Maps a MaterialKind to the S206 draw item kind it renders (the S206
// draw_kind_to_shader precedent): Solid->SolidRect, Border->BorderRect,
// Text->TextGlyph, Monochrome->SolidRect (a mask quad; the mono fragment
// consumes it — carried, unexercised), None->SolidRect (unreachable — a None
// visual is skipped before append). Pure + headless-testable.
VAML_VULKAN_EXPORT DrawItemKind material_to_draw_kind(MaterialKind kind) noexcept;

// ---------------------------------------------------------------------------
// The headline encoder (the REAL S207 surface; walks the REAL core visual tree)
// ---------------------------------------------------------------------------

// Encodes the core visual tree rooted at `root` into the S206 draw list `out`
// as a depth-first PRE-ORDER walk (this, then children in index/visual order):
//
//   * Z-ORDER VIA TREE ORDER: the emitted sequence IS the z-order — a later
//     sibling paints ON TOP of an earlier one because it is appended later.
//   * DRAW LIST BY MATERIAL: `visualOf(element)` returns the element's
//     ElementVisual (material + draw); non-None visuals append their item.
//   * SKIP RULES (per visited element, in visual_walk_steps order):
//       MaterialKind::None  -> contributed nothing (its subtree still walks)
//       Visibility::Collapsed -> contributed nothing (its subtree still walks)
//       ui_item_degenerate(item) (zero/negative extent) -> skipped (no draw)
//   * `out` is APPENDED to in tree order (the caller may feed a scratch vector);
//     it is z-ordered AND naturally material-grouped (adjacent same-material
//     elements are contiguous) — the exact input S206's batch_plan turns into
//     one vkCmdDrawIndexed per contiguous same-material run.
//
// `visualOf` is a non-owning, read-only classifier over the BORROWED tree (the
// element outlive this call). Default: pass classify_element-wrapping lambdas
// until real control materials land (S400/S364 replace the seam, not a backfill).
//
// FEED CONTRACT (completes "visual tree → UiScene"): a consumer calls
//     encode_visual_tree(root, visualOf, items);
//     scene->set_items(items);      // compacts + encodes buffers + batch_plan
//     scene->record_draw(cmd);      // the batched indexed draws
// The emitted list is directly consumable: scene->batch_count() equals
// MaterialGroupCount(items) (for a fully-encoded list; the live [s207-live]
// POC block asserts exactly this on a real driver). This function is a PURE
// WALKER — it owns nothing, allocates transiently into `out`, and adds zero
// locks (ADR-018).
VAML_VULKAN_EXPORT void
encode_visual_tree(const vaml::UIElement& root,
                   const std::function<ElementVisual(const vaml::UIElement&)>& visualOf,
                   std::vector<DrawItem>& out);

} // namespace vaml::vulkan::ui
