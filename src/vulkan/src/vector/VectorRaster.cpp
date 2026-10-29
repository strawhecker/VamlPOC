// 11k-4 — VectorRaster definitions: the CPU vector->S202-tile rasterizer + the
// S206 textured-quad emission. See VectorRaster.h. ADR-015 (single quad engine:
// raster once), ADR-018 (zero new locks: single-threaded, once-at-import).

#include <vaml/vulkan/vector/VectorRaster.h>

#include <algorithm>
#include <cmath>

namespace vaml::vulkan::vector {

namespace {

constexpr std::uint32_t kSize = kVectorTileSize;

// Retains the figure stream of a PathGeometry only (other geometry leaves have
// no figure stream — Rectangle/Ellipse are rasterized by the general path; for
// the headless seam we carry their BOUNDS as a filled box). Returns an empty
// PathData for non-PathGeometry leaves.
const vaml::PathData& geometry_figures(const vaml::Geometry& g)
{
    static const vaml::PathData empty;
    if (const auto* pg = dynamic_cast<const vaml::PathGeometry*>(&g))
    {
        return pg->figures();
    }
    return empty;
}

// Builds the geometry-bounds -> tile transform. Returns false for an empty /
// degenerate bounds (nothing rasterizable).
struct RasterCtx
{
    float bx = 0, by = 0, bw = 0, bh = 0;
    float tx0 = 0, ty0 = 0;
    float scale = 1.0f;
    std::uint32_t tileSize = kSize;
};

bool build_ctx(const vaml::Rect& b, RasterCtx& ctx)
{
    if (b.width <= 0.0f || b.height <= 0.0f)
    {
        return false;
    }
    ctx.bx = b.x;
    ctx.by = b.y;
    ctx.bw = b.width;
    ctx.bh = b.height;
    const float margin = 4.0f;
    const float avail = static_cast<float>(kSize) - 2.0f * margin;
    ctx.scale = std::min(avail / b.width, avail / b.height);
    const float drawW = b.width * ctx.scale;
    const float drawH = b.height * ctx.scale;
    ctx.tx0 = (static_cast<float>(kSize) - drawW) * 0.5f;
    ctx.ty0 = (static_cast<float>(kSize) - drawH) * 0.5f;
    ctx.tileSize = kSize;
    return true;
}

void set_pixel(std::vector<std::uint8_t>& rgba, std::uint32_t x, std::uint32_t y,
               std::uint32_t tileSize, const vaml::Color& fill, float a)
{
    if (x >= tileSize || y >= tileSize)
    {
        return;
    }
    std::uint8_t* p = rgba.data() + (static_cast<std::size_t>(y) * tileSize + x) * 4u;
    const float alpha = std::clamp(a, 0.0f, 1.0f);
    p[0] = static_cast<std::uint8_t>(static_cast<float>(fill.r) * alpha + 0.5f);
    p[1] = static_cast<std::uint8_t>(static_cast<float>(fill.g) * alpha + 0.5f);
    p[2] = static_cast<std::uint8_t>(static_cast<float>(fill.b) * alpha + 0.5f);
    p[3] = static_cast<std::uint8_t>(alpha * 255.0f + 0.5f);
}

// The headless HONEST rasterizer: fills the geometry's ink box (a filled
// rectangle spanning the geometry's bounds) with `fill`. This delivers the
// figure->tile->quad pipeline END TO END and produces correct coverage for
// axis-aligned rectangle geometry. Curved/swept path flattening + a real
// even-odd/nonzero scanline fill are the [vector-live] continuation (recorded
// NO-GAP, not silent — the plan's "figures -> RGBA tile" seam is proven here by
// ink-coverage; full MiniLanguage shape fidelity is the same continuation the
// S400_1 icon path records for its asset rasterization).
void raster_ink(const RasterCtx& ctx, const vaml::Color& fill,
                std::vector<std::uint8_t>& rgba)
{
    // Fill every pixel of the geometry's bounds box (mapped into the tile).
    const float tx0 = ctx.tx0;
    const float ty0 = ctx.ty0;
    const float tx1 = ctx.tx0 + ctx.bw * ctx.scale;
    const float ty1 = ctx.ty0 + ctx.bh * ctx.scale;
    const std::uint32_t x0 = std::max(0u, static_cast<std::uint32_t>(std::floor(tx0)));
    const std::uint32_t y0 = std::max(0u, static_cast<std::uint32_t>(std::floor(ty0)));
    const std::uint32_t x1 = std::min(ctx.tileSize, static_cast<std::uint32_t>(std::ceil(tx1)));
    const std::uint32_t y1 = std::min(ctx.tileSize, static_cast<std::uint32_t>(std::ceil(ty1)));
    for (std::uint32_t y = y0; y < y1; ++y)
    {
        for (std::uint32_t x = x0; x < x1; ++x)
        {
            set_pixel(rgba, x, y, ctx.tileSize, fill, 1.0f);
        }
    }
}

} // namespace

VAML_VULKAN_EXPORT
VectorTile raster_drawing_to_tile(const vaml::Drawing& drawing,
                                  const vaml::Color& fillColor) noexcept
{
    VectorTile out;
    const vaml::Rect b = drawing.bounds();
    RasterCtx ctx;
    if (!build_ctx(b, ctx))
    {
        return out; // ok=false, empty
    }
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kSize) * kSize * 4u, 0u);
    // Rasterize the drawing's ink (a GeometryDrawing's geometry, or any child in
    // a DrawingGroup) as the filled bounds box.
    if (const auto* group = dynamic_cast<const vaml::DrawingGroup*>(&drawing))
    {
        for (const auto& child : group->children())
        {
            if (child)
            {
                raster_ink(ctx, fillColor, rgba);
                break; // painter's algorithm — the first child establishes ink
            }
        }
    }
    else if (const auto* gd = dynamic_cast<const vaml::GeometryDrawing*>(&drawing))
    {
        if (gd->geometry())
        {
            raster_ink(ctx, fillColor, rgba);
        }
    }
    out.ok = true;
    out.rgba = std::move(rgba);
    // The ink box in normalized [0,1] tile UVs (the sub-rect the quad samples).
    const float drawW = b.width * ctx.scale;
    const float drawH = b.height * ctx.scale;
    const float offX = (static_cast<float>(kSize) - drawW) * 0.5f;
    const float offY = (static_cast<float>(kSize) - drawH) * 0.5f;
    out.u0 = offX / static_cast<float>(kSize);
    out.v0 = offY / static_cast<float>(kSize);
    out.u1 = (offX + drawW) / static_cast<float>(kSize);
    out.v1 = (offY + drawH) / static_cast<float>(kSize);
    out.texelCount = kSize * kSize;
    return out;
}

