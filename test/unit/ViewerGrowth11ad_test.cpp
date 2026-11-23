// 11ad (dep:S835) — VIEWER-GROWTH(11ad): ARRANGE-SQUEEZE + CLIPTOBOUNDS +
// ARRANGE-GUARDS RENDERED LIVE.
//
// A corpus-shaped sample.s11ad.vaml authors the 11ad closures, and each must
// RESOLVE + RENDER through the generic renderer (the S222_1 / 11k-6 RELOAD
// re-demonstrated, proving NO regression from the 11ad arrange/clip/guard
// changes):
//   1. S832 SQUEEZE — a Rectangle Width=200 inside a 120-wide panel is arranged
//      at its FULL unclipped 200 + reports needs_clip; the S833 scissor is the
//      120 slot-client (FrameworkElement.cs:4597-4606).
//   2. S833/S834 CLIP-TO-BOUNDS — a FITTING ClipToBounds=true element is STILL
//      clipped (forced mode); a FITTING ClipToBounds=false element is NOT
//      (FE.cs:4874 `NeedsClipBounds || ClipToBounds`).
//   3. S835 ARRANGE GUARDS — a Collapsed sibling is SKIPPED at arrange while its
//      visible partner draws; a non-finite arrange slot THROWS
//      std::invalid_argument (UIElement.cs:798-809).
//
// Under test (vaml-presentation-tests): the S212 encode_visual_slice render-plan
// is provable. ADR-018 zero locks.

#include "doctest/doctest.h"

#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // StackPanel / Rectangle
#include "vaml/core/UIElement.h"          // Visibility / needs_clip / clip_rect
#include "vaml/core/Geometry.h"           // Size / Rect
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem / DrawItemKind
#include "viewer_growth_11ad.h"           // the 11ad ADR-021 consumer seam

#include <memory>
#include <string>
#include <vector>

using vaml::Rectangle;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::vulkan::ui::DrawItem;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11ad_plan;
using vaml::viewer::viewer_growth_11ad_render_plan;

// The authored sample under test — the SAME element markup as
// vaml_viewer/sample.s11ad.vaml (the [both-apps] live-display proof).
const std::string kSampleS11ad =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"640\" Height=\"520\" Margin=\"6\" "
    "Background=\"#F0F0F0\">"
    "<TextBlock Text=\"11ad — arrange-squeeze + ClipToBounds + arrange guards render live\" "
    "FontSize=\"16\" Margin=\"4\"/>"
    "<StackPanel x:Name=\"SqueezePanel\" Width=\"120\" "
    "HorizontalAlignment=\"Left\" Margin=\"4\" Background=\"#E0E0E0\">"
    "<Rectangle x:Name=\"SqueezeBox\" Width=\"200\" Height=\"40\" Fill=\"#FF0000\"/>"
    "</StackPanel>"
    "<StackPanel x:Name=\"ClipHostPanel\" Width=\"400\" "
    "HorizontalAlignment=\"Left\" Margin=\"4\" Background=\"#E8E8E8\">"
    "<Rectangle x:Name=\"ClipBox\" Width=\"100\" Height=\"60\" "
    "HorizontalAlignment=\"Left\" ClipToBounds=\"True\" Fill=\"#0000FF\"/>"
    "<Rectangle x:Name=\"NoClipBox\" Width=\"100\" Height=\"60\" "
    "HorizontalAlignment=\"Left\" ClipToBounds=\"False\" Fill=\"#00FFFF\"/>"
    "</StackPanel>"
    "<StackPanel x:Name=\"TogglePanel\" Background=\"#D0D0D0\">"
    "<Rectangle x:Name=\"CollapsedSibling\" Height=\"40\" Fill=\"#FF00FF\" "
    "Visibility=\"Collapsed\"/>"
    "<Rectangle x:Name=\"VisibleSibling\" Height=\"40\" Fill=\"#00FF00\"/>"
    "</StackPanel>"
    "<StackPanel x:Name=\"ProbeHost\" Width=\"120\" "
    "HorizontalAlignment=\"Left\" Margin=\"4\" Background=\"#F8F8F8\">"
    "<Rectangle x:Name=\"NonFiniteProbe\" Width=\"50\" Height=\"50\" Fill=\"#808080\"/>"
    "</StackPanel>"
    "</StackPanel>";

