// S511 (dep:S510) — the vaml-vulkan EFFECT render seam (headless VERTICAL-SLICE):
// the UiSceneEncoder emits a DropShadowEffect's offset translucent shadow quad
// BEHIND the element's own draw, and a BlurEffect passes through.
//
// WPF-faithful (rev 3, D0-REVISED user-accepted): UIElement.Effect is rendered to
// an OFFSCREEN surface and blitted back through a shader. The REAL offscreen
// separable-Gaussian blur/blit shader pass is the recorded NO-GAP forward token
// (see RENDER_SURFACE.md "Effects"); THIS headless slice proves the encoder emits
// the honest shadow quad (offset + tint at Effect.Opacity) BEFORE the element's
// own draw (z-order: shadow behind), and that a BlurEffect adds no draw.
//
// Pure + headless (Rail 1, ADR-021). doctest main from smoke_test.cpp.

#include <vaml/vulkan/ui/UiScene.h>        // DrawItem, Color4, solid_rect/border_rect/text_glyph
#include <vaml/vulkan/ui/UiSceneEncoder.h> // encode_visual_tree + append_effect_draws

#include <vaml/core/Element.h>   // StackPanel
#include <vaml/core/UIElement.h>
#include <vaml/core/Effect.h>    // DropShadowEffect / BlurEffect
#include <vaml/core/Geometry.h>  // Size, Rect

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;
using namespace vaml::vulkan;

using vaml::Color;
using vaml::drop_shadow_offset;
using vaml::DropShadowEffect;
using vaml::BlurEffect;

namespace {

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

void clear_visuals() { g_visuals.clear(); }

} // namespace

TEST_CASE("S511: a DropShadowEffect emits an offset translucent shadow BEHIND the element")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(20.0f, 20.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 50.0f });

    const Color4 fill{ 0.3f, 0.6f, 0.9f, 1.0f };
    assign_solid(*leaf, fill);

    // The element carries a DropShadowEffect: blur radius 5, depth 5, default
    // direction 315° (casts to the LOWER-RIGHT), opaque black at Opacity 1.0.
    auto ds = std::make_unique<vaml::DropShadowEffect>();
    (void)ds->set_blur_radius(5.0);
    (void)ds->set_shadow_depth(5.0);
    (void)ds->set_direction(315.0);
    (void)ds->set_opacity(1.0);
    (void)ds->set_color(vaml::Color{ 0, 0, 0, 255 });
    leaf->set_effect(std::move(ds));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);

    // The element's draw + its shadow = 2 items, the shadow FIRST (behind).
    REQUIRE(out.size() == 2u);
    const DrawItem& shadow = out[0];
    const DrawItem& element = out[1];

    // The shadow is a SOLID rect carrying the BLACK tint at alpha 255 (opaque).
    CHECK(shadow.kind == DrawItemKind::SolidRect);
    CHECK(shadow.r == doctest::Approx(0.0f));
    CHECK(shadow.g == doctest::Approx(0.0f));
    CHECK(shadow.b == doctest::Approx(0.0f));
    CHECK(shadow.a == doctest::Approx(1.0f));

    // The shadow is OFFSET by the WPF-faithful Direction=315 offset. The core
    // seam gives +x,+y (lower-right); at depth 5 that's the box shifted by the
    // sin/cos-derived ~3.536 in both axes.
    double dx = 0, dy = 0;
    drop_shadow_offset(315.0, 5.0, dx, dy);
    CHECK(shadow.x == doctest::Approx(leaf->final_rect().x + static_cast<float>(dx)));
    CHECK(shadow.y == doctest::Approx(leaf->final_rect().y + static_cast<float>(dy)));

    // The element's own draw came AFTER the shadow (z-order: element on top).
    CHECK(element.x == doctest::Approx(leaf->final_rect().x));
    CHECK(element.r == doctest::Approx(0.3f));
}

TEST_CASE("S511: the shadow alpha reflects Effect.Opacity")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(20.0f, 20.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 50.0f });
    assign_solid(*leaf, { 0.4f, 0.4f, 0.4f, 1.0f });

    auto ds = std::make_unique<vaml::DropShadowEffect>();
    (void)ds->set_color(vaml::Color{ 0, 0, 0, 255 });
    (void)ds->set_opacity(0.5);   // half-opaque shadow
    leaf->set_effect(std::move(ds));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 2u);
    // drop_shadow_color(tint, 0.5) -> alpha 128/255.
    CHECK(out[0].a == doctest::Approx(128.0f / 255.0f));
}

TEST_CASE("S511: a BlurEffect adds no draw (pass-through)")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(20.0f, 20.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 50.0f });
    assign_solid(*leaf, { 0.2f, 0.8f, 0.2f, 1.0f });

    auto bl = std::make_unique<vaml::BlurEffect>();
    (void)bl->set_radius(4.0);
    leaf->set_effect(std::move(bl));

    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    // No shadow; only the element's own draw (the real blur is the NO-GAP pass).
    REQUIRE(out.size() == 1u);
    CHECK(out[0].g == doctest::Approx(0.8f));
}

TEST_CASE("S511: no effect / a None visual emit no shadow")
{
    clear_visuals();
    vaml::StackPanel root;
    SolidLeaf* leaf = root.create_child<SolidLeaf>(20.0f, 20.0f);
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 50.0f });

    // No effect set.
    assign_solid(*leaf, { 0.1f, 0.2f, 0.3f, 1.0f });
    std::vector<DrawItem> out;
    encode_visual_tree(root, visual_of, out);
    REQUIRE(out.size() == 1u);  // just the element, no shadow
    CHECK(out[0].r == doctest::Approx(0.1f));

    // A DropShadowEffect on a NONE visual (base container leaf unassigned) adds
    // no shadow either.
    clear_visuals();
    vaml::StackPanel root2;
    SolidLeaf* invisible = root2.create_child<SolidLeaf>(10.0f, 10.0f);
    (void)invisible;
    auto ds = std::make_unique<vaml::DropShadowEffect>();
    invisible->set_effect(std::move(ds));

    std::vector<DrawItem> out2;
    encode_visual_tree(root2, visual_of, out2);
    CHECK(out2.empty());  // None visual -> no draw, no shadow
}
