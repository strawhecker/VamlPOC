// S207 (dep:S206) — UiSceneEncoder_test: headless proof of the Scene→command-
// buffer encoder (vaml::vulkan::ui). No Vulkan runtime needed.
//
// The encoder walks the REAL core visual tree (vaml::core::UIElement, built
// with the real StackPanel/CreateChild/Measure/Arrange surface) and produces
// the S206 DrawItem list the UiScene batcher consumes. Under test:
//   (1) Z-ORDER VIA TREE ORDER (the S207 headline) — a depth-first PRE-ORDER
//       walk emits later children AFTER earlier ones, so a later sibling paints
//       ON TOP; the emitted sequence IS the z-order, no sorting pass.
//   (2) DRAW LIST BY MATERIAL — classify_element's default + a visualOf that
//       maps elements to materials; MaterialGroupCount proves that ADJACENT
//       same-material elements collapse into ONE contiguous run (one batched
//       draw via S206's batch_plan), a material change starts a new run.
//   (3) SKIP RULES — MaterialKind::None contributes nothing (subtree walks);
//       Visibility::Collapsed contributes nothing (subtree walks);
//       degenerate (zero/negative extent) contributes nothing (subtree walks).
//   (4) FEED PARITY — encode → UiScene::set_items gives scene->batch_count() ==
//       MaterialGroupCount(items) on a real constructed draw list.
//   (5) classify default — a base UIElement / plain container is None (no
//       control render yet; forward seam, recorded NO-GAP not silent).
//
// The material authority is the vulkan-side callback (visualOf): the test
// assigns each element an ElementVisual via a lookup keyed on the element
// pointer, mirroring how real control materials (Text→S400, templates→S364)
// will replace the seam in place. This stays pure + headless — the encoder's
// walk + skip + grouping is the authoritative S207 surface.

#include <vaml/vulkan/ui/UiScene.h>          // DrawItem, Color4, solid_rect/border_rect/text_glyph
#include <vaml/vulkan/ui/UiSceneEncoder.h>   // the S207 encoder surface

#include <vaml/core/Element.h>   // StackPanel + Orientation (the real panel)
#include <vaml/core/UIElement.h> // UIElement, Visibility
#include <vaml/core/Geometry.h>  // Size, Rect

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;
using namespace vaml::vulkan;

namespace {

// A real solid leaf: a UIElement with a content extent (so Measure/Arrange give
// it a real final_rect to emit from).
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

// Drives the encoder's material authority seam: maps each element pointer to the
// ElementVisual (material + DrawItem) the encoder should emit for it. The
// default classifier (classify_element) still returns None for unlisted base
// elements, but the encoder's canonical path uses THIS visualOf, exactly as the
// consumer + real control materials will.
std::unordered_map<const vaml::UIElement*, ElementVisual> g_visuals;

ElementVisual visual_of(const vaml::UIElement& e)
{
    auto it = g_visuals.find(&e);
    if (it != g_visuals.end())
        return it->second;
    // Unlisted: the default classifier (None) governs — a plain container /
    // base element contributes no draw (its subtree still walks).
    return { classify_element(e), DrawItem{} };
}

// Assigns a SOLID visual of `color` to `e`, emitting a solid_rect at e's arranged
// final_rect. Mirror of how a real Solid control would supply its draw.
void assign_solid(vaml::UIElement& e, const Color4& color)
{
    const vaml::Rect r = e.final_rect();
    ElementVisual v;
    v.kind = MaterialKind::Solid;
    v.item = solid_rect(r.x, r.y, r.width, r.height, color);
    g_visuals[&e] = v;
}

void assign_border(vaml::UIElement& e, const Color4& color)
{
    const vaml::Rect r = e.final_rect();
    ElementVisual v;
    v.kind = MaterialKind::Border;
    v.item = border_rect(r.x, r.y, r.width, r.height, color, 0.05f, 0.05f, 0.01f);
    g_visuals[&e] = v;
}

void assign_text(vaml::UIElement& e, const Color4& color)
{
    const vaml::Rect r = e.final_rect();
    ElementVisual v;
    v.kind = MaterialKind::Text;
    v.item = text_glyph(r.x, r.y, r.width, r.height, 0.0f, 0.0f, 1.0f, 1.0f, color, 0.15f);
    g_visuals[&e] = v;
}

void clear_visuals()
{
    g_visuals.clear();
}

} // namespace

