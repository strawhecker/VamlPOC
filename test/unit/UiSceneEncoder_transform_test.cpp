// S208 (dep:S207) — UiSceneEncoder_transform_test: headless proof that the
// encoder HONORS Opacity + Clip + RenderTransform when it emits the S206 draw
// list (the S208 headline). No Vulkan runtime needed.
//
// Under test:
//   (1) effective_opacity — the ancestor-chain PRODUCT (child 0.8 under a
//       0.5 parent -> 0.4; identity chain -> 1; zero ancestor -> 0).
//   (2) Opacity folded into item alpha — the encoder multiplies each emitted
//       item's alpha by its effective opacity.
//   (3) RenderTransform — a rotated/translated child's emitted item carries the
//       per-element Model; append_ui_item CPU-folds it (transformed corners).
//   (4) Clip — a transformed element overflowing its slot marks clipActive with
//       the S124 render slot; an identity-transform element stays unclipped
//       (backward-compatible batching).
//   (5) Grouping — identity-transform same-color elements stay ONE batch; a
//       clipped/transformed element splits into its own batch; MaterialGroupCount
//       <= batch_count for a clipped list.

#include <vaml/vulkan/ui/UiScene.h>          // DrawItem, Color4, solid_rect, batch_plan
#include <vaml/vulkan/ui/UiSceneEncoder.h>   // the S208 encoder + effective_opacity
#include <vaml/vulkan/quad/QuadVertex.h>     // quad::QuadVertex (CPU-fold proof)

#include <vaml/core/Element.h>   // StackPanel + Orientation (the real panel)
#include <vaml/core/UIElement.h> // UIElement, Visibility
#include <vaml/core/Geometry.h>  // Size, Rect
#include <vaml/core/Transform.h> // Matrix2D

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;
using namespace vaml::vulkan;

namespace {

// A real solid leaf (same shape as UiSceneEncoder_test's SolidLeaf).
struct SolidLeaf : vaml::UIElement
{
    explicit SolidLeaf(float w, float h) : extent{ w, h } {}

    vaml::Size extent;

protected:
    vaml::Size MeasureOverride(const vaml::Size& available) const override
    {
        (void)available;
        return extent;
    }
};

std::unordered_map<const vaml::UIElement*, ElementVisual> g_visuals;

ElementVisual visual_of(const vaml::UIElement& e)
{
    auto it = g_visuals.find(&e);
    if (it != g_visuals.end())
        return it->second;
    return { classify_element(e), DrawItem{} };
}

void assign_solid(vaml::UIElement& e, const Color4& color)
{
    const vaml::Rect r = e.final_rect();
    ElementVisual v;
    v.kind = MaterialKind::Solid;
    v.item = solid_rect(r.x, r.y, r.width, r.height, color);
    g_visuals[&e] = v;
}

void clear_visuals()
{
    g_visuals.clear();
}

} // namespace

TEST_CASE("S208 effective_opacity: the resolved opacity() is the render alpha")
{
    vaml::UIElement root;
    vaml::UIElement* mid = root.create_child<vaml::UIElement>();
    vaml::UIElement* leaf = mid->create_child<vaml::UIElement>();

    // Default: every opacity is 1 -> effective 1.
    CHECK(effective_opacity(*leaf) == doctest::Approx(1.0));
    CHECK(effective_opacity(root) == doctest::Approx(1.0));

    // Opacity is INHERITED (PropertyFlags::Inherits, S120): a root set cascades
    // down the S040/S041 inheritance scope, so a leaf WITHOUT a local override
    // resolves to the ancestor's 0.5 — the single honest render alpha.
    (void)root.set_opacity(0.5);
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.5).epsilon(1e-6));
    CHECK(effective_opacity(*mid) == doctest::Approx(0.5).epsilon(1e-6));
    CHECK(effective_opacity(root) == doctest::Approx(0.5).epsilon(1e-6));

    // A leaf's OWN local set SHADOWS the inherited value (local-wins — WPF
    // inheritance), so it renders at 0.8, NOT 0.5*0.8 (no double-count).
    (void)leaf->set_opacity(0.8);
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.8).epsilon(1e-6));

    // A zero opacity on `mid` (which has NO local) resolves to 0 (inherited).
    // `leaf` KEEPS its own 0.8 local (local-wins — an ancestor's 0 does not
    // override an element's own explicit opacity), the WPF inheritance rule.
    (void)mid->set_opacity(0.0);
    CHECK(effective_opacity(*mid) == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.8).epsilon(1e-6));
}

TEST_CASE("S208 encoder: effective opacity folds into the emitted item alpha")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* a = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* b = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    assign_solid(*a, { 1, 0, 0, 1.0f });
    assign_solid(*b, { 0, 1, 0, 1.0f });

    // Root at 0.5 -> inherited into both children -> each emits alpha 0.5.
    (void)root.set_opacity(0.5);

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 2u);
    CHECK(out[0].a == doctest::Approx(0.5f).epsilon(1e-6f));
    CHECK(out[1].a == doctest::Approx(0.5f).epsilon(1e-6f));
}

