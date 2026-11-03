// 11n-5 — Media3DRaster definitions: the headless CPU software rasterizer
// over an authored Viewport3D -> S202 atlas tile + the S206 textured-quad
// emission. See Media3DRaster.h. ADR-015 (single quad engine: raster once),
// ADR-018 (zero new locks).

#include <vaml/vulkan/media3d/Media3DRaster.h>
#include <vaml/core/Model3D.h>    // GeometryModel3D, Model3DGroup, DiffuseMaterial
#include <vaml/core/Geometry3D.h> // MeshGeometry3D

#include <algorithm>
#include <cmath>

namespace vaml::vulkan::media3d {

namespace {

constexpr std::uint32_t kSize = kMedia3DTileSize;

// The resolved per-model tint: honors the model's DiffuseMaterial color, else
// the caller's fallback.
vaml::Color model_color(const vaml::Model3D& model, const vaml::Color& fallback)
{
    if (const auto* gm = dynamic_cast<const vaml::GeometryModel3D*>(&model))
    {
        if (gm->material())
        {
            if (const auto* dm = dynamic_cast<const vaml::DiffuseMaterial*>(gm->material().get()))
            {
                return dm->color();
            }
        }
    }
    return fallback;
}

void set_pixel(std::vector<std::uint8_t>& rgba, std::int32_t x, std::int32_t y,
               std::uint32_t tileSize, const vaml::Color& fill, float a)
{
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(tileSize) ||
        y >= static_cast<std::int32_t>(tileSize))
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

// Project + rasterize one triangle into the tile. `sx/sy` are the projected
// 2D screen positions of the 3 vertices (already mapped into pixel space);
// the triangle is filled with `fill`.
void raster_triangle(std::vector<std::uint8_t>& rgba, std::uint32_t tileSize,
                     float x0, float y0, float x1, float y1, float x2, float y2,
                     const vaml::Color& fill)
{
    // 2D bounding box (clamped to the tile).
    const float minX = std::min({ x0, x1, x2 });
    const float maxX = std::max({ x0, x1, x2 });
    const float minY = std::min({ y0, y1, y2 });
    const float maxY = std::max({ y0, y1, y2 });
    const std::int32_t bx0 = std::max(0, static_cast<std::int32_t>(std::floor(minX)));
    const std::int32_t bx1 = std::min(static_cast<std::int32_t>(tileSize),
                                      static_cast<std::int32_t>(std::ceil(maxX)));
    const std::int32_t by0 = std::max(0, static_cast<std::int32_t>(std::floor(minY)));
    const std::int32_t by1 = std::min(static_cast<std::int32_t>(tileSize),
                                      static_cast<std::int32_t>(std::ceil(maxY)));

    // Edge-function barycentric test.
    auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    };
    const float area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0.0f)
    {
        return; // degenerate (collinear)
    }
    for (std::int32_t y = by0; y < by1; ++y)
    {
        for (std::int32_t x = bx0; x < bx1; ++x)
        {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float w0 = edge(x1, y1, x2, y2, px, py);
            const float w1 = edge(x2, y2, x0, y0, px, py);
            const float w2 = edge(x0, y0, x1, y1, px, py);
            if (area > 0.0f)
            {
                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                {
                    set_pixel(rgba, x, y, tileSize, fill, 1.0f);
                }
            }
            else
            {
                if (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)
                {
                    set_pixel(rgba, x, y, tileSize, fill, 1.0f);
                }
            }
        }
    }
}

// A single projected point (2D screen px). `valid` = passes the camera (in
// front of the near plane, finite projection).
struct ScreenPt
{
    bool  valid = false;
    float x = 0.0f, y = 0.0f;
};

