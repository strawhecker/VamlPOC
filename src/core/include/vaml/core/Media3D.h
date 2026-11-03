#pragma once
// VAML Media3D — the 3D VALUE + BASE spine (11n-5, dep:11n-2 Freezable +
// S208 Matrix2D). Namespace vaml::core territory (vaml::vulkan holds the
// rasterizer — ADR-011).
//
// This header delivers the 11n-5 CORE-DATA half of the 3D object model:
//   - the 3D VALUE types (Point3D/Vector3D/Size3D/Rect3D — the 3D analogues
//     of Size/Rect/Point) + Matrix3D (the 4x4 row-major affine value type —
//     the 3D analogue of Matrix2D/Transform), with the finite-validity gate;
//   - the projection seam (perspective + orthographic) the headless rasterizer
//     (vaml::vulkan::media3d) consumes — the camera cone maps 3D to 2D NDC;
//   - `Animatable : Freezable` — the 11n-0 DAG anchor (`Animatable -> Freezable`,
//     the 11n-0 row) flips DONE HERE. WPF ground truth: `Model3D : Animatable`
//     and `Geometry3D : Animatable` (PresentationCore Media3D). It is a REAL
//     kept base over the S146 Freezable (ADR-018: zero new locks — the freeze
//     state + Changed ride the existing Freezable, unchanged).
//
// WPF-FIDELITY (validated against E:\GitRepos\wpf, PresentationCore
// System.Windows.Media.Media3D):
//   * `Point3D`/`Vector3D`/`Size3D`/`Rect3D` are pure value structs
//     (System.Windows.Media.Media3D: Point3D, Vector3D, Size3D, Rect3D).
//   * `Matrix3D` — a 4x4 affine (WPF Matrix3D: M11..M44 row-major + the OffsetX/
//     OffsetY/OffsetZ translation column + perspective M14/M24/M34/M44). VAML
//     keeps it a concrete affine value like Matrix2D (the S208 single-concrete-
//     type decision), sufficient for the authored Transform + rasterizer.
//   * `Animatable : Freezable` — WPF Animatable : Freezable (System.Windows.Media.Animation.Animatable).
//
// ADR-018: zero new locks (single-threaded affinity; Freezable + the value
// types ride the existing pattern). ADR-011: this is CORE DATA — the render
// (projection -> atlas tile -> quad) stays vaml::vulkan::media3d.

#include "vaml/core/Freezable.h"    // Animatable : Freezable (the 11n-0 base)
#include "vaml/core/export.h"

#include <cmath>
#include <cstdint>

namespace vaml {

// ---------------------------------------------------------------------------
// 3D value types (pure; the 3D analogues of Size/Rect).
// ---------------------------------------------------------------------------

// A 3D point (x, y, z) in model/world space. Pure value type.
struct Point3D
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Point3D&) const noexcept = default;
};

// A 3D vector (dx, dy, dz). Pure value type.
struct Vector3D
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Vector3D&) const noexcept = default;
};

// A 3D extent (width, height, depth). Pure value type (the 3D Size).
struct Size3D
{
    float width  = 0.0f;
    float height = 0.0f;
    float depth  = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Size3D&) const noexcept = default;
};

// An axis-aligned 3D box (origin + extent). Pure value type (the 3D Rect).
struct Rect3D
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float sizeX = 0.0f, sizeY = 0.0f, sizeZ = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Rect3D&) const noexcept = default;

    [[nodiscard]] constexpr bool is_empty() const noexcept
    {
        return sizeX <= 0.0f || sizeY <= 0.0f || sizeZ <= 0.0f;
    }
};

// ---------------------------------------------------------------------------
// Matrix3D — the concrete 4x4 affine value (the 3D Matrix2D / Transform).
// ---------------------------------------------------------------------------

// A 4x4 row-major affine transform in model/world space. Rows are the basis
// vectors + the translation column (OffsetX/OffsetY/OffsetZ). Default identity.
// A pure value type (float only, trivially copyable).
struct Matrix3D
{
    // Row-major 4x4: the LINEAR part + the translation column + the perspective
    // row are all explicit (WPF Matrix3D M11..M44 layout).
    float m11 = 1.0f, m12 = 0.0f, m13 = 0.0f, m14 = 0.0f; // row 0
    float m21 = 0.0f, m22 = 1.0f, m23 = 0.0f, m24 = 0.0f; // row 1
    float m31 = 0.0f, m32 = 0.0f, m33 = 1.0f, m34 = 0.0f; // row 2
    // Row 3 = the 4th row (the translation outside the affine 3x3 + the
    // homogeneous w row). OffsetX/Y/Z live in the 4th column conceptually, but
    // for a row-major value the translation is the last ROW's first three, and
    // M44 is the homogeneous w.
    float m41 = 0.0f, m42 = 0.0f, m43 = 0.0f, m44 = 1.0f; // row 3

