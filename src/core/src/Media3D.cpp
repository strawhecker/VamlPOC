// 11n-5 — Media3D definitions: the 3D value types + the Animatable base + the
// projection seam. See Media3D.h. Dep:11n-2 Freezable / S208 Matrix2D.
// ADR-018: zero new locks. ADR-011: CORE DATA.

#include "vaml/core/Media3D.h"

#include <algorithm>
#include <cmath>

namespace vaml {

VAML_CORE_EXPORT
Matrix3D Matrix3D::translation(float dx, float dy, float dz) noexcept
{
    Matrix3D m;
    m.m41 = dx;
    m.m42 = dy;
    m.m43 = dz;
    return m;
}

VAML_CORE_EXPORT
bool is_matrix3d_valid(const Matrix3D& m) noexcept
{
    return std::isfinite(m.m11) && std::isfinite(m.m12) && std::isfinite(m.m13) && std::isfinite(m.m14) &&
           std::isfinite(m.m21) && std::isfinite(m.m22) && std::isfinite(m.m23) && std::isfinite(m.m24) &&
           std::isfinite(m.m31) && std::isfinite(m.m32) && std::isfinite(m.m33) && std::isfinite(m.m34) &&
           std::isfinite(m.m41) && std::isfinite(m.m42) && std::isfinite(m.m43) && std::isfinite(m.m44);
}

VAML_CORE_EXPORT
Matrix3D multiply3d(const Matrix3D& a, const Matrix3D& b) noexcept
{
    // Row-major 4x4 product: r[i][j] = sum_k a[i][k]*b[k][j].
    Matrix3D r;
    const float* A = &a.m11;
    const float* B = &b.m11;
    float* R = &r.m11;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            R[i * 4 + j] =
                A[i * 4 + 0] * B[0 * 4 + j] +
                A[i * 4 + 1] * B[1 * 4 + j] +
                A[i * 4 + 2] * B[2 * 4 + j] +
                A[i * 4 + 3] * B[3 * 4 + j];
        }
    }
    return r;
}

VAML_CORE_EXPORT
void transform_point3d(const Matrix3D& m, float x, float y, float z,
                       float& ox, float& oy, float& oz) noexcept
{
    // ROW-VECTOR convention: out = (x,y,z,1) · m. The affine translation lives
    // in the LAST row (m41/m42/m43); the homogeneous w is the 4th column
    // x*m14 + y*m24 + z*m34 + m44. Consistent with translation()/look_at()/
    // projection() (the OpenGL-style row-major layout).
    const float w = x * m.m14 + y * m.m24 + z * m.m34 + m.m44;
    const float inv = (w != 0.0f && w != 1.0f) ? (1.0f / w) : 1.0f;
    ox = (x * m.m11 + y * m.m21 + z * m.m31 + m.m41) * inv;
    oy = (x * m.m12 + y * m.m22 + z * m.m32 + m.m42) * inv;
    oz = (x * m.m13 + y * m.m23 + z * m.m33 + m.m43) * inv;
}

VAML_CORE_EXPORT
bool try_invert3d(const Matrix3D& m, Matrix3D& out) noexcept
{
    const float* M = &m.m11;
    auto minor = [&](int r, int c) -> float {
        float sub[3][3];
        int sr = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (i == r) { continue; }
            int sc = 0;
            for (int j = 0; j < 4; ++j)
            {
                if (j == c) { continue; }
                sub[sr][sc] = M[i * 4 + j];
                ++sc;
            }
            ++sr;
        }
        return sub[0][0] * (sub[1][1] * sub[2][2] - sub[1][2] * sub[2][1])
             - sub[0][1] * (sub[1][0] * sub[2][2] - sub[1][2] * sub[2][0])
             + sub[0][2] * (sub[1][0] * sub[2][1] - sub[1][1] * sub[2][0]);
    };

    float det = 0.0f;
    for (int j = 0; j < 4; ++j)
    {
        det += M[j] * minor(0, j) * ((j % 2 == 0) ? 1.0f : -1.0f);
    }
    if (det == 0.0f || !std::isfinite(det))
    {
        return false;
    }
    const float invDet = 1.0f / det;
    Matrix3D adj;
    float* A = &adj.m11;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            const float cof = minor(j, i) * (((i + j) % 2 == 0) ? 1.0f : -1.0f);
            A[i * 4 + j] = cof * invDet;
        }
    }
    out = adj;
    return true;
}

