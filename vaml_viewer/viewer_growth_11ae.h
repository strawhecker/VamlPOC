// vaml_viewer - Sprint 11ae VIEWER-GROWTH seam (dep:S843, Sprint 11ae). THE
// S839/S840 + S841 + S842 LAYOUT CLOSURES RENDERED LIVE via UpdateLayout (S843).
//
// Proves each 11ae closure resolves at the LIVE element layer through the S843
// public UIElement::UpdateLayout() driver (NOT a raw Measure/Arrange call):
//   1. S839/S840 - a child whose UNCLIPPED DesiredSize (200) exceeds its offered
//      slot (120) re-dirties + re-lays-out its parent on an actual size change,
//      while an unchanged sibling stays measure-clean (OnChildDesiredSizeChanged
//      waterfall; UIElement.cs:1478-1516).
//   2. S841 - the squeezed child RenderSize (ink, 200) is visibly DISTINCT from
//      its layout slot (120) (child-wins-then-clip; FrameworkElement.cs:4597-4606)
//      AND the G3 observable contract fires: ActualWidth == 200 + SizeChanged raised.
//   3. S842 - a panel auto-drives an unchanged-constraint child with NO redundant
//      re-measure: after invalidating only the wide child, the unchanged sibling
//      stays measure-valid (the element constraint-cache short-circuits; the panel
//      gate is gone - S844/G1).
//
// Headless Rail 1. ADR-021 consumer path. NO MINIMAL REAL. ADR-018 zero locks.

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/vulkan/ui/UiScene.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vaml::viewer {

struct ViewerGrowth11aePlanResult
{
    bool ok = false;
    std::unique_ptr<vaml::DependencyObject> root;
    std::unique_ptr<vaml::VamlReader> reader;
    std::string detail;

    vaml::StackPanel* squeezePanel = nullptr;
    vaml::Rectangle*  wideChild    = nullptr;
    vaml::Rectangle*  cleanSibling = nullptr;

    float wideUnclipped     = 0.0f;
    float offeredSlot       = 0.0f;
    bool  childExceedsSlot  = false;
    bool  siblingStaysClean = false;
    float wideRenderWidth   = 0.0f;
    float wideSlotWidth     = 0.0f;
    bool  renderDistinctFromSlot = false;
    float wideActualWidth   = 0.0f;
    int   sizeChangedCount  = 0;
    bool  wideRecomputedOnDirty = false;
    bool  siblingNotRemeasured  = false;
};

[[nodiscard]] inline ViewerGrowth11aePlanResult
run_viewer_growth_11ae_plan(const std::string& text)
{
    ViewerGrowth11aePlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr) { throw std::runtime_error("sample.s11ae.vaml produced no root"); }
        (void)r.reader->finalize_visual_tree();

        auto* rootUi = dynamic_cast<vaml::UIElement*>(r.root.get());
        if (rootUi == nullptr) { throw std::runtime_error("11ae sample: root is not a UIElement"); }

        r.squeezePanel = dynamic_cast<vaml::StackPanel*>(r.reader->named("SqueezePanel"));
        r.wideChild    = dynamic_cast<vaml::Rectangle*>(r.reader->named("WideChild"));
        r.cleanSibling = dynamic_cast<vaml::Rectangle*>(r.reader->named("CleanSibling"));
        if (r.squeezePanel == nullptr || r.wideChild == nullptr || r.cleanSibling == nullptr)
            throw std::runtime_error("11ae sample: a closure element is not reachable by x:Name");

        (void)r.wideChild->add_routed_handler(
            vaml::RoutedEvent::registered("SizeChanged", vaml::RouteEventDirection::Bubble),
            [&r](vaml::RoutedEventArgs&) { ++r.sizeChangedCount; });

        const vaml::Size viewport{ 640.0f, 520.0f };
        const vaml::Rect slot{ 0.0f, 0.0f, 640.0f, 520.0f };

        rootUi->UpdateLayout(viewport, slot);

        r.wideUnclipped    = r.wideChild->unclipped_desired_size().width;
        r.offeredSlot      = r.squeezePanel->final_rect().width;
        r.childExceedsSlot = (r.wideUnclipped > r.offeredSlot + 0.5f);
        r.wideRenderWidth  = r.wideChild->render_size().width;
        r.wideSlotWidth    = r.squeezePanel->final_rect().width;
        r.renderDistinctFromSlot = (std::fabs(r.wideRenderWidth - r.wideSlotWidth) > 0.5f);
        r.wideActualWidth  = r.wideChild->actual_width();
        r.siblingStaysClean = !r.cleanSibling->is_measure_dirty();

        const float cleanBefore = r.cleanSibling->desired_size().width;
        r.wideChild->InvalidateMeasure();
        rootUi->UpdateLayout(viewport, slot);
        r.wideRecomputedOnDirty = !r.wideChild->is_measure_dirty();
        r.siblingNotRemeasured  = (!r.cleanSibling->is_measure_dirty()) && (std::fabs(r.cleanSibling->desired_size().width - cleanBefore) < 0.5f);
        r.siblingStaysClean = r.siblingStaysClean && (!r.cleanSibling->is_measure_dirty());

        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();
    }
    return r;
}

[[nodiscard]] inline std::size_t
viewer_growth_11ae_render_plan(vaml::DependencyObject* root,
                               std::vector<vaml::vulkan::ui::DrawItem>& out,
                               float width = 640.0f, float height = 520.0f)
{
    if (root == nullptr) { return 0; }
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr) { return 0; }
    ui->UpdateLayout(vaml::Size{ width, height }, vaml::Rect{ 0, 0, width, height });
    vaml::presentation::encode_visual_slice(*ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