    [[nodiscard]] constexpr bool operator==(const Matrix3D&) const noexcept = default;

    // The identity transform (no-op).
    [[nodiscard]] static constexpr Matrix3D identity() noexcept { return {}; }

    // Pure translation by (dx, dy, dz).
    [[nodiscard]] static Matrix3D translation(float dx, float dy, float dz) noexcept;
};

// True iff the matrix is FINITE (no NaN/Inf in any element) — the S208-style
// finite-validity gate. A 3D Transform with a non-finite matrix is REJECTED at
// Set, so the rasterizer never sees NaN/Inf geometry. Pure + headless-testable.
VAML_CORE_EXPORT bool is_matrix3d_valid(const Matrix3D&) noexcept;

// Returns a × b (concatenation: applying `a` AFTER `b` — the right-most matrix
// is applied to the point first). Pure 4x4 row-major product.
VAML_CORE_EXPORT Matrix3D multiply3d(const Matrix3D& a, const Matrix3D& b) noexcept;

// Applies the affine map to the point (x, y, z), writing the transformed point
// to (ox, oy, oz). For an affine (perspective-free, M44=1, M14=M24=M34=0) this
// is a standard 3x3 linear + translation. Pure + headless-testable.
VAML_CORE_EXPORT void transform_point3d(const Matrix3D& m, float x, float y, float z,
                                        float& ox, float& oy, float& oz) noexcept;

// The inverse of `m`. Returns false when `m` is singular (non-invertible);
// on false `out` is left unchanged. Pure + headless-testable.
VAML_CORE_EXPORT bool try_invert3d(const Matrix3D& m, Matrix3D& out) noexcept;

// ---------------------------------------------------------------------------
// Projection seam (perspective + orthographic).
// ---------------------------------------------------------------------------

// A camera cone that maps world 3D -> 2D normalized device coordinates for the
// headless rasterizer. WPF-faithful (Camera/ProjectionCamera: PerspectiveCamera
// + OrthographicCamera). Pure value (the authored Camera DPs + the rasterizer
// both consume it).
struct Camera3D
{
    // View: the camera position + the look-at target + the up vector.
    Point3D position{};
    Point3D target{};
    Vector3D up{ 0.0f, 1.0f, 0.0f };

    // Field-of-view (degrees, vertical) for perspective; orthographic half-
    // height in world units. `isPerspective` selects the cone.
    bool  isPerspective = true;
    float fovDegrees    = 45.0f;
    float orthoHeight   = 1.0f;
    float nearPlane     = 0.1f;
    float farPlane      = 1000.0f;
    float aspect        = 1.0f;

    [[nodiscard]] constexpr bool operator==(const Camera3D&) const noexcept = default;
};

// Builds the VIEW matrix (world -> camera space) for `cam`. Pure. On a
// degenerate view (position == target, or a zero/parallel up), returns false.
VAML_CORE_EXPORT bool look_at(const Camera3D& cam, Matrix3D& view) noexcept;

// Builds the PROJECTION matrix (camera space -> NDC) for `cam`. Pure. Returns
// false when the cone is degenerate (non-finite / non-positive near/far /
// non-positive aspect or fov).
VAML_CORE_EXPORT bool projection(const Camera3D& cam, Matrix3D& proj) noexcept;

// ---------------------------------------------------------------------------
// Animatable — the 11n-0 DAG anchor (animatable content base), authored here.
// ---------------------------------------------------------------------------

// WPF: `public abstract partial class Animatable : Freezable` — the shared base
// for animatable content (Model3D/Geometry3D, and in WPF also the brushes/
// transforms/geometry). VAML keeps it MINIMAL-faithful as a REAL kept base over
// the S146 Freezable: it OWNs the freeze invariant + Changed subscription
// (Freezable), AND the 11n-5 concrete leaves (Model3D/Geometry3D) derive from
// it. This is the 11n-0 DAG anchor (the `Animatable -> Freezable` row) — a real
// base, NOT a MINIMAL REAL marker (it is kept + extended by Model3D/Geometry3D
// in the same 11n-5 step, never a silent backfill).
class VAML_CORE_EXPORT Animatable : public Freezable
{
public:
    ~Animatable() override;

protected:
    Animatable() = default;

private:
    Animatable(const Animatable&) = delete;
    Animatable& operator=(const Animatable&) = delete;
};

} // namespace vaml
