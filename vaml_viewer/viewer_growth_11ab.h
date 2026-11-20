// vaml_viewer — Sprint 11ab VIEWER-GROWTH seam (dep:S824, Sprint 11ab). THE
// CLOSED SURFACES RENDERED LIVE.
//
// The SPIRIT of this seam: author a corpus-shaped `sample.s11ab.vaml` that
// authors ALL FOUR S821/S822/S823/S824 closures and prove EACH resolves + RENDERS
// through the generic renderer (the S222_1/11k-6 RELOAD re-demonstrated — the
// anti-grave-whistle for the 11ab closure surface):
//   1. Visibility="Collapsed" (S821 — the ComputeValidateVisibility boundary
//      gate). The authored Collapsed element must NOT render (the S207
//      encoder's SkipCollapsed rule) while its visible sibling does.
//   2. a panel Opacity="0.75" with a child Opacity="0.8" (S822 — the compositor
//      subtree model). The S208 encoder's effective_opacity is the visual-
//      ancestor chain PRODUCT: the panel renders at 0.75, the child at 0.75×0.8
//      = 0.6 (compositor subtree alpha).
//   3. RenderTransformOrigin="0.5,0.5" + an authored RotateTransform Angle="45"
//      (S823 — the WPF-faithful Transform-object DP). The S208 encoder resolves
//      the authored Transform's matrix() and folds the RenderTransformOrigin
//      about the fraction center (WPF ArrangeCore); the rotated element's
//      emitted item carries a NON-identity Model + a dynamic clip.
//   4. FE-branch alignment/Width/Height on a FrameworkElement-branch control
//      (S824 — the layout-slot re-key under owner "FrameworkElement"). The
//      authored Width/HAlignment/VAlignment constrain the element's final_rect.
//
// Headless Rail 1 (the [both-apps, not CI] window display stays the on-machine
// local gate — this is the CI-checkable half of the exact code path).
//
// The real plan runner (`run_viewer_growth_11ab_plan`) wires the sample through
// a RETAINED VamlReader, Loads + finalizes (so the declared children render),
// resolves each closure element through the reader's named() registry, and
// asserts the CLOSURE at the LIVE element layer (the DP values the renderer
// consumes): Visibility==Collapsed, the parent/child Opacity locals, the
// Transform-object RenderTransform + RenderTransformOrigin, and the FE-owner
// Width/HAlignment on the FrameworkElement-branch control.
//
// The render-plan seam (`viewer_growth_11ab_render_plan`) wraps the S212
// encode_visual_slice over the default classifier and returns the merged
// ElementVisuals + the emitted DrawItems so a test can assert the RENDER-
// SURFACE closures: the Collapsed element contributes NO draw (absent), the
// compositor alpha product (0.6 child under a 0.75 parent), and the rotated
// element's NON-identity Model.
//
// ADR-021 consumer path (the S503/S512/S11q/S11s precedent). NO MINIMAL REAL.
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"        // StackPanel / Rectangle / UIElement (FrameworkElement-branch)
#include "vaml/core/UIElement.h"      // Visibility / Opacity / RenderTransform / RenderTransformOrigin
#include "vaml/core/Transform.h"      // Transform / RotateTransform / is_transform_identity
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/core/Geometry.h"       // Point / Size / Rect
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiSceneEncoder.h>  // effective_opacity (the compositor product)
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11ab viewer growth (closed-surfaces-live) plan run.
struct ViewerGrowth11abPlanResult
{
    bool ok = false;                          // loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;  // the loaded live tree
    std::unique_ptr<vaml::VamlReader> reader;      // RETAINED — keeps named() armed
    std::string detail;                       // the structured failure reason (never silent)

    // Closure element handles (resolved through the reader's named() registry).
    vaml::Rectangle* colRect  = nullptr;   // Visibility=Collapsed (closure 1)
    vaml::Rectangle* visRect  = nullptr;   // Visibility=Visible (closure 1)
    vaml::StackPanel* opPanel = nullptr;   // Opacity=0.75 panel (closure 2)
    vaml::Rectangle* opChild = nullptr;    // Opacity=0.8 child (closure 2)
    vaml::Rectangle* rotRect = nullptr;    // RenderTransform+Origin (closure 3)
    vaml::Rectangle* feRect  = nullptr;    // FE-branch alignment/Width/Height (closure 4)

