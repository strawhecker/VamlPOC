#pragma once
// VAML Visual3D — the 11n-5 visual-3D container surface (IVisual3DContainer +
// Visual3D + ModelVisual3D). WPF ground truth (PresentationCore
// System.Windows.Media.Media3D, validated against the real product source):
//
//   * `Visual3D : DependencyObject, IVisual3DContainer` — CRITICALLY, Visual3D
//     is a DependencyObject, NOT a Visual (the 11n-0 DAG cluster + the 11n-5
//     title). It exposes `Transform` (Transform3D) and hosts Visual3D children
//     via IVisual3DContainer (AddVisual3DChild/RemoveVisual3DChild/
//     GetVisual3DChildrenCount/GetVisual3DChild).
//   * `IVisual3DContainer` — the insertion/lookup contract the Viewport3D's
//     Visual3DCollection consumes.
//   * `ModelVisual3D : Visual3D` — hosts a `Content` (Model3D) + `Transform`;
//     the 3D leaf that links a Visual3D to an authored Model3D.
//
// These are CORE tree/data types (ADR-011: the 3D tree is CORE DATA; the
// headless rasterizer in vaml::vulkan::media3d walks them). Visual3D is NOT a
// UIElement (WPF-faithful) — it is a lightweight DependencyObject node hosting
// 3D children, distinct from the 2D UIElement visual tree.

#include "vaml/core/Media3D.h"       // Animatable (base), Matrix3D, Point3D
#include "vaml/core/DependencyObject.h" // DependencyObject, SetResult
#include "vaml/core/export.h"

#include <memory>
#include <vector>

namespace vaml {

class Model3D;    // fwd (ModelVisual3D hosts a Model3D)
class Visual3D;   // fwd (IVisual3DContainer references it)

// IVisual3DContainer — the Visual3D-children insertion/lookup contract
// (WPF IVisual3DContainer). Children are held by SHARED ownership over
// reader-retained objects (the 11k-4 no-op-deleter precedent) — the reader
// OWNS the objects, the container holds a borrowed shared_ptr so the headless
// rasterizer can consume the 3D tree without double-free.
class VAML_CORE_EXPORT IVisual3DContainer
{
public:
    virtual ~IVisual3DContainer() = default;

    // Adds `child` as the LAST Visual3D child (owned, appended).
    virtual void add_visual3d_child(std::shared_ptr<Visual3D> child) = 0;
    // Removes the child at `index` (drops the container's reference).
    virtual std::size_t remove_visual3d_child(std::size_t index) = 0;
    virtual std::size_t visual3d_children_count() const = 0;
    virtual Visual3D*   visual3d_child_at(std::size_t index) = 0;
};

// Visual3D — the abstract visual-3D base (WPF-faithful: DependencyObject +
// IVisual3DContainer, NOT a Visual). Owns a Transform (Matrix3D) + an owned
// ordered Visual3D child list.
class VAML_CORE_EXPORT Visual3D : public DependencyObject, public IVisual3DContainer
{
public:
    Visual3D();
    ~Visual3D() override;

    // `Transform` — a Matrix3D (WPF Visual3D.Transform). Member data (the
    // 11k-4 DrawingGroup precedent).
    [[nodiscard]] SetResult set_transform(const Matrix3D& m);
    [[nodiscard]] Matrix3D transform() const noexcept { return transform_; }

    // --- IVisual3DContainer -------------------------------------------------
    void add_visual3d_child(std::shared_ptr<Visual3D> child) override;
    std::size_t remove_visual3d_child(std::size_t index) override;
    std::size_t visual3d_children_count() const override { return children_.size(); }
    Visual3D*   visual3d_child_at(std::size_t index) override;

    // Non-owning read of the Visual3D child at `index` (const-safe).
    [[nodiscard]] Visual3D* child_at(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t child_count() const noexcept { return children_.size(); }

private:
    std::vector<std::shared_ptr<Visual3D>> children_;
    Matrix3D transform_{};
};

// ModelVisual3D — the concrete leaf that hosts an authored Model3D (`Content`)
// + a Transform. WPF: ModelVisual3D : Visual3D with `[ContentProperty("Content")]`.
class VAML_CORE_EXPORT ModelVisual3D : public Visual3D
{
public:
    ModelVisual3D();
    ~ModelVisual3D() override;

    // `Content` — the owned authored Model3D (GeometryModel3D / Model3DGroup).
    void set_content(std::shared_ptr<Model3D> content) { content_ = std::move(content); }
    [[nodiscard]] const std::shared_ptr<Model3D>& content() const noexcept { return content_; }

private:
    std::shared_ptr<Model3D> content_;
};

} // namespace vaml
