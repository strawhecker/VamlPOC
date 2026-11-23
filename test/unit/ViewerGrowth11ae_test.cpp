// 11ae (dep:S843) - VIEWER-GROWTH(11ae): S839/S840 + S841 + S842 LAYOUT CLOSURES
// RENDERED LIVE, driven through UIElement::UpdateLayout() (the S843 entry).
//
// A corpus-shaped sample.s11ae.vaml authors the 11ae closures; each must RESOLVE
// + RENDER through the generic renderer (the S222_1 / 11k-6 RELOAD re-demonstrated):
//   1. S839/S840 - a child whose UNCLIPPED desired (200) exceeds its offered slot
//      (120) re-lays-out its parent on a size change; the unchanged sibling stays
//      measure-clean.
//   2. S841 - the squeezed child RenderSize (200) is DISTINCT from its slot (120);
//      G3 fires: ActualWidth == 200 + SizeChanged raised.
//   3. S842 - after invalidating only the wide child, the unchanged sibling is not
//      redundantly re-measured (the element constraint-cache short-circuits).
//
// ADR-018 zero locks.

#include "doctest/doctest.h"

#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Geometry.h"
#include <vaml/vulkan/ui/UiScene.h>
#include "viewer_growth_11ae.h"

#include <cmath>
#include <string>
#include <vector>

using vaml::vulkan::ui::DrawItem;
using vaml::vulkan::ui::DrawItemKind;
using vaml::viewer::run_viewer_growth_11ae_plan;
using vaml::viewer::viewer_growth_11ae_render_plan;

const std::string kSampleS11ae =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "x:Name=\"Root\" Width=\"640\" Height=\"520\" Background=\"#F0F0F0\">"
    "<StackPanel x:Name=\"SqueezePanel\" Width=\"120\" HorizontalAlignment=\"Left\" Background=\"#E0E0E0\">"
    "<Rectangle x:Name=\"WideChild\" Width=\"200\" Height=\"40\" Fill=\"#FF0000\"/>"
    "<Rectangle x:Name=\"CleanSibling\" Width=\"50\" Height=\"40\" Fill=\"#00AA00\"/>"
    "</StackPanel>"
    "</StackPanel>";

namespace {
bool near(float a, float b, float eps = 0.01f)
{
    return (a > b - eps) && (a < b + eps);
}
const DrawItem* find_solid(const std::vector<DrawItem>& items, float r, float g, float b)
{
    for (const auto& it : items)
    {
        if (it.kind == DrawItemKind::SolidRect && near(it.r, r) && near(it.g, g) && near(it.b, b))
        {
            return &it;
        }
    }
    return nullptr;
}
} // namespace

TEST_CASE("11ae: the layout-closure sample loads cleanly")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ae_plan(kSampleS11ae);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    if (!r.ok) { INFO("detail: " << r.detail); }
    SHOULD(ShouldlyIntegral(r.squeezePanel != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.wideChild != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.cleanSibling != nullptr).ShouldBe(1));
}

TEST_CASE("11ae: S839/S840 + S841 + S842 resolve at the LIVE element layer via UpdateLayout")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ae_plan(kSampleS11ae);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.childExceedsSlot ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideUnclipped, 200.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.offeredSlot, 120.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.siblingStaysClean ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.renderDistinctFromSlot ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideRenderWidth, 200.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideSlotWidth, 120.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(near(r.wideActualWidth, 200.0f) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.sizeChangedCount >= 1 ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.wideRecomputedOnDirty ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.siblingNotRemeasured ? 1 : 0).ShouldBe(1));
}

TEST_CASE("11ae: the render-plan emits the closures at the DrawItem layer")
{
    vaml::register_builtin_elements();
    auto r = run_viewer_growth_11ae_plan(kSampleS11ae);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    std::vector<DrawItem> items;
    const std::size_t n = viewer_growth_11ae_render_plan(r.root.get(), items, 640.0f, 520.0f);
    SHOULD(ShouldlyIntegral(n > 0u ? 1 : 0).ShouldBe(1));
    const DrawItem* wide = find_solid(items, 1.0f, 0.0f, 0.0f);
    SHOULD(ShouldlyIntegral(wide != nullptr).ShouldBe(1));
    if (wide != nullptr) { SHOULD(ShouldlyIntegral(near(wide->w, 200.0f) ? 1 : 0).ShouldBe(1)); }
}
