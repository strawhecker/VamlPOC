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

MaterialKind classify_element(const vaml::UIElement& /*element*/) noexcept
{
    // The DEFAULT classifier: a base UIElement / a plain container contributes
    // no visual yet — no real control material exists until Text→S400 /
    // Button→S401 / templates→S364 replace this seam (kept-and-extended, never
    // silently backfilled). A host element that WANTS a draw supplies its own
    // ElementVisual via the encoder's `visualOf` callback (which may consult
    // control state); classify_element is the default for unhandled elements.
    return MaterialKind::None;
}

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
        const ElementVisual visual = visualOf(element);

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
        //   4. Append: the element contributes its draw (in tree/z-order).
        out.push_back(visual.item);
    });
}

} // namespace vaml::vulkan::ui
