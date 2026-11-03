#pragma once
// VAML Model3D — the 11n-5 model-3D base + the concrete leaves
// (vaml::core CORE DATA, ADR-011 held: the model stays core, the renderer
// stays vaml::vulkan::media3d).
//
// WPF ground truth (System.Windows.Media.Media3D, validated against the real
// product source):
//   * `Model3D : Animatable` is ABSTRACT (`public abstract partial class
//     Model3D : Animatable`) with a `Bounds` (Rect3D).
//   * `GeometryModel3D : Model3D` — `Geometry` (Geometry3D) + `Material`
//     (Material) + `BackMaterial`.
//   * `Model3DGroup : Model3D` — `[ContentProperty("Children")]` + `Transform`
//     (Transform3D) + an owned, ordered Children (Model3DCollection) + a
//     Bounds union.
//   * `Material` is an ABSTRACT base (System.Windows.Media.Media3D.Material);
//     the concrete materials (DiffuseMaterial/SpecularMaterial...) are later
//     material steps.
//
// VAML keeps it MINIMAL-faithful: the abstract Model3D + the two concrete
// leaves (GeometryModel3D + Model3DGroup) deliver a real authorable, BOUNDED,
// rasterizable 3D surface. `Material` is a light placeholder value identity
// (the abstract base + a DiffuseMaterial solid-tint leaf) — the full material
// hierarchy (SpecularMaterial/EmissiveMaterial/MaterialGroup) is recorded
// NO-GAP = the S649 continuation (control-material re-bind), never silent.

#include "vaml/core/Geometry3D.h"    // Geometry3D, MeshGeometry3D
#include "vaml/core/Media3D.h"       // Animatable, Rect3D, Matrix3D
#include "vaml/core/Color.h"         // Color (the solid-tint material identity)
#include "vaml/core/DependencyObject.h" // SetResult
#include "vaml/core/export.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace vaml {

// Material — the abstract material base (WPF-faithful, NOT authorable).
class VAML_CORE_EXPORT Material : public Animatable
{
public:
    ~Material() override;

protected:
    Material() = default;

private:
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
};

// DiffuseMaterial — a solid-tint material leaf (`Color` = the opaque tint the
// headless rasterizer fills a model's projected triangles with). The concrete
// authorable material identity for this step (MaterialGroup + the specular/
// emissive family = the S649 continuation, recorded NO-GAP). DO leaf.
class VAML_CORE_EXPORT DiffuseMaterial : public Material
{
public:
    DiffuseMaterial();
    ~DiffuseMaterial() override;

    // `Color` — the straight-alpha solid tint (default opaque black).
    [[nodiscard]] SetResult set_color(const Color& c);
    [[nodiscard]] Color color() const noexcept { return color_; }

private:
    Color color_{ 0, 0, 0, 255 };
};

// Model3D — the abstract 3D-model base (WPF-faithful, NOT authorable).
class VAML_CORE_EXPORT Model3D : public Animatable
{
public:
    ~Model3D() override;

    // Axis-aligned 3D bounds in model space (derived by the concrete leaf).
    [[nodiscard]] virtual Rect3D bounds() const = 0;

protected:
    Model3D() = default;

private:
    Model3D(const Model3D&) = delete;
    Model3D& operator=(const Model3D&) = delete;
};

// GeometryModel3D — the concrete geometry+material model leaf (authorable).
class VAML_CORE_EXPORT GeometryModel3D : public Model3D
{
public:
    GeometryModel3D();
    ~GeometryModel3D() override;

    // `Geometry` — the owned authored Geometry3D (typically a MeshGeometry3D).
    void set_geometry(std::shared_ptr<Geometry3D> g) { geometry_ = std::move(g); }
    [[nodiscard]] const std::shared_ptr<Geometry3D>& geometry() const noexcept { return geometry_; }

    // `Material` — the owned authored tint material (typically a DiffuseMaterial).
    void set_material(std::shared_ptr<Material> m) { material_ = std::move(m); }
    [[nodiscard]] const std::shared_ptr<Material>& material() const noexcept { return material_; }

    [[nodiscard]] Rect3D bounds() const override;

private:
    std::shared_ptr<Geometry3D> geometry_;
    std::shared_ptr<Material>   material_;
};

// Model3DGroup — the owned, ordered model-collection leaf (authorable).
class VAML_CORE_EXPORT Model3DGroup : public Model3D
{
public:
    Model3DGroup();
    ~Model3DGroup() override;

    void add_model(std::shared_ptr<Model3D> child);
    [[nodiscard]] const std::vector<std::shared_ptr<Model3D>>& children() const noexcept { return children_; }
    [[nodiscard]] std::size_t child_count() const noexcept { return children_.size(); }

    // `Transform` — a Matrix3D on the group (WPF Model3DGroup.Transform).
    [[nodiscard]] SetResult set_transform(const Matrix3D& m);
    [[nodiscard]] Matrix3D transform() const noexcept { return transform_; }

    [[nodiscard]] Rect3D bounds() const override;

private:
    std::vector<std::shared_ptr<Model3D>> children_;
    Matrix3D transform_{};
};

} // namespace vaml
