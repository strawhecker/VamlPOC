// vaml_viewer — Sprint 11ac VIEWER-GROWTH seam (dep:S829, Sprint 11ac). THE
// CORRECTED MEASURE SEMANTICS RENDERED LIVE.
//
// The SPIRIT of this seam: author a corpus-shaped `sample.s11ac.vaml` that
// authors the THREE S827/S828/S829 closures and prove EACH resolves + RENDERS
// through the generic renderer (the S222_1/11k-6 RELOAD re-demonstrated — the
// anti-grave-whistle for the 11ac measure surface):
//   1. S827 DesiredSize-BOUNDS — a child whose content exceeds its offered slot
//      still reports its wider CONTENT extent. WPF FrameworkElement.cs:4394
//      `Size unclippedDesiredSize = desiredSize;` — MeasureOverride's output is
//      the un-clamped content extent.
//   2. S828 the Stretch/Fill PASS — alignment is read ONLY in arrange. WPF
//      FrameworkElement.cs:4597-4620 (the child's own ArrangeCore), NOT in
//      Measure (`compute_measured_desired` takes no alignment). The removed
//      invented "Stretch + infinite → wrap" branch stays absent: an unbounded
//      axis is CONTENT, never a wrap.
//   3. S829 measure-ENTRY — the Collapsed skip + re-measure on reveal, driven
//      by WPF's OWN signal path: the Visibility transition
//      (UIElement.cs:2984-3034 → signalDesiredSizeChange) plus
//      OnChildDesiredSizeChanged (UIElement.cs:323-330). NO harness-side
//      invalidate is used — a harness invalidate would certify the harness.
//
// Headless Rail 1 (the [both-apps, not CI] window display stays the on-machine
// local gate — this is the CI-checkable half of the exact code path).
//
// [WPF DIVERGENCE] (recorded, NOT asserted as WPF-true — RESOLVED BY S832):
// our single `desired_size_` store is WPF's `UnclippedDesiredSize`
// (FrameworkElement.cs:4394/4473-4491); the clamped parent-facing DesiredSize,
// `NeedsClipBounds`, `GetLayoutClip` (FrameworkElement.cs:4579-4599/4874,
// UIElement.cs:3322-3333), the non-Stretch grow-to-unclipped-desired-size arm
// (FrameworkElement.cs:4617-4623) and the `effectiveMaxWidth` arrange cap
// (FrameworkElement.cs:4636-4655) are the S832 (Sprint 11ad) payload. The
// closures below assert the MEASURE half that IS WPF-true today, at the LIVE
// element layer; closure A additionally records that the arranged slot stays
// parent-bounded (today's truth, WPF's post-S832 truth differs only in the
// clip/grow arms).
//
// ADR-021 consumer path (the S503/S512/S11q/S11s/11ab precedent). NO MINIMAL
// REAL. ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"        // StackPanel / Rectangle / TextBlock (FrameworkElement-branch)
#include "vaml/core/UIElement.h"      // Visibility / OnChildDesiredSizeChanged
#include "vaml/core/Geometry.h"       // Size / Rect
#include "vaml/core/LayoutSlot.h"     // compute_measured_desired (the S827/S828 finalizer)
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11ac viewer growth (corrected-measure-semantics) plan run.
struct ViewerGrowth11acPlanResult
{
    bool ok = false;                               // loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;  // the loaded live tree
    std::unique_ptr<vaml::VamlReader> reader;      // RETAINED — keeps named() armed
    std::string detail;                            // the structured failure reason (never silent)

    // Closure element handles (resolved through the reader's named() registry).
    vaml::StackPanel* squeezePanel     = nullptr;  // closure A host (Width=120)
    vaml::TextBlock*  wideText         = nullptr;  // closure A subject (40-char token)
    vaml::StackPanel* stretchPanel     = nullptr;  // closure B host (Width=300, vertical)
    vaml::Rectangle*  stretchChild     = nullptr;  // closure B: default Stretch, no Width
    vaml::Rectangle*  leftChild        = nullptr;  // closure B: identical authoring, HAlign=Left
    vaml::StackPanel* togglePanel      = nullptr;  // closure C host
    vaml::Rectangle*  collapsedSibling = nullptr;  // closure C: Visibility=Collapsed
    vaml::Rectangle*  visibleSibling   = nullptr;  // closure C: Visibility=Visible