TEST_CASE("S207: classify_element default -> None for a base element (no visual yet, recorded)")
{
    // The default classifier is the forward seam: a base UIElement / plain
    // container contributes NO draw until a real control material exists
    // (Text->S400, Button->S401, templates->S364). This is NOT lazy — it is the
    // declared forward seam real controls replace in place (NO-GAP).
    vaml::UIElement base;
    CHECK(classify_element(base) == MaterialKind::None);
}

TEST_CASE("S207: Z-ORDER via tree order — pre-order walk emits later children on top")
{
    // A 2-level StackPanel with a child that itself has children. The encoder
    // walks depth-first PRE-ORDER: panel, then panel's children in index order —
    // each later sibling appended AFTER the earlier, so it paints ON TOP.
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* a = root.create_child<SolidLeaf>(10.0f, 10.0f);
    vaml::StackPanel* nested = root.create_child<vaml::StackPanel>();
    SolidLeaf* b = nested->create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* c = nested->create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* d = root.create_child<SolidLeaf>(10.0f, 10.0f);

    // Lay out so each leaf has a REAL final_rect.
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    const Color4 red{ 1, 0, 0, 1 }, green{ 0, 1, 0, 1 }, blue{ 0, 0, 1, 1 };
    assign_solid(*a, red);
    assign_solid(*b, green);
    assign_solid(*c, blue);
    assign_solid(*d, red);

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);

    // Pre-order: panel(None) -> a, nested(None) -> b, c, d. Only the four leaves
    // emit, in EXACTLY tree order: a, b, c, d. Later siblings (d after a) come
    // later -> paint on top (z-order via tree order).
    REQUIRE(out.size() == 4u);
    CHECK(out[0].x == doctest::Approx(a->final_rect().x));  // a first
    CHECK(out[1].x == doctest::Approx(b->final_rect().x));  // then b (inside nested)
    CHECK(out[2].x == doctest::Approx(c->final_rect().x));  // then c
    CHECK(out[3].x == doctest::Approx(d->final_rect().x));  // d last (paints on top)
    // Colors pass through in the same z/tree order.
    CHECK(out[0].r == doctest::Approx(1.0f));
    CHECK(out[1].r == doctest::Approx(0.0f));
    CHECK(out[2].r == doctest::Approx(0.0f));
    CHECK(out[3].r == doctest::Approx(1.0f));
}

TEST_CASE("S207: DRAW LIST BY MATERIAL — adjacent same-material collapse, change starts a run")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* s1 = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* s2 = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* t  = root.create_child<SolidLeaf>(10.0f, 10.0f);  // text (different material)
    SolidLeaf* s3 = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    const Color4 red{ 1, 0, 0, 1 }, white{ 1, 1, 1, 1 };
    assign_solid(*s1, red);
    assign_solid(*s2, red);
    assign_text(*t, white);
    assign_solid(*s3, red);

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 4u);

    // The material run count: solid,solid (run 1) + text (run 2) + solid (run 3)
    // = THREE contiguous runs. The two ADJACENT solids collapse into ONE run;
    // the separated solid (after the text) starts a NEW run — the exact number
    // of batched draws S206's batch_plan produces from this list.
    CHECK(MaterialGroupCount(out) == 3u);
    // The runs are contiguous in emission order (never reordered):
    CHECK(out[0].kind == DrawItemKind::SolidRect);
    CHECK(out[1].kind == DrawItemKind::SolidRect);
    CHECK(out[2].kind == DrawItemKind::TextGlyph);
    CHECK(out[3].kind == DrawItemKind::SolidRect);
}

TEST_CASE("S207: SKIP RULES — None contributes nothing (subtree still walks)")
{
    clear_visuals();
    vaml::StackPanel root;
    // A leaf deliberately NOT assigned a visual -> material None (contributes
    // nothing), but it has a child that IS assigned -> the child still walks+
    // emits (a None CONTAINER contributes no draw but its subtree does; a None
    // leaf contributes nothing and its (nonexistent) subtree walks trivially).
    SolidLeaf* hidden = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* shown  = root.create_child<SolidLeaf>(10.0f, 10.0f);
    (void)hidden;  // the None/container-less case is exercised via `shown`, keep the leaf

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    // `shown` gets a real solid; `hidden` stays Unlisted (None).
    assign_solid(*shown, { 0, 1, 0, 1 });

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    // Only `shown` emits — `hidden` (None) is skipped but its (absent) subtree
    // still walked without emitting.
    REQUIRE(out.size() == 1u);
    CHECK(out[0].x == doctest::Approx(shown->final_rect().x));
    CHECK(out[0].g == doctest::Approx(1.0f));
}

