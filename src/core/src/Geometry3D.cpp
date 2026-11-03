// 11n-5 — Geometry3D definitions: the abstract 3D-geometry base + the
// MeshGeometry3D leaf. See Geometry3D.h. ADR-011 CORE DATA (render vulkan).

#include "vaml/core/Geometry3D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vaml {

VAML_CORE_EXPORT
Geometry3D::~Geometry3D() = default;

VAML_CORE_EXPORT
MeshGeometry3D::MeshGeometry3D() = default;

VAML_CORE_EXPORT
MeshGeometry3D::~MeshGeometry3D() = default;

VAML_CORE_EXPORT
SetResult MeshGeometry3D::set_transform(const Matrix3D& m)
{
    // Finite-validity gate (the S208 discipline): a non-finite 3D transform is
    // REJECTED so the rasterizer never sees NaN/Inf geometry.
    if (!is_matrix3d_valid(m))
    {
        return SetResult::REJECTED;
    }
    this->transform_ = m;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Rect3D MeshGeometry3D::bounds() const
{
    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();
    bool  any  = false;
    for (const Point3D& p : positions_)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        {
            continue;
        }
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        minZ = std::min(minZ, p.z);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
        maxZ = std::max(maxZ, p.z);
        any  = true;
    }
    if (!any)
    {
        return {};
    }
    return { minX, minY, minZ,
             maxX - minX, maxY - minY, maxZ - minZ };
}

} // namespace vaml
