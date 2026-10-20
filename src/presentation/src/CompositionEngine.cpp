// S174 — CompositionEngine implementation.
//
// Implements the S174 pure decision seams (headless-testable) + the composition
// surface owner shape. See CompositionEngine.h for the full split + the
// engineering-out rationale + the grow-only/crisp-max policy.

#include <vaml/composition/CompositionEngine.h>

#include <algorithm>
#include <limits>

namespace vaml::composition {

bool clip_to_zero(std::uint32_t width, std::uint32_t height) noexcept
{
    // A zero width OR height means the pane is minimized — "clip to zero, draw
    // nothing, no churn". The zero/minimized pane never enters the compose path.
    return width == 0 || height == 0;
}

namespace {

// The grow-only reserve headroom factor (default 2x). A pane resize within the
// pre-reserved headroom is ZERO reallocation (the engineering-out). Named (not
// magic) — a documented policy constant, not a library contract.
constexpr std::uint32_t kGrowOnlyReserveFactor = 2;

} // namespace

SurfaceReserve virtual_surface_reserve(const std::vector<DestRect>& panes,
                                       CompositionOptions options) noexcept
{
    // The extent = the max pane extent, seeded 1x1 so a Degenerate/empty set still
    // yields a valid surface (never a 0x0 allocation).
    VkExtent2D extent{ 1u, 1u };
    for (const DestRect& p : panes)
    {
        if (p.width  > extent.width)  extent.width  = p.width;
        if (p.height > extent.height) extent.height = p.height;
    }

    SurfaceReserve r;
    r.extent  = extent;
    r.reserve = extent;

    if (options == CompositionOptions::GrowOnlyReserve)
    {
        // OVER-ALLOCATE with reserve headroom (grow-only, never shrink): a pane
        // resize within the headroom stays inside the reserved surface — ZERO
        // reallocation. Grows only when a destination exceeds the reserve.
        // Clamp so width/height never overflow (a 0 headroom is impossible to hit
        // with a uint32 max window, but guard against wrapping).
        r.reserve.width  = extent.width  > (std::numeric_limits<std::uint32_t>::max() / kGrowOnlyReserveFactor)
                             ? std::numeric_limits<std::uint32_t>::max()
                             : extent.width * kGrowOnlyReserveFactor;
        r.reserve.height = extent.height > (std::numeric_limits<std::uint32_t>::max() / kGrowOnlyReserveFactor)
                             ? std::numeric_limits<std::uint32_t>::max()
                             : extent.height * kGrowOnlyReserveFactor;
        r.reserved = true;
    }
    else
    {
        // CrispMax: the reserve = the extent (no over-allocation). Each destination
        // pixel maps 1:1 — crisp; a larger pane grows it, a smaller pane is a
        // constant-time re-map (no shrink within a resize batch).
        r.reserved = false;
    }
    return r;
}

vaml::vulkan::ViewportScissor
compute_composition_viewport(VkExtent2D surfaceExtent,
                             const DestRect& destRect,
                             CompositionOptions /*options*/) noexcept
{
    // CONSTANT-TIME: a pane resize is ONLY this recomputation — no Vulkan object
    // (swapchain/framebuffer/image) changes. The virtual surface is FIXED; the
    // viewport/scissor that maps it into the destination rect is recomputed.
    vaml::vulkan::ViewportScissor vs;

    // The viewport covers the whole destination rect: the virtual surface is scaled
    // (or cropped via the scissor) to fit the pane. (CrispMax would place the
    // 1:1-mapped surface at (0,0); the scissor below still clips to the visible rect.)
    vs.viewport.x        = static_cast<float>(destRect.x);
    vs.viewport.y        = static_cast<float>(destRect.y);
    vs.viewport.width    = static_cast<float>(destRect.width);
    vs.viewport.height   = static_cast<float>(destRect.height);
    vs.viewport.minDepth = 0.0f;
    vs.viewport.maxDepth = 1.0f;

    // The scissor clips the composed output to the pane's visible rectangle. For a
    // non-minimized pane this equals the destination rect (the viewport covers it).
    vs.scissor.offset.x      = static_cast<std::int32_t>(destRect.x);
    vs.scissor.offset.y      = static_cast<std::int32_t>(destRect.y);
    vs.scissor.extent.width  = destRect.width;
    vs.scissor.extent.height = destRect.height;

    // Guard: if the surface is smaller than the destination, the viewport still maps
    // the surface into the full rect (scaling up); the scissor never exceeds the
    // destination. No object churn either way.
    (void)surfaceExtent;
    return vs;
}

} // namespace vaml::composition
