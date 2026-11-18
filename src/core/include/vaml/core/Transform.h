#pragma once
// VAML Matrix2D + the WPF-faithful Transform OBJECT hierarchy (S823, dep:S822).
// S823 RESOLVES the S208/S362/S364-recorded "abstract transform tree" NO-GAP
// here, mirroring the S643b Brush-object closure: the UIElement RenderTransform
// DP is now a `Transform` OBJECT (Value::Transform(shared_ptr<Transform>)), with
// the authorable WPF-real concrete leaves (MatrixTransform/RotateTransform/
// ScaleTransform/TranslateTransform) composing the resolved Matrix2D affine that
// the renderer consumes. This is the resolution at the feature's own step, not a
// backfill (NO-GAP). Matrix2D remains the composable resolved-value seam.

#include "vaml/core/export.h"

#include <cstdint>
#include <memory>
//
// S208 RESOLVES the S120 UIElement transform-owner sentinel here: UIElement now
// owns a REAL RenderTransform member/DP (a Matrix2D), and the renderer performs
// it (the vulkan encoder CPU-folds each element's Model into the quad vertex
// positions before upload — "composes the draw Model matrix into the MVP").
// This is the resolution at the feature's own step, not a backfill (NO-GAP).
//
// The S208-era single concrete affine value type (like Color/Thickness/
// GridLength) is the COMPOSABLE RESOLVED SEAM. It is NOT the RenderTransform DP's
// payload type — S823 (dep:S822) migrates the DP onto the WPF-faithful Transform
// OBJECT (the S643b Brush-object closure mirror) because WPF's
// `RenderTransformProperty` is `typeof(Transform)` (UIElement.cs:1335-1340, the
// WPF source audit), with NO ValidateValueCallback (any Transform is accepted —
// a NaN matrix renders garbage but is WPF-legal; the finite-validity gate VAML
// needs stays ONLY at the raster/encode boundary, never the DP Set).
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

// ---------------------------------------------------------------------------
// S823 — the WPF-faithful TRANSFORM OBJECT hierarchy (the S643b Brush closure
// mirror). WPF `Transform : GeneralTransform : Freezable : DependencyObject`
// (Transform.cs); the RenderTransform DP is `typeof(Transform)` with default
// `Transform.Identity` (UIElement.cs:1335-1340) and NO ValidateValueCallback —
// any Transform is accepted. Each authored per-element Transform composes a
// resolved Matrix2D affine (matrix()) that the S208 encoder folds into the quad
// vertex positions.
//   - `Transform` is the abstract base (NOT authorable as a bare element — the
//     11k-4 abstract-base precedent); it exposes `virtual Matrix2D matrix() const
//     = 0` and the static `Transform::identity()` factory (the shared identity
//     MatrixTransform default).
//   - The concrete leaves are the WPF-real transform types, authorable ONLY
//     inside a property-element value (they are NOT UIElement/FrameworkElement,
//     so NOT ElementFactoryRegistry built-ins — a reader-local builder): the
//     UIElement RenderTransform DP holds a `Value::Transform(shared_ptr<Transform>)`.
//   - ADR-018: single-threaded UI use; no locks added.
// (Forward decl: the center-fold inline bodies below concatenate via multiply,
// which is declared after the classes.)
VAML_CORE_EXPORT Matrix2D multiply(const Matrix2D& a, const Matrix2D& b) noexcept;

class VAML_CORE_EXPORT Transform
{
public:
    // The resolved 2D affine matrix (layout-pixel space) this transform applies.
    // Pure; the renderer CPU-folds it into the quad. Identity for a degenerate/
    // empty transform.
    [[nodiscard]] virtual Matrix2D matrix() const = 0;

    // The shared IDENTITY transform (WPF `Transform.Identity` default). A shared
    // singleton MatrixTransform whose matrix() is Matrix2D::identity().
    [[nodiscard]] static std::shared_ptr<Transform> identity();

    virtual ~Transform() = default;

protected:
    Transform() = default;
};

