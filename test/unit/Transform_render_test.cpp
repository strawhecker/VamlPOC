// S208 (dep:S207) — Transform_render_test: headless proof of the Matrix2D
// affine value type + the UIElement RenderTransform dependent property (the
// S120 transform-owner resolution). No Vulkan runtime needed — pure core.
//
// Under test:
//   (1) Matrix2D factories — identity / translation / scaling / rotation.
//   (2) affine math — multiply (concatenation), try_invert, transform_point.
//   (3) validity — is_transform_valid (rejects NaN/Inf) + is_transform_identity.
//   (4) Value tag — the S208 ADDITIVE Matrix2D ValueType round-trips (the
//       Value_roundtrip_test.cpp additive case covers the full tag matrix; this
//       case re-proves the factory/equality path on the real type).
//   (5) RenderTransform DP — default identity; local set; NOT inherited; a
//       non-finite matrix is REJECTED (store unchanged); S043 attached-aware
//       global resolution; Set triggers InvalidateArrange.

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

TEST_CASE("S208 UIElement RenderTransform DP: default identity, local set, NOT inherited")
{
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Default is identity (no transform).
    CHECK(is_transform_identity(child->render_transform()));

    // Setting a local transform APPLIES and reads back.
    const Matrix2D t = Matrix2D::translation(5.0f, 6.0f);
    CHECK(child->set_render_transform(t) == SetResult::APPLIED);
    CHECK(child->render_transform() == t);

    // NOT inherited: a parent's transform does NOT cascade to a child (unlike
    // Opacity — RenderTransform is registered with PropertyFlags::None).
    const Matrix2D pt = Matrix2D::translation(100.0f, 0.0f);
    (void)parent.set_render_transform(pt);
    CHECK(child->render_transform() == t);  // child KEEPS its own local transform
    CHECK(parent.render_transform() == pt);
}

TEST_CASE("S208 UIElement RenderTransform DP: a no-local child under a transformed parent stays identity")
{
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Parent transformed; child has NO local -> still identity (NOT inherited,
    // the S208 RenderTransform inheritance decision).
    (void)parent.set_render_transform(Matrix2D::translation(50.0f, 50.0f));
    CHECK(is_transform_identity(child->render_transform()));
}

TEST_CASE("S208 UIElement RenderTransform DP: non-finite matrix is REJECTED")
{
    UIElement e;
    const Matrix2D bad = Matrix2D::translation(std::numeric_limits<float>::quiet_NaN(), 0.0f);
    CHECK(e.set_render_transform(bad) == SetResult::REJECTED);
    // Store unchanged (still identity).
    CHECK(is_transform_identity(e.render_transform()));
}

TEST_CASE("S208 UIElement RenderTransform DP: Set triggers InvalidateArrange")
{
    UIElement e;
    e.Measure(Size{ 10.0f, 10.0f });
    e.Arrange(Rect{ 0.0f, 0.0f, 10.0f, 10.0f });
    CHECK(!e.is_arrange_dirty());  // arranged clean

    // A render-transform change is a visual change -> arrange-dirty (fresh pass).
    (void)e.set_render_transform(Matrix2D::translation(1.0f, 1.0f));
    CHECK(e.is_arrange_dirty());
}

TEST_CASE("S208 UIElement RenderTransform DP: S043 attached-aware global resolution")
{
    // RenderTransform is globally registered (self-registration), so it resolves
    // on ANY DependencyObject (S043 attached-aware) and via the PropertyId path.
    UIElement e;
    (void)e.set_render_transform(Matrix2D::translation(3.0f, 4.0f));

    // Unregistered default on a fresh object (the global descriptor applies).
    UIElement fresh;
    CHECK(is_transform_identity(fresh.render_transform()));

    // PropertyId path reads the effective value back.
    const Value v = fresh.Get(PropertyId::of("RenderTransform"));
    CHECK(v.type() == ValueType::Matrix2D);
    CHECK(v.get<Matrix2D>() == Matrix2D::identity());
}
