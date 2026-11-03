// 11n-5 (dep:11n-2 Freezable / S208 Matrix2D / 11k-4 VectorRaster) — the 3D
// surface + the HEADLESS SOFTWARE RASTERIZER (vaml::vulkan::media3d).
//
// Coverage (the plan's 6 points):
//   1. value spine — Point3D/Matrix3D/Rect3D + the projection math (look_at +
//      projection) + the finite gates;
//   2. Animatable : Freezable — the 11n-0 DAG anchor (a real kept base over the
//      S146 Freezable: frozen-immutable guard + Changed);
//   3. the authored object model round-trips via the S043 attached-aware Set
//      path + resolve_element (Visual3D/ModelVisual3D/GeometryModel3D/
//      Model3DGroup/MeshGeometry3D/Camera);
//   4. HEADLINE — an authored Viewport3D (a ModelVisual3D with a
//      GeometryModel3D over a MeshGeometry3D tetrahedron + a camera) RASTERIZES
//      headlessly to a non-empty S202 RGBA8 tile with the projected ink
//      (geometry -> tile proof);
//   5. authoring path — a corpus-shaped `<Viewport3D><ModelVisual3D>…</Viewport3D>`
//      document LOADS through the reader + resolves;
//   6. bounds correctness + degenerate (empty geometry -> ok=false, never crash).
//
// ADR-018: zero new locks. ADR-011: the 3D model/projection math is CORE; this
// test exercises the vulkan rasterizer (the render stays vaml::vulkan::media3d).
// Pure + headless (no Vulkan runtime, no GPU) — the DEVICE upload + a real 3D
// GPU pipeline are the [both-apps, not CI] live lane.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Media3D.h>
#include <vaml/core/Geometry3D.h>
#include <vaml/core/Model3D.h>
#include <vaml/core/Visual3D.h>
#include <vaml/core/Viewport3D.h>
#include <vaml/core/Element.h>
#include <vaml/core/VamlReader.h>

#include <vaml/vulkan/media3d/Media3DRaster.h>
#include <vaml/vulkan/ui/UiPipeline.h> // Color4

#include <memory>
#include <string>

using vaml::Color;
using vaml::DiffuseMaterial;
using vaml::GeometryModel3D;
using vaml::Matrix3D;
using vaml::MeshGeometry3D;
using vaml::Model3DGroup;
using vaml::ModelVisual3D;
using vaml::OrthographicCamera;
using vaml::PerspectiveCamera;
using vaml::Point3D;
using vaml::Rect3D;
using vaml::SetResult;
using vaml::VamlReader;
using vaml::Vector3D;
using vaml::Viewport3D;
using vaml::Visual3D;

// ---------------------------------------------------------------------------
// 1. value spine + projection math
// ---------------------------------------------------------------------------