    // Closure 1 (S821): the Collapsed element's Visibility reads Collapsed.
    bool collapsed = false;
    // Closure 2 (S822): the compositor locals read as authored (panel 0.75,
    // child 0.8 — effective_opacity is the PRODUCT the encoder computes).
    bool panelOpacity = false;
    bool childOpacity = false;
    // Closure 3 (S823): the RenderTransform is a NON-identity Transform object
    // (Angle=45 rotation) + the Origin reads (0.5, 0.5).
    bool rotated = false;
    bool origin   = false;
    // Closure 4 (S824): the FE-owner Width + HAlignment resolve on the
    // FrameworkElement-branch control.
    bool feWidth  = false;
    bool feAlign  = false;
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): Load
// `text` through a RETAINED VamlReader, finalize_visual_tree (the S120/S121
// hand-off that RENDERS the declared children), then resolve each closure
// element through the reader's named() registry and assert the closure at the
// LIVE element layer. Returns { ok, root, reader, …proofs, detail }.
[[nodiscard]] inline ViewerGrowth11abPlanResult
run_viewer_growth_11ab_plan(const std::string& text)
{
    ViewerGrowth11abPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.s11ab.vaml did not produce a root");
        // The S120/S121 hand-off: transfer the declared children into their
        // parents' S121 visual-children lists so the tree RENDERS them.
        (void)r.reader->finalize_visual_tree();

        // --- Closure 1 (S821): Visibility Collapsed vs Visible -------------
        r.colRect = dynamic_cast<vaml::Rectangle*>(r.reader->named("ColRect"));
        r.visRect = dynamic_cast<vaml::Rectangle*>(r.reader->named("VisRect"));
        if (r.colRect == nullptr || r.visRect == nullptr)
            throw std::runtime_error("11ab sample: ColRect/VisRect not reachable by x:Name");
        r.collapsed = (r.colRect->visibility() == vaml::Visibility::Collapsed) &&
                      (r.visRect->visibility() == vaml::Visibility::Visible);

        // --- Closure 2 (S822): compositor locals (the encoder PRODUCT) -----
        r.opPanel = dynamic_cast<vaml::StackPanel*>(r.reader->named("OpPanel"));
        r.opChild = dynamic_cast<vaml::Rectangle*>(r.reader->named("OpChild"));
        if (r.opPanel == nullptr || r.opChild == nullptr)
            throw std::runtime_error("11ab sample: OpPanel/OpChild not reachable by x:Name");
        r.panelOpacity = (r.opPanel->opacity() == 0.75);
        r.childOpacity = (r.opChild->opacity() == 0.8);

        // --- Closure 3 (S823): Transform object + Origin -------------------
        r.rotRect = dynamic_cast<vaml::Rectangle*>(r.reader->named("RotRect"));
        if (r.rotRect == nullptr)
            throw std::runtime_error("11ab sample: RotRect not reachable by x:Name");
        {
            const std::shared_ptr<vaml::Transform> tf = r.rotRect->render_transform();
            r.rotated = (tf != nullptr) && !vaml::is_transform_identity(tf->matrix());
        }
        r.origin = (r.rotRect->render_transform_origin() == vaml::Point{ 0.5f, 0.5f });

        // --- Closure 4 (S824): FE-owner Width/Height/HAlignment ------------
        r.feRect = dynamic_cast<vaml::Rectangle*>(r.reader->named("FeRect"));
        if (r.feRect == nullptr)
            throw std::runtime_error("11ab sample: FeRect not reachable by x:Name");
        r.feWidth = (r.feRect->width() == 200.0);
        r.feAlign = (r.feRect->horizontal_alignment() == vaml::HorizontalAlignment::Center);

        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();               // never silent
    }
    return r;
}

// The RENDER-PLAN seam: wraps the S212 `encode_visual_slice` over the default
// classifier AND captures each closure element's emitted draw so the RENDER-
// SURFACE closures are provable headlessly. Arranges the root at `width`x`height`
// (so the S122/S124 layout rects are real) then encodes into `out`. Returns the
// number of DrawItems emitted.
[[nodiscard]] inline std::size_t
viewer_growth_11ab_render_plan(vaml::DependencyObject* root,
                               std::vector<vaml::vulkan::ui::DrawItem>& out,
                               float width = 640.0f, float height = 520.0f)
{
    if (root == nullptr)
    {
        return 0;
    }
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
    {
        return 0;
    }
    ui->Measure(vaml::Size{width, height});
    ui->Arrange(vaml::Rect{0, 0, width, height});
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
