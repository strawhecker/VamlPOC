// 11n-5 — Visual3D definitions: IVisual3DContainer + Visual3D + ModelVisual3D.
// See Visual3D.h. WPF-faithful (Visual3D : DependencyObject, IVisual3DContainer).
// ADR-011 CORE DATA (the 3D tree).

#include "vaml/core/Visual3D.h"

#include "vaml/core/Model3D.h" // Model3D (ModelVisual3D's content complete type)

namespace vaml {

VAML_CORE_EXPORT
Visual3D::Visual3D() = default;

VAML_CORE_EXPORT
Visual3D::~Visual3D() = default;

VAML_CORE_EXPORT
SetResult Visual3D::set_transform(const Matrix3D& m)
{
    if (!is_matrix3d_valid(m))
    {
        return SetResult::REJECTED;
    }
    transform_ = m;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
void Visual3D::add_visual3d_child(std::shared_ptr<Visual3D> child)
{
    if (!child)
    {
        return;
    }
    children_.push_back(std::move(child));
}

VAML_CORE_EXPORT
std::size_t Visual3D::remove_visual3d_child(std::size_t index)
{
    if (index >= children_.size())
    {
        return children_.size();
    }
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    return children_.size();
}

VAML_CORE_EXPORT
Visual3D* Visual3D::visual3d_child_at(std::size_t index)
{
    return (index < children_.size()) ? children_[index].get() : nullptr;
}

VAML_CORE_EXPORT
Visual3D* Visual3D::child_at(std::size_t index) const noexcept
{
    return (index < children_.size()) ? children_[index].get() : nullptr;
}

VAML_CORE_EXPORT
ModelVisual3D::ModelVisual3D() = default;

VAML_CORE_EXPORT
ModelVisual3D::~ModelVisual3D() = default;

} // namespace vaml
