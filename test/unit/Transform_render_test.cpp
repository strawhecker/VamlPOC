// S208 + S823 (dep:S822) — Transform_render_test: headless proof of the
// Matrix2D affine value type + the WPF-faithful Transform OBJECT hierarchy +
// the UIElement RenderTransform/RenderTransformOrigin dependent properties (the
// S120 transform-owner resolution). No Vulkan runtime needed — pure core.
//
// Under test:
//   (1) Matrix2D factories — identity / translation / scaling / rotation.
//   (2) affine math — multiply (concatenation), try_invert, transform_point.
//   (3) validity — is_transform_valid (rejects NaN/Inf) + is_transform_identity.
//   (4) S823 Transform OBJECT hierarchy — Transform::identity() default; the
//       concrete leaves (TranslateTransform/RotateTransform/ScaleTransform/
//       MatrixTransform) resolve their matrix().
//   (5) Value tags — the ADDITIVE Matrix2D / Transform / Point ValueType
//       round-trips.
//   (6) S823 RenderTransform DP — default identity Transform; local set as a
//       Transform OBJECT; NOT inherited; ACCEPTS ANY Transform incl. a NaN
//       matrix WITHOUT throwing (the S810_1 removal — WPF has no
//       ValidateValueCallback for RenderTransform); S043 attached-aware global
//       resolution; Set triggers InvalidateArrange.
//   (7) S823 RenderTransformOrigin — default (0,0); round-trip; NOT inherited;
//       Point value path.

#include "doctest/doctest.h"
#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include <vaml/core/Transform.h>   // Matrix2D + the affine seams
#include <vaml/core/UIElement.h>   // UIElement, SetResult
#include <vaml/core/DependencyObject.h>  // SetResult (APPLIED/REJECTED/NO_CHANGE)
#include <vaml/core/DependencyProperty.h>  // S043 attached-aware resolution

#include <cmath>
#include <limits>

using namespace vaml;
using vaml::test::ShouldThrow;