namespace {

bool near(float a, float b, float eps = 0.01f)
{
    return (a > b - eps) && (a < b + eps);
}

// Finds the FIRST SolidRect of color (r,g,b) — each closure's authored Fill
// resolves to a UNIQUE RGB, so the item is found unambiguously.
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

TEST_CASE("11ad: the arrange/clip/guard sample loads cleanly")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ad_plan(kSampleS11ad);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    if (!r.ok)
    {
        INFO("detail: " << r.detail);
    }

    SHOULD(ShouldlyIntegral(r.squeezePanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.squeezeBox != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.clipHostPanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.clipBox != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.noClipBox != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.togglePanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.collapsedSibling != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.visibleSibling != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.nonFiniteProbe != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. CLOSURE-AT-LIVE-ELEMENT — each closure reads at the LIVE element layer.
// ---------------------------------------------------------------------------

TEST_CASE("11ad: the three closures resolve at the LIVE element layer")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ad_plan(kSampleS11ad);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // --- Closure A (S832): child-wins-then-clip ----------------------------
    SHOULD(ShouldlyIntegral(near(r.squeezeBoxWidth, 200.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.squeezeBoxNeedsClip ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.squeezeBoxClipWidth, 120.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.squeezePanelWidth, 120.0f) ? 1 : 0).ShouldBe(1));

    // --- Closure B (S833/S834): CLIP-TO-BOUNDS forced vs absent -----------
    SHOULD(ShouldlyIntegral(r.clipBoxClipToBounds ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.clipBoxNeedsClip ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(r.noClipBoxClipToBounds ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(r.noClipBoxNeedsClip ? 1 : 0).ShouldBe(0));

    // --- Closure C (S835): Collapsed skip + Inf/NaN rejection -------------
    SHOULD(ShouldlyIntegral(r.collapsedIsCollapsed ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.collapsedSkipped ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.visibleHeight, 40.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.nonFiniteRejected ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. RENDER-SURFACE — the S212 encoder proves the closures at the DrawItem
//    layer (the generic renderer — S222_1 RELOAD).
// ---------------------------------------------------------------------------

TEST_CASE("11ad: the render-plan emits the closures at the DrawItem layer")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ad_plan(kSampleS11ad);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    std::vector<DrawItem> items;
    const std::size_t n = viewer_growth_11ad_render_plan(r.root.get(), items, 640.0f, 520.0f);
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));

    // Closure A (S832): SqueezeBox (#FF0000) draws at its FULL 200 width with a
    // 120-wide scissor (slot-client).
    const DrawItem* squeeze = find_solid_color(items, 1.0f, 0.0f, 0.0f);
    SHOULD(ShouldlyIntegral(squeeze != nullptr).ShouldBe(1));
    if (squeeze != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(squeeze->w, 200.0f) ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(squeeze->clipActive ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(near(squeeze->clipW, 120.0f) ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(near(squeeze->clipH, 40.0f) ? 1 : 0).ShouldBe(1));
    }

    // Closure B (S833/S834): ClipBox (#0000FF) FITS (w≈100) yet is clipped
    // (clipActive true; clipW = its 400-wide slot-client), while NoClipBox
    // (#00FFFF) FITS and is NOT clipped (clipActive false).
    const DrawItem* clipBox = find_solid_color(items, 0.0f, 0.0f, 1.0f);
    SHOULD(ShouldlyIntegral(clipBox != nullptr).ShouldBe(1));
    if (clipBox != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(clipBox->w, 100.0f) ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(clipBox->clipActive ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(near(clipBox->clipW, 400.0f) ? 1 : 0).ShouldBe(1));
    }
    const DrawItem* noClipBox = find_solid_color(items, 0.0f, 1.0f, 1.0f);
    SHOULD(ShouldlyIntegral(noClipBox != nullptr).ShouldBe(1));
    if (noClipBox != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(noClipBox->w, 100.0f) ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(noClipBox->clipActive ? 1 : 0).ShouldBe(0));
    }

    // Closure C (S835): the Collapsed sibling (#FF00FF) contributes NO draw
    // while its visible partner (#00FF00) draws its 40-tall box.
    SHOULD(ShouldlyIntegral(find_solid_color(items, 1.0f, 0.0f, 1.0f) == nullptr).ShouldBe(1));
    const DrawItem* visible = find_solid_color(items, 0.0f, 1.0f, 0.0f);
    SHOULD(ShouldlyIntegral(visible != nullptr).ShouldBe(1));
    if (visible != nullptr)
    {
        SHOULD(ShouldlyIntegral(near(visible->h, 40.0f) ? 1 : 0).ShouldBe(1));
    }
}
