// vaml_viewer — Sprint 11m VIEWER-GROWTH(11m) seam (dep: S557).
//
// THE S558 SEAM — a corpus-tail-shaped `sample.tail.xaml` AUTHORS the DAG tail
// set (TreeView / DatePicker / Calendar / DataGrid / Polyline / Menu + the
// item/header scaffolding), and the LIVE viewer loads + renders the tail
// interactively (the generic renderer via classify_element). ADR-021 consumer
// pattern (same as S503/S512/11k-6) — compiled by both apps + the presentation
// test target. NO MINIMAL REAL introduced — every seam is real permanent code.
//
// SPLIT (the standing S221/S256/S329_1/S503 precedent):
//   * The REAL plan runner — `run_viewer_growth_11m_plan(text)`: wires the tail
//     sample through a RETAINED VamlReader, Loads it, finalize_visual_tree (so
//     the declared children render), and returns { ok, root, reader, detail }.
//     The reader is RETAINED so the tree's x:Name registry + authored resources
//     stay armed after the plan returns.
//   * The RENDER-PLAN seam — `viewer_growth_11m_render_plan(root, out)`: wraps
//     the S212 `encode_visual_slice` (the default classifier) so a test can
//     assert the tail's Solid/Monochrome draw counts (the RENDER-SURFACE proof).
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

// The outcome of the 11m viewer-growth plan run.
struct ViewerGrowth11mPlanResult
{
    bool ok = false;                              // the tail loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root; // the loaded live tail tree
    std::unique_ptr<vaml::VamlReader> reader;     // RETAINED — keeps named() + resources armed
    std::string detail;                           // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): loads
// the corpus-tail sample through a RETAINED VamlReader + finalize_visual_tree
// (the S120/S121 hand-off that RENDERS the declared children). The tail set
// (TreeView/DatePicker/Calendar/DataGrid/Polyline/Menu + the item/header
// scaffolding) is rendered by the generic classifier at render time.
// Returns ok=false on ANY reader/parse error with the structured detail (never
// silent).
[[nodiscard]] inline ViewerGrowth11mPlanResult
run_viewer_growth_11m_plan(const std::string& text)
{
    ViewerGrowth11mPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.tail.xaml did not produce a root");
        (void)r.reader->finalize_visual_tree();
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what(); // never silent
    }
    return r;
}

// The RENDER-PLAN seam: wraps the S212 `encode_visual_slice` over the default
// classifier. Arranges the root at `width`x`height` (so the S122/S124 layout
// rects are real) then encodes into `out`. Returns the number of DrawItems
// emitted (the RENDER-SURFACE proof a test asserts the tail's draw counts
// against).
[[nodiscard]] inline std::size_t
viewer_growth_11m_render_plan(vaml::DependencyObject* root,
                              std::vector<vaml::vulkan::ui::DrawItem>& out,
                              float width = 480.0f, float height = 360.0f)
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
