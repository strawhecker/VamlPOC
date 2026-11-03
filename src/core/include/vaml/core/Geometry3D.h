#pragma once
// VAML Geometry3D — the 11n-5 geometry-3D base + MeshGeometry3D leaf
// (vaml::core CORE DATA, ADR-011 held: the geometry model stays core, the
// rasterizer stays vaml::vulkan::media3d).
//
// WPF ground truth (System.Windows.Media.Media3D, validated against the real
// product source):
//   * `Geometry3D : Animatable` is ABSTRACT (WPF: `public abstract partial
//     class Geometry3D : Animatable`).
//   * `MeshGeometry3D : Geometry3D` — `Positions` (Point3DCollection) +
//     `TriangleIndices` (Int32Collection) + a `Bounds` (Rect3D) derived from
//     the positions. This is the concrete leaf the headless rasterizer
//     consumes.
//
// The abstract base is NOT authorable as a bare element (WPF-faithful:
// abstract + internal ctor — the 11k-4 Geometry/Drawing precedent). The
// concrete MeshGeometry3D leaf is the authorable + testable surface.

#include "vaml/core/Media3D.h"    // Animatable, Point3D, Rect3D, Matrix3D
#include "vaml/core/DependencyObject.h" // SetResult (via Animatable : Freezable : DependencyObject)
#include "vaml/core/export.h"

#include <cstdint>
#include <vector>

namespace vaml {

// Geometry3D — the abstract 3D-geometry base (WPF-faithful, NOT authorable).
class VAML_CORE_EXPORT Geometry3D : public Animatable
{
public:
    ~Geometry3D() override;

    // Axis-aligned 3D bounds of the geometry in its local space (derived by
    // the concrete leaf). Pure.
    [[nodiscard]] virtual Rect3D bounds() const = 0;

    // `Transform` — a Matrix3D on the geometry (WPF Geometry3D.Transform
    // analog). The concrete leaf derives it; the base exposes the const read so
    // the rasterizer can fold the per-geometry model matrix.
    [[nodiscard]] virtual Matrix3D transform() const noexcept { return transform_; }

protected:
    Geometry3D() = default;
    Matrix3D transform_{};

private:
    Geometry3D(const Geometry3D&) = delete;
    Geometry3D& operator=(const Geometry3D&) = delete;
};

// MeshGeometry3D — the concrete triangle-mesh leaf (authorable DO resource).
// `Positions` = the vertex list (Point3D); `TriangleIndices` = the triangle
// index triples (each triple references 3 positions, WPF Int32Collection
// semantics). `Bounds` = the axis-aligned box over the positions.
class VAML_CORE_EXPORT MeshGeometry3D : public Geometry3D
{
public:
    MeshGeometry3D();
    ~MeshGeometry3D() override;

    // `Positions` — the authored vertex list. Accepted raw (a pure data fill;
    // the reader authoring path parses "x,y,z ..." into them). Empty is legal
    // (a mesh with no vertices has empty bounds + rasterizes to ok=false).
    void set_positions(std::vector<Point3D> positions) { positions_ = std::move(positions); }
    [[nodiscard]] const std::vector<Point3D>& positions() const noexcept { return positions_; }

    // `TriangleIndices` — the authored index triples (WPF Int32Collection).
    void set_triangle_indices(std::vector<std::int32_t> indices) { indices_ = std::move(indices); }
    [[nodiscard]] const std::vector<std::int32_t>& triangle_indices() const noexcept { return indices_; }

    // `Transform` — a Matrix3D on the geometry (WPF MeshGeometry3D.Transform /
    // Geometry3D.Transform analog). Writes the base's transform_ member (the
    // base exposes the const read). The finite-validity gate REJECTS a non-
    // finite matrix (the rasterizer never sees NaN/Inf geometry).
    [[nodiscard]] SetResult set_transform(const Matrix3D& m);

    [[nodiscard]] Rect3D bounds() const override;

private:
    std::vector<Point3D>    positions_;
    std::vector<std::int32_t> indices_;
};

} // namespace vaml
