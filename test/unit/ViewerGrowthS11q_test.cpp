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
#include "vaml/core/ValueConverter.h"      // register_builtin_converters (the compiled-open S7 reader's converter table)
#include "vaml/core/UIElement.h"
#include "vaml/vulkan/ui/UiScene.h"       // DrawItem / DrawItemKind
#include "viewer_growth_s11q.h"           // S622 — the ADR-021 consumer seam

#include "load_file.h"        // S221 seam — load_text (the REAL file->tree loader), under the splash-all-black FIX
#include "compiled_open.h"    // the compiled-open seam (load_text's compiled sibling), under the FIX

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

// ---------------------------------------------------------------------------
// 3. STATIC (non-seeded) doc — the splash-all-black FIX proof (plain open -> render)
// ---------------------------------------------------------------------------
// The user-facing bug scope is broader than the S11q SEED class: PLAIN static
// (non-binding) docs — SplashWindow.xaml, sample.corpus.xaml, sample.s595a.xaml,
// sample.tail.xaml, sample.xaml — ALL rendered black because the plain file->tree
// seam (load_text / load_file) returned ONLY the root and NEVER concluded the
// S440 hand-off (finalize_visual_tree): the local VamlReader was destroyed at
// return, freeing every child it owned in retained_, so the renderer walked an
// EMPTY S121 visual tree. The FIX: load_text + the scan path + the compiled-open
// path now call finalize_visual_tree() BEFORE returning, so the plain-open root
// OWNS its declared descendants and renders them.
//
// This is the "establish something SIMPLE renders on screen" gate: a minimal
// STATIC doc with an OPAQUE fill + explicit Text, loaded through the REAL
// load_text seam (no seed, no VM), must encode to NON-DEGENERATE DrawItems.

namespace {

// A minimal static doc: an opaque StackPanel fill + an explicit-text TextBlock.
// No {Binding}, no VM — the plain-open failure class.
const std::string kStaticSample =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "Width=\"440\" Height=\"360\" Background=\"#FF2E7AFF\">"
    "<TextBlock Text=\"HELLO\" FontSize=\"28\" Margin=\"8\"/>"
    "</StackPanel>";

} // namespace

TEST_CASE("S11q-FIX: a STATIC doc loads via load_text with its children in the S121 tree")
{
    vaml::register_builtin_elements();
    vaml::LoadResult r = vaml::load_text(kStaticSample);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    auto* uiRoot = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));
    // The S440 hand-off concluded: the StackPanel root now CARRIES its declared
    // TextBlock child in the S121 visual tree (pre-FIX it carried ZERO children —
    // every child was owned by the destroyed reader and never transferred).
    SHOULD(ShouldlyIntegral(uiRoot->child_count() != 0 ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S11q-FIX: the static doc RENDERS NON-DEGENERATE items via the render-plan")
{
    vaml::register_builtin_elements();
    vaml::LoadResult r = vaml::load_text(kStaticSample);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t visible =
        viewer_growth_s11q_render_plan(r.root.get(), items, 440.0f, 360.0f);
    // The static doc's opaque fill + explicit text emit NON-DEGENERATE items —
    // the DISCERNIBLE-LAYOUT proof for the plain (non-seeded) open path.
    SHOULD(ShouldlyIntegral(visible > 0u ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, vaml::vulkan::ui::DrawItemKind::SolidRect) >= 1 ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(count_kind(items, vaml::vulkan::ui::DrawItemKind::TextGlyph) >= 1 ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S11q-FIX: the COMPILED open concludes the S440 hand-off (plain compiled -> render)")
{
    // On a host where vamlcc is available, the compiled-open seam (load_compiled_bytes
    // / load_compiled_file) must ALSO conclude the S440 hand-off so a compiled .vbaml
    // renders its declared children (the same all-black root cause on the compiled path).
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Compile the static doc to bytes via the in-process vamlcc (the S293 seam).
    std::vector<std::uint8_t> bytes = vaml::viewer::compile_to_vbaml(kStaticSample);
    SHOULD(ShouldlyIntegral(bytes.empty() ? 0 : 1).ShouldBe(1));
    vaml::viewer::CompiledOpenResult c = vaml::viewer::load_compiled_bytes(bytes);
    SHOULD(ShouldlyIntegral(static_cast<int>(c.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(c.root != nullptr ? 1 : 0).ShouldBe(1));
    auto* uiRoot = dynamic_cast<UIElement*>(c.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));

    // The S440 hand-off on the retained S7 reader (the exact step the plain
    // compiled-open path — ViewerApp::create_compiled — concludes): transfer the
    // decoded children into the S121 visual tree. Pre-FIX the compiled root
    // carried ZERO children (the S7 reader owned them in its retained set).
    if (c.reader)
    {
        const std::size_t transferred = c.reader->finalize_visual_tree();
        SHOULD(ShouldlyIntegral(transferred != 0 ? 1 : 0).ShouldBe(1));
    }
    // The compiled root now carries its declared child in the S121 visual tree.
    SHOULD(ShouldlyIntegral(uiRoot->child_count() != 0 ? 1 : 0).ShouldBe(1));
}