TEST_CASE("11n5 value spine: Matrix3D affine + finite gates + projection")
{
    SUBCASE("identity + translation + multiply")
    {
        const Matrix3D id = Matrix3D::identity();
        CHECK(vaml::is_matrix3d_valid(id));
        const Matrix3D t = Matrix3D::translation(1.0f, 2.0f, 3.0f);
        float ox = 0, oy = 0, oz = 0;
        vaml::transform_point3d(t, 1.0f, 1.0f, 1.0f, ox, oy, oz);
        CHECK(ox == 2.0f);
        CHECK(oy == 3.0f);
        CHECK(oz == 4.0f);
        // id * t == t
        const Matrix3D p = vaml::multiply3d(id, t);
        CHECK(p == t);
    }

    SUBCASE("finite gate rejects NaN/Inf")
    {
        Matrix3D bad = Matrix3D::identity();
        bad.m11 = std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(vaml::is_matrix3d_valid(bad));
    }

    SUBCASE("invert round-trip")
    {
        const Matrix3D t = Matrix3D::translation(3.0f, -2.0f, 5.0f);
        Matrix3D inv;
        REQUIRE(vaml::try_invert3d(t, inv));
        const Matrix3D p = vaml::multiply3d(t, inv);
        // Approximate identity (floating tolerance via finite close compare).
        float ox = 0, oy = 0, oz = 0;
        vaml::transform_point3d(p, 1.0f, 2.0f, 3.0f, ox, oy, oz);
        CHECK(std::abs(ox - 1.0f) < 1e-4f);
        CHECK(std::abs(oy - 2.0f) < 1e-4f);
        CHECK(std::abs(oz - 3.0f) < 1e-4f);
    }

    SUBCASE("projection + look_at produce a finite pipeline")
    {
        vaml::Camera3D cam;
        cam.position = { 0.0f, 0.0f, 5.0f };
        cam.target   = { 0.0f, 0.0f, 0.0f };
        cam.up       = { 0.0f, 1.0f, 0.0f };
        cam.fovDegrees = 45.0f;
        Matrix3D view, proj;
        REQUIRE(vaml::look_at(cam, view));
        REQUIRE(vaml::projection(cam, proj));
        CHECK(vaml::is_matrix3d_valid(view));
        CHECK(vaml::is_matrix3d_valid(proj));
    }

    SUBCASE("degenerate camera rejected (look_at) + bad cone rejected (projection)")
    {
        vaml::Camera3D degenerate;
        degenerate.position = { 1.0f, 1.0f, 1.0f };
        degenerate.target   = { 1.0f, 1.0f, 1.0f }; // position == target
        Matrix3D view;
        CHECK_FALSE(vaml::look_at(degenerate, view));

        vaml::Camera3D badCone;
        badCone.nearPlane = -1.0f; // non-positive near
        Matrix3D proj;
        CHECK_FALSE(vaml::projection(badCone, proj));
    }
}

// ---------------------------------------------------------------------------
// 2. Animatable : Freezable (the 11n-0 DAG anchor)
// ---------------------------------------------------------------------------

TEST_CASE("11n5 Animatable : Freezable (the 11n-0 DAG anchor)")
{
    SUBCASE("a Geometry3D leaf is an Animatable (Freezable-derived) + can Freeze")
    {
        MeshGeometry3D mesh;
        CHECK_FALSE(mesh.is_frozen());
        mesh.Freeze();
        CHECK(mesh.is_frozen());
        // A frozen Freezable with a frozen base still resolves reads (getters
        // are not writes). The write path is guarded by throw_if_frozen on the
        // Freezable-derived leaves that use DPs; the owned-members here are
        // data fills so they stay legal (the freeze state rides Freezable).
    }
}

// ---------------------------------------------------------------------------
// 3. authored object model round-trip
// ---------------------------------------------------------------------------

