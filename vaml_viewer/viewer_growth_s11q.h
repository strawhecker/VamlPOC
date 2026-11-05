// vaml_viewer — Sprint 11q VIEWER-GROWTH seam (S622, dep:S621).
//
// The S622 viewer-growth proof: the SEED-context (S620+S621) is exercised on an
// IN-REPO doc that reproduces the EXACT failure class of the previously-black
// external corpus files (Splash/OverlayWindow/ShadowLocationIndicator/
// SignalDisplayView/ReadingControl/…): bound-but-DATA-EMPTY text +
// background/foreground bound to {Binding} with NO ViewModel. Before S11q these
// rendered BLACK/TRANSPARENT (every {Binding} resolved against a null data
// context -> Class B NotReadable -> default invisible DPs). With the seed, the
// doc renders DISCERNIBLE layout — visible placeholder text + visible opaque
// fills.
//
// The external corpus is IP-walled and NOT in this repo, so the S622 proof is
// `sample.seed.vaml` (in-repo, ADR-021) — a faithful reproduction of the failure
// class, driven through the exact code the LIVE viewer runs:
//   1. run_viewer_growth_s11q_plan(text): Load through a RETAINED VamlReader,
//      SEED the app-owned ObservableObject data-context (typed — S621),
//      set_data_context + BindExtensions(), finalize_visual_tree — the REAL
//      viewer bind+render path. Headless.
//   2. viewer_growth_s11q_render_plan(root, out): arrange + encode_visual_slice
//      (the S212 render-plan) — the RENDER-SURFACE proof: the slice emits
//      NON-DEGENERATE DrawItems (discernible layout; the encoder is where a
//      black/transparent doc contributes nothing / degenerate items).
//
// ADR-021 consumer path (the S503/S512/S-kA-5 precedent). NO MINIMAL REAL.
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "viewer_seed_context.h"          // S620+S621: seed_typed_bindings + ViewerSeedPlanResult

#include "vaml/core/DependencyObject.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem + ui_item_degenerate

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the S11q viewer growth plan run.
// Member ORDER = reverse-destruction-safe for the live binding: the reader's
// reader-owned live bindings (BindExtensions) subscribe to the seed VM, so the
// reader MUST be destroyed BEFORE the seed VM (the reader's binding dtors run
// their unsubscribe while the VM is still alive). Declaration: root, seedVm,
// reader -> destruction: reader, seedVm, root (the S256/S620 precedent).
struct ViewerGrowthS11qPlanResult
{
    bool ok = false;                                // seeded + bound + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;   // the seeded bound live tree
    std::shared_ptr<vaml::ObservableObject> seedVm; // the app-owned SEED VM
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED — keeps the seed bindings armed
    std::size_t seeded = 0;                         // {Binding} leaf paths seeded (S620+S621)
    std::size_t bound  = 0;                         // the S252 BindExtensions bound-row count
    std::string detail;                             // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs on a
// bound-but-data-empty doc): Load `text` through a RETAINED VamlReader, SEED
// the app-owned ObservableObject data-context (typed, S620+S621), set_data_context
// + BindExtensions() (the S252 markup resolution), then finalize_visual_tree()
// (the S440 hand-off that RENDERS the declared children). Returns { ok, root,
// reader, seedVm, seeded, bound, detail }. The reader + seed VM are returned
// owned so the S243 live wires stay armed after the plan returns.
[[nodiscard]] inline ViewerGrowthS11qPlanResult
run_viewer_growth_s11q_plan(const std::string& text)
{
    ViewerGrowthS11qPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.seedVm = std::make_shared<vaml::ObservableObject>();
        r.root   = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.seed.vaml did not produce a root");
        // SEED the data-context from the deferred single-segment {Binding} paths
        // BEFORE BindExtensions (the S620+S621 injection — viewer-only).
        r.seeded = seed_typed_bindings(*r.reader, *r.seedVm);
        r.reader->set_data_context(r.seedVm.get());   // the app owns the VM; the reader borrows it
        r.bound = r.reader->BindExtensions();          // S252: resolve {Binding} -> live bindings
        (void)r.reader->finalize_visual_tree();        // S440: transfer children into the visual tree
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                           // never silent
    }
    return r;
}

// The RENDER-PLAN seam: arranges the seeded bound tree + encodes it through the
// S212 encoder (default classifier). Returns the number of NON-DEGENERATE
// DrawItems emitted (0 on a null/non-UIElement root, or when the doc contributes
// nothing — the BLACK/TRANSPARENT failure the seed fixes). The discardible-layout
// proof: a prior-black doc now emits visible items (text + fills).
[[nodiscard]] inline std::size_t
viewer_growth_s11q_render_plan(vaml::DependencyObject* root,
                               std::vector<vaml::vulkan::ui::DrawItem>& out,
                               float width = 440.0f, float height = 360.0f)
{
    if (root == nullptr)
        return 0;
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
        return 0;
    ui->Measure(vaml::Size{ width, height });
    ui->Arrange(vaml::Rect{ 0, 0, width, height });
    std::vector<vaml::vulkan::ui::DrawItem> items;
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), items);
    // Count the NON-DEGENERATE items (the renderable slice — a black/empty doc
    // contributes only degenerate/skipped items, a visible doc emits real ones).
    std::size_t visible = 0;
    for (const auto& item : items)
    {
        if (!vaml::vulkan::ui::ui_item_degenerate(item))
        {
            ++visible;
        }
    }
    out.insert(out.end(), items.begin(), items.end());
    return visible;
}

} // namespace vaml::viewer