// The raw-matrix transform: holds an authored/derived Matrix2D verbatim
// (WPF-faithful "MatrixTransform" — an explicit affine).
class VAML_CORE_EXPORT MatrixTransform : public Transform
{
public:
    explicit MatrixTransform(Matrix2D m = {}) : m_(m) {}
    void set_matrix(Matrix2D m) noexcept { m_ = m; }
    [[nodiscard]] Matrix2D matrix() const override { return m_; }

private:
    Matrix2D m_;
};

// The translation transform (WPF "TranslateTransform"): X/Y offset in layout px.
class VAML_CORE_EXPORT TranslateTransform : public Transform
{
public:
    TranslateTransform() = default;
    TranslateTransform(float x, float y) : x_(x), y_(y) {}
    void set_x(float v) noexcept { x_ = v; }
    void set_y(float v) noexcept { y_ = v; }
    [[nodiscard]] float x() const noexcept { return x_; }
    [[nodiscard]] float y() const noexcept { return y_; }
    // T(x, y).
    [[nodiscard]] Matrix2D matrix() const override { return Matrix2D::translation(x_, y_); }

private:
    float x_ = 0.0f, y_ = 0.0f;
};

// The rotation transform (WPF "RotateTransform"): `angle` degrees about the
// optional `center` (CenterX/CenterY, default origin 0,0). WPF-faithful
// composition: translate to center, rotate, translate back.
class VAML_CORE_EXPORT RotateTransform : public Transform
{
public:
    RotateTransform() = default;
    void set_angle(float degrees) noexcept { angle_ = degrees; }
    void set_center(float cx, float cy) noexcept { cx_ = cx; cy_ = cy; }
    [[nodiscard]] float angle() const noexcept { return angle_; }
    [[nodiscard]] Matrix2D matrix() const override
    {
        const Matrix2D r = Matrix2D::rotation(angle_);
        if (cx_ == 0.0f && cy_ == 0.0f)
        {
            return r;
        }
        // T(cx, cy) · R · T(−cx, −cy) (WPF RenderTransformOrigin-style center fold).
        return multiply(Matrix2D::translation(cx_, cy_),
                        multiply(r, Matrix2D::translation(-cx_, -cy_)));
    }

private:
    float angle_ = 0.0f, cx_ = 0.0f, cy_ = 0.0f;
};

// The scaling transform (WPF "ScaleTransform"): (ScaleX,ScaleY) about the
// optional `center` (default origin). WPF-faithful center composition.
class VAML_CORE_EXPORT ScaleTransform : public Transform
{
public:
    ScaleTransform() = default;
    ScaleTransform(float sx, float sy, float cx = 0.0f, float cy = 0.0f)
        : sx_(sx), sy_(sy), cx_(cx), cy_(cy) {}
    void set_scale(float sx, float sy) noexcept { sx_ = sx; sy_ = sy; }
    void set_center(float cx, float cy) noexcept { cx_ = cx; cy_ = cy; }
    [[nodiscard]] float scale_x() const noexcept { return sx_; }
    [[nodiscard]] float scale_y() const noexcept { return sy_; }
    [[nodiscard]] Matrix2D matrix() const override
    {
        const Matrix2D s = Matrix2D::scaling(sx_, sy_);
        if (cx_ == 0.0f && cy_ == 0.0f)
        {
            return s;
        }
        return multiply(Matrix2D::translation(cx_, cy_),
                        multiply(s, Matrix2D::translation(-cx_, -cy_)));
    }

private:
    float sx_ = 1.0f, sy_ = 1.0f, cx_ = 0.0f, cy_ = 0.0f;
};

// True iff the matrix is FINITE (no NaN/Inf in any element). S823 re-scopes
// `is_transform_valid` from the S208 "DP Set validate" gate to the encode-
// boundary guard ONLY: with the RenderTransform DP migrated to the WPF-faithful
// Transform OBJECT (S823), a Transform (any matrix, incl. NaN/Inf) is ACCEPTED
// at the DP Set exactly as WPF accepts any Transform (no ValidateValueCallback).
// The finite-validity guard VAML needs stays ONLY at the raster/encode boundary
// (the S208 encoder folds defensively — a non-finite resolved matrix must never
// crash the vertex/build path), never the DP Set. Pure + headless-testable.
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
