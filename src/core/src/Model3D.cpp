// 11n-5 — Model3D definitions: the abstract model base + GeometryModel3D +
// Model3DGroup + the Material identity. See Model3D.h. ADR-011 CORE DATA.

#include "vaml/core/Model3D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vaml {

VAML_CORE_EXPORT
Material::~Material() = default;

VAML_CORE_EXPORT
DiffuseMaterial::DiffuseMaterial() = default;

VAML_CORE_EXPORT
DiffuseMaterial::~DiffuseMaterial() = default;

VAML_CORE_EXPORT
SetResult DiffuseMaterial::set_color(const Color& c)
{
    color_ = c;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Model3D::~Model3D() = default;

VAML_CORE_EXPORT
GeometryModel3D::GeometryModel3D() = default;

VAML_CORE_EXPORT
GeometryModel3D::~GeometryModel3D() = default;

VAML_CORE_EXPORT
Rect3D GeometryModel3D::bounds() const
{
    return geometry_ ? geometry_->bounds() : Rect3D{};
}

VAML_CORE_EXPORT
Model3DGroup::Model3DGroup() = default;

VAML_CORE_EXPORT
Model3DGroup::~Model3DGroup() = default;

VAML_CORE_EXPORT
void Model3DGroup::add_model(std::shared_ptr<Model3D> child)
{
    children_.push_back(std::move(child));
}

VAML_CORE_EXPORT
SetResult Model3DGroup::set_transform(const Matrix3D& m)
{
    if (!is_matrix3d_valid(m))
    {
        return SetResult::REJECTED;
    }
    transform_ = m;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Rect3D Model3DGroup::bounds() const
{
    Rect3D r{};
    bool any = false;
    for (const auto& c : children_)
    {
        if (!c)
        {
            continue;
        }
        const Rect3D b = c->bounds();
        if (!any)
        {
            r = b;
            any = true;
        }
        else
        {
            r.x = std::min(r.x, b.x);
            r.y = std::min(r.y, b.y);
            r.z = std::min(r.z, b.z);
            const float ex = std::max(r.x + r.sizeX, b.x + b.sizeX) - r.x;
            const float ey = std::max(r.y + r.sizeY, b.y + b.sizeY) - r.y;
            const float ez = std::max(r.z + r.sizeZ, b.z + b.sizeZ) - r.z;
            r.sizeX = std::max(ex, 0.0f);
            r.sizeY = std::max(ey, 0.0f);
            r.sizeZ = std::max(ez, 0.0f);
        }
    }
    return r;
}

} // namespace vaml
