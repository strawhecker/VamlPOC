// S208 + S823 (dep:S822) — UiSceneEncoder_transform_test: headless proof that the
// encoder HONORS Opacity + Clip + RenderTransform + RenderTransformOrigin when it
// emits the S206 draw list (the S208 headline, migrated onto the Transform
// OBJECT at S823). No Vulkan runtime needed.
//
// Under test:
//   (1) effective_opacity — the ancestor-chain PRODUCT (child 0.8 under a
//       0.5 parent -> 0.4; identity chain -> 1; zero ancestor -> 0).
//   (2) Opacity folded into item alpha — the encoder multiplies each emitted
//       item's alpha by its effective opacity.
//   (3) RenderTransform — a transformed child's emitted item carries the
//       RESOLVED Model (the authored Transform's matrix()); append_ui_item
//       CPU-folds it.
//   (4) RenderTransformOrigin — the origin folds the model about the
//       normalized-fraction center (T(−o)·M·T(+o)); identity -> no fold; zero
//       origin -> no origin terms.
//   (5) Clip (S833 conditional) — the render-slot clip is emitted ONLY when the
//       element's arrange reported needs_clip (the S832 NeedsClipBounds mirror).
//       A FITTING element is NOT clipped (clipActive stays false) even if it
//       carries a RenderTransform; a needs_clip element is scissored to the
//       slot-client window (clip_rect), NOT final_rect.
//   (6) NaN-matrix Transform — the DP accepts it (WPF-legal, no throw) and the
//       ENCODE boundary folds it defensively to identity (no crash, still
//       renders unclipped/identity) — the finite-validity gate is encode-only.
//   (7) Grouping — identity-transform same-color elements stay ONE batch; a
//       clipped element splits into its own batch.

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
#include <limits>
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

TEST_CASE("S208 effective_opacity: the compositor subtree model (ancestor-chain PRODUCT)")
{
    vaml::UIElement root;
    vaml::UIElement* mid = root.create_child<vaml::UIElement>();
    vaml::UIElement* leaf = mid->create_child<vaml::UIElement>();

    // Default: every opacity is its own 1.0 -> effective 1.
    CHECK(effective_opacity(*leaf) == doctest::Approx(1.0));
    CHECK(effective_opacity(root) == doctest::Approx(1.0));

    // Opacity is NOT inherited (PropertyFlags::None, S822): each opacity() reads
    // ONLY its own local. The subtREE alpha is the encoder's ancestor-chain
    // PRODUCT — effective_opacity(e) = Π opacity(x) over the chain incl. e.
    // A root at 0.5 renders the whole subtree at 0.5 (each descendant's factor
    // is 1.0, so 0.5 × 1.0 × 1.0 = 0.5), reached by COMPOSITION, not DP
    // inheritance.
    (void)root.set_opacity(0.5);
    CHECK(effective_opacity(root) == doctest::Approx(0.5).epsilon(1e-6));
    CHECK(effective_opacity(*mid) == doctest::Approx(0.5).epsilon(1e-6));
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.5).epsilon(1e-6));

    // A CHILD's own local opacity MULTIPLIES IN under its ancestors (WPF
    // `base.VisualOpacity` composition): 0.8 under a 0.5 parent -> 0.4. This is
    // the genuine WPF-observable result, now reached at encode (no double-count,
    // because no DP inheritance exists to conflate).
    (void)leaf->set_opacity(0.8);
    CHECK(effective_opacity(*mid) == doctest::Approx(0.5).epsilon(1e-6));
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.4).epsilon(1e-6));

    // A zero opacity ANCESTOR folds the whole chain to 0 (product); `leaf`
    // (which set NO local — its own opacity is still 1.0) reads 0.5 × 0.0 = 0.
    (void)mid->set_opacity(0.0);
    CHECK(effective_opacity(*mid) == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(effective_opacity(*leaf) == doctest::Approx(0.0).epsilon(1e-6));

    // NON-inheritance anti-claim: a DETACHED element (no ancestor) is UNAFFECTED
    // by a root's 0.5 — its opacity() reads ONLY its own default 1.0, so the
    // product is 1.0. A parent's Opacity does NOT cascade through the visual
    // edge.
    vaml::UIElement detached;
    CHECK(effective_opacity(detached) == doctest::Approx(1.0));
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

    // Root at 0.5 -> the ancestor-chain PRODUCT folds each child to 0.5
    // (each child's OWN opacity is 1.0; 0.5 × 1.0 = 0.5).
    (void)root.set_opacity(0.5);

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 2u);
    CHECK(out[0].a == doctest::Approx(0.5f).epsilon(1e-6f));
    CHECK(out[1].a == doctest::Approx(0.5f).epsilon(1e-6f));
}