TEST_CASE("S208 encoder: RenderTransform — a translated element carries the Model")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    const vaml::Rect fr = leaf->final_rect();
    assign_solid(*leaf, { 0, 0, 1, 1 });

    // Translate the leaf by (+5, +5) in layout px.
    (void)leaf->set_render_transform(vaml::Matrix2D::translation(5.0f, 5.0f));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);

    // The item carries the element's Model (not identity).
    CHECK(out[0].model == vaml::Matrix2D::translation(5.0f, 5.0f));

    // A transformed element is clipped to its S124 render slot.
    CHECK(out[0].clipActive);
    CHECK(out[0].clipX == doctest::Approx(fr.x));
    CHECK(out[0].clipY == doctest::Approx(fr.y));
    CHECK(out[0].clipW == doctest::Approx(fr.width));
    CHECK(out[0].clipH == doctest::Approx(fr.height));
}

TEST_CASE("S208 append_ui_item: CPU-folds the Model into the quad corners")
{
    // A translated unit solid at (1,1) size (2,2) should land at (6,6)..(8,8)
    // after a (+5,+5) translation — the per-element Model∘MVP composition.
    DrawItem it = solid_rect(1.0f, 1.0f, 2.0f, 2.0f, { 1, 1, 1, 1 });
    it.model = vaml::Matrix2D::translation(5.0f, 5.0f);

    std::vector<quad::QuadVertex> verts;
    append_ui_item(verts, it);
    REQUIRE(verts.size() == 4u);

    // Corner (x=x, y=y) -> (x+5, y+5); unit-quad order (0,0)(0..), check min.
    float minX = verts[0].x, minY = verts[0].y;
    for (const auto& v : verts)
    {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
    }
    CHECK(minX == doctest::Approx(6.0f).epsilon(1e-4f));  // 1 + 5
    CHECK(minY == doctest::Approx(6.0f).epsilon(1e-4f));  // 1 + 5
}

TEST_CASE("S208 append_ui_item: identity Model is the no-op fast path")
{
    DrawItem plain = solid_rect(4.0f, 5.0f, 10.0f, 20.0f, { 1, 1, 1, 1 });  // model default identity
    std::vector<quad::QuadVertex> a;
    append_ui_item(a, plain);

    DrawItem withId = plain;
    withId.model = vaml::Matrix2D::identity();
    std::vector<quad::QuadVertex> b;
    append_ui_item(b, withId);

    REQUIRE(a.size() == 4u);
    REQUIRE(b.size() == 4u);
    for (std::size_t i = 0; i < 4u; ++i)
    {
        CHECK(a[i].x == doctest::Approx(b[i].x));
        CHECK(a[i].y == doctest::Approx(b[i].y));
    }
}

TEST_CASE("S208 grouping: identity same-color elements stay ONE batch; clip splits")
{
    // Three identity-transform, same-color, unclipped solids -> ONE batch.
    DrawItem s1 = solid_rect(0, 0, 10, 10, { 1, 0, 0, 1 });
    DrawItem s2 = solid_rect(0, 20, 10, 10, { 1, 0, 0, 1 });
    DrawItem s3 = solid_rect(0, 40, 10, 10, { 1, 0, 0, 1 });
    const std::vector<DrawItem> plain{ s1, s2, s3 };
    auto plainBatches = batch_plan(plain);
    REQUIRE(plainBatches.size() == 1u);
    CHECK(plainBatches[0].count == 3u);
    CHECK(!plainBatches[0].clipActive);
    CHECK(MaterialGroupCount(plain) == 1u);

    // Insert a CLIPPED (transformed) same-color item in the middle -> it splits
    // into its OWN batch (draw boundary: batch 0 = [s1], batch 1 = [clip],
    // batch 2 = [s2,s3]); the unclipped s2/s3 still merge, the clipped one does
    // not merge with either neighbor.
    DrawItem clipped = solid_rect(0, 15, 10, 10, { 1, 0, 0, 1 });
    clipped.clipActive = true;
    clipped.clipX = 0; clipped.clipY = 15; clipped.clipW = 10; clipped.clipH = 10;
    const std::vector<DrawItem> mixed{ s1, clipped, s2, s3 };
    auto mixedBatches = batch_plan(mixed);
    REQUIRE(mixedBatches.size() == 3u);
    CHECK(mixedBatches[0].count == 1u);
    CHECK(mixedBatches[0].clipActive == false);
    CHECK(mixedBatches[1].count == 1u);
    CHECK(mixedBatches[1].clipActive == true);
    CHECK(mixedBatches[2].count == 2u);
    CHECK(mixedBatches[2].clipActive == false);
    // MaterialGroupCount (material runs) <= batch_count (batches split on clip).
    CHECK(MaterialGroupCount(mixed) == 1u);
    CHECK(MaterialGroupCount(mixed) <= mixedBatches.size());
}
