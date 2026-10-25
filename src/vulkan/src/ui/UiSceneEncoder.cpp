// S207 — Scene→command-buffer encoder implementation: walks the REAL core
// visual tree and produces the S206 DrawItem list (draw list by material,
// z-order via tree order). See UiSceneEncoder.h for the split + the feed
// contract + NO-GAP + LIFETIMES.
//
// The encoder is pure glue over (a) the REAL core visual tree (UIElement +
// children() + walk(), the S121 surface; Visibility, the S120 DP) and (b) the
// S206 pure decision seams (DrawItem + ui_item_degenerate + the rect/glyph
// constructors). It emits z-ordered, material-grouped items in exactly the
// order UiScene::set_items + batch_plan consume.

#include <vaml/vulkan/ui/UiSceneEncoder.h>

namespace vaml::vulkan::ui {

MaterialKind classify_element(const vaml::UIElement& element) noexcept
{
    // The material authority for the S207 "draw list by material" grouping. The
    // S400 real per-control material is authored HERE: a vaml::Text classifies
    // to MaterialKind::Text (the SDF glyph-quad material), so the encoder emits
    // its glyph quads. Every non-Text base UIElement / plain container
    // contributes NO material (None) — real controls replace this seam in place
    // (Button->S401, templates->S364, etc.), kept-and-extended, never silently
    // backfilled. A host element that WANTS a draw supplies its own ElementVisual
    // via the encoder's `visualOf` callback; classify_element is the default for
    // unhandled elements.
    if (dynamic_cast<const vaml::Text*>(&element) != nullptr)
    {
        return MaterialKind::Text;
    }
    // S400_1: a real Icon control classifies to the MONOCHROME material (the
    // S400_1 TintMode default — Monochrome tint; the actual shader path is
    // chosen by the DP + produced by IconGlyphPlan's render seam, and the full
    // multicolor/Preserve path lands with the real atlas at S409_1). The S207
    // material authority is real here (a control contributes a draw material).
    if (dynamic_cast<const vaml::Icon*>(&element) != nullptr)
    {
        return MaterialKind::Monochrome;
    }
    // S401: a real Button control classifies to the SOLID material (the S401
    // default Button material — the default fill quad, whose Background brush
    // color the S364 visual states swap for Hover/Pressed/Disabled). The S207
    // material authority is real (a Button contributes its default draw).
    if (dynamic_cast<const vaml::Button*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    // S402: a real TextBox control classifies to the SOLID material (the S402
    // default edit-field fill box; the typed text glyphs are the S409_1 viewer
    // path, recorded NO-GAP not silent).
    if (dynamic_cast<const vaml::TextBox*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    // S403/S404/S405: the real Slider / CheckBox / ScrollViewer controls all
    // classify to the SOLID material (the S403 track, S404 check box, S405
    // viewport-clip default draw boxes — the S207 material authority is real).
    if (dynamic_cast<const vaml::Slider*>(&element) != nullptr ||
        dynamic_cast<const vaml::CheckBox*>(&element) != nullptr ||
        dynamic_cast<const vaml::ScrollViewer*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    return MaterialKind::None;
}

// S208 effective opacity — the S208 Opacity seam, honored at the renderer. The
// element's RESOLVED opacity() IS the on-render alpha: Opacity is registered
// with PropertyFlags::Inherits (S120), so the S040/S041 inheritance cascade
// ALREADY propagates an ancestor's opacity into a descendant that does NOT set
// a local override (a parent at 0.5 fades its whole subtree to 0.5 via the
// legal `local ?? inherited ?? default` resolution). Multiplying a chain of
// opacity() values here would DOUBLE-COUNT the inherited cascade (an ancestor's
// value appears both as its own factor AND as the inherited value inside each
// descendant's opacity()) — so effective_opacity delegates to the resolved DP
// value, the single honest number the renderer multiplies into the item alpha.
// A child's OWN local set shadows the inherited ancestor value (0.8 under a 0.5
// parent renders at 0.8 — local-wins inheritance semantics, WPF-faithful). Pure
// + headless-testable; the encoder folds the result into the item alpha.
double effective_opacity(const vaml::UIElement& element) noexcept
{
    return element.opacity();
}

namespace {
// S208: marks an emitted DrawItem with the element's render state — the per-
// element Model (RenderTransform) + the Clip. The MODEL is always folded in
// (identity is the no-op fast path, so unchanged batching). The CLIP is marked
// active ONLY when a non-identity Model could push content outside the slot
// (a rotated/scaled/translated element overflowing its S124 render slot is
// culled by a dynamic scissor to final_rect). An identity-transform element
// already fits its final_rect exactly, so clipActive stays false — the
// existing S206/S207 batching is preserved (backward compatible).
void apply_element_render_state(const vaml::UIElement& element, DrawItem& item) noexcept
{
    item.model = element.render_transform();
    const bool transformed = !is_transform_identity(item.model);
    if (transformed)
    {
        // Clip the transformed content to the S124 render slot (final_rect):
        // a rotated/scaled quad that overflows its slot is exactly culled here.
        const vaml::Rect fr = element.final_rect();
        if (fr.width > 0.0f && fr.height > 0.0f)
        {
            item.clipActive = true;
            item.clipX = fr.x;
            item.clipY = fr.y;
            item.clipW = fr.width;
            item.clipH = fr.height;
        }
    }
}
} // namespace

DrawItemKind material_to_draw_kind(MaterialKind kind) noexcept
{
    switch (kind)
    {
    case MaterialKind::Border:     return DrawItemKind::BorderRect;
    case MaterialKind::Text:       return DrawItemKind::TextGlyph;
    case MaterialKind::Monochrome: // a mask quad; the monochrome-tint fragment consumes it
    case MaterialKind::Solid:
    case MaterialKind::None:
    default:                       return DrawItemKind::SolidRect;
    }
}

std::uint32_t MaterialGroupCount(std::span<const DrawItem> items) noexcept
{
    // S208 note: this counts CONTIGUOUS same-MATERIAL runs (the S207 "draw list
    // by material" proof). batch_plan additionally splits a same-material run on
    // a Clip boundary (a batch sets ONE scissor), so for a clipped list
    // MaterialGroupCount <= batch_count. The S208 grouping tests assert this
    // relationship directly (the scene batch count is the authoritative draw
    // count; MaterialGroupCount is the material-run lower bound).
    std::uint32_t groups = 0u;
    DrawItemKind last = DrawItemKind::SolidRect;
    bool haveLast = false;
    for (const DrawItem& item : items)
    {
        if (ui_item_degenerate(item))
            continue;  // skipped (mirror batch_plan) — carries no material run
        const DrawItemKind k = item.kind;
        if (!haveLast || k != last)
        {
            ++groups;  // a NEW contiguous same-material run
            last = k;
            haveLast = true;
        }
        // else: same kind as the previous run -> the run continues (no new group)
    }
    return groups;
}

const std::vector<VisualWalkStep>& visual_walk_steps() noexcept
{
    static const std::vector<VisualWalkStep> plan = {
        VisualWalkStep::Classify,
        VisualWalkStep::SkipNone,
        VisualWalkStep::SkipCollapsed,
        VisualWalkStep::SkipDegenerate,
        VisualWalkStep::Append,
    };
    return plan;
}

void encode_visual_tree(const vaml::UIElement& root,
                        const std::function<ElementVisual(const vaml::UIElement&)>& visualOf,
                        std::vector<DrawItem>& out)
{
    // Depth-first PRE-ORDER walk over the REAL core visual tree: UIElement::walk
    // visits (this, then children in index/visual order), which IS the z-order
    // (tree order) — the S121 traversal is the single source of it. Honoring
    // the skip rules independently per element: a None/Collapsed/degenerate
    // element still contributes no draw but its SUBTREE still walks.
    root.walk([&](vaml::UIElement& element) {
        // Classify -> ElementVisual via the caller's `visualOf` callback (the
        // material authority seam; real controls replace it at S400/S364).
        ElementVisual visual = visualOf(element);

        // Skip rules (visual_walk_steps order):
        //   1. SkipNone: None material -> contribute nothing (subtree still walks)
        if (visual.kind == MaterialKind::None)
            return;
        //   2. SkipCollapsed: Visibility::Collapsed -> not rendered (subtree walks)
        if (element.visibility() == vaml::Visibility::Collapsed)
            return;
        //   3. SkipDegenerate: zero/negative extent -> no draw (subtree walks)
        if (ui_item_degenerate(visual.item))
            return;

        // S208 Opacity: fold the element's EFFECTIVE opacity (the ancestor-chain
        // product) into the item alpha. A zero-opacity element keeps alpha 0
        // (no visible draw) but its subtree still walked above. The folded alpha
        // feeds ui_batch_key's color, so distinct alphas -> distinct batches.
        visual.item.a *= static_cast<float>(effective_opacity(element));

        // S208 RenderTransform + Clip: fold the per-element Model into the item
        // and (for a transformed element) mark the clip to the S124 render slot.
        apply_element_render_state(element, visual.item);

        //   4. Append: the element contributes its draw (in tree/z-order).
        out.push_back(visual.item);
    });
}

} // namespace vaml::vulkan::ui