// Projects a world point through the view->clip pipeline into tile pixel px.
ScreenPt project(const vaml::Matrix3D& view,
                 const vaml::Matrix3D& proj,
                 float wx, float wy, float wz,
                 const vaml::Rect3D& bounds)
{
    // ROW-VECTOR convention: clip = (x,y,z,1) · view · proj. `view` maps
    // world->camera, `proj` maps camera->clip; combined VP = view·proj.
    vaml::Matrix3D vp = vaml::multiply3d(view, proj);
    const float cw = wx * vp.m14 + wy * vp.m24 + wz * vp.m34 + vp.m44;
    if (cw <= 0.0f)
    {
        return {}; // behind the camera / clipped
    }
    const float cx = wx * vp.m11 + wy * vp.m21 + wz * vp.m31 + vp.m41;
    const float cy = wx * vp.m12 + wy * vp.m22 + wz * vp.m32 + vp.m42;
    const float ndcX = cx / cw;
    const float ndcY = cy / cw;
    // NDC to tile pixel (y flips: top-left origin). Fit the model bounds into
    // the tile content box.
    const float margin = 4.0f;
    const float avail = static_cast<float>(kSize) - 2.0f * margin;
    const float bw = (bounds.sizeX > 0.0f) ? bounds.sizeX : 1.0f;
    const float bh = (bounds.sizeY > 0.0f) ? bounds.sizeY : 1.0f;
    const float scale = std::min(avail / bw, avail / bh);
    const float drawW = bw * scale;
    const float drawH = bh * scale;
    const float tx0 = (static_cast<float>(kSize) - drawW) * 0.5f;
    const float ty0 = (static_cast<float>(kSize) - drawH) * 0.5f;
    const float px = tx0 + (ndcX * 0.5f + 0.5f) * drawW;
    const float py = ty0 + (1.0f - (ndcY * 0.5f + 0.5f)) * drawH;
    ScreenPt s;
    s.valid = std::isfinite(px) && std::isfinite(py);
    s.x = px;
    s.y = py;
    return s;
}

} // namespace

