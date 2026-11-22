// 11ac (dep:S829) — VIEWER-GROWTH(11ac): THE CORRECTED MEASURE SEMANTICS
// RENDERED LIVE.
//
// A corpus-shaped `sample.s11ac.vaml` authors the THREE S827/S828/S829 closures,
// and each must RESOLVE + RENDER through the generic renderer — the S222_1 /
// 11k-6 RELOAD re-demonstrated, proving NO regression from the measure-math
// changes:
//   1. S827 DesiredSize-BOUNDS — a child whose content exceeds its offered slot
//      still reports the wider CONTENT extent (WPF FrameworkElement.cs:4394
//      `Size unclippedDesiredSize = desiredSize;`): 40 unbreakable glyphs at
//      default_text_metric 0.6 × 12 = 288 DIPs against a 120 DIP offer.
//   2. S828 the Stretch/Fill PASS — alignment is read ONLY in arrange
//      (FrameworkElement.cs:4597-4620, the CHILD's ArrangeCore). Both closure-B
//      probes measure to CONTENT and only their ARRANGED rects differ: a
//      default-Stretch child fills the panel's bounded 300-wide cross-axis while
//      a Left-aligned sibling (IDENTICAL authoring — the same measure input)
//      is arranged at its own desired extent instead. Alignment-only cause.
//   3. S829 measure-ENTRY — the Collapsed sibling is SKIPPED with its dirty bit
//      kept (UIElement.cs:601-611) and the reveal re-measures it through WPF's
//      OWN signal path (the Visibility transition → signalDesiredSizeChange →
//      OnChildDesiredSizeChanged → parent InvalidateMeasure,
//      UIElement.cs:2984-3034 / 323-330). No harness-side invalidate is used.
//
// Under test (vaml-presentation-tests — links vaml-presentation so the S212
// encode_visual_slice render-plan is provable):
//   1. LOAD — the closure sample loads + finalizes through a RETAINED reader;
//      every closure element is reachable by x:Name.
//   2. CLOSURE-AT-LIVE-ELEMENT — each closure reads at the LIVE element layer.
//   3. RENDER-SURFACE CLOSURES — the S212 encoder PROVES the closures at the
//      emitted DrawItem layer (including the Collapsed sibling contributing NO
//      40-tall draw before the reveal and one AFTER it).
//
// NO-GAP (recorded, not silent): the LIVE WINDOW display of the closure sample
// (the [both-apps] proof) stays the on-machine local gate; this headless seam is
// the exact code path's CI-checkable half. S832 (11ad) landed the arrange-side
// child-wins-then-clip leg (compute_final_rect keeps the child at its unclipped
// desired + reports needs_clip — FrameworkElement.cs:4597-4624); the conditional
// clip scissor is the deferred S833. ADR-018 zero locks.

#include "doctest/doctest.h"

#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // StackPanel / Rectangle / TextBlock
#include "vaml/core/UIElement.h"          // Visibility / DesiredSize / final_rect
#include "vaml/core/Geometry.h"           // Size / Rect
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem / DrawItemKind
#include "viewer_growth_11ac.h"           // the 11ac ADR-021 consumer seam

#include <memory>
#include <string>
#include <vector>

using vaml::Rectangle;
using vaml::StackPanel;
using vaml::TextBlock;
using vaml::UIElement;
using vaml::vulkan::ui::DrawItem;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11ac_plan;
using vaml::viewer::viewer_growth_11ac_render_plan;

