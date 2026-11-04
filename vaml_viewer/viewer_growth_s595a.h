// vaml_viewer — Sprint 11o S595a VIEWER-GROWTH seam (dep: S595).
//
// The S595a ADR-021 consumer seam — proves `IsHitTestVisible` renders the
// ScreenCaptureOverlay-shaped click-pass-through surface HEADLESSLY:
//   * An OVERLAY tree (a near-transparent capture Rectangle + a non-hit-test-
//     visible dim/mask layer, the ScreenCaptureOverlay shape the external app
//     uses) LOADS through a RETAINED VamlReader + finalize_visual_tree.
//   * The RENDER-PLAN (viewer_growth_s595a_render_plan) wraps the S212
//     `encode_visual_slice` to prove the overlay STILL RENDERS even while
//     non-hit-test-visible (decode: an element with IsHitTestVisible=false is
//     still emitted — the S595a decoupling).
//   * The TOGGLE proof (idle<->active, app-side only — ZERO library policy):
//     the idle mode has the input elements IsHitTestVisible=false -> HitTestAt
//     at an overlay/dim coordinate returns the UNDERLYING interactive element
//     (pass-through); the active mode flips the SAME element to true ->
//     HitTestAt now returns the overlay element itself (capture/"handled"). The
//     flip is a plain property change in the app's own code, never library policy.
//
// ADR-018: zero new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the S595a viewer-growth plan run.
struct ViewerGrowthS595aPlanResult
{
    bool ok = false;                                 // the overlay loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;    // the loaded overlay tree
    std::unique_ptr<vaml::VamlReader> reader;        // RETAINED — keeps the tree armed
    vaml::UIElement* dimLayer = nullptr;             // the non-hit-test-visible dim layer (hit-test-observer)
    std::string detail;                              // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): loads
// the ScreenCaptureOverlay-shaped overlay doc through a RETAINED VamlReader +
// finalize_visual_tree, and locates the non-hit-test-visible DIM layer by name
// (the x:Name registry, retained). Returns ok=false on ANY reader/parse error
// with the structured detail (never silent).
[[nodiscard]] inline ViewerGrowthS595aPlanResult
run_viewer_growth_s595a_plan(const std::string& text)
{
    ViewerGrowthS595aPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("overlay did not produce a root");
        (void)r.reader->finalize_visual_tree();
        // Locate the dim/mask layer (the non-hit-test-visible overlay chrome) by
        // its x:Name — the retained named() registry resolves it.
        r.dimLayer = dynamic_cast<vaml::UIElement*>(r.reader->named("DimLayer"));
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what(); // never silent
    }
    return r;
}

// The RENDER-PLAN seam: encodes the overlay tree through the default classifier
// (the S212 encoder). Proves the overlay STILL RENDERS even while the dim layer
// is non-hit-test-visible (the S595a decoupling: IsHitTestVisible=true renders +
// is hit-testable; IsHitTestVisible=false renders but is NOT). `out` is the
// emitted draw list; returns the emitted count. The caller arranges `root` first.
[[nodiscard]] inline std::size_t
viewer_growth_s595a_render_plan(vaml::DependencyObject* root,
                                std::vector<vaml::vulkan::ui::DrawItem>& out,
                                float width = 320.0f, float height = 240.0f)
{
    if (root == nullptr)
        return 0;
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
        return 0;
    ui->Measure(vaml::Size{ width, height });
    ui->Arrange(vaml::Rect{ 0, 0, width, height });
    vaml::presentation::encode_visual_slice(*ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

// The TOGGLE proof (app-side only — the library carries ZERO emergency-overlay
// policy): returns the hit-test result at `(x, y)` on the tree. The IDLE (the
// input elements are IsHitTestVisible=false -> pass-through to the UNDERLYING
// element) vs ACTIVE (the same element flipped to true -> capture/"handled")
// distinction is driven by the CALLER toggling IsHitTestVisible before the hit
// (a plain app-side property change, never library policy); this seam is the
// thin read passthrough so a test asserts the hit at a coordinate.
[[nodiscard]] inline vaml::UIElement*
viewer_growth_s595a_hit(vaml::UIElement* root, float x, float y)
{
    if (root == nullptr)
        return nullptr;
    return root->HitTestAt(x, y);
}

} // namespace vaml::viewer
