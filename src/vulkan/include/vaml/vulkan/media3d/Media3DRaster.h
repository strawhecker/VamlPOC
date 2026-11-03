#pragma once
// VAML Media3DRaster — the 11n-5 RENDER-side pure seam (vaml::vulkan::media3d):
// a HEADLESS SOFTWARE RASTERIZER over an authored CORE Viewport3D, projecting
// each Visual3D's Model3D through the camera + per-model transforms and
// rasterizing the 3D triangles into the S202 atlas tile RGBA8 bytes, then
// emitting the S206 textured-quad DrawItem that samples that tile.
//
// 11n-5 SPLIT (ADR-011, the 11k-4 VectorRaster precedent): the 3D MODEL tree
// (Viewport3D/Visual3D/Model3D/MeshGeometry3D/Camera) lives in CORE
// (vaml::core). This vulkan surface is the RENDER half — it RASTERIZES the
// authored 3D scene once into the S202 atlas tile + emits the quad. Pure +
// headless-testable: no Vulkan runtime, only the S202 layout (for the UV
// derivation) + the authored scene.
//
// ADR-015 "single quad engine" DISCHARGED (the 3D analogue of the 11k-4 vector
// discharge): the 3D scene is rasterized ONCE (CPU) into RGBA8 atlas tiles,
// then sampled through the S206 quad path — NO dedicated on-screen 3D shader
// in the headless CI proof. The GPU path (a real depth-buffered 3D Vulkan
// pipeline) is the [both-apps, not CI] live/POC lane — recorded at this step,
// never a CI gate, never silent.
//
// The rasterizer is CPU + single-threaded + once-at-import (ADR-018: zero new
// locks). It is a PURE seam the Viewport3D_s11n5_test proves headlessly; the
// DEVICE upload (TextureAtlas::upload_tile + flush + DrawItem::texture binding)
// is the live [both-apps, not CI] path.

#include <vaml/core/Viewport3D.h>      // the CORE 3D scene root (Viewport3D)
#include <vaml/core/Color.h>           // Color
#include <vaml/vulkan/ui/UiPipeline.h> // Color4 (std::array<float,4>)
#include <vaml/vulkan/quad/TextureAtlas.h> // AtlasLayout (the S202 exact-grid UV source)
#include <vaml/vulkan/ui/UiScene.h>    // DrawItem (the emitted textured quad)

#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::media3d {

// The output of rasterizing one authored Viewport3D to ONE S202 atlas tile:
// the tightly-packed RGBA8 bytes (tileW*tileH*4, the S202 format) + the actual
// content bounds within the tile (normalized [0,1] UVs the quad samples).
struct Media3DTile
{
    bool        ok = false;        // false = nothing rasterizable (empty/degenerate)
    std::vector<std::uint8_t> rgba; // tightly-packed RGBA8, tileW*tileH*4
    float       u0 = 0.0f, v0 = 0.0f; // normalized sub-rect of the ink
    float       u1 = 1.0f, v1 = 1.0f;
    std::uint32_t texelCount = 0;   // == tileW*tileH
};

// The fixed raster size for a 3D-scene tile (a modest S202-style atlas cell).
constexpr std::uint32_t kMedia3DTileSize = 64u; // 64x64 RGBA8 tile

// Rasterizes an authored CORE Viewport3D (its Visual3D children's Model3D
// content, projected through the camera) into ONE RGBA8 atlas tile
// (kMedia3DTileSize²). The projected ink of each GeometryModel3D's MeshGeometry3D
// is drawn into the tile's content box (fitted + centred with a small margin).
//
// Returns ok=false (empty rgba) when the scene has no rasterizable geometry
// (no Visual3D with a GeometryModel3D carrying a non-empty MeshGeometry3D, or
// a degenerate camera). NEVER silent, NEVER a crash (the 11k-4 ADR-015
// NO-GAP discipline).
VAML_VULKAN_EXPORT Media3DTile
raster_viewport_to_tile(const vaml::Viewport3D& viewport,
                        const vaml::Color& fallbackColor) noexcept;

// Emits ONE S206 textured-quad DrawItem that samples `tile`'s UV sub-rect at
// (x,y) sized (w,h) in logical px. Mirror of the 11k-4 vector_quad. Pure +
// headless-testable.
VAML_VULKAN_EXPORT vaml::vulkan::ui::DrawItem
media3d_quad(float x, float y, float w, float h,
             const Media3DTile& tile, const vaml::vulkan::ui::Color4& color) noexcept;

} // namespace vaml::vulkan::media3d