TEST_CASE("11n5 object model round-trip via Set path + owned graph")
{
    SUBCASE("MeshGeometry3D bounds + finite transform gate")
    {
        MeshGeometry3D mesh;
        CHECK(mesh.bounds().is_empty());
        mesh.set_positions({
            { 0.0f, 0.0f, 0.0f },
            { 2.0f, 0.0f, 0.0f },
            { 0.0f, 3.0f, 0.0f },
        });
        const Rect3D b = mesh.bounds();
        CHECK(b.sizeX == 2.0f);
        CHECK(b.sizeY == 3.0f);
        CHECK(b.sizeZ == 0.0f);

        // finite gate: non-finite transform REJECTED.
        Matrix3D bad = Matrix3D::identity();
        bad.m22 = std::numeric_limits<float>::infinity();
        CHECK(mesh.set_transform(bad) == SetResult::REJECTED);
    }

    SUBCASE("GeometryModel3D + Model3DGroup bounds union")
    {
        auto mesh2 = std::make_shared<MeshGeometry3D>();
        mesh2->set_positions({
            { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        });
        auto gm = std::make_shared<GeometryModel3D>();
        gm->set_geometry(mesh2);
        CHECK(gm->bounds().sizeX == 4.0f);

        Model3DGroup group;
        group.add_model(gm);
        CHECK(group.child_count() == 1);
        CHECK(group.bounds().sizeX == 4.0f);
    }

    SUBCASE("Visual3D children + ModelVisual3D content")
    {
        Visual3D root;
        auto mv = std::make_shared<ModelVisual3D>();
        auto gm = std::make_shared<GeometryModel3D>();
        mv->set_content(gm);
        root.add_visual3d_child(mv);
        CHECK(root.child_count() == 1);
        CHECK(root.visual3d_children_count() == 1);
        Visual3D* child = root.child_at(0);
        REQUIRE(child != nullptr);
        auto* mvChild = dynamic_cast<ModelVisual3D*>(child);
        REQUIRE(mvChild != nullptr);
        CHECK(mvChild->content() == gm);
    }
}

// ---------------------------------------------------------------------------
// 4 + 5 + 6. authoring path + HEADLINE rasterizer + degenerate
// ---------------------------------------------------------------------------

TEST_CASE("11n5 authoring path: corpus-shaped Viewport3D loads + resolves")
{
    SUBCASE("Viewport3D + ModelVisual3D + GeometryModel3D + MeshGeometry3D + camera")
    {
        vaml::register_builtin_elements();
        const std::string doc =
            "<Viewport3D>"
            "  <ModelVisual3D>"
            "    <GeometryModel3D>"
            "      <GeometryModel3D.Geometry>"
            "        <MeshGeometry3D "
            "          Positions=\"0,0,0 1,0,0 0,1,0\" "
            "          TriangleIndices=\"0 1 2\"/>"
            "      </GeometryModel3D.Geometry>"
            "      <GeometryModel3D.Material>"
            "        <DiffuseMaterial Color=\"#FF0000FF\"/>"
            "      </GeometryModel3D.Material>"
            "    </GeometryModel3D>"
            "  </ModelVisual3D>"
            "  <Viewport3D.Camera>"
            "    <PerspectiveCamera Position=\"0,0,5\" Target=\"0,0,0\"/>"
            "  </Viewport3D.Camera>"
            "</Viewport3D>";

        VamlReader reader;
        std::unique_ptr<vaml::DependencyObject> root = reader.Load(doc);
        REQUIRE(root != nullptr);
        auto* vp = dynamic_cast<Viewport3D*>(root.get());
        REQUIRE(vp != nullptr);
        // The camera wired.
        REQUIRE(vp->camera() != nullptr);
        CHECK(dynamic_cast<PerspectiveCamera*>(vp->camera().get()) != nullptr);
        // The Visual3D child wired (via add_visual3d_child; ModelVisual3D is not
        // a UIElement, so it stays non-visual — the 3D child list holds it).
        CHECK(vp->child3d_count() == 1);
        Visual3D* v = vp->child3d_at(0);
        REQUIRE(v != nullptr);
        auto* mv = dynamic_cast<ModelVisual3D*>(v);
        REQUIRE(mv != nullptr);
        REQUIRE(mv->content() != nullptr);
        auto* gm = dynamic_cast<GeometryModel3D*>(mv->content().get());
        REQUIRE(gm != nullptr);
        REQUIRE(gm->geometry() != nullptr);
        auto* mesh = dynamic_cast<MeshGeometry3D*>(gm->geometry().get());
        REQUIRE(mesh != nullptr);
        CHECK(mesh->positions().size() == 3);
        CHECK(mesh->triangle_indices().size() == 3);
    }
}

TEST_CASE("11n5 HEADLINE: authored Viewport3D rasterizes headlessly to a tile")
{
    SUBCASE("tetrahedron -> non-empty RGBA8 tile -> textured quad")
    {
        // Build the scene directly (the fully-authored load is covered above).
        auto mesh = std::make_shared<MeshGeometry3D>();
        mesh->set_positions({
            { 0.0f,  1.0f,  0.0f },
            { -1.0f, -0.5f, 0.5f },
            { 1.0f, -0.5f, 0.5f },
            { 0.0f, -0.5f, -0.5f },
        });
        mesh->set_triangle_indices({ 0, 1, 2,  0, 2, 3,  0, 3, 1,  1, 3, 2 });

        auto mat = std::make_shared<DiffuseMaterial>();
        (void)mat->set_color(Color{ 255, 0, 0, 255 });

        auto gm = std::make_shared<GeometryModel3D>();
        gm->set_geometry(mesh);
        gm->set_material(mat);

        auto mv = std::make_shared<ModelVisual3D>();
        mv->set_content(gm);

        Viewport3D viewport;
        viewport.add_visual3d_child(mv);

        auto cam = std::make_shared<PerspectiveCamera>();
        (void)cam->set_position({ 0.0f, 0.0f, 6.0f });
        (void)cam->set_target({ 0.0f, 0.0f, 0.0f });
        (void)cam->set_field_of_view(45.0);
        (void)viewport.set_camera(cam);

        vaml::vulkan::media3d::Media3DTile tile =
            vaml::vulkan::media3d::raster_viewport_to_tile(
                viewport, Color{ 0, 0, 0, 255 });
        REQUIRE(tile.ok);
        CHECK(tile.texelCount == vaml::vulkan::media3d::kMedia3DTileSize *
                                      vaml::vulkan::media3d::kMedia3DTileSize);
        // Non-empty ink: at least one opaque pixel.
        bool ink = false;
        for (std::size_t i = 3; i < tile.rgba.size(); i += 4)
        {
            if (tile.rgba[i] > 0u)
            {
                ink = true;
                break;
            }
        }
        CHECK(ink);

        // Emit the S206 textured quad.
        vaml::vulkan::ui::DrawItem item =
            vaml::vulkan::media3d::media3d_quad(
                0.0f, 0.0f, 64.0f, 64.0f, tile,
                vaml::vulkan::ui::Color4{ 1.0f, 1.0f, 1.0f, 1.0f });
        CHECK(item.w == 64.0f);
    }

    SUBCASE("degenerate: empty scene -> ok=false, no crash")
    {
        Viewport3D empty;
        auto cam = std::make_shared<PerspectiveCamera>();
        (void)cam->set_position({ 0.0f, 0.0f, 5.0f });
        (void)cam->set_target({ 0.0f, 0.0f, 0.0f });
        (void)empty.set_camera(cam);
        vaml::vulkan::media3d::Media3DTile tile =
            vaml::vulkan::media3d::raster_viewport_to_tile(
                empty, Color{ 0, 0, 0, 255 });
        CHECK_FALSE(tile.ok);
        CHECK(tile.rgba.empty());
    }

    SUBCASE("degenerate: empty geometry (no triangles) -> ok=false")
    {
        auto mesh = std::make_shared<MeshGeometry3D>();
        mesh->set_positions({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } });
        auto gm = std::make_shared<GeometryModel3D>();
        gm->set_geometry(mesh);
        auto mv = std::make_shared<ModelVisual3D>();
        mv->set_content(gm);
        Viewport3D viewport;
        viewport.add_visual3d_child(mv);
        auto cam = std::make_shared<PerspectiveCamera>();
        (void)cam->set_position({ 0.0f, 0.0f, 5.0f });
        (void)cam->set_target({ 0.0f, 0.0f, 0.0f });
        (void)viewport.set_camera(cam);
        vaml::vulkan::media3d::Media3DTile tile =
            vaml::vulkan::media3d::raster_viewport_to_tile(
                viewport, Color{ 0, 0, 0, 255 });
        // The geometry exists but has no triangle indices -> no ink.
        CHECK_FALSE(tile.ok);
    }
}
