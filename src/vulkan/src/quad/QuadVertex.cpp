// S200 — QuadVertex + unit-quad mesh seams implementation.
//
// Pure: no Vulkan runtime required. See QuadVertex.h for the vertex layout + the
// unit-quad geometry contract.

#include <vaml/vulkan/quad/QuadVertex.h>

namespace vaml::vulkan::quad {

namespace {

// The four unit-quad positions in vertex order: (0,0), (1,0), (1,1), (0,1) —
// a counter-clockwise unit quad in XY. Each is a {u, v} pair.
constexpr std::array<float, 8> kUnitQuadPositions = {
    0.0f, 0.0f,  // v0
    1.0f, 0.0f,  // v1
    1.0f, 1.0f,  // v2
    0.0f, 1.0f,  // v3
};

// The fixed per-quad index list: two unit triangles (0,1,2 / 0,2,3) covering the
// quad counter-clockwise (the no-cull front face).
constexpr std::array<std::uint16_t, 6> kQuadIndices = { 0, 1, 2, 0, 2, 3 };

} // namespace

std::uint32_t quad_vertex_count() noexcept
{
    return 4u;
}

std::uint32_t quad_index_count() noexcept
{
    return 6u;
}

const std::array<std::uint16_t, 6>& quad_indices() noexcept
{
    return kQuadIndices;
}

std::span<const float> unit_quad_positions() noexcept
{
    return std::span<const float>(kUnitQuadPositions.data(), kUnitQuadPositions.size());
}

void
append_quad(std::vector<QuadVertex>& out,
            float x, float y, float w, float h,
            float u0, float v0, float u1, float v1,
            float r, float g, float b, float a)
{
    // The four unit positions (u,v pairs). A real vertex is the unit position
    // scaled by (w,h) + offset (x,y), with the uv interpolated u0→u1 / v0→v1.
    struct P { float u, v; };
    constexpr P kPos[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };

    std::array<QuadVertex, 4> quad;
    for (std::size_t i = 0; i < 4; ++i)
    {
        QuadVertex v;
        v.x = x + w * kPos[i].u;
        v.y = y + h * kPos[i].v;
        v.u = u0 + (u1 - u0) * kPos[i].u;
        v.v = v0 + (v1 - v0) * kPos[i].v;
        v.r = r; v.g = g; v.b = b; v.a = a;
        quad[i] = v;
    }
    out.insert(out.end(), quad.begin(), quad.end());
}

} // namespace vaml::vulkan::quad