    // --- Closure A (S827): the MEASURE half that is WPF-true today ----------
    // The finalizer's unit proof: compute_measured_desired returns the supplied
    // content extent UNCHANGED (no clamp, no alignment term) —
    // FrameworkElement.cs:4394.
    bool  finalizerUnclamped   = false;
    // WideText measured at a NARROW offer (the 120-wide slot) still reports its
    // wider CONTENT width — DesiredSize may exceed the offer.
    float wideTextDesiredWidth = 0.0f;   // ≈ 288.0 (40 × 0.6 × 12)
    bool  desiredExceedsOffer  = false;  // > 120.0
    // The arranged slot stays parent-bounded (today's truth; the clip/grow arms
    // are the [WPF DIVERGENCE] above, RESOLVED BY S832).
    float wideTextSlotWidth    = 0.0f;   // == 120.0
    float squeezePanelWidth    = 0.0f;   // == 120.0 (HAlign=Left honours Width)

    // --- Closure B (S828): Stretch fills at ARRANGE, measure is CONTENT -----
    float stretchContentDesired = 0.0f;  // 0.0 — measure = content, alignment-agnostic
    float stretchFilledSlot     = 0.0f;  // == the panel's cross-axis (300)
    float stretchPanelSlot      = 0.0f;  // 300.0 (the bounded cross-axis)
    float leftContentDesired    = 0.0f;  // == stretchContentDesired (same measure input)
    float leftFilledSlot        = 0.0f;  // == leftContentDesired (non-Stretch pins to desired)

