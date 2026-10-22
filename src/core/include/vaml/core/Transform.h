#pragma once
// VAML Matrix2D — the REAL per-element 2D affine transform (S208, dep:S207).
//
// S208 RESOLVES the S120 UIElement transform-owner sentinel here: UIElement now
// owns a REAL RenderTransform member/DP (a Matrix2D), and the renderer performs
// it (the vulkan encoder CPU-folds each element's Model into the quad vertex
// positions before upload — "composes the draw Model matrix into the MVP").
// This is the resolution at the feature's own step, not a backfill (NO-GAP).
//
// A single concrete affine value type (like Color/Thickness/GridLength), NOT an
// abstract Transform base + TranslateTransform/RotateTransform/ScaleTransform
// tree. Rationale (S208 decision): a concrete affine is consistent with every
// other core value type and sufficient for v1 renderer composition; an abstract
// transform hierarchy is a later styling/template concern (S362/S364, recorded
// NO-GAP not silent).
//
// Layout — a 2x3 affine (row x col, COLUMN-vector convention):
//   | m11 m12 dx |   | x |
//   | m21 m22 dy | * | y |
//   | 0   0   1  |   | 1 |
// i.e. ox = m11*x + m12*y + dx ; oy = m21*x + m22*y + dy.
// Translation (dx, dy) is in LAYOUT PIXELS (the element's logical space).
//
// Semantics (S208 decision): the element renders at its layout final_rect
// (S122/S124 render slot); RenderTransform maps its content within/beyond that
// slot; the clip (scissor = final_rect) culls any overflow. This follows VAML's
// established "final_rect IS clip bounds" (S124); v1 clips always (the S124
// contract; WPF ClipToBounds-vs-clip-always nuance noted, not adopted in v1).

#include "vaml/core/export.h"

#include <cstdint>

namespace vaml {

// A 2D affine transform in layout-pixel space. A pure value type (FLOATS only —
// trivially copyable, fits the Value tagged union). Default = identity.
struct Matrix2D
{
    float m11 = 1.0f, m12 = 0.0f;   // row 0
    float m21 = 0.0f, m22 = 1.0f;   // row 1
    float dx  = 0.0f, dy  = 0.0f;   // translation (layout px)

    [[nodiscard]] constexpr bool operator==(const Matrix2D&) const noexcept = default;

    // The identity transform (no-op).
    [[nodiscard]] static constexpr Matrix2D identity() noexcept { return {}; }

    // Pure translation by (dxt, dyt) layout px.
    [[nodiscard]] static constexpr Matrix2D translation(float dxt, float dyt) noexcept
    {
        return { 1.0f, 0.0f, 0.0f, 1.0f, dxt, dyt };
    }

    // Uniform scale by `s` about the origin.
    [[nodiscard]] static constexpr Matrix2D scaling(float s) noexcept
    {
        return { s, 0.0f, 0.0f, s, 0.0f, 0.0f };
    }

    // Axis-aligned scale by (sx, sy) about the origin.
    [[nodiscard]] static constexpr Matrix2D scaling(float sx, float sy) noexcept
    {
        return { sx, 0.0f, 0.0f, sy, 0.0f, 0.0f };
    }

    // Rotation by `degrees` counter-clockwise in the top-left-origin layout
    // space (the renderer's y-flip makes screen-visible rotation clockwise for
    // a positive angle, matching WPF's RenderTransform). Precomputed affine.
    [[nodiscard]] static Matrix2D rotation(float degrees) noexcept;
};

// True iff the matrix is FINITE (no NaN/Inf in any element) — the S208
// finite-validity gate. A RenderTransform with a non-finite matrix is REJECTED
// at Set (the DP validate), so the renderer never sees NaN/Inf geometry.
// Pure + headless-testable.
VAML_CORE_EXPORT bool is_transform_valid(const Matrix2D&) noexcept;

// True iff the matrix is (exactly) the identity — the fast path the encoder
// uses to skip the per-vertex fold (an identity Model leaves today's rect
// geometry untouched, preserving backward-compatible batching).
[[nodiscard]] constexpr bool is_transform_identity(const Matrix2D& m) noexcept
{
    return m.m11 == 1.0f && m.m12 == 0.0f && m.m21 == 0.0f &&
           m.m22 == 1.0f && m.dx == 0.0f && m.dy == 0.0f;
}

// Returns a × b (concatenation: applying `a` AFTER `b` — WPF RenderTransform
// compose order; the right-most matrix is applied to the point first). Pure.
VAML_CORE_EXPORT Matrix2D multiply(const Matrix2D& a, const Matrix2D& b) noexcept;

// The inverse of `m`. Returns false when `m` is singular (non-invertible, e.g.
// a zero scale); on false `out` is left unchanged (callers gate on the bool).
// Pure + headless-testable.
VAML_CORE_EXPORT bool try_invert(const Matrix2D& m, Matrix2D& out) noexcept;

// Applies the affine map to the point (x, y) in the element's logical space,
// writing the transformed point to (ox, oy). Pure + headless-testable.
VAML_CORE_EXPORT void transform_point(const Matrix2D& m, float x, float y,
                                      float& ox, float& oy) noexcept;

} // namespace vaml