TEST_CASE("S208/S823 encoder: RenderTransform — a translated Transform-object element carries the Model")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    assign_solid(*leaf, { 0, 0, 1, 1 });

    // Translate the leaf by (+5, +5) as a Transform OBJECT (S823).
    (void)leaf->set_render_transform(
        std::make_shared<vaml::TranslateTransform>(5.0f, 5.0f));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);

    // The item carries the resolved Model (the Transform's matrix), not identity.
    CHECK(out[0].model == vaml::Matrix2D::translation(5.0f, 5.0f));

    // S833: a RenderTransform is NOT a clip trigger. The leaf FITS its slot
    // (needs_clip false), so it stays UNCLIPPED even though it is transformed —
    // WPF GetLayoutClip returns null for a fitting element (UIElement.cs:3322).
    CHECK(!leaf->needs_clip());
    CHECK(!out[0].clipActive);
}

TEST_CASE("S823 encoder: RenderTransformOrigin folds the model about the fraction center")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(100.0f, 100.0f);
    (void)root.set_horizontal_alignment(vaml::HorizontalAlignment::Left);
    (void)root.set_vertical_alignment(vaml::VerticalAlignment::Top);
    root.Measure(vaml::Size{ 200.0f, 200.0f });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 200.0f, 200.0f });
    leaf->Measure(vaml::Size{ 200.0f, 200.0f });
    leaf->Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    const vaml::Rect fr = leaf->final_rect();
    (void)fr;
    assign_solid(*leaf, { 1, 0, 0, 1 });

    // A translate + origin (0.5,0.5): the model folds about the center.
    (void)leaf->set_render_transform(
        std::make_shared<vaml::TranslateTransform>(10.0f, 10.0f));
    (void)leaf->set_render_transform_origin(vaml::Point{ 0.5f, 0.5f });

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);

    // originPx = (100·0.5, 100·0.5) = (50,50). M' = T(−50)·T(10)·T(+50):
    // a pure translation that CANCELS the origin terms (translate is
    // translation-invariant about any center) -> still T(10).
    CHECK(out[0].model == vaml::Matrix2D::translation(10.0f, 10.0f));
    // S833: the leaf fits its 100x100 slot (needs_clip false) -> unclipped.
    CHECK(!leaf->needs_clip());
    CHECK(!out[0].clipActive);
}

TEST_CASE("S823 encoder: RenderTransformOrigin zero origin adds NO origin terms")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(10.0f, 10.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });
    assign_solid(*leaf, { 1, 0, 0, 1 });

    (void)leaf->set_render_transform(
        std::make_shared<vaml::TranslateTransform>(5.0f, 0.0f));
    // Explicit (0,0) origin (the WPF ArrangeCore "zero origin -> no origin terms").
    (void)leaf->set_render_transform_origin(vaml::Point{ 0.0f, 0.0f });

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].model == vaml::Matrix2D::translation(5.0f, 0.0f));
}

