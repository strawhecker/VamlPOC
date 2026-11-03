#pragma once
// VAML Viewport3D — the 11n-5 FrameworkElement-branch isolatable 3D host
// (vaml::core). WPF ground truth (PresentationFramework
// System.Windows.Controls -- Viewport3D.cs + PresentationCore Media3D):
//
//   * `Viewport3D : FrameworkElement, IAddChild` — the isolatable FrameworkElement
//     that hosts 3D content. `[ContentProperty("Children")]`. DPs: `Children`
//     (read-only Visual3DCollection) + `Camera` (Camera, default
//     `new PerspectiveCamera()`). `ClipToBounds` overridden true. Its internal
//     Viewport3DVisual supplies the camera + viewport; layout ArrangeOverride
//     fills its final rect.
//   * `Camera : Animatable` (abstract) -> `ProjectionCamera` ->
//     `PerspectiveCamera` / `OrthographicCamera`, + `MatrixCamera`.
//   * The 11n-0 DAG cluster (Viewport3D/Visual3D/Model3D/Geometry3D/Camera)
//     flips DONE here.
//
// VAML mapping (ADR-011 CORE DATA + the headless rasterizer in
// vaml::vulkan::media3d): Viewport3D derives from UIElement (the 2D layout
// host) and owns an ORDERED list of Visual3D children + a Camera. It is the
// isolatable render root the rasterizer consumes: project each Visual3D's
// Model3D through the authored Camera, rasterize into the S202 atlas tile.

#include "vaml/core/Media3D.h"       // Camera3D, Matrix3D, Animatable
#include "vaml/core/UIElement.h"     // UIElement (the 2D layout host base)
#include "vaml/core/Visual3D.h"      // Visual3D (children), IVisual3DContainer
#include "vaml/core/DependencyObject.h" // SetResult
#include "vaml/core/export.h"

#include <memory>
#include <vector>

namespace vaml {

// Camera — the abstract 3D-camera base (WPF-faithful, NOT authorable).
class VAML_CORE_EXPORT Camera : public Animatable
{
public:
    ~Camera() override;

    // The headless projection cone (view + projection seeds the rasterizer).
    // Derived by the concrete leaf; builds a Camera3D describing this camera.
    [[nodiscard]] virtual Camera3D cone() const = 0;

protected:
    Camera() = default;

private:
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
};

// PerspectiveCamera — the perspective cone (`Position`/`LookDirection`/
// `UpDirection`/`FieldOfView`/`NearPlaneDistance`/`FarPlaneDistance`).
// Authorable DO leaf. WPF: PerspectiveCamera : ProjectionCamera.
class VAML_CORE_EXPORT PerspectiveCamera : public Camera
{
public:
    PerspectiveCamera();
    ~PerspectiveCamera() override;

    [[nodiscard]] SetResult set_position(const Point3D& p);
    [[nodiscard]] SetResult set_target(const Point3D& t);       // a LookDirection convenience
    [[nodiscard]] SetResult set_up(const Vector3D& u);
    [[nodiscard]] SetResult set_field_of_view(double fovDegrees);
    [[nodiscard]] SetResult set_near_plane(double nearPlane);
    [[nodiscard]] SetResult set_far_plane(double farPlane);

    [[nodiscard]] Point3D  position() const noexcept { return position_; }
    [[nodiscard]] Point3D  target()   const noexcept { return target_; }
    [[nodiscard]] Vector3D up()       const noexcept { return up_; }
    [[nodiscard]] double   field_of_view() const noexcept { return fovDegrees_; }
    [[nodiscard]] double   near_plane() const noexcept { return nearPlane_; }
    [[nodiscard]] double   far_plane()  const noexcept { return farPlane_; }

    [[nodiscard]] Camera3D cone() const override;

private:
    Point3D  position_{};
    Point3D  target_{ 0.0f, 0.0f, 1.0f };
    Vector3D up_{ 0.0f, 1.0f, 0.0f };
    double   fovDegrees_ = 45.0;
    double   nearPlane_  = 0.1;
    double   farPlane_   = 1000.0;
};

// OrthographicCamera — the orthographic cone (WPF OrthographicCamera : ProjectionCamera).
class VAML_CORE_EXPORT OrthographicCamera : public Camera
{
public:
    OrthographicCamera();
    ~OrthographicCamera() override;

    [[nodiscard]] SetResult set_position(const Point3D& p);
    [[nodiscard]] SetResult set_target(const Point3D& t);
    [[nodiscard]] SetResult set_up(const Vector3D& u);
    [[nodiscard]] SetResult set_width(double orthoHeight);
    [[nodiscard]] SetResult set_near_plane(double nearPlane);
    [[nodiscard]] SetResult set_far_plane(double farPlane);

    [[nodiscard]] Point3D  position()    const noexcept { return position_; }
    [[nodiscard]] Point3D  target()      const noexcept { return target_; }
    [[nodiscard]] Vector3D up()          const noexcept { return up_; }
    [[nodiscard]] double   width()       const noexcept { return orthoHeight_; }
    [[nodiscard]] double   near_plane()  const noexcept { return nearPlane_; }
    [[nodiscard]] double   far_plane()   const noexcept { return farPlane_; }

    [[nodiscard]] Camera3D cone() const override;

private:
    Point3D  position_{};
    Point3D  target_{ 0.0f, 0.0f, 1.0f };
    Vector3D up_{ 0.0f, 1.0f, 0.0f };
    double   orthoHeight_ = 1.0;
    double   nearPlane_   = 0.1;
    double   farPlane_    = 1000.0;
};

// Viewport3D — the isolatable 3D host (FrameworkElement-branch UIElement).
// Owns an ordered Visual3D child list + a Camera. NOT authorable as a bare
// visual child; it is the 3D ROOT the reader/sample composes.
class VAML_CORE_EXPORT Viewport3D : public UIElement,
                                    public IVisual3DContainer
{
public:
    Viewport3D();
    ~Viewport3D() override;

    // --- IVisual3DContainer -------------------------------------------------
    void add_visual3d_child(std::shared_ptr<Visual3D> child) override;
    std::size_t remove_visual3d_child(std::size_t index) override;
    std::size_t visual3d_children_count() const override { return children3d_.size(); }
    Visual3D*   visual3d_child_at(std::size_t index) override;
    // const read (the headless rasterizer consumes a const Viewport3D).
    [[nodiscard]] const Visual3D* visual3d_child_at(std::size_t index) const;

    // Non-owning read of the Visual3D child at `index`.
    [[nodiscard]] const Visual3D* child3d_at(std::size_t index) const noexcept;
    [[nodiscard]] Visual3D* child3d_at(std::size_t index) noexcept;
    [[nodiscard]] std::size_t child3d_count() const noexcept { return children3d_.size(); }

    // `Camera` — the owned authored Camera (PerspectiveCamera default).
    [[nodiscard]] SetResult set_camera(std::shared_ptr<Camera> camera);
    [[nodiscard]] const std::shared_ptr<Camera>& camera() const noexcept { return camera_; }
    [[nodiscard]] std::shared_ptr<Camera>& camera() noexcept { return camera_; }

    // Measure/Arrange: the 3D host fills its final render rect (the isolatable
    // FrameworkElement contract). DesiredSize = the available bound.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    std::vector<std::shared_ptr<Visual3D>> children3d_;
    std::shared_ptr<Camera> camera_;
};

} // namespace vaml