    // --- Closure C (S829): Collapsed skip + reveal re-measure ---------------
    bool  collapsedSkipped     = false;  // dirty bit KEPT + contributed 0
    bool  visibleSiblingOk     = false;  // the visible sibling did measure (40)
    float collapsedHeightBefore = -1.0f; // 0.0 (skipped: no contribution)
    float toggleHeightBefore    = -1.0f; // panel height with ONE visible child
    bool  revealSignalled       = false; // the Visibility transition dirtied the
                                         // ANCESTOR CHAIN (WPF's own path)
    float collapsedHeightAfter  = -1.0f; // 40.0 (re-measured on reveal)
    float toggleHeightAfter     = -1.0f; // panel height with BOTH children
    bool  collapsedCleanAfter   = false; // the reveal cleared its dirty bit
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): Load
// `text` through a RETAINED VamlReader, finalize_visual_tree (the S120/S121
// hand-off that RENDERS the declared children), run the S123/S124
// Measure+Arrange pass, then assert each closure at the LIVE element layer.
// Returns { ok, root, reader, …proofs, detail }.
[[nodiscard]] inline ViewerGrowth11acPlanResult
run_viewer_growth_11ac_plan(const std::string& text)
{
    ViewerGrowth11acPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.s11ac.vaml did not produce a root");
        // The S120/S121 hand-off: transfer the declared children into their
        // parents' S121 visual-children lists so the tree RENDERS them.
        (void)r.reader->finalize_visual_tree();

        auto* rootUi = dynamic_cast<vaml::UIElement*>(r.root.get());
        if (rootUi == nullptr)
            throw std::runtime_error("11ac sample: root is not a UIElement");

        // --- Closure element handles (named() registry) ---------------------
        r.squeezePanel = dynamic_cast<vaml::StackPanel*>(r.reader->named("SqueezePanel"));
        r.wideText     = dynamic_cast<vaml::TextBlock*>(r.reader->named("WideText"));
        r.stretchPanel = dynamic_cast<vaml::StackPanel*>(r.reader->named("StretchPanel"));
        r.stretchChild = dynamic_cast<vaml::Rectangle*>(r.reader->named("StretchChild"));
        r.leftChild    = dynamic_cast<vaml::Rectangle*>(r.reader->named("LeftChild"));
        r.togglePanel  = dynamic_cast<vaml::StackPanel*>(r.reader->named("TogglePanel"));
        r.collapsedSibling =
            dynamic_cast<vaml::Rectangle*>(r.reader->named("CollapsedSibling"));
        r.visibleSibling =
            dynamic_cast<vaml::Rectangle*>(r.reader->named("VisibleSibling"));
        if (r.squeezePanel == nullptr || r.wideText == nullptr ||
            r.stretchPanel == nullptr || r.stretchChild == nullptr ||
            r.leftChild == nullptr || r.togglePanel == nullptr ||
            r.collapsedSibling == nullptr || r.visibleSibling == nullptr)
        {
            throw std::runtime_error(
                "11ac sample: a closure element is not reachable by x:Name");
        }

        // --- Closure A (S827): the finalizer's unit contract ----------------
        // WPF FrameworkElement.cs:4394 `Size unclippedDesiredSize = desiredSize;`
        // — MeasureCore passes MeasureOverride's content extent through
        // UNCHANGED (before the over-constrained clamp, which is the S832 half).
        {
            const vaml::Size fin = vaml::compute_measured_desired(vaml::Size{ 288.0f, 14.4f });
            r.finalizerUnclamped = (fin.width == 288.0f) && (fin.height == 14.4f);
        }

        // Measure WideText at a NARROW offer — the offered slot a 120-wide panel
        // would hand it. The content (one 40-char unbreakable token) exceeds it
        // and the report must NOT be clamped to the offer.
        r.wideText->Measure(vaml::Size{ 120.0f, vaml::Size::Infinite().height });
        r.wideTextDesiredWidth = r.wideText->desired_size().width;
        r.desiredExceedsOffer  = r.wideTextDesiredWidth > 120.0f;

        // --- The S123/S124 pass over the live tree --------------------------
        vaml::Size viewport{ 640.0f, 520.0f };
        rootUi->Measure(viewport);
        rootUi->Arrange(vaml::Rect{ 0.0f, 0.0f, viewport.width, viewport.height });

        // Closure A: the arranged slot stays parent-bounded (today's truth).
        r.wideTextSlotWidth = r.wideText->final_rect().width;
        r.squeezePanelWidth = r.squeezePanel->final_rect().width;

        // --- Closure B (S828): measure = CONTENT, arrange = FILL -------------
        // FrameworkElement.cs:4597-4620 — the Stretch arm is the CHILD's
        // ArrangeCore; the panel only hands out the cross-axis slot.
        r.stretchContentDesired = r.stretchChild->desired_size().width;
        r.stretchFilledSlot     = r.stretchChild->final_rect().width;
        r.stretchPanelSlot      = r.stretchPanel->final_rect().width;
        r.leftContentDesired    = r.leftChild->desired_size().width;
        r.leftFilledSlot        = r.leftChild->final_rect().width;

        // --- Closure C (S829): Collapsed skip --------------------------------
        // UIElement.cs:601-611 — the Collapsed element is NOT measured, its
        // dirty bit is KEPT (so a reveal re-measures) and it contributes nothing.
        r.collapsedHeightBefore = r.collapsedSibling->desired_size().height;
        r.visibleSiblingOk      = (r.visibleSibling->desired_size().height == 40.0f);
        r.toggleHeightBefore    = r.togglePanel->desired_size().height;
        r.collapsedSkipped      = r.collapsedSibling->is_measure_dirty() &&
                                  (r.collapsedHeightBefore == 0.0f);

        // --- Closure C (S829): the REVEAL through WPF's own signal path ------
        // UIElement.cs:2984-3010/3043-3056 → ensureVisible → signalDesiredSizeChange
        // → parent->OnChildDesiredSizeChanged (UIElement.cs:323-330). This call
        // is the ONLY trigger: no InvalidateMeasure() appears in this seam.
        (void)r.collapsedSibling->set_visibility(vaml::Visibility::Visible);
        r.revealSignalled = rootUi->is_measure_dirty() &&
                            r.togglePanel->is_measure_dirty();

        rootUi->Measure(viewport);
        rootUi->Arrange(vaml::Rect{ 0.0f, 0.0f, viewport.width, viewport.height });

        r.collapsedHeightAfter = r.collapsedSibling->desired_size().height;
        r.toggleHeightAfter    = r.togglePanel->desired_size().height;
        r.collapsedCleanAfter  = !r.collapsedSibling->is_measure_dirty();

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
// Arranges the root at `width`x`height` (so the S122/S124 layout rects are real)
// then encodes into `out`. Returns the number of DrawItems emitted.
[[nodiscard]] inline std::size_t
viewer_growth_11ac_render_plan(vaml::DependencyObject* root,
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