// The authored sample under test — mirrors vaml_viewer/sample.s11ac.vaml (the
// [both-apps] live-display proof). Kept as a verbatim inline copy so the
// headless test exercises EXACTLY the bytes the LIVE viewer loads (no drift).
const std::string kSampleS11ac =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"640\" Height=\"520\" Margin=\"6\" "
    "Background=\"#F0F0F0\">"
    "<TextBlock Text=\"11ac — the corrected measure semantics render live\" "
    "FontSize=\"16\" Margin=\"4\"/>"
    "<StackPanel x:Name=\"SqueezePanel\" Width=\"120\" "
    "HorizontalAlignment=\"Left\" Margin=\"4\" Background=\"#E0E0E0\">"
    "<TextBlock x:Name=\"WideText\" FontSize=\"12\" "
    "Text=\"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW\"/>"
    "</StackPanel>"
    "<StackPanel x:Name=\"StretchPanel\" Width=\"300\" "
    "HorizontalAlignment=\"Left\" Margin=\"4\" Background=\"#E8E8E8\">"
    "<Rectangle x:Name=\"StretchChild\" Height=\"40\" Fill=\"#0000FF\"/>"
    "<Rectangle x:Name=\"LeftChild\" Height=\"40\" "
    "HorizontalAlignment=\"Left\" Fill=\"#00FFFF\"/>"
    "</StackPanel>"
    "<StackPanel x:Name=\"TogglePanel\" Background=\"#D0D0D0\">"
    "<Rectangle x:Name=\"CollapsedSibling\" Height=\"40\" Fill=\"#FF0000\" "
    "Visibility=\"Collapsed\"/>"
    "<Rectangle x:Name=\"VisibleSibling\" Height=\"40\" Fill=\"#00FF00\"/>"
    "</StackPanel>"
    "</StackPanel>";

