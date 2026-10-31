// vaml_viewer — the S503 VIEWER-GROWTH(11f) seam (dep:S502, dep:S500, dep:S501).
//
// THE 11f SEAM — sample authors a DataTemplate + a converter + a behavior, and
// the LIVE viewer renders templated items + the converter output + the behavior
// fires the command on the routed event.
//
// This is the S503 consumer seam (ADR-021): it wires the app-owned VM (with the
// bool the BoolVis converter maps) + the app-owned DoIt RelayCommand + the
// registered BoolVis converter onto a RETAINED VamlReader, Loads the authored
// `sample.11f.vaml`, and BindExtensions. The authored surface resolves:
//   * the S500 DataTemplate (ListBox.ItemTemplate -> per-item ContentPresenters),
//   * the S501 Converter= output (BoolVis over IsAvailable -> the Visibility DP),
//   * the S502 behavior (<i:Interaction.Behaviors><i:EventTrigger><InvokeCommandAction
//     Command="{Command DoIt}"/></EventTrigger></i:Interaction.Behaviors>) — when
//     the root's Click routed event raises, the behavior executes DoIt (the S503
//     behavior-fires-command proof).
//
// SPLIT (the standing S221/S256/S329_1 precedent):
//   * The REAL plan runner — `run_viewer_growth_11f_plan(text, cmd)`: wires the
//     app-owned VM + converter + command onto a RETAINED VamlReader, Loads the
//     text (the exact code the live viewer runs), and returns { ok, root, reader,
//     vm, bound, detail }. The reader/VM are retained so the live bindings + the
//     attached behavior stay armed after the plan returns.
//   * The RENDER-PLAN seam — `viewer_growth_11f_render_plan(root, out)`: wraps the
//     S212 `encode_visual_slice` (over default_visual_of) so a test can assert the
//     templated-items + converter-output draw counts (the RENDER-SURFACE proof).
//   * The drive seam for the routed event is the S502 model itself (RaiseEvent on
//     the root fires the attached behavior's command) — no app-side re-routing.
//
// This header is compiled by BOTH apps AND the presentation test target (the
// ADR-021 consumer pattern). NO MINIMAL REAL introduced — the VM + the plan runner
// are real permanent code. ADR-018: ZERO new locks (single-threaded on the
// viewer's UI thread).

#pragma once

#include "vaml/core/BooleanToVisibilityConverter.h" // S501 — the sole framework BoolVis converter
#include "vaml/core/DependencyObject.h"             // the plan result owns a unique_ptr<DependencyObject>
#include "vaml/core/ICommand.h"                     // S246 — the command the behavior executes
#include "vaml/core/ObservableObject.h"             // the VM base (set_data_context target)
#include "vaml/core/UIElement.h"                    // the root (Measure/Arrange + encode)
#include "vaml/core/VamlReader.h"                   // VamlReader (Load + register_converter + register_command)
#include "vaml/mvvm/property_bindable.h"            // PROPERTY_BINDABLE (the S247_1 convenience layer)
#include <vaml/presentation/VamlRenderer.h>         // S212 — encode_visual_slice + default_visual_of

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The S503 app-constructed ViewModel: the bool the BoolVis converter maps.
// `{Binding IsAvailable, Converter=BoolVis}` resolves `IsAvailable`; the
// converter pushes Visible(0)/Collapsed(2) onto the authored Visibility DP.
class ViewerGrowth11fViewModel final : public ObservableObject
{
public:
    ViewerGrowth11fViewModel() = default;
    PROPERTY_BINDABLE(bool, IsAvailable);
};

// The outcome of the 11f viewer growth plan run.
struct ViewerGrowth11fPlanResult
{
    bool ok = false;                                // text loaded + bound cleanly
    std::unique_ptr<vaml::DependencyObject> root;   // the loaded + bound live tree
    std::shared_ptr<ViewerGrowth11fViewModel> vm;   // the app-owned VM (IsAvailable)
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED — keeps bindings + the behavior armed
    std::size_t bound = 0;                          // the S252 BindExtensions bound-row count
    std::string detail;                             // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): wires
// the app-owned VM + the BoolVis converter + the doIt command onto a RETAINED
// VamlReader, Loads the authored text (sample.11f.vaml), registers the Click event
// + the DoIt command, and BindExtensions. The behavior is authored in the text
// (`<i:Interaction.Behaviors>`), so the attached EventTrigger is armed on the root.
// Returns ok=false on ANY reader/parse error with the structured detail (never
// silent). The reader/VM are returned owned so the caller can raise Click on the
// root and observe the behavior fire `cmd` (the S503 behavior-fires-command proof).
[[nodiscard]] inline ViewerGrowth11fPlanResult
run_viewer_growth_11f_plan(const std::string& text, vaml::ICommand* doIt)
{
    ViewerGrowth11fPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.vm = std::make_shared<ViewerGrowth11fViewModel>();
        (void)r.vm->set_IsAvailable(true);               // the converter maps true -> Visible

        r.reader->set_data_context(r.vm.get());
        r.reader->register_converter("BoolVis",
            std::make_shared<vaml::BooleanToVisibilityConverter>()); // S501
        r.reader->register_command("DoIt", doIt);                    // S253/S502
        r.root = r.reader->Load(text);
        r.bound = r.reader->BindExtensions();                        // S252/S501
        // S440 — transfer the retained children into the visual tree so the
        // render-plan (encode_visual_slice) walks the REAL UIElement children
        // (the ListBox + its per-item ContentPresenters + the Status TextBlock).
        (void)r.reader->finalize_visual_tree();
        r.ok = (r.root != nullptr);
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                           // never silent
    }
    return r;
}

// The RENDER-PLAN seam: wraps the S212 `encode_visual_slice` over the default
// classifier. Arranges the root at `width`x`height` (so the S122/S124 layout
// rects are real) then encodes into `out`. Returns the number of DrawItems
// emitted. Headless + pure — the RENDER-SURFACE proof a test asserts draw counts
// against (the templated items + the converter/hidden-state handling).
[[nodiscard]] inline std::size_t
viewer_growth_11f_render_plan(vaml::DependencyObject* root,
                              std::vector<vaml::vulkan::ui::DrawItem>& out,
                              float width = 280.0f, float height = 240.0f)
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