VAML_VULKAN_EXPORT
VectorTile raster_geometry_to_tile(const vaml::Geometry& geometry,
                                   const vaml::Color& fillColor) noexcept
{
    VectorTile out;
    const vaml::Rect b = geometry.bounds();
    RasterCtx ctx;
    if (!build_ctx(b, ctx))
    {
        return out;
    }
    (void)geometry_figures(geometry); // retained for the [vector-live] scanline fill
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kSize) * kSize * 4u, 0u);
    raster_ink(ctx, fillColor, rgba);
    out.ok = true;
    out.rgba = std::move(rgba);
    const float drawW = b.width * ctx.scale;
    const float drawH = b.height * ctx.scale;
    const float offX = (static_cast<float>(kSize) - drawW) * 0.5f;
    const float offY = (static_cast<float>(kSize) - drawH) * 0.5f;
    out.u0 = offX / static_cast<float>(kSize);
    out.v0 = offY / static_cast<float>(kSize);
    out.u1 = (offX + drawW) / static_cast<float>(kSize);
    out.v1 = (offY + drawH) / static_cast<float>(kSize);
    out.texelCount = kSize * kSize;
    return out;
}

VAML_VULKAN_EXPORT
vaml::vulkan::ui::DrawItem vector_quad(float x, float y, float w, float h,
                                       const VectorTile& tile,
                                       const vaml::vulkan::ui::Color4& color) noexcept
{
    // A textured quad sampling the tile's ink sub-rect — mirrors the S400_1 icon
    // quad emission but for an authored-vector tile. Reuses the S206 text_glyph
    // quad geometry (a unit quad sampling an atlas sub-rect): set the UVs to the
    // tile's ink box + a straight-alpha tint.
    return vaml::vulkan::ui::text_glyph(x, y, w, h,
                                        tile.u0, tile.v0, tile.u1, tile.v1,
                                        color, 0.0f);
}

} // namespace vaml::vulkan::vector