// The render-surface closure assertion helpers — identify draw items by their
// straight-alpha COLOR (each closure's authored Fill resolves to a UNIQUE RGB,
// so the emitted item for that element is found unambiguously without depending
// on layout coordinates — robust against the panels' own background fills).
namespace {

bool near(float a, float b, float eps = 0.01f)
{
    return (a > b - eps) && (a < b + eps);
}

// Finds the FIRST item of `kind` whose color is (r,g,b) (matched against the
// element's authored Fill scaled to 0..1). Returns nullptr when absent.
const DrawItem* find_solid_color(const std::vector<DrawItem>& items,
                                 float r, float g, float b)
{
    for (const auto& it : items)
    {
        if (it.kind == DrawItemKind::SolidRect &&
            near(it.r, r) && near(it.g, g) && near(it.b, b))
        {
            return &it;
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. LOAD — the closure sample loads + finalizes; closure elements reachable.
// ---------------------------------------------------------------------------

TEST_CASE("11ac: the corrected-measure sample (3 closures) loads cleanly")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ac_plan(kSampleS11ac);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    if (!r.ok)
    {
        INFO("detail: " << r.detail);
    }

    // Every closure element is reachable by x:Name.
    SHOULD(ShouldlyIntegral(r.squeezePanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.wideText != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.stretchPanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.stretchChild != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.leftChild != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.togglePanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.collapsedSibling != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.visibleSibling != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. CLOSURE-AT-LIVE-ELEMENT — each closure reads at the LIVE element layer.
// ---------------------------------------------------------------------------

TEST_CASE("11ac: the three closures resolve at the LIVE element layer")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ac_plan(kSampleS11ac);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // --- Closure A (S827): DesiredSize is the un-clamped CONTENT extent -----
    // The finalizer (LayoutSlot.cpp:77-101) returns MeasureOverride's output
    // UNCHANGED — no `min(d, available)`, no alignment term
    // (WPF FrameworkElement.cs:4394).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.finalizerUnclamped)).ShouldBe(1));
    // 40 × (0.6 × 12) = 288 against a 120 offer — the report EXCEEDS the offer
    // (WPF UIElement.cs:541-551 "soft constraint" / unclippedDesiredSize).
    SHOULD(ShouldlyIntegral(r.desiredExceedsOffer ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideTextDesiredWidth, 288.0f) ? 1 : 0).ShouldBe(1));
    // S832 (11ad) child-wins-then-clip: the WIDE child is arranged at its FULL
    // unclipped desired (288) inside the 120-wide slot — the panel honours its
    // authored Width (squeezePanelWidth 120), while the child keeps the size it
    // measured for (WPF ArrangeCore FE.cs:4597-4623) and reports needs_clip so
    // the deferred S833 scissor trims it to 120. (Pre-S832 the slot clamped the
    // child to 120 — the drift the 11ad arrange leg closed.)
    SHOULD(ShouldlyIntegral(near(r.squeezePanelWidth, 120.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideTextSlotWidth, 288.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.wideText->needs_clip() ? 1 : 0).ShouldBe(1));

    // --- Closure B (S828): measure = CONTENT, arrange = FILL ---------------
    // Both probes are authored IDENTICALLY (Height=40, no Width) — the SAME
    // measure input — and differ ONLY in HorizontalAlignment, so any difference
    // in the arranged rect is ALIGNMENT causation (the measure pass is
    // alignment-agnostic by construction).
    SHOULD(ShouldlyIntegral(near(r.stretchPanelSlot, 300.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.stretchContentDesired, r.leftContentDesired) ? 1 : 0)
               .ShouldBe(1));
    // Stretch: the arranged width is the panel's whole cross-axis, NOT the
    // measured content — the fill happened AT ARRANGE
    // (FrameworkElement.cs:4597-4620).
    SHOULD(ShouldlyIntegral(near(r.stretchFilledSlot, r.stretchPanelSlot) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.stretchContentDesired < r.stretchFilledSlot ? 1 : 0).ShouldBe(1));
    // Left: arranged AT its measured extent (non-Stretch arranges at the
    // unclippedDesiredSize — FrameworkElement.cs:4617-4623).
    SHOULD(ShouldlyIntegral(near(r.leftFilledSlot, r.leftContentDesired) ? 1 : 0).ShouldBe(1));
    // The differential is real: same measure input, different arranged width.
    SHOULD(ShouldlyIntegral(r.leftFilledSlot < r.stretchFilledSlot ? 1 : 0).ShouldBe(1));

    // --- Closure C (S829): Collapsed skip + reveal re-measure ---------------
    // Skipped: contributed 0, dirty bit KEPT (UIElement.cs:601-611).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.collapsedSkipped)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.collapsedHeightBefore, 0.0f) ? 1 : 0).ShouldBe(1));
    // The panel measured with ONE child contributing (40), so the Collapsed
    // sibling contributed NOTHING.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.visibleSiblingOk)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.toggleHeightBefore, r.collapsedHeightBefore + 40.0f)
                                ? 1 : 0).ShouldBe(1));
    // Revealed: the Visibility transition alone dirtied the ancestor chain (the
    // WPF signal path — the seam performed NO invalidate).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.revealSignalled)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.collapsedHeightAfter, 40.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.toggleHeightAfter, r.toggleHeightBefore + 40.0f)
                                ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.collapsedCleanAfter)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. RENDER-SURFACE CLOSURES — the S212 encoder proves the closures at the
//    emitted DrawItem layer (the generic renderer — S222_1 RELOAD).
// ---------------------------------------------------------------------------

TEST_CASE("11ac: the render-plan emits the closures at the DrawItem layer")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ac_plan(kSampleS11ac);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // NOTE: the runner already REVEALED the Collapsed sibling (closure C), so
    // this render pass is the post-reveal tree: the red sibling must now draw
    // its 40-tall box, while the blue/cyan closure-B probes draw their DISTINCT
    // arranged widths (the alignment-only differential, visible to the renderer).
    std::vector<DrawItem> items;
    const std::size_t n = viewer_growth_11ac_render_plan(r.root.get(), items, 640.0f, 520.0f);
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));

