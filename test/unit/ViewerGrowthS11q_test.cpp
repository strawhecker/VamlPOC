// S622 (dep:S621) — VIEWER-GROWTH(11q): the SEED-context (S620+S621) is exercised
// on an IN-REPO doc that reproduces the EXACT failure class of the previously-
// black external corpus files (Splash/OverlayWindow/ShadowLocationIndicator/
// SignalDisplayView/ReadingControl/…): bound-but-DATA-EMPTY text +
// background/foreground bound to {Binding} with NO ViewModel. Before S11q these
// rendered BLACK/TRANSPARENT (every {Binding} resolved against a null data
// context -> Class B NotReadable -> default invisible DPs). With the seed, the
// doc renders DISCERNIBLE layout — visible placeholder text + visible opaque
// fills.
//
// Under test (headless Rail 1, vaml-presentation-tests — it links vaml-presentation
// so the S212 encode_visual_slice render-plan is provable; the ADR-021 consumer
// path viewer_growth_s11q.h):
//   1. LOAD+SEED — the seeded sample (sample.seed.vaml) loads through a RETAINED
//      reader; the SEED context (typed, S620+S621) is injected BEFORE
//      BindExtensions; the seed VM + live bindings (reader-owned) stay armed.
//   2. RENDER-PLAN — the S212 encode_visual_slice emits NON-DEGENERATE DrawItems
//      (the visible placeholder text + the opaque Background fill) — the
//      DISCERNIBLE-LAYOUT proof. A black/empty doc would emit only degenerate
//      (zero/negative-size) items.
//
// The LIVE WINDOW display (the [both-apps] proof) stays the on-machine local
// gate; this headless seam is the exact code path's CI-checkable half. This
// also closes Sprint 11q with ZERO new locks (ADR-018) + the SURVIVOR gate.
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"            // register_builtin_elements
#include "vaml/core/UIElement.h"
#include "vaml/vulkan/ui/UiScene.h"       // DrawItem / DrawItemKind
#include "viewer_growth_s11q.h"           // S622 — the ADR-021 consumer seam

#include <string>
#include <vector>

using vaml::UIElement;
using vaml::viewer::run_viewer_growth_s11q_plan;
using vaml::viewer::viewer_growth_s11q_render_plan;

namespace {

int count_kind(const std::vector<vaml::vulkan::ui::DrawItem>& items,
               vaml::vulkan::ui::DrawItemKind kind)
{
    int n = 0;
    for (const auto& it : items)
        if (it.kind == kind)
            ++n;
    return n;
}

} // namespace

// The in-repo S11q SEED sample — mirrors vaml_viewer/sample.seed.vaml (the
// [both-apps] live-display proof): a bound-but-DATA-EMPTY doc (text +
// background/foreground bound to {Binding} with NO VM).
const std::string kSampleSeed =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"SeedRoot\" Width=\"440\" Height=\"360\" Margin=\"6\" "
    "Background=\"{Binding Bg}\">"
    "<TextBlock Text=\"{Binding Title}\" FontSize=\"20\" Margin=\"4\"/>"
    "<TextBlock Text=\"{Binding Subtitle}\" FontSize=\"12\" Opacity=\"{Binding Op}\" Margin=\"4\"/>"
    "<Rectangle Fill=\"{Binding Fill}\" Width=\"120\" Height=\"60\" Margin=\"4\"/>"
    "</StackPanel>";

// ---------------------------------------------------------------------------
// 1. LOAD+SEED — the previously-black doc now seeds + binds + finalizes
// ---------------------------------------------------------------------------

TEST_CASE("S622: the bound-but-data-empty sample loads + seeds + binds via the plan")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_s11q_plan(kSampleSeed);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.reader != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.seedVm != nullptr ? 1 : 0).ShouldBe(1));
    // The four {Binding} leaf paths (Bg/Title/Subtitle/Op/Fill = 5) are seeded.
    SHOULD(ShouldlyIntegral(r.seeded == 5u).ShouldBe(1));
    // The S252 BindExtensions bound the {Binding} rows against the seed VM.
    SHOULD(ShouldlyIntegral(r.bound == 5u).ShouldBe(1));
    // The root is a UIElement (finalize_visual_tree transferred it renderable).
    auto* uiRoot = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. RENDER-PLAN — the S212 encoder emits NON-DEGENERATE items (discernible layout)
// ---------------------------------------------------------------------------

TEST_CASE("S622: the seeded sample renders NON-DEGENERATE items (discernible layout)")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_s11q_plan(kSampleSeed);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t visible =
        viewer_growth_s11q_render_plan(r.root.get(), items, 440.0f, 360.0f);
    // The render-plan returns the NON-DEGENERATE (renderable) count — a black/
    // empty doc returns ~0; the seeded doc emits its visible placeholders.
    SHOULD(ShouldlyIntegral(visible > 0u ? 1 : 0).ShouldBe(1));
    // The Root StackPanel's seeded OPAQUE Background emits a SolidRect fill, and
    // the TextBlock placeholders emit their Text glyph draws (the visible text).
    SHOULD(ShouldlyIntegral(count_kind(items, vaml::vulkan::ui::DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, vaml::vulkan::ui::DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));
}
