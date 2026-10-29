#pragma once
// VAML VectorRaster — the 11k-4 RENDER-side pure seam (vaml::vulkan::vector):
// converts an authored CORE vector/drawing resource (a PathGeometry /
// GeometryGroup / RectangleGeometry / EllipseGeometry inside a GeometryDrawing /
// DrawingBrush) into the S202 atlas tile RGBA8 bytes, then emits the S206
// textured-quad DrawItem that samples that tile.
//
// 11k-4 SPLIT (ADR-011 / the S400 TextGlyphPlan + S400_1 IconGlyphPlan
// precedent): the vector/drawing RESOURCE MODEL (Geometry/Drawing/Brush object
// graph, authored via the S453/S460 path) lives in CORE (vaml::core::
// VectorDrawing). This vulkan surface is the RENDER half — it RASTERIZES the
// authored geometry once into the S202 atlas tile + emits the quad. Pure +
// headless-testable: no Vulkan runtime, only the S202 layout (for the UV
// derivation) + the authored drawing.
//
// ADR-015 "single quad engine" DISCHARGED: geometric drawings are rasterized
// ONCE (CPU) into RGBA8 atlas tiles, then sampled through the S206 solid/text
// quad path — NO second renderer, NO vector-on-screen shader. This mirrors the
// S202_1 icon raster pipeline (nanosvg/stb -> S202 tile) but for AUTHORED /
// non-asset geometry (path figures drawn from the core model, not an SVG asset).
//
// The rasterizer is CPU + single-threaded + once-at-import (ADR-018: zero new
// locks — the caller rasterizes one drawing to one tile off the render hot
// path). It is a PURE seam the 11k-4 tests prove headlessly; the DEVICE upload
// (TextureAtlas::upload_tile + flush + the DrawItem::texture binding) is the
// live [both-apps, not CI] path (the S209 texture-binding + S400_1 "live"
// convention).

#include <vaml/core/VectorDrawing.h>      // the CORE geometry/drawing object model
#include <vaml/core/Color.h>              // Color
#include <vaml/vulkan/ui/UiPipeline.h>    // Color4 (std::array<float,4>)
#include <vaml/vulkan/quad/TextureAtlas.h> // AtlasLayout (the S202 exact-grid UV source)
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem (the emitted textured quad)

#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::vector {

// The output of rasterizing one authored drawing to ONE S202 atlas tile: the
// tightly-packed RGBA8 bytes (tileW*tileH*4, the S202 format) + the actual
// content bounds within the tile (normalized [0,1] UVs the quad samples) so a
// subsequent quad can scale/centre the drawing correctly.
struct VectorTile
{
    bool        ok = false;        // false = nothing rasterizable (empty/degenerate)
    std::vector<std::uint8_t> rgba; // tightly-packed RGBA8, tileW*tileH*4
    float       u0 = 0.0f, v0 = 0.0f; // normalized sub-rect of the ink
    float       u1 = 1.0f, v1 = 1.0f;
    std::uint32_t texelCount = 0;   // == tileW*tileH (the S213_1-style best-fit key)
};

// The fixed raster size for a drawing tile (a modest S202-style atlas cell). A
// drawing is rasterized at this resolution once; the S213_1-style best-fit
// picks a differently-sized atlas slot when the viewer needs another N.
constexpr std::uint32_t kVectorTileSize = 64u; // 64x64 RGBA8 tile

// Rasterizes an authored core DRAWING (a DrawingBrush's content — a
// DrawingGroup/GeometryDrawing tree) into ONE RGBA8 atlas tile (kVectorTileSize²).
// The ink is drawn into the tile's content box (the drawing's bounds, fitted +
// centred into the tile with a small margin), the fill colour = the drawing's
// resolved foreground (the caller supplies it; the core representative_color
// fallback is used when not resolvable headlessly). Pure + headless-testable.
//
// Returns ok=false (empty rgba) when the drawing has no rasterizable geometry
// (empty bounds / no GeometryDrawing with geometry) — the caller emits the
// S400_1-style fallback glyph + structured diagnostic (ADR-015 NO-GAP): NEVER
// silent, NEVER a crash.
VAML_VULKAN_EXPORT VectorTile
raster_drawing_to_tile(const vaml::Drawing& drawing,
                       const vaml::Color& fillColor) noexcept;

// The simplest raster path: rasterizes ONE authored geometry (a PathGeometry /
// RectangleGeometry / EllipseGeometry / GeometryGroup) directly into a tile.
VAML_VULKAN_EXPORT VectorTile
raster_geometry_to_tile(const vaml::Geometry& geometry,
                        const vaml::Color& fillColor) noexcept;

// Emits ONE S206 textured-quad DrawItem that samples `tile`'s UV sub-rect at
// (x,y) sized (w,h) in logical px, `color` the straight-alpha tint (white for
// an opaque authored fill). Mirror of the S400_1 icon quad emission. Pure +
// headless-testable.
VAML_VULKAN_EXPORT vaml::vulkan::ui::DrawItem
vector_quad(float x, float y, float w, float h,
            const VectorTile& tile, const vaml::vulkan::ui::Color4& color) noexcept;

} // namespace vaml::vulkan::vector