TEST_CASE("S833 encoder: a needs_clip element is scissored to the slot-client window")
{
    // A leaf whose unclipped desired (200x50) EXCEEDS its 50x50 slot -> arrange
    // reports needs_clip (the S832 child-wins-then-clip contract). The encoder
    // must emit a scissor to the WPF GetLayoutClip SLOT-CLIENT rect (50x50),
    // NOT to final_rect (200x50 — clipping that to itself would clip nothing).
    clear_visuals();
    SolidLeaf leaf(200.0f, 50.0f);
    leaf.Measure(vaml::Size{ 50.0f, 50.0f });
    leaf.Arrange(vaml::Rect{ 0.0f, 0.0f, 50.0f, 50.0f });

    CHECK(leaf.needs_clip());
    // final_rect is the oversized arranged box; clip_rect is the slot client.
    CHECK(leaf.final_rect().width == doctest::Approx(200.0f));
    CHECK(leaf.clip_rect().width  == doctest::Approx(50.0f));
    CHECK(leaf.clip_rect().height == doctest::Approx(50.0f));

    assign_solid(leaf, { 0, 1, 0, 1 });

    std::vector<DrawItem> out;
    encode_visual_tree(leaf, visual_of, out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].clipActive);
    // Clipped to the slot-client window, NOT the oversized final_rect.
    CHECK(out[0].clipX == doctest::Approx(leaf.clip_rect().x));
    CHECK(out[0].clipY == doctest::Approx(leaf.clip_rect().y));
    CHECK(out[0].clipW == doctest::Approx(50.0f));
    CHECK(out[0].clipH == doctest::Approx(50.0f));
}

TEST_CASE("S833 encoder: a fitting element produces NO clip (clipActive false)")
{
    // A leaf that fits its slot (default Stretch fills the 100x100 slot at its
    // tiny natural size) -> needs_clip false -> NO scissor (unchanged batching).
    clear_visuals();
    SolidLeaf leaf(10.0f, 10.0f);
    leaf.Measure(vaml::Size{ 100.0f, 100.0f });
    leaf.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    CHECK(!leaf.needs_clip());

    assign_solid(leaf, { 1, 0, 0, 1 });

    std::vector<DrawItem> out;
    encode_visual_tree(leaf, visual_of, out);
    REQUIRE(out.size() == 1u);
    CHECK(!out[0].clipActive);
}

TEST_CASE("S834 encoder: a fitting ClipToBounds element is STILL scissored to its slot-client window")
{
    // A leaf that FITS its 100x100 slot (natural 10x10, default Stretch fills):
    // needs_clip() is false (no overflow), but ClipToBounds=true opts into the
    // WPF forced-mode layout clip (FrameworkElement.GetLayoutClip gates on
    // `NeedsClipBounds || ClipToBounds`, FE.cs:4874). The encoder must emit an
    // ACTIVE scissor to the slot-client window even though nothing overflows —
    // the closure-audit case, distinct from the S832 conditional overflow mode.
    clear_visuals();
    SolidLeaf leaf(10.0f, 10.0f);
    (void)leaf.set_clip_to_bounds(true);
    leaf.Measure(vaml::Size{ 100.0f, 100.0f });
    leaf.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 100.0f });

    CHECK(!leaf.needs_clip());       // content fits — no overflow
    CHECK(leaf.clip_to_bounds());    // but the forced-mode opt-in is on

    assign_solid(leaf, { 0, 0, 1, 1 });

    std::vector<DrawItem> out;
    encode_visual_tree(leaf, visual_of, out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].clipActive);
    // Scissored to the slot-client window (100x100 here, margin 0).
    CHECK(out[0].clipW == doctest::Approx(100.0f));
    CHECK(out[0].clipH == doctest::Approx(100.0f));
}

TEST_CASE("S823 encoder: a NaN-matrix Transform is accepted at Set and folded to identity at encode")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(10.0f, 10.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });
    assign_solid(*leaf, { 1, 0, 0, 1 });

    // A NaN TRANSLATION Transform — WPF-legal at Set (no throw), accepted.
    auto bad = std::make_shared<vaml::MatrixTransform>(
        vaml::Matrix2D::translation(std::numeric_limits<float>::quiet_NaN(), 0.0f));
    (void)leaf->set_render_transform(bad);
    CHECK(!vaml::is_transform_valid(bad->matrix()));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);

    // The ENCODE-boundary finite-validity gate folds the non-finite matrix to
    // identity — no NaN/Inf geometry reaches the rasterizer, no crash, and the
    // element stays UNCLIPPED (identity → backward-compatible batching).
    CHECK(vaml::is_transform_identity(out[0].model));
    CHECK(!out[0].clipActive);
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
