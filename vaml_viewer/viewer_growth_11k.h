// vaml_viewer — the 11k-6 VIEWER-GROWTH(11k) seam (dep: S11k1+2+3+4+5).
//
// THE 11k-6 SEAM — a corpus-shaped `sample.corpus.xaml` AUTHORS the FULL 11k
// feature trust, and the LIVE viewer loads + renders it:
//   * a real <TextBlock> glyph (the S409_1/S400 SDF-text path — inline content
//     -> the Text DP, the WPF IAddChild content routing authored at 11k-1),
//   * the 11k-2 added controls (RepeatButton / ScrollBar / Viewbox / ToolBar /
//     ToolBarTray — the WPF-missing corpus set),
//   * a VECTOR resource authored through the 11k-4 grammar (a DrawingBrush ->
//     DrawingGroup -> GeometryDrawing -> PathGeometry, captured by x:Key into a
//     ResourceDictionary) + RESOLVED + drawn via the S202 atlas mesh (the
//     vaml::vulkan::vector raster seam folds the drawing into a 64x64 RGBA8
//     tile + emits the S206 textured-quad DrawItem that samples it),
//   * an EventTrigger (element-level <Control.Triggers>, the 11j-7 path),
//     parsed into the S362 trigger model as TriggerKind::Event (RoutedEvent +
//     optional SourceName) — the 11k-5 authoring surface, whose BeginStoryboard
//     Actions payload evaluation is live per S512a.
//
// SPLIT (the standing S221/S256/S329_1/S503 precedent):
//   * The REAL plan runner — `run_viewer_growth_11k_plan(text)`: wires the
//     sample through a RETAINED VamlReader, Loads it, finalize_visual_tree (so
//     the declared children render), and returns { ok, root, reader,
//     vectorOk, detail }. It also RESOLVES + RASTERS the authored DrawingBrush
//     resource (the S202 atlas-mesh proof) so the S206 textured-quad lands in
//     `vectorDraw`. The reader is RETAINED so the tree's x:Name registry + the
//     authored resources stay armed after the plan returns.
//   * The RENDER-PLAN seam — `viewer_growth_11k_render_plan(root, out)`: wraps
//     the S212 `encode_visual_slice` (the default classifier) so a test can
//     assert the Text (TextBlock glyph) + Solid (the 11k-2 controls) + the
//     authored vector textured-quad draw counts (the RENDER-SURFACE proof).
//
// This header is compiled by BOTH apps AND the presentation test target (the
// ADR-021 consumer pattern — same as the S503/S512 seams). NO MINIMAL REAL
// introduced — every seam is real permanent code. ADR-018: ZERO new locks
// (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"   // the plan result owns a unique_ptr<DependencyObject>
#include "vaml/core/ResourceDictionary.h" // find_for_element (resolve the authored DrawingBrush)
#include "vaml/core/Style.h"              // Trigger / TriggerKind / trigger_kind (EventTrigger record)
#include "vaml/core/UIElement.h"          // the root (Measure/Arrange + encode)
#include "vaml/core/VamlReader.h"         // VamlReader (Load + finalize_visual_tree + named + element_triggers)
#include "vaml/core/VectorDrawing.h"      // the 11k-4 authored DrawingBrush/Drawing graph
#include <vaml/presentation/VamlRenderer.h> // S212 — encode_visual_slice + default_visual_of
#include <vaml/vulkan/vector/VectorRaster.h> // 11k-4 — the S202 atlas-mesh raster seam + vector_quad
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11k-6 viewer growth plan run.
struct ViewerGrowth11kPlanResult
{
    bool ok = false;                             // text loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root; // the loaded live tree
    std::unique_ptr<vaml::VamlReader> reader;    // RETAINED — keeps named() + resources armed
    bool vectorOk = false;                       // the authored DrawingBrush resolved + rasterized
    std::vector<vaml::vulkan::ui::DrawItem> vectorDraw; // the S206 textured-quad(s) the S202 mesh emits
    std::string detail;                          // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): loads
// the corpus sample through a RETAINED VamlReader, finalize_visual_tree (the
// S120/S121 hand-off that RENDERS the declared children), then RESOLVES the
// authored 11k-4 DrawingBrush resource by x:Key (arrowBrush) through the S360
// three-scope walk and runs it through the vulkan vector raster seam —
// raster_drawing_to_tile (the DrawingGroup -> GeometryDrawing -> PathGeometry
// authored graph folded into a 64x64 S202 RGBA8 tile) + vector_quad (the S206
// textured-quad DrawItem sampling the tile's ink-box UVs). This is the "drawn
// via the S202 atlas mesh" proof, headlessly + on the exact code path. Also
// surfaces the authored EventTrigger (element_triggers, the 11j-7/11k-5 arm) so
// a test can assert it parsed as TriggerKind::Event.
// Returns ok=false on ANY reader/parse error with the structured detail (never
// silent).
[[nodiscard]] inline ViewerGrowth11kPlanResult
run_viewer_growth_11k_plan(const std::string& text)
{
    ViewerGrowth11kPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.corpus.xaml did not produce a root");
        // The S120/S121 hand-off: transfer the declared children into their
        // parents' S121 visual-children lists so the tree RENDERS them (NOT just
        // the root). The EventTrigger is a per-Load element_trigger record (NOT a
        // visual child — finalize does not touch it).
        (void)r.reader->finalize_visual_tree();

        // RESOLVE + RASTER the authored 11k-4 DrawingBrush resource (arrowBrush)
        // through the S360 three-scope walk (the element's own Resources -> parent
        // chain -> app -> system). This is the "authored vector resource resolved
        // through the S360/S453 resource path" headline.
        vaml::UIElement* rootUi = dynamic_cast<vaml::UIElement*>(r.root.get());
        if (rootUi != nullptr)
        {
            const vaml::ResourceValue* rv = vaml::find_for_element(*rootUi, "arrowBrush");
            if (rv != nullptr && rv->kind() == vaml::ResourceValue::Kind::Object)
            {
                const auto* brush = dynamic_cast<const vaml::DrawingBrush*>(rv->object());
                if (brush != nullptr && brush->drawing() != nullptr)
                {
                    // The S202 atlas-mesh seam: fold the authored drawing into a
                    // 64x64 RGBA8 tile (the drawing's representative color), then
                    // emit the S206 textured-quad that samples its ink-box UVs.
                    const vaml::Color fill = brush->representative_color();
                    const vaml::vulkan::vector::VectorTile tile =
                        vaml::vulkan::vector::raster_drawing_to_tile(*brush->drawing(), fill);
                    if (tile.ok)
                    {
                        const auto tint = vaml::vulkan::ui::Color4{
                            1.0f, 1.0f, 1.0f, 1.0f };
                        r.vectorDraw.push_back(vaml::vulkan::vector::vector_quad(
                            20.0f, 200.0f, 48.0f, 48.0f, tile, tint));
                        r.vectorOk = true;
                    }
                }
            }
        }

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
// classifier. Arranges the root at `width`x`height` (so the S122/S124 layout
// rects are real) then encodes into `out`. Returns the number of DrawItems
// emitted. Headless + pure — the RENDER-SURFACE proof a test asserts draw counts
// against (the TextBlock glyph + the 11k-2 control solids).
[[nodiscard]] inline std::size_t
viewer_growth_11k_render_plan(vaml::DependencyObject* root,
                              std::vector<vaml::vulkan::ui::DrawItem>& out,
                              float width = 440.0f, float height = 360.0f)
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
    // Arrange the root so final_rect() / DesiredSize are real (the S123/S124
    // layout rects the encoder + hit-testing consume).
    ui->Measure(vaml::Size{width, height});
    ui->Arrange(vaml::Rect{0, 0, width, height});
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