VAML_VULKAN_EXPORT
Media3DTile raster_viewport_to_tile(const vaml::Viewport3D& viewport,
                                    const vaml::Color& fallbackColor) noexcept
{
    Media3DTile out;
    const vaml::Camera* cam = viewport.camera().get();
    if (cam == nullptr)
    {
        return out;
    }
    const vaml::Camera3D cone = cam->cone();
    vaml::Matrix3D view, proj;
    if (!vaml::look_at(cone, view) || !vaml::projection(cone, proj))
    {
        return out; // degenerate camera
    }
    // The scene's total 3D bounds (for the tile-fit center/scale).
    vaml::Rect3D sceneBounds{};
    bool anyGeo = false;
    std::vector<std::shared_ptr<vaml::Visual3D>> visuals;
    for (std::size_t i = 0; i < viewport.visual3d_children_count(); ++i)
    {
        if (const vaml::Visual3D* v = viewport.child3d_at(i))
        {
            visuals.emplace_back(const_cast<vaml::Visual3D*>(v),
                                 [](vaml::Visual3D*) {});
        }
    }
    auto collect_bounds = [&](const vaml::Visual3D& visual) {
        if (const auto* mv = dynamic_cast<const vaml::ModelVisual3D*>(&visual))
        {
            if (mv->content())
            {
                const vaml::Rect3D b = mv->content()->bounds();
                if (!b.is_empty())
                {
                    if (!anyGeo)
                    {
                        sceneBounds = b;
                        anyGeo = true;
                    }
                    else
                    {
                        const float x0 = std::min(sceneBounds.x, b.x);
                        const float y0 = std::min(sceneBounds.y, b.y);
                        const float z0 = std::min(sceneBounds.z, b.z);
                        const float x1 = std::max(sceneBounds.x + sceneBounds.sizeX, b.x + b.sizeX);
                        const float y1 = std::max(sceneBounds.y + sceneBounds.sizeY, b.y + b.sizeY);
                        const float z1 = std::max(sceneBounds.z + sceneBounds.sizeZ, b.z + b.sizeZ);
                        sceneBounds = { x0, y0, z0, x1 - x0, y1 - y0, z1 - z0 };
                    }
                }
            }
        }
    };
    for (const auto& v : visuals)
    {
        if (v)
        {
            collect_bounds(*v);
        }
    }
    if (!anyGeo || sceneBounds.is_empty())
    {
        return out;
    }

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kSize) * kSize * 4u, 0u);

    // Rasterize each geometry-bearing model child.
    auto rasterize_visual = [&](const vaml::Visual3D& visual) {
        if (const auto* mv = dynamic_cast<const vaml::ModelVisual3D*>(&visual))
        {
            if (const auto* gm = dynamic_cast<const vaml::GeometryModel3D*>(mv->content().get()))
            {
                if (gm->geometry())
                {
                    const auto* mesh = dynamic_cast<const vaml::MeshGeometry3D*>(gm->geometry().get());
                    if (mesh && mesh->triangle_indices().size() >= 3)
                    {
                        const vaml::Color color = model_color(*gm, fallbackColor);
                        const auto& pos = mesh->positions();
                        const auto& idx = mesh->triangle_indices();
                        // Apply the model transform (content Transform if any).
                        vaml::Matrix3D modelT = gm->geometry()->transform();
                        for (std::size_t t = 0; t + 2 < idx.size(); t += 3)
                        {
                            const int ia = idx[t], ib = idx[t + 1], ic = idx[t + 2];
                            if (ia < 0 || ib < 0 || ic < 0 ||
                                ia >= static_cast<int>(pos.size()) ||
                                ib >= static_cast<int>(pos.size()) ||
                                ic >= static_cast<int>(pos.size()))
                            {
                                continue;
                            }
                            const vaml::Point3D pa = pos[ia], pb = pos[ib], pc = pos[ic];
                            vaml::Point3D ta, tb, tc;
                            if (!vaml::is_matrix3d_valid(modelT))
                            {
                                ta = pa; tb = pb; tc = pc;
                            }
                            else
                            {
                                vaml::transform_point3d(modelT, pa.x, pa.y, pa.z, ta.x, ta.y, ta.z);
                                vaml::transform_point3d(modelT, pb.x, pb.y, pb.z, tb.x, tb.y, tb.z);
                                vaml::transform_point3d(modelT, pc.x, pc.y, pc.z, tc.x, tc.y, tc.z);
                            }
                            const ScreenPt s0 = project(view, proj, ta.x, ta.y, ta.z, sceneBounds);
                            const ScreenPt s1 = project(view, proj, tb.x, tb.y, tb.z, sceneBounds);
                            const ScreenPt s2 = project(view, proj, tc.x, tc.y, tc.z, sceneBounds);
                            if (s0.valid && s1.valid && s2.valid)
                            {
                                raster_triangle(rgba, kSize, s0.x, s0.y, s1.x, s1.y, s2.x, s2.y, color);
                            }
                        }
                    }
                }
            }
        }
    };
    for (const auto& v : visuals)
    {
        if (v)
        {
            rasterize_visual(*v);
        }
    }

    out.ok = true;
    out.rgba = std::move(rgba);
    // Compute the ink sub-rect UVs (a conservative full-tile box).
    // A more precise ink box is [raster-live]; the full tile is sampled.
    out.u0 = 0.0f; out.v0 = 0.0f;
    out.u1 = 1.0f; out.v1 = 1.0f;
    out.texelCount = kSize * kSize;
    return out;
}

VAML_VULKAN_EXPORT
vaml::vulkan::ui::DrawItem media3d_quad(float x, float y, float w, float h,
                                        const Media3DTile& tile,
                                        const vaml::vulkan::ui::Color4& color) noexcept
{
    return vaml::vulkan::ui::text_glyph(x, y, w, h,
                                        tile.u0, tile.v0, tile.u1, tile.v1,
                                        color, 0.0f);
}

} // namespace vaml::vulkan::media3d