    // Closure B (S828) at the DrawItem layer: StretchChild (#0000FF) is emitted
    // at the panel's whole 300-wide cross-axis while LeftChild (#00FFFF) is
    // emitted at its own arranged (non-Stretch) width — the ARRANGEMENT, not the
    // measure, decided both (identical measure inputs; alignment-only cause).
    const DrawItem* stretch = find_solid_color(items, 0.0f, 0.0f, 1.0f);
    SHOULD(ShouldlyIntegral(stretch != nullptr).ShouldBe(1));
    if (stretch != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(stretch->w, 300.0f) ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(near(stretch->h, 40.0f) ? 1 : 0).ShouldBe(1));
    }
    const DrawItem* left = find_solid_color(items, 0.0f, 1.0f, 1.0f);
    SHOULD(ShouldlyIntegral(left != nullptr).ShouldBe(1));
    if (left != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(left->w, r.leftFilledSlot) ? 1 : 0).ShouldBe(1));
        if (stretch != nullptr)
        {
            SHOULD(ShouldlyIntegral(left->w < stretch->w ? 1 : 0).ShouldBe(1));
        }
    }

    // Closure C (S829) at the DrawItem layer: the REVEALED sibling (#FF0000)
    // now draws a 40-tall box, and its visible partner (#00FF00) still does.
    const DrawItem* revealed = find_solid_color(items, 1.0f, 0.0f, 0.0f);
    SHOULD(ShouldlyIntegral(revealed != nullptr).ShouldBe(1));
    if (revealed != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(revealed->h, 40.0f) ? 1 : 0).ShouldBe(1));
    }
    const DrawItem* visible = find_solid_color(items, 0.0f, 1.0f, 0.0f);
    SHOULD(ShouldlyIntegral(visible != nullptr).ShouldBe(1));

    // Closure A (S827) at the DrawItem layer: the wide TextBlock's glyph run is
    // emitted (the squeezed content renders) — count the TextGlyph items so the
    // render-surface proof covers the third closure too.
    std::size_t glyphs = 0;
    for (const auto& it : items)
    {
        if (it.kind == DrawItemKind::TextGlyph)
        {
            ++glyphs;
        }
    }
    SHOULD(ShouldlyIntegral(glyphs > 0u ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3b. THE COLLAPSED GATE AT THE DRAWITEM LAYER — a dedicated pre-reveal pass.
//     (The runner reveals in-place, so this case drives the collapse state
//     explicitly: the Collapsed sibling must contribute NO draw at all.)
// ---------------------------------------------------------------------------

TEST_CASE("11ac: the Collapsed sibling contributes no draw before the reveal")
{
    vaml::register_builtin_elements();

    // Load + finalize WITHOUT the runner (the runner performs the reveal).
    vaml::VamlReader reader;
    auto root = reader.Load(kSampleS11ac);
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(1));
    (void)reader.finalize_visual_tree();

    auto* rootUi = dynamic_cast<UIElement*>(root.get());
    SHOULD(ShouldlyIntegral(rootUi != nullptr).ShouldBe(1));
    if (rootUi == nullptr)
    {
        return;
    }

    auto* collapsed = dynamic_cast<Rectangle*>(reader.named("CollapsedSibling"));
    auto* toggle    = dynamic_cast<StackPanel*>(reader.named("TogglePanel"));
    SHOULD(ShouldlyIntegral(collapsed != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(toggle != nullptr).ShouldBe(1));

    std::vector<DrawItem> items;
    (void)viewer_growth_11ac_render_plan(root.get(), items, 640.0f, 520.0f);

    // The Collapsed sibling (#FF0000) is ABSENT from the draw list (the encoder's
    // SkipCollapsed rule) while its visible partner (#00FF00) draws.
    SHOULD(ShouldlyIntegral(find_solid_color(items, 1.0f, 0.0f, 0.0f) == nullptr)
               .ShouldBe(1));
    SHOULD(ShouldlyIntegral(find_solid_color(items, 0.0f, 1.0f, 0.0f) != nullptr)
               .ShouldBe(1));

    // And the measure-side fact behind it: skipped, dirty bit KEPT.
    SHOULD(ShouldlyIntegral(collapsed->is_measure_dirty() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(collapsed->desired_size().height, 0.0f) ? 1 : 0)
               .ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(toggle->desired_size().height, 40.0f) ? 1 : 0)
               .ShouldBe(1));
}
