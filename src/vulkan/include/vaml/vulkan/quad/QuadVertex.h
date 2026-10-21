#pragma once
// S200 — the CPU-side quad datum + the fixed quad mesh seams (vaml::vulkan::quad).
//
// This is the first file of the S200 quad-renderer module (`vaml::vulkan::quad`).
// It is the pure value-type + mesh-seam half of S200's "vertex/index buffers":
// QuadVertex is the CPU-side vertex a quad is built from, and the pure seams
// (quad_vertex_count / quad_index_count / quad_indices / unit_quad_positions /
// append_quad) define the FIXED unit-quad geometry + the per-quad index topology
// that S201's CPU-list encoder consumes. S200's QuadPipeline + QuadScene live in
// the same `quad` namespace (see QuadPipeline.h / QuadScene.h).
//
// QuadVertex layout (POSITION_2D + UV + STRAIGHT-ALPHA COLOR), matching the
// embedded quad shader's vertex-input contract + QuadPipeline's vertex-input
// state (see QuadPipeline.h):
//   * x, y     — position in LOGICAL px (S200's uniform-MVP/UI-scale space; the
//                vertex shader maps it through the pushed mvp → clip space).
//   * u, v     — texture coordinate (0..1; sampled by the quad shader; the S200
//                embedded quad shader reads the location-1 uv varying).
//   * r, g, b, a — straight-alpha premultiplied color (S200's alpha-blend path:
//                the blend attachment uses SRC_ALPHA/ONE_MINUS_SRC_ALPHA on color
//                + ONE/ONE_MINUS_SRC_ALPHA on alpha — QuadVertex carries STRAIGHT
//                (non-premultiplied) RGBA, matching that blend state).
//
// The single quad is ONE unit-quad of 4 vertices + 6 indices (two unit triangles
// 0,1,2 / 0,2,3) forming a counter-clockwise quad spanning unit space:
//   v0 (x,y) = (0,0)  v1 = (1,0)  v2 = (1,1)  v3 = (0,1)
// A real quad is an INSTANCE of this unit quad positioned/scaled by append_quad:
// vertex i is at (x + w*u_i, y + h*v_i) with its uv, where (u_i, v_i) is the
// unit-quad position. This is the S200 geometry S201's scene encodes into the
// vertex buffer (4N vertices) + index buffer (6N indices) per frame.
//
// NO-GAP recorded (not silent): the UNIT QUAD + the mesh seams are S200 real
// geometry (permanent, forward-resolvable). The vertex INPUT binding semantics
// (stride + 3 attributes) live in QuadPipeline.h (S200); the per-frame N-quad
// CPU-list encoder that turns a std::vector<QuadVertex> into buffers + the N-quad
// indexed draw live in QuadScene.h (S201). S205 generalizes the descriptor/shader
// surface; S208 composes a per-element Model into the MVP.

#include <cstdint>
#include <array>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::quad {

// A single quad vertex in logical px: position (x, y) + texture coord (u, v) +
// straight-alpha color (r, g, b, a). Layout order is fixed (position, uv, color)
// to match the quad shader + QuadPipeline vertex attributes exactly.
struct QuadVertex
{
    float x = 0.0f;   // position X (logical px)
    float y = 0.0f;   // position Y (logical px)
    float u = 0.0f;   // texture U (0..1)
    float v = 0.0f;   // texture V (0..1)
    float r = 1.0f;   // straight-alpha red
    float g = 1.0f;   // straight-alpha green
    float b = 1.0f;   // straight-alpha blue
    float a = 1.0f;   // straight-alpha alpha
};

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The fixed vertex count of ONE quad (4). S201 encodes N quads as 4N vertices.
VAML_VULKAN_EXPORT std::uint32_t quad_vertex_count() noexcept;

// The fixed index count of ONE quad (6 — two unit triangles 0,1,2 / 0,2,3).
// S201 encodes N quads as 6N indices + one vkCmdDrawIndexed(6N, 1, 0, 0, 0).
VAML_VULKAN_EXPORT std::uint32_t quad_index_count() noexcept;

// The fixed per-quad index list (uint16_t — the cursor never exceeds 4 vertices
// per quad, so a per-quad offset of 4*i stays within uint16 for any realistic N;
// S201 writes the same [0,1,2,0,2,3] pattern offset by 4*i for quad i). Returns
// the six indices [0,1,2, 0,2,3] (counter-clockwise — the no-cull front face).
VAML_VULKAN_EXPORT const std::array<std::uint16_t, 6>& quad_indices() noexcept;

// The four unit-quad POSITIONS in order (v0..v3): (0,0), (1,0), (1,1), (0,1).
// Each is a {u, v} pair. A real quad scales/offsets these by (x, y, w, h) so the
// vertex i = (x + w*p[i].x, y + h*p[i].y). Exposed for the S200 headless contract
// (QuadScene_test asserts the encoder reproduces these scaled positions).
VAML_VULKAN_EXPORT std::span<const float> unit_quad_positions() noexcept;

// Appends ONE quad to `out` as 4 vertices at (x, y) sized (w, h), with the uv
// sub-rect (u0,v0)-(u1,v1) and an optional straight-alpha color. The four
// vertices are written in unit-quad order (v0..v3) at the scaled positions
// (x + w*pu, y + h*pv). This is S201's per-quad encoder primitive: S201 builds
// the per-frame vertex list by calling append_quad once per CPU-list quad.
VAML_VULKAN_EXPORT void
append_quad(std::vector<QuadVertex>& out,
            float x, float y, float w, float h,
            float u0, float v0, float u1, float v1,
            float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

} // namespace vaml::vulkan::quad