// ---------------------------------------------------------------------------
// Projection seam
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
bool look_at(const Camera3D& cam, Matrix3D& view) noexcept
{
    const float fx = cam.target.x - cam.position.x;
    const float fy = cam.target.y - cam.position.y;
    const float fz = cam.target.z - cam.position.z;
    const float fLen = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (fLen <= 0.0f)
    {
        return false;
    }
    const float f[3] = { fx / fLen, fy / fLen, fz / fLen };

    const float ux = cam.up.x, uy = cam.up.y, uz = cam.up.z;
    const float s[3] = { f[1] * uz - f[2] * uy,
                         f[2] * ux - f[0] * uz,
                         f[0] * uy - f[1] * ux };
    const float sLen = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (sLen <= 0.0f)
    {
        return false;
    }
    const float sn[3] = { s[0] / sLen, s[1] / sLen, s[2] / sLen };
    const float u[3] = { sn[1] * f[2] - sn[2] * f[1],
                         sn[2] * f[0] - sn[0] * f[2],
                         sn[0] * f[1] - sn[1] * f[0] };

    Matrix3D m;
    m.m11 = sn[0]; m.m12 = sn[1]; m.m13 = sn[2];
    m.m21 = u[0];  m.m22 = u[1];  m.m23 = u[2];
    m.m31 = -f[0]; m.m32 = -f[1]; m.m33 = -f[2];
    m.m41 = -(sn[0] * cam.position.x + sn[1] * cam.position.y + sn[2] * cam.position.z);
    m.m42 = -(u[0]  * cam.position.x + u[1]  * cam.position.y + u[2]  * cam.position.z);
    m.m43 =  (f[0]  * cam.position.x + f[1]  * cam.position.y + f[2]  * cam.position.z);
    view = m;
    return true;
}

VAML_CORE_EXPORT
bool projection(const Camera3D& cam, Matrix3D& proj) noexcept
{
    if (!std::isfinite(cam.nearPlane) || !std::isfinite(cam.farPlane) ||
        cam.nearPlane <= 0.0f || cam.farPlane <= cam.nearPlane ||
        !std::isfinite(cam.aspect) || cam.aspect <= 0.0f)
    {
        return false;
    }
    if (cam.isPerspective)
    {
        if (!std::isfinite(cam.fovDegrees) || cam.fovDegrees <= 0.0f ||
            cam.fovDegrees >= 180.0f)
        {
            return false;
        }
        const float rad = cam.fovDegrees * (3.14159265358979323846f / 180.0f);
        const float cot = 1.0f / std::tan(rad * 0.5f);
        Matrix3D m;
        m.m11 = cot / cam.aspect;
        m.m22 = -cot;
        m.m33 = cam.farPlane / (cam.nearPlane - cam.farPlane);
        m.m34 = -1.0f;
        m.m43 = (cam.nearPlane * cam.farPlane) / (cam.nearPlane - cam.farPlane);
        m.m44 = 0.0f;
        proj = m;
        return true;
    }
    if (!std::isfinite(cam.orthoHeight) || cam.orthoHeight <= 0.0f)
    {
        return false;
    }
    const float halfH = cam.orthoHeight * 0.5f;
    const float halfW = halfH * cam.aspect;
    Matrix3D m;
    m.m11 = 1.0f / halfW;
    m.m22 = -1.0f / halfH;
    m.m33 = 1.0f / (cam.nearPlane - cam.farPlane);
    m.m43 = cam.nearPlane / (cam.nearPlane - cam.farPlane);
    proj = m;
    return true;
}

// ---------------------------------------------------------------------------
// Animatable base
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
Animatable::~Animatable() = default;

} // namespace vaml
