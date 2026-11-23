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
    // S400 real per-control material is authored HERE: a vaml::TextBlock (11k-1
    // re-binds the S087 `Text` name) classifies to MaterialKind::Text (the SDF
    // glyph-quad material), so the encoder emits its glyph quads. Every
    // non-text-leaf base UIElement / plain container contributes NO material
    // (None) — real controls replace this seam in place (Button->S401,
    // templates->S364, etc.), kept-and-extended, never silently backfilled. A
    // host element that WANTS a draw supplies its own ElementVisual via the
    // encoder's `visualOf` callback; classify_element is the default for
    // unhandled elements.
    if (dynamic_cast<const vaml::TextBlock*>(&element) != nullptr)
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
    // Sprint 11c (S470–S473): the DockPanel layout container + the selection
    // controls all classify to the SOLID material (the S470 DockPanel fill, the
    // S471 RadioButton check-glyph, the S472 ComboBox field, the S473 ListBox/
    // ListView fill). The S207 material authority is real — each contributes its
    // default draw.
    if (dynamic_cast<const vaml::DockPanel*>(&element) != nullptr ||
        dynamic_cast<const vaml::RadioButton*>(&element) != nullptr ||
        dynamic_cast<const vaml::ComboBox*>(&element) != nullptr ||
        dynamic_cast<const vaml::ListBox*>(&element) != nullptr ||
        dynamic_cast<const vaml::ListView*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    // Sprint 11d (S480–S486) — the core display + input controls.
    //  * S481 Border is the FIRST production user of the dormant BORDER
    //    material: it classifies to MaterialKind::Border (the S205 border_rect
    //    pipeline), closing the dormant seam at its own step (NO-GAP). The
    //    full stroke rectangle (thickness/radius) is emitted by
    //    classify_merge_element at the presentation boundary.
    if (dynamic_cast<const vaml::Border*>(&element) != nullptr)
    {
        return MaterialKind::Border;
    }
    //  * S483 Image classifies to MONOCHROME (the S400_1 icon asset pipeline:
    //    mirrors the Icon — the headless default emits the fallback glyph via
    //    IconGlyphPlan/IconBestFit; the real atlas quad is the S409_1-style
    //    render path, recorded NO-GAP not silent).
    if (dynamic_cast<const vaml::Image*>(&element) != nullptr)
    {
        return MaterialKind::Monochrome;
    }
    //  * S480 Label (the content box; glyphs defactor to the S400 Text path),
    //    S482 Rectangle/Ellipse/Line (the fill/stroke solid boxes), S484
    //    ProgressBar (the track + determinate fill), S485 PasswordBox (the
    //    masked edit-field fill), GroupBox/Expander (the bordered container),
    //    Separator (the thin rule), StatusBar (the bar host) all classify to
    //    SOLID.
    if (dynamic_cast<const vaml::Label*>(&element) != nullptr ||
        dynamic_cast<const vaml::Rectangle*>(&element) != nullptr ||
        dynamic_cast<const vaml::Ellipse*>(&element) != nullptr ||
        dynamic_cast<const vaml::Line*>(&element) != nullptr ||
        dynamic_cast<const vaml::ProgressBar*>(&element) != nullptr ||
        dynamic_cast<const vaml::PasswordBox*>(&element) != nullptr ||
        dynamic_cast<const vaml::GroupBox*>(&element) != nullptr ||
        dynamic_cast<const vaml::Expander*>(&element) != nullptr ||
        dynamic_cast<const vaml::Separator*>(&element) != nullptr ||
        dynamic_cast<const vaml::StatusBar*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    // 11k-2 — the missing WPF controls the corpus consumes. RepeatButton /
    // ScrollBar / ToolBar / ToolBarTray are REAL re-based UIElements that
    // classify to the SOLID material (the repeat spin-button slot, the
    // scrollbar track + thumb, the toolbar strip, the toolbar-tray host).
    // Viewbox classifies to NONE — "Viewbox -> its content" (the scale-contain
    // single-child decorator renders its CONTENT CHILD's own material, NOT a
    // box of its own). The Viewbox arm is placed here so the content child
    // carries the material; the Viewbox itself contributes no draw.
    if (dynamic_cast<const vaml::RepeatButton*>(&element) != nullptr ||
        dynamic_cast<const vaml::ScrollBar*>(&element) != nullptr ||
        dynamic_cast<const vaml::ToolBar*>(&element) != nullptr ||
        dynamic_cast<const vaml::ToolBarTray*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    if (dynamic_cast<const vaml::Viewbox*>(&element) != nullptr)
    {
        return MaterialKind::None;
    }
    // Sprint 11e (S490–S495): the container + menu controls. TabControl /
    // TabItem / Menu / MenuItem / ContextMenu / ToolTip / Popup / GridSplitter
    // all classify to the SOLID material (the S490 tab strip + content chrome,
    // the S491 menu bar / item rows / popup panel, the S492 tooltip/popup panel,
    // the S493 splitter bar). DataGrid classifies to the SOLID material (the
    // S494 list/grid fill — header + row cells emitted at the presentation
    // boundary).
    if (dynamic_cast<const vaml::TabControl*>(&element) != nullptr ||
        dynamic_cast<const vaml::TabItem*>(&element) != nullptr ||
        dynamic_cast<const vaml::Menu*>(&element) != nullptr ||
        dynamic_cast<const vaml::MenuItem*>(&element) != nullptr ||
        dynamic_cast<const vaml::ContextMenu*>(&element) != nullptr ||
        dynamic_cast<const vaml::ToolTip*>(&element) != nullptr ||
        dynamic_cast<const vaml::Popup*>(&element) != nullptr ||
        dynamic_cast<const vaml::GridSplitter*>(&element) != nullptr ||
        dynamic_cast<const vaml::DataGrid*>(&element) != nullptr)
    {
        return MaterialKind::Solid;
    }
    return MaterialKind::None;
}

// S208 effective opacity — the S208 Opacity seam, honored at the renderer. The
// WPF compositor-subtree model (S822 dep:S821): WPF reaches a subtree's alpha by
// COMPOSITION — `base.VisualOpacity` (UIElement.cs:2689-2691) multiplies down
// the visual-ancestor chain, so a child under an Opacity=0.5 panel renders at
// 0.5 × its own opacity. Opacity is registered with PropertyFlags::None (NOT
// inherited), so each element's opacity() reads ONLY its OWN local (default
// 1.0); there is therefore no DP-inheritance cascade to double-count, and the
// genuine WPF-observable subtree result is the render-tree ancestor-chain
// PRODUCT at encode time: effective_opacity(e) = Π opacity(x) over the visual-
// ancestor chain INCLUDING e. This is exactly the number the renderer
// multiplies into the item alpha. Pure + headless-testable.
double effective_opacity(const vaml::UIElement& element) noexcept
{
    double product = 1.0;
    for (const vaml::DependencyObject* cur = &element; cur != nullptr; cur = cur->parent())
    {
        const auto* ue = dynamic_cast<const vaml::UIElement*>(cur);
        if (ue == nullptr)
        {
            break;  // a non-UIElement ancestor cannot participate in the tree
        }
        product *= ue->opacity();
    }
    return product;
}

namespace {
// S208/S823/S833: marks an emitted DrawItem with the element's render state —
// the per-element Model (the RESOLVED RenderTransform) + the Clip.
//
// S833 (dep:S832) — CONDITIONAL render-slot clip. The clip is emitted ONLY when
// the element's arrange reported `needs_clip()` (the S832 NeedsClipBounds
// mirror). A FITTING element is NOT clipped — `clipActive` stays false, so the
// existing S206/S207 batching is preserved (unchanged). WPF ground truth: the
// layout clip is triggered solely by `NeedsClipBounds || ClipToBounds`
// (FrameworkElement.GetLayoutClip, FE.cs:4862-4872); a RenderTransform is NOT a
// trigger (it never sets NeedsClipBounds) and GetLayoutClip returns `null` for a
// fitting element (base UIElement.GetLayoutClip, UIElement.cs:3322-3333). The
// clip RECT is the WPF GetLayoutClip rectangular result — the SLOT-CLIENT window
// `{slot.x+ml, slot.y+mt, inset, inset}` (clip_rect()), NOT final_rect() (which
// may EXCEED the slot; clipping it to itself would be a no-op for the overflow
// case — exactly the bug S833 closes). S834 (RESOLVED) OR's the real
// `ClipToBounds` Bool DP into the trigger — the gate is now
// `needs_clip() || clip_to_bounds()` (WPF `NeedsClipBounds || ClipToBounds`);
// the math (which rect) is unchanged. A `ClipToBounds="true"` element whose
// content FITS is therefore STILL clipped to its slot-client window (the
// forced-mode case, FE.cs:4874/4917-4923).
//
// S823 — RenderTransformOrigin (Point, default (0,0), NOT inherited): the
// transform operates about the normalized-fraction center. The WPF ArrangeCore
// fold (UIElement.cs:1253-1296) is `T(−originPx) · M · T(+originPx)` with
// `originPx = (fr.width·origin.x, fr.height·origin.y)`, plus two runtime guards:
// identity transform -> NO fold at all; zero origin -> NO origin terms. This is
// the ENCODE-boundary finite-validity gate too: a non-finite resolved matrix is
// folded defensively (is_transform_valid false -> treated as identity so NaN/Inf
// geometry can never crash the vertex/build path) — the finite-validity gate
// VAML keeps stays HERE, never at the DP Set (WPF accepts any Transform).
void apply_element_render_state(const vaml::UIElement& element, DrawItem& item) noexcept
{
    // Resolve the Transform object's matrix (default identity for null/shared).
    vaml::Matrix2D model = vaml::Matrix2D::identity();
    const std::shared_ptr<vaml::Transform> tf = element.render_transform();
    if (tf)
    {
        model = tf->matrix();
    }

    // S823 ENCODE-BOUNDARY finite-validity gate (RE-SCOPED here, NOT the DP Set):
    // a non-finite resolved matrix is WPF-legal at Set but must never reach the
    // rasterizer — fall back to identity (defensive fold, never a crash).
    if (!is_transform_valid(model))
    {
        model = vaml::Matrix2D::identity();
    }

    // S823 RenderTransformOrigin — WPF ArrangeCore fold (UIElement.cs:1253-1296):
    // identity -> NO fold; zero origin -> NO origin terms. Otherwise
    // M' = T(−originPx)·M·T(+originPx), originPx = (fr.width·ox, fr.height·oy).
    const vaml::Point origin = element.render_transform_origin();
    if (!is_transform_identity(model))
    {
        const vaml::Rect fr = element.final_rect();
        if (origin.x != 0.0f || origin.y != 0.0f)
        {
            const float ox = fr.width * origin.x;
            const float oy = fr.height * origin.y;
            // T(−o)·M·T(+o): row-vector/post-multiply — translate to origin, apply
            // M, translate back (WPF ArrangeCore's exact composition order).
            model = vaml::multiply(vaml::Matrix2D::translation(-ox, -oy),
                                   vaml::multiply(model, vaml::Matrix2D::translation(ox, oy)));
        }
    }

    item.model = model;

    // S833/S834 — the CONDITIONAL render-slot clip. Emit a scissor ONLY when the
    // arrange reported NeedsClipBounds (S832) OR the element opted into the
    // forced-mode ClipToBounds (S834) — WPF's `NeedsClipBounds || ClipToBounds`
    // (FE.cs:4874). A fitting element with ClipToBounds false leaves clipActive
    // false (WPF GetLayoutClip returns null -> unchanged batching). The scissor
    // target is the WPF GetLayoutClip rectangular result — the slot-client
    // window (clip_rect()), NOT final_rect() (final_rect may exceed the slot, so
    // clipping it to itself would clip nothing).
    if (element.needs_clip() || element.clip_to_bounds())
    {
        const vaml::Rect cr = element.clip_rect();
        if (cr.width > 0.0f && cr.height > 0.0f)
        {
            item.clipActive = true;
            item.clipX = cr.x;
            item.clipY = cr.y;
            item.clipW = cr.width;
            item.clipH = cr.height;
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

void append_effect_draws(const vaml::Effect* effect,
                         const vaml::UIElement& element,
                         const ElementVisual& visual,
                         std::vector<DrawItem>& out) noexcept
{
    if (effect == nullptr)
        return;
    // Only DropShadowEffect contributes a shadow draw here; BlurEffect passes
    // through (its offscreen blur is the NO-GAP forward token).
    const auto* ds = dynamic_cast<const vaml::DropShadowEffect*>(effect);
    if (ds == nullptr || visual.kind == MaterialKind::None)
        return;

    const vaml::Rect r = element.final_rect();
    if (r.width <= 0.0f || r.height <= 0.0f)
        return;

    // The WPF-faithful offset + tint (core seams).
    double dx = 0.0, dy = 0.0;
    drop_shadow_offset(ds->direction(), ds->shadow_depth(), dx, dy);
    const vaml::Color tint = drop_shadow_color(ds->color(), ds->opacity());
    const float a = static_cast<float>(tint.a) / 255.0f;
    // The shadow quad: the element's box OFFSET by the shadow offset, drawn
    // BEHIND (appended before the element's own quad — z-order via tree order).
    DrawItem shadow = solid_rect(
        r.x + static_cast<float>(dx), r.y + static_cast<float>(dy), r.width, r.height,
        Color4{ static_cast<float>(tint.r) / 255.0f,
                static_cast<float>(tint.g) / 255.0f,
                static_cast<float>(tint.b) / 255.0f,
                a });
    apply_element_render_state(element, shadow);
    out.push_back(shadow);
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

        // S409_1 — MULTI-GLYPH text: a TextBlock whose visual carries a non-empty
        // glyph list renders EVERY glyph quad (N glyphs = N DrawItems), replacing
        // the single `item`. The degenerate-skip rule then applies PER GLYPH
        // (a degenerate glyph is skipped; non-degenerate ones still draw). When
        // the glyph list is empty we fall through to the single-item contract.
        const bool multiGlyph = !visual.glyphs.empty();
        if (multiGlyph)
        {
            // S208 Opacity: fold the element's effective opacity into EVERY glyph
            // (a zero-opacity Text renders nothing). The S208 RenderTransform+Clip
            // fold applies per glyph too (a needs_clip Text clips each quad to the
            // slot-client window — S833; a fitting Text stays unclipped). The
            // S511 drop-shadow is emitted once behind the glyph block (the
            // block's z-order host).
            const float op = static_cast<float>(effective_opacity(element));
            append_effect_draws(element.effect(), element, visual, out);
            for (DrawItem& glyph : visual.glyphs)
            {
                glyph.a *= op;
                apply_element_render_state(element, glyph);
                if (ui_item_degenerate(glyph))
                    continue;  // per-glyph skip rule
                out.push_back(glyph);
            }
            return;
        }

        // Single-item degenerate-skip rule (unchanged contract).
        if (ui_item_degenerate(visual.item))
            return;

        // S208 Opacity: fold the element's EFFECTIVE opacity (the ancestor-chain
        // product) into the item alpha. A zero-opacity element keeps alpha 0
        // (no visible draw) but its subtree still walked above. The folded alpha
        // feeds ui_batch_key's color, so distinct alphas -> distinct batches.
        visual.item.a *= static_cast<float>(effective_opacity(element));

        // S208 RenderTransform + Clip: fold the per-element Model into the item
        // and (S833) mark the clip to the slot-client window ONLY when the
        // element reported needs_clip — a fitting element stays unclipped.
        apply_element_render_state(element, visual.item);

        // S511 — EFFECT: when the element carries a DropShadowEffect, emit the
        // offset translucent shadow quad BEHIND the element's own draw (z-order
        // via tree order — the shadow is appended first, so the element paints
        // over it). BlurEffect passes through (NO-GAP).
        append_effect_draws(element.effect(), element, visual, out);

        //   4. Append: the element contributes its draw (in tree/z-order).
        out.push_back(visual.item);
    });
}

} // namespace vaml::vulkan::ui
