// S208 — Matrix2D out-of-line definitions: rotation, finite-validity,
// multiply, invert, transform_point. See Transform.h for the affine layout +
// the single-concrete-type decision + semantics.

#include "vaml/core/Transform.h"

#include <cmath>

namespace vaml {

VAML_CORE_EXPORT
Matrix2D Matrix2D::rotation(float degrees) noexcept
{
    const float rad = degrees * (3.14159265358979323846f / 180.0f);
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    // CCW in the top-left-origin layout space: | c -s 0 | (rendered clockwise
    // on screen due to the renderer's y-flip — WPF-faithful).
    //                                         | s  c 0 |
    //                                         | 0  0 1 |
    return { c, -s, s, c, 0.0f, 0.0f };
}

VAML_CORE_EXPORT
bool is_transform_valid(const Matrix2D& m) noexcept
{
    return std::isfinite(m.m11) && std::isfinite(m.m12) &&
           std::isfinite(m.m21) && std::isfinite(m.m22) &&
           std::isfinite(m.dx) && std::isfinite(m.dy);
}

VAML_CORE_EXPORT
Matrix2D multiply(const Matrix2D& a, const Matrix2D& b) noexcept
{
    // (a ∘ b) as a column-major 2x3 product: the linear part is a.L * b.L, the
    // translation is a.L * b.d + a.d (right-most applied first).
    Matrix2D r;
    r.m11 = a.m11 * b.m11 + a.m12 * b.m21;
    r.m12 = a.m11 * b.m12 + a.m12 * b.m22;
    r.m21 = a.m21 * b.m11 + a.m22 * b.m21;
    r.m22 = a.m21 * b.m12 + a.m22 * b.m22;
    r.dx  = a.m11 * b.dx + a.m12 * b.dy + a.dx;
    r.dy  = a.m21 * b.dx + a.m22 * b.dy + a.dy;
    return r;
}

VAML_CORE_EXPORT
bool try_invert(const Matrix2D& m, Matrix2D& out) noexcept
{
    // det = m11*m22 - m12*m21
    const float det = m.m11 * m.m22 - m.m12 * m.m21;
    if (det == 0.0f)
    {
        return false;  // singular: no inverse (out left unchanged)
    }
    const float inv = 1.0f / det;
    Matrix2D r;
    r.m11 =  m.m22 * inv;
    r.m12 = -m.m12 * inv;
    r.m21 = -m.m21 * inv;
    r.m22 =  m.m11 * inv;
    // translation inverse: -L^{-1} * d
    r.dx = -(r.m11 * m.dx + r.m12 * m.dy);
    r.dy = -(r.m21 * m.dx + r.m22 * m.dy);
    out = r;
    return true;
}

VAML_CORE_EXPORT
void transform_point(const Matrix2D& m, float x, float y,
                     float& ox, float& oy) noexcept
{
    ox = m.m11 * x + m.m12 * y + m.dx;
    oy = m.m21 * x + m.m22 * y + m.dy;
}

} // namespace vaml