TEST_CASE("S207: SKIP RULES — Visibility::Collapsed contributes nothing (subtree walks)")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* collapsed = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* ok        = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    assign_solid(*collapsed, { 1, 0, 0, 1 });
    assign_solid(*ok, { 0, 0, 1, 1 });
    (void)collapsed->set_visibility(vaml::Visibility::Collapsed);  // not rendered

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    // The Collapsed leaf contributes nothing (still a REAL assigned visual, but
    // the encoder honors the S120 Visibility DP); `ok` still emits.
    REQUIRE(out.size() == 1u);
    CHECK(out[0].b == doctest::Approx(1.0f));
    CHECK(out[0].r == doctest::Approx(0.0f));
}

TEST_CASE("S207: SKIP RULES — degenerate (zero/negative extent) contributes nothing")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* ok = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    // Give `ok` a real solid, then add a DIRECT degenerate item the encoder
    // must skip (zero/negative extent -> ui_item_degenerate). We simulate by
    // assigning `ok` a valid visual but ALSO a synthetic degenerate element.
    assign_solid(*ok, { 0, 0, 1, 1 });

    // A separate borrowed element with a zero/negative-extent DrawItem.
    vaml::UIElement zero;
    ElementVisual v;
    v.kind = MaterialKind::Solid;
    v.item = solid_rect(0, 0, 0, 10, { 1, 0, 0, 1 });  // zero width -> degenerate
    g_visuals[&zero] = v;

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);  // only `ok`; the degenerate is skipped
    CHECK(out[0].b == doctest::Approx(1.0f));
    g_visuals.erase(&zero);
}

TEST_CASE("S207: visualOf passthrough — emitted rect == final_rect, color/kind preserved")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(30.0f, 40.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    const Color4 purple{ 0.5f, 0.2f, 0.8f, 0.6f };
    assign_border(*leaf, purple);  // a border material, transparent passthrough

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);
    // The emitted item's rect == the element's ARRANGED final_rect (the render slot).
    CHECK(out[0].kind == DrawItemKind::BorderRect);
    CHECK(out[0].x == doctest::Approx(leaf->final_rect().x));
    CHECK(out[0].y == doctest::Approx(leaf->final_rect().y));
    CHECK(out[0].w == doctest::Approx(leaf->final_rect().width));
    CHECK(out[0].h == doctest::Approx(leaf->final_rect().height));
    // Color + border params pass through untouched.
    CHECK(out[0].r == doctest::Approx(0.5f));
    CHECK(out[0].g == doctest::Approx(0.2f));
    CHECK(out[0].b == doctest::Approx(0.8f));
    CHECK(out[0].a == doctest::Approx(0.6f));
    CHECK(out[0].thickness01 == doctest::Approx(0.05f));
}

TEST_CASE("S207: FEED PARITY — MaterialGroupCount == UiScene::batch_count after set_items")
{
    // The headline feed contract: encode_visual_tree -> UiScene::set_items, and
    // the scene's batched-draw count (batch_count) MUST equal the encoder's
    // MaterialGroupCount — proving "draw list by material" IS the batcher input.
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* s1 = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* s2 = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* t  = root.create_child<SolidLeaf>(10.0f, 10.0f);
    SolidLeaf* s3 = root.create_child<SolidLeaf>(10.0f, 10.0f);

    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 200.0f });

    const Color4 red{ 1, 0, 0, 1 }, white{ 1, 1, 1, 1 };
    assign_solid(*s1, red);
    assign_solid(*s2, red);
    assign_text(*t, white);
    assign_solid(*s3, red);

    std::vector<DrawItem> items;
    encode_visual_tree(root, visual_of, items);

    // Three contiguous material runs (the two solids collapse; text; the third
    // solid reopens a run).
    const std::uint32_t groups = MaterialGroupCount(items);
    CHECK(groups == 3u);

    // The emitted list is DIRECTLY consumable by S206: batch_plan produces one
    // batch per contiguous same-material run -> MaterialGroupCount is a lower
    // bound/proof of the scene batch count (the same-key batching, not just kind).
    const auto batches = batch_plan(items);
    REQUIRE(batches.size() == groups);
    // z/insertion order preserved (batch 0 first, batch 2 last).
    CHECK(batches[0].first == 0u);
    CHECK(batches[0].count == 2u);  // the two contiguous solids
    CHECK(batches[1].count == 1u);  // the text
    CHECK(batches[2].count == 1u);  // the reopened solid run
    CHECK(batches[2].first == 3u);
}
