#pragma once
// S174 — Framework Composition engine: the engineering-out of pane resize.
//
// Sprint 4B (S173-S175), the OPTIONAL presentation/composition framework ABOVE the
// VAML primitives. The COMPOSITION policy is NOT in the library — it lives ONLY in
// this optional framework. This S174 engine (namespace vaml::composition) owns a
// FIXED-RESOLUTION offscreen "virtual-surface" render target (color + depth,
// OVER-ALLOCATED) + viewport/scissor + scaling/cropping into ANY destination rect.
//
// The headline property: pane/splitter resize = a DESTINATION-RECT CHANGE only ->
// a viewport/scissor update -> CONSTANT-TIME, ZERO object churn. This is the
// "engineering-out" of the pane-resize failure: resizing a pane never rebuilds any
// Vulkan object (no swapchain, no framebuffer, no image) — the virtual surface is
// fixed; only the viewport/scissor that maps it into the destination rect changes.
// A minimized pane is clipped to zero (draw nothing, no churn).
//
// `grow-only` reserve is the DEFAULT (over-allocate, never shrink, reserve headroom
// so a resize stays within the already-reserved surface); `crisp-max` is a
// `CompositionOptions` FLAG (the framework/app choice, NOT a library contract) —
// the crispy pixel-exact companion (each destination pixel maps 1:1, no filtering).
//
// Split (the S162..S167_1 pure-seam precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime) —
//       compose_dest_rect(paneRect, virtualSurface, options)  (constant-time dest map)
//       virtual_surface_reserve(panes, options)               (grow-only reserve math)
//       compute_composition_viewport(surfaceExtent, destRect, options) (viewport+scissor)
//       clip_to_zero(isMinimized)                             (the draw-nothing signal)
//     These prove the zero-churn + viewport/scissor + minimize-clip math headlessly.
//   * `CompositionEngine` — the real owner of the virtual surface (the S174
//     composition surface, routed through the S173 presenter's draw body).
//
// LIFETIMES: the CompositionEngine is a PURE owner of the virtual-surface policy
// + extent math in v1 (S174 is the decision-layer + owner shape; the real
// offscreen color/depth target + live compose are the S175 POC dogfood, [both apps,
// not CI]). It does not itself own Vulkan objects beyond what a presenting host
// supplies through the Presenter's draw body.

#include <vaml/vulkan/Pipeline.h>  // VkViewport/VkRect2D + VkExtent2D

#include <cstdint>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::composition {

// The composition policy — the framework/app choice (a flag, NOT a library/primitive
// contract):
//   * GrowOnlyReserve (default) — the virtual surface is over-allocated with reserve
//     headroom and NEVER shrinks. A pane resize stays inside the reserved surface
//     (ZERO reallocation — the engineering-out). Grows only when a destination
//     exceeds the reserved extent.
//   * CrispMax — the crisp pixel-exact companion: the virtual surface matches the
//     LARGEST requested destination extent exactly (each destination pixel maps 1:1,
//     no scaling/filtering). A larger pane grows the surface; a smaller pane is a
//     constant-time re-map (never a shrink below the pre-reserved max within a
//     resize batch).
enum class CompositionOptions
{
    GrowOnlyReserve = 0,
    CrispMax        = 1,
};

// A destination rectangle (a pane's area in the composition surface). Pure 2D
// geometry — the resize input. `width`/`height` are the pane's visible size;
// `int` allows a minimized pane to be reported as 0.
struct DestRect
{
    std::uint32_t x      = 0;
    std::uint32_t y      = 0;
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
};

// The RESERVE decision for the virtual surface. `reserve` is the physical (fixed)
// surface extent the engine allocates; `extent` is the current computed extent.
struct SurfaceReserve
{
    VkExtent2D   extent{ 0, 0 };   // the current computed extent
    VkExtent2D   reserve{ 0, 0 };  // the over-allocated (grow-only) physical extent
    bool         reserved = false; // true when the surface is over-allocated (grow-only)
};

// True when a pane is MINIMIZED (zero width/height — nothing to draw). This is the
// "clip to zero, draw nothing, no churn" signal: a minimized pane never enters the
// compose path.
VAML_PRESENTATION_EXPORT
bool clip_to_zero(std::uint32_t width, std::uint32_t height) noexcept;

// Computes the CHANGE-DRIVEN reserve for the virtual surface given the destination
// rectangles of all panes + the policy:
//   * GrowOnlyReserve — the extent is the MAX pane extent (seed 1x1 so a fixed
//     800x600 style surface has base headroom); the RESERVE is that extent grown by
//     a headroom factor (default 2x) so a subsequent pane resize stays inside it.
//   * CrispMax — the extent = the max pane extent; the reserve = the extent (no
//     over-allocation; each destination pixel maps 1:1).
// Pure + headless-testable.
VAML_PRESENTATION_EXPORT
SurfaceReserve virtual_surface_reserve(const std::vector<DestRect>& panes,
                                       CompositionOptions options) noexcept;

// Maps a pane's destination rect + the virtual surface to the viewport/scissor that
// SCALES/CROPS the virtual surface into the destination. CONSTANT-TIME + ZERO object
// churn: a pane resize is ONLY this re-computation (no Vulkan object changes). The
// viewport covers the full destination rect (assumes the surface is scaled to fit);
// the scissor clips to the visible destination rect (a minimized pane was already
// clipped to zero by clip_to_zero). Pure + headless-testable.
VAML_PRESENTATION_EXPORT
vaml::vulkan::ViewportScissor
compute_composition_viewport(VkExtent2D surfaceExtent,
                             const DestRect& destRect,
                             CompositionOptions options) noexcept;

} // namespace vaml::composition