TEST_CASE("S208 Matrix2D: factories — identity / translation / scaling / rotation")
{
    // Identity: no-op affine.
    const Matrix2D id = Matrix2D::identity();
    CHECK(id == Matrix2D{});
    CHECK(is_transform_identity(id));

    // Translation by (dx, dy) layout px.
    const Matrix2D t = Matrix2D::translation(3.0f, -5.0f);
    CHECK(t.dx == doctest::Approx(3.0f));
    CHECK(t.dy == doctest::Approx(-5.0f));
    CHECK(!is_transform_identity(t));

    // Uniform scale.
    const Matrix2D s = Matrix2D::scaling(2.0f);
    CHECK(s.m11 == doctest::Approx(2.0f));
    CHECK(s.m22 == doctest::Approx(2.0f));

    // Axis-aligned scale.
    const Matrix2D sa = Matrix2D::scaling(2.0f, 3.0f);
    CHECK(sa.m11 == doctest::Approx(2.0f));
    CHECK(sa.m22 == doctest::Approx(3.0f));

    // Rotation by 90°: |0 -1| — a top-left-origin CCW affine (rendered
    // clockwise on screen after the renderer's y-flip, WPF-faithful).
    const Matrix2D r = Matrix2D::rotation(90.0f);
    CHECK(r.m11 == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(r.m12 == doctest::Approx(-1.0f).epsilon(1e-4f));
    CHECK(r.m21 == doctest::Approx(1.0f).epsilon(1e-4f));
    CHECK(r.m22 == doctest::Approx(0.0f).epsilon(1e-4f));
}

TEST_CASE("S208 Matrix2D: transform_point under translation / scale / rotation")
{
    // Translation moves the point by (dx, dy).
    float ox = 0.0f, oy = 0.0f;
    transform_point(Matrix2D::translation(10.0f, 20.0f), 1.0f, 2.0f, ox, oy);
    CHECK(ox == doctest::Approx(11.0f));
    CHECK(oy == doctest::Approx(22.0f));

    // Scale about the origin.
    transform_point(Matrix2D::scaling(2.0f), 3.0f, 4.0f, ox, oy);
    CHECK(ox == doctest::Approx(6.0f));
    CHECK(oy == doctest::Approx(8.0f));

    // 90° rotation maps (x, y) -> (-y, x) in layout space.
    transform_point(Matrix2D::rotation(90.0f), 1.0f, 0.0f, ox, oy);
    CHECK(ox == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(oy == doctest::Approx(1.0f).epsilon(1e-4f));
}

TEST_CASE("S208 Matrix2D: multiply concatenates — right-most applied first")
{
    // T(10) * T(5) should map a point by +15 (concatenated translation).
    const Matrix2D t10 = Matrix2D::translation(10.0f, 0.0f);
    const Matrix2D t5  = Matrix2D::translation(5.0f, 0.0f);
    const Matrix2D both = multiply(t10, t5);  // apply t5 first, then t10
    float ox = 0.0f, oy = 0.0f;
    transform_point(both, 0.0f, 0.0f, ox, oy);
    CHECK(ox == doctest::Approx(15.0f));
    CHECK(oy == doctest::Approx(0.0f));

    // Identity is the neutral element on both sides.
    CHECK(multiply(Matrix2D::identity(), t5) == t5);
    CHECK(multiply(t5, Matrix2D::identity()) == t5);
}

TEST_CASE("S208 Matrix2D: try_invert — inverse round-trips; singular is rejected")
{
    // A scale+translate is invertible: applying then inverting returns a point
    // to its origin (within float tolerance).
    const Matrix2D m = multiply(Matrix2D::translation(3.0f, -2.0f), Matrix2D::scaling(4.0f));
    Matrix2D inv{};
    CHECK(try_invert(m, inv));
    const Matrix2D back = multiply(inv, m);
    float ox = 0.0f, oy = 0.0f;
    transform_point(back, 7.0f, -3.0f, ox, oy);
    CHECK(ox == doctest::Approx(7.0f).epsilon(1e-3f));
    CHECK(oy == doctest::Approx(-3.0f).epsilon(1e-3f));

    // A singular (zero-scale) matrix has no inverse -> false, out unchanged.
    Matrix2D untouched{};
    untouched.m11 = 123.0f;
    Matrix2D out = untouched;
    CHECK(!try_invert(Matrix2D::scaling(0.0f), out));
    CHECK(out == untouched);  // out left unchanged on failure
}

TEST_CASE("S208 Matrix2D: validity — rejects NaN/Inf, accepts finite")
{
    CHECK(is_transform_valid(Matrix2D::identity()));
    const Matrix2D bad = Matrix2D::translation(std::numeric_limits<float>::quiet_NaN(), 0.0f);
    CHECK(!is_transform_valid(bad));
    const Matrix2D inf = Matrix2D::translation(0.0f, std::numeric_limits<float>::infinity());
    CHECK(!is_transform_valid(inf));
}

TEST_CASE("S208 Value: Matrix2D factory → tag + equality round-trip (additive)")
{
    const Matrix2D m = Matrix2D::translation(1.0f, 2.0f);
    const Value v = Value::Matrix2D(m);
    CHECK(v.type() == ValueType::Matrix2D);
    CHECK(v.get<Matrix2D>() == m);
    CHECK(Value::Matrix2D(v.get<Matrix2D>()) == v);
    // A Matrix2D value is NOT equal to any other tag with a same-shaped payload.
    CHECK(!(v == Value::Thickness(Thickness{1, 2, 3, 4})));
}

TEST_CASE("S823 Transform OBJECT hierarchy: identity + TranslateTransform + center folds")
{
    // Transform::identity() is the WPF `Transform.Identity` default: identity matrix.
    auto id = Transform::identity();
    REQUIRE(id);
    CHECK(is_transform_identity(id->matrix()));

    // A TranslateTransform resolves its translation.
    auto t = std::make_shared<TranslateTransform>(3.0f, -5.0f);
    CHECK(t->matrix() == Matrix2D::translation(3.0f, -5.0f));

    // A ScaleTransform with a Center folds: T(c)·S·T(−c) — scale about the center.
    auto s = std::make_shared<ScaleTransform>(2.0f, 2.0f, 10.0f, 10.0f);
    // Check a corner: scale about (10,10) by 2 maps (12,12) -> (14,14).
    float ox = 0.0f, oy = 0.0f;
    transform_point(s->matrix(), 12.0f, 12.0f, ox, oy);
    CHECK(ox == doctest::Approx(14.0f).epsilon(1e-4f));
    CHECK(oy == doctest::Approx(14.0f).epsilon(1e-4f));

    // A RotateTransform with a Center folds the rotation about the center.
    auto r = std::make_shared<RotateTransform>();
    r->set_angle(90.0f);
    r->set_center(10.0f, 0.0f);
    // Rotate (10,1) 90° CCW about center (10,0): the offset (0,1) -> (-1,0),
    // added to the center -> (9, 0).
    transform_point(r->matrix(), 10.0f, 1.0f, ox, oy);
    CHECK(ox == doctest::Approx(9.0f).epsilon(1e-4f));
    CHECK(oy == doctest::Approx(0.0f).epsilon(1e-4f));

    // A MatrixTransform holds the raw affine verbatim.
    auto m = std::make_shared<MatrixTransform>(Matrix2D::translation(1.0f, 2.0f));
    CHECK(m->matrix() == Matrix2D::translation(1.0f, 2.0f));
}

TEST_CASE("S823 Value: Transform + Point factory → tag + round-trip (additive)")
{
    auto t = std::make_shared<TranslateTransform>(1.0f, 2.0f);
    const Value v = Value::Transform(t);
    CHECK(v.type() == ValueType::Transform);
    CHECK(v.get<std::shared_ptr<Transform>>() == t); // shared ownership: same object

    const vaml::Point p{ 0.5f, 0.25f };
    const Value pv = Value::Point(p);
    CHECK(pv.type() == ValueType::Point);
    CHECK(pv.get<vaml::Point>() == p);
}

TEST_CASE("S823 UIElement RenderTransform DP: default identity Transform, local set, NOT inherited")
{
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Default is the shared identity Transform (Transform.Identity, UIElement.cs:1335).
    auto def = child->render_transform();
    REQUIRE(def);
    CHECK(is_transform_identity(def->matrix()));

    // Setting a local Transform APPLIES and reads back (shared ownership).
    auto t = std::make_shared<TranslateTransform>(5.0f, 6.0f);
    CHECK(child->set_render_transform(t) == SetResult::APPLIED);
    CHECK(child->render_transform() == t);
    CHECK(child->render_transform()->matrix() == Matrix2D::translation(5.0f, 6.0f));

    // NOT inherited: a parent's transform does NOT cascade to a child (unlike
    // Opacity — RenderTransform is registered with PropertyFlags::None).
    (void)parent.set_render_transform(std::make_shared<TranslateTransform>(100.0f, 0.0f));
    CHECK(child->render_transform() == t);  // child KEEPS its own local transform
}

TEST_CASE("S823 UIElement RenderTransform DP: a no-local child under a transformed parent stays identity")
{
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Parent transformed; child has NO local -> still identity (NOT inherited).
    (void)parent.set_render_transform(std::make_shared<TranslateTransform>(50.0f, 50.0f));
    CHECK(is_transform_identity(child->render_transform()->matrix()));
}

TEST_CASE("S823 UIElement RenderTransform DP: accepts ANY Transform incl. a NaN matrix WITHOUT throwing")
{
    // S823 (WPF fidelity — Rule 1/2): RenderTransformProperty is typedef
    // typeof(Transform) with NO ValidateValueCallback (UIElement.cs:1335-1340),
    // so a NON-FINITE matrix is WPF-LEGAL and ACCEPTED at Set — the old S810_1
    // throw is REMOVED. The finite-validity gate VAML needs stays ONLY at the
    // S208 raster/encode boundary (is_transform_valid re-scoped there), never the
    // DP Set. This is the inverse of the S208-era CHECK_THROWS_AS test.
    UIElement e;
    auto bad = std::make_shared<MatrixTransform>(
        Matrix2D::translation(std::numeric_limits<float>::quiet_NaN(), 0.0f));
    // Does NOT throw: the Transform object is ACCEPTED (WPF accepts any Transform).
    (void)e.set_render_transform(bad);
    // The NaN matrix is STORED (WPF-legal at the DP); the ENCODE boundary folds it
    // defensively (is_transform_valid false -> identity), never the DP.
    CHECK(e.render_transform() == bad);
    CHECK(!is_transform_valid(bad->matrix()));
}

TEST_CASE("S823 UIElement RenderTransform DP: Set triggers InvalidateArrange")
{
    UIElement e;
    e.Measure(Size{ 10.0f, 10.0f });
    e.Arrange(Rect{ 0.0f, 0.0f, 10.0f, 10.0f });
    CHECK(!e.is_arrange_dirty());  // arranged clean

    // A render-transform change is a visual change -> arrange-dirty (fresh pass).
    (void)e.set_render_transform(std::make_shared<TranslateTransform>(1.0f, 1.0f));
    CHECK(e.is_arrange_dirty());
}

TEST_CASE("S823 UIElement RenderTransform DP: S043 attached-aware global resolution")
{
    // RenderTransform is globally registered (self-registration), so it resolves
    // on ANY DependencyObject (S043 attached-aware) and via the PropertyId path.
    UIElement e;
    (void)e.set_render_transform(std::make_shared<TranslateTransform>(3.0f, 4.0f));

    // Unregistered default on a fresh object (the global descriptor applies).
    UIElement fresh;
    CHECK(is_transform_identity(fresh.render_transform()->matrix()));

    // PropertyId path reads the effective value back (Transform-typed, additive).
    const Value v = fresh.Get(PropertyId::of("RenderTransform", "UIElement"));
    CHECK(v.type() == ValueType::Transform);
}

TEST_CASE("S823 UIElement RenderTransformOrigin DP: default (0,0), round-trip, NOT inherited")
{
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Default (0,0).
    CHECK(child->render_transform_origin() == vaml::Point{});

    // Round-trip.
    const vaml::Point p{ 0.5f, 0.25f };
    CHECK(child->set_render_transform_origin(p) == SetResult::APPLIED);
    CHECK(child->render_transform_origin() == p);

    // NOT inherited: a parent's origin does NOT cascade to a child
    // (PropertyFlags::None).
    (void)parent.set_render_transform_origin(vaml::Point{ 1.0f, 1.0f });
    CHECK(child->render_transform_origin() == p);  // child KEEPS its own local origin
}

TEST_CASE("S823 UIElement RenderTransformOrigin DP: PropertyId path is Point-typed")
{
    UIElement e;
    (void)e.set_render_transform_origin(vaml::Point{ 0.2f, 0.8f });
    const Value v = e.Get(PropertyId::of("RenderTransformOrigin", "UIElement"));
    CHECK(v.type() == ValueType::Point);
    CHECK(v.get<vaml::Point>() == vaml::Point{ 0.2f, 0.8f });
}
