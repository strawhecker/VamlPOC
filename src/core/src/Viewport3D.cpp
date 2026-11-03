// 11n-5 — Viewport3D definitions: the Camera family + the Section3D viewport
// host. See Viewport3D.h. WPF-faithful (Camera, PerspectiveCamera,
// OrthographicCamera + Viewport3D : FrameworkElement). ADR-011 CORE DATA.

#include "vaml/core/Viewport3D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vaml {

VAML_CORE_EXPORT
Camera::~Camera() = default;

// ---------------------------------------------------------------------------
// PerspectiveCamera
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
PerspectiveCamera::PerspectiveCamera() = default;

VAML_CORE_EXPORT
PerspectiveCamera::~PerspectiveCamera() = default;

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_position(const Point3D& p)
{
    position_ = p;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_target(const Point3D& t)
{
    target_ = t;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_up(const Vector3D& u)
{
    up_ = u;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_field_of_view(double fov)
{
    if (!std::isfinite(fov) || fov <= 0.0 || fov >= 180.0)
    {
        return SetResult::REJECTED;
    }
    fovDegrees_ = fov;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_near_plane(double nearPlane)
{
    if (!std::isfinite(nearPlane) || nearPlane <= 0.0)
    {
        return SetResult::REJECTED;
    }
    nearPlane_ = nearPlane;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PerspectiveCamera::set_far_plane(double farPlane)
{
    if (!std::isfinite(farPlane) || farPlane <= nearPlane_)
    {
        return SetResult::REJECTED;
    }
    farPlane_ = farPlane;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Camera3D PerspectiveCamera::cone() const
{
    Camera3D c;
    c.position      = position_;
    c.target        = target_;
    c.up            = up_;
    c.isPerspective = true;
    c.fovDegrees    = static_cast<float>(fovDegrees_);
    c.nearPlane     = static_cast<float>(nearPlane_);
    c.farPlane      = static_cast<float>(farPlane_);
    return c;
}

// ---------------------------------------------------------------------------
// OrthographicCamera
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
OrthographicCamera::OrthographicCamera() = default;

VAML_CORE_EXPORT
OrthographicCamera::~OrthographicCamera() = default;

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_position(const Point3D& p)
{
    position_ = p;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_target(const Point3D& t)
{
    target_ = t;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_up(const Vector3D& u)
{
    up_ = u;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_width(double orthoHeight)
{
    if (!std::isfinite(orthoHeight) || orthoHeight <= 0.0)
    {
        return SetResult::REJECTED;
    }
    orthoHeight_ = orthoHeight;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_near_plane(double nearPlane)
{
    if (!std::isfinite(nearPlane) || nearPlane <= 0.0)
    {
        return SetResult::REJECTED;
    }
    nearPlane_ = nearPlane;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult OrthographicCamera::set_far_plane(double farPlane)
{
    if (!std::isfinite(farPlane) || farPlane <= nearPlane_)
    {
        return SetResult::REJECTED;
    }
    farPlane_ = farPlane;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Camera3D OrthographicCamera::cone() const
{
    Camera3D c;
    c.position      = position_;
    c.target        = target_;
    c.up            = up_;
    c.isPerspective = false;
    c.orthoHeight   = static_cast<float>(orthoHeight_);
    c.nearPlane     = static_cast<float>(nearPlane_);
    c.farPlane      = static_cast<float>(farPlane_);
    return c;
}

// ---------------------------------------------------------------------------
// Viewport3D
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
Viewport3D::Viewport3D()
    : camera_(std::make_shared<PerspectiveCamera>())
{
}

VAML_CORE_EXPORT
Viewport3D::~Viewport3D() = default;

VAML_CORE_EXPORT
void Viewport3D::add_visual3d_child(std::shared_ptr<Visual3D> child)
{
    if (!child)
    {
        return;
    }
    children3d_.push_back(std::move(child));
}

VAML_CORE_EXPORT
std::size_t Viewport3D::remove_visual3d_child(std::size_t index)
{
    if (index >= children3d_.size())
    {
        return children3d_.size();
    }
    children3d_.erase(children3d_.begin() + static_cast<std::ptrdiff_t>(index));
    return children3d_.size();
}

VAML_CORE_EXPORT
Visual3D* Viewport3D::visual3d_child_at(std::size_t index)
{
    return (index < children3d_.size()) ? children3d_[index].get() : nullptr;
}

VAML_CORE_EXPORT
const Visual3D* Viewport3D::visual3d_child_at(std::size_t index) const
{
    return (index < children3d_.size()) ? children3d_[index].get() : nullptr;
}

VAML_CORE_EXPORT
const Visual3D* Viewport3D::child3d_at(std::size_t index) const noexcept
{
    return (index < children3d_.size()) ? children3d_[index].get() : nullptr;
}

VAML_CORE_EXPORT
Visual3D* Viewport3D::child3d_at(std::size_t index) noexcept
{
    return (index < children3d_.size()) ? children3d_[index].get() : nullptr;
}

VAML_CORE_EXPORT
SetResult Viewport3D::set_camera(std::shared_ptr<Camera> camera)
{
    if (!camera)
    {
        return SetResult::REJECTED;
    }
    camera_ = std::move(camera);
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Size Viewport3D::MeasureOverride(const vaml::Size& available) const
{
    // The isolatable host fills its available bound (the FrameworkElement
    // contract — WPF Viewport3D fills its final rect).
    vaml::Size s = available;
    if (s.width == std::numeric_limits<float>::infinity())
    {
        s.width = 1.0f;
    }
    if (s.height == std::numeric_limits<float>::infinity())
    {
        s.height = 1.0f;
    }
    return s;
}

VAML_CORE_EXPORT
void Viewport3D::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // Fills its final rect — nothing to arrange (the 3D content is rendered
    // by the rasterizer, not laid out as 2D children).
    (void)finalSlot;
}

} // namespace vaml
