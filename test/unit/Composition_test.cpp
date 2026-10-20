// S174 — the Composition engine decision seams: headless (Rail 1).
//
// Sprint 4B (S174), the OPTIONAL composition engine ABOVE the primitives. This TU
// proves the engineering-out of pane resize — the pure viewport/scissor/reserve/clip
// math — headlessly, WITHOUT a live Vulkan runtime (the standing S160+ precedent).
// The headline property asserted here:
//   * A pane/splitter resize is a DESTINATION-RECT CHANGE ONLY -> recompute the
//     viewport/scissor -> ZERO object churn (no swapchain/framebuffer/image rebuild).
//   * A minimized pane is clipped to zero (draw nothing, no churn).
//   * GrowOnlyReserve (default) over-allocates + never shrinks; CrispMax is the
//     crisp 1:1 pixel-exact flag (framework/app choice, NOT a library contract).
//
// The LIVE offscreen color/depth virtual surface + the full-screen splitter compose
// are the S175 POC dogfood, [both apps, not CI].

#include <vaml/composition/CompositionEngine.h>

#include "doctest/doctest.h"

#include <vector>

using namespace vaml::composition;
using vaml::vulkan::ViewportScissor;

// ---------------------------------------------------------------------------
// clip_to_zero — a minimized pane is clipped to zero (draw nothing, no churn)
// ---------------------------------------------------------------------------
TEST_CASE("S174 clip_to_zero: a minimized pane is clipped to zero")
{
    CHECK(clip_to_zero(0, 0));
    CHECK(clip_to_zero(0, 600));
    CHECK(clip_to_zero(800, 0));
    CHECK_FALSE(clip_to_zero(1, 1));
    CHECK_FALSE(clip_to_zero(800, 600));
}

// ---------------------------------------------------------------------------
// virtual_surface_reserve — grow-only default vs crisp-max flag
// ---------------------------------------------------------------------------
TEST_CASE("S174 virtual_surface_reserve: GrowOnlyReserve over-allocates + reserves")
{
    const std::vector<DestRect> panes = {
        { 0, 0, 800, 600 },
    };
    const SurfaceReserve r = virtual_surface_reserve(panes, CompositionOptions::GrowOnlyReserve);

    // The extent = the max pane extent (800x600).
    CHECK(r.extent.width  == 800);
    CHECK(r.extent.height == 600);
    // The reserve = the extent grown by the headroom factor (2x) — over-allocated
    // so a pane resize stays inside it (ZERO reallocation).
    CHECK(r.reserve.width  == 1600);
    CHECK(r.reserve.height == 1200);
    CHECK(r.reserved);
}

TEST_CASE("S174 virtual_surface_reserve: CrispMax matches the extent (no over-allocation)")
{
    const std::vector<DestRect> panes = {
        { 0, 0, 800, 600 },
    };
    const SurfaceReserve r = virtual_surface_reserve(panes, CompositionOptions::CrispMax);

    // CrispMax: the surface = the max pane extent exactly (1:1 pixel-exact, no scale).
    CHECK(r.extent.width  == 800);
    CHECK(r.extent.height == 600);
    CHECK(r.reserve.width  == 800);
    CHECK(r.reserve.height == 600);
    CHECK_FALSE(r.reserved);
}

TEST_CASE("S174 virtual_surface_reserve: the extent is the MAX across panes")
{
    const std::vector<DestRect> panes = {
        { 0,   0,   800, 600 },
        { 800, 0,   320, 240 },
        { 0,   600, 100, 50  },
    };
    const SurfaceReserve r = virtual_surface_reserve(panes, CompositionOptions::CrispMax);

    // The extent is driven by the LARGEST pane (the composition surface must fit it).
    CHECK(r.extent.width  == 800);
    CHECK(r.extent.height == 600);
}

// ---------------------------------------------------------------------------
// compute_composition_viewport — CONSTANT-TIME zero-churn dest-rect mapping
// ---------------------------------------------------------------------------
TEST_CASE("S174 compute_composition_viewport: a resize recomputes viewport/scissor only")
{
    const VkExtent2D surface{ 800, 600 };
    // A pane moved/resized within the fixed virtual surface.
    const DestRect paneA{ 0,   0,   400, 600 };
    const DestRect paneB{ 400, 0,   400, 600 };

    const ViewportScissor a = compute_composition_viewport(surface, paneA, CompositionOptions::GrowOnlyReserve);
    const ViewportScissor b = compute_composition_viewport(surface, paneB, CompositionOptions::GrowOnlyReserve);

    // The viewport maps the virtual surface into each pane's destination rect.
    CHECK(a.viewport.x == 0.0f);
    CHECK(a.viewport.y == 0.0f);
    CHECK(a.viewport.width  == 400.0f);
    CHECK(a.viewport.height == 600.0f);
    CHECK(b.viewport.x == 400.0f);
    CHECK(b.viewport.y == 0.0f);
    CHECK(b.viewport.width  == 400.0f);
    CHECK(b.viewport.height == 600.0f);

    // The scissor clips to the pane's visible rectangle.
    CHECK(a.scissor.offset.x == 0);
    CHECK(a.scissor.offset.y == 0);
    CHECK(a.scissor.extent.width  == 400);
    CHECK(a.scissor.extent.height == 600);
    CHECK(b.scissor.offset.x == 400);
    CHECK(b.scissor.offset.y == 0);
    CHECK(b.scissor.extent.width  == 400);
    CHECK(b.scissor.extent.height == 600);
}

TEST_CASE("S174 compute_composition_viewport: the virtual surface is fixed while dests vary")
{
    // The engineering-out headline: changing a destination rect changes ONLY the
    // viewport/scissor (constant-time), never the surface extent (the virtual surface
    // is fixed/over-allocated). This is the ZERO-object-churn property.
    const VkExtent2D surface{ 1600, 1200 };  // the grow-only reserve
    const DestRect destA{ 0, 0, 800, 600 };
    const DestRect destB{ 0, 0, 1200, 900 };  // a "resized" pane inside the reserved surface

    const ViewportScissor a = compute_composition_viewport(surface, destA, CompositionOptions::GrowOnlyReserve);
    const ViewportScissor b = compute_composition_viewport(surface, destB, CompositionOptions::GrowOnlyReserve);

    // Only the viewport/scissor changed — no surface rebuild (the surface extent is
    // untouched; there is no object churn in the pure math).
    CHECK(a.viewport.width  == 800.0f);
    CHECK(a.viewport.height == 600.0f);
    CHECK(b.viewport.width  == 1200.0f);
    CHECK(b.viewport.height == 900.0f);
    CHECK(a.scissor.extent.width  == 800);
    CHECK(a.scissor.extent.height == 600);
    CHECK(b.scissor.extent.width  == 1200);
    CHECK(b.scissor.extent.height == 900);
}
