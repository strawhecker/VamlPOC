// vaml_viewer — Sprint 11ad VIEWER-GROWTH seam (dep:S835, Sprint 11ad). THE
// ARRANGE-SQUEEZE + CLIPTOBOUNDS + ARRANGE-GUARD CLOSURES RENDERED LIVE.
//
// Proves each 11ad closure resolves + RENDERS through the generic renderer (the
// S222_1/11k-6 RELOAD re-demonstrated):
//   1. S832 SQUEEZE — a Rectangle Width=200 in a 120-wide slot is arranged at its
//      FULL unclipped 200 + reports needs_clip; the scissor is the 120 slot-client
//      (FrameworkElement.cs:4597-4606) — never shrunk below its desired ink.
//   2. S833/S834 CLIP-TO-BOUNDS — a FITTING ClipToBounds=true element is STILL
//      clipped (forced mode); a FITTING ClipToBounds=false element is NOT
//      (FE.cs:4874 `NeedsClipBounds || ClipToBounds`).
//   3. S835 ARRANGE GUARDS — a Collapsed sibling is SKIPPED at arrange; a
//      non-finite slot THROWS std::invalid_argument (UIElement.cs:798-809).
//
// Headless Rail 1. ADR-021 consumer path. NO MINIMAL REAL. ADR-018 zero locks.

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"        // StackPanel / Rectangle
#include "vaml/core/UIElement.h"      // Visibility / needs_clip / clip_rect / clip_to_bounds / Arrange
#include "vaml/core/Geometry.h"       // Size / Rect
#include "vaml/core/LayoutSlot.h"     // compute_final_rect (the S832/S833 arrange leg)
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11ad viewer growth plan run.
struct ViewerGrowth11adPlanResult
{
    bool ok = false;
    std::unique_ptr<vaml::DependencyObject> root;
    std::unique_ptr<vaml::VamlReader> reader;
    std::string detail;

    // Closure element handles (resolved through the reader's named() registry).
    vaml::StackPanel* squeezePanel     = nullptr;
    vaml::Rectangle*  squeezeBox       = nullptr;
    vaml::StackPanel* clipHostPanel    = nullptr;
    vaml::Rectangle*  clipBox          = nullptr;
    vaml::Rectangle*  noClipBox        = nullptr;
    vaml::StackPanel* togglePanel      = nullptr;
    vaml::Rectangle*  collapsedSibling = nullptr;
    vaml::Rectangle*  visibleSibling   = nullptr;
    vaml::Rectangle*  nonFiniteProbe   = nullptr;

    // Closure A (S832): squeeze child-wins-then-clip.
    float squeezeBoxWidth     = 0.0f;   // == 200.0
    bool  squeezeBoxNeedsClip = false;  // true
    float squeezeBoxClipWidth = 0.0f;   // == 120.0
    float squeezePanelWidth   = 0.0f;   // == 120.0

    // Closure B (S833/S834): conditional vs forced clip.
    bool  clipBoxClipToBounds   = false;  // true
    bool  clipBoxNeedsClip      = false;  // false
    bool  noClipBoxClipToBounds = false;  // false
    bool  noClipBoxNeedsClip    = false;  // false

    // Closure C (S835): Collapsed skip + Inf/NaN rejection.
    bool  collapsedIsCollapsed = false;  // true
    bool  collapsedSkipped     = false;  // true
    float visibleHeight        = 0.0f;   // == 40.0
    bool  nonFiniteRejected    = false;  // true
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs).
[[nodiscard]] inline ViewerGrowth11adPlanResult
run_viewer_growth_11ad_plan(const std::string& text)
{
    ViewerGrowth11adPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.s11ad.vaml did not produce a root");
        (void)r.reader->finalize_visual_tree();

        auto* rootUi = dynamic_cast<vaml::UIElement*>(r.root.get());
        if (rootUi == nullptr)
            throw std::runtime_error("11ad sample: root is not a UIElement");

        r.squeezePanel   = dynamic_cast<vaml::StackPanel*>(r.reader->named("SqueezePanel"));
        r.squeezeBox     = dynamic_cast<vaml::Rectangle*>(r.reader->named("SqueezeBox"));
        r.clipHostPanel  = dynamic_cast<vaml::StackPanel*>(r.reader->named("ClipHostPanel"));
        r.clipBox        = dynamic_cast<vaml::Rectangle*>(r.reader->named("ClipBox"));
        r.noClipBox      = dynamic_cast<vaml::Rectangle*>(r.reader->named("NoClipBox"));
        r.togglePanel    = dynamic_cast<vaml::StackPanel*>(r.reader->named("TogglePanel"));
        r.collapsedSibling =
            dynamic_cast<vaml::Rectangle*>(r.reader->named("CollapsedSibling"));
        r.visibleSibling =
            dynamic_cast<vaml::Rectangle*>(r.reader->named("VisibleSibling"));
        r.nonFiniteProbe =
            dynamic_cast<vaml::Rectangle*>(r.reader->named("NonFiniteProbe"));
        if (r.squeezePanel == nullptr || r.squeezeBox == nullptr ||
            r.clipHostPanel == nullptr || r.clipBox == nullptr ||
            r.noClipBox == nullptr || r.togglePanel == nullptr ||
            r.collapsedSibling == nullptr || r.visibleSibling == nullptr ||
            r.nonFiniteProbe == nullptr)
        {
            throw std::runtime_error(
                "11ad sample: a closure element is not reachable by x:Name");
        }

        vaml::Size viewport{ 640.0f, 520.0f };
        rootUi->Measure(viewport);
        rootUi->Arrange(vaml::Rect{ 0.0f, 0.0f, viewport.width, viewport.height });

        // Closure A (S832): squeeze child-wins-then-clip.
        r.squeezeBoxWidth     = r.squeezeBox->final_rect().width;
        r.squeezeBoxNeedsClip = r.squeezeBox->needs_clip();
        r.squeezeBoxClipWidth = r.squeezeBox->clip_rect().width;
        r.squeezePanelWidth   = r.squeezePanel->final_rect().width;

        // Closure B (S833/S834): the DP + the arrange flags.
        r.clipBoxClipToBounds   = r.clipBox->clip_to_bounds();
        r.clipBoxNeedsClip      = r.clipBox->needs_clip();
        r.noClipBoxClipToBounds = r.noClipBox->clip_to_bounds();
        r.noClipBoxNeedsClip    = r.noClipBox->needs_clip();

        // Closure C (S835): Collapsed skip.
        r.collapsedIsCollapsed =
            (r.collapsedSibling->visibility() == vaml::Visibility::Collapsed);
        r.collapsedSkipped = r.collapsedSibling->is_measure_dirty() &&
                             (r.collapsedSibling->desired_size().height == 0.0f);
        r.visibleHeight    = r.visibleSibling->desired_size().height;

        // Closure C (S835): Inf/NaN arrange rejection (WPF UIElement.cs:798-809).
        {
            bool threw = false;
            try
            {
                const float nan = std::nanf("");
                const float inf = std::numeric_limits<float>::infinity();
                r.nonFiniteProbe->Arrange(vaml::Rect{ nan, 0.0f, inf, 100.0f });
            }
            catch (const std::invalid_argument&)
            {
                threw = true;
            }
            r.nonFiniteRejected = threw;
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
// classifier so the closures are provable at the emitted DrawItem layer.
[[nodiscard]] inline std::size_t
viewer_growth_11ad_render_plan(vaml::DependencyObject* root,
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
    ui->Measure(vaml::Size{ width, height });
    ui->Arrange(vaml::Rect{ 0, 0, width, height });
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
