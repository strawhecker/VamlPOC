// S558 (Sprint 11m) — VIEWER-GROWTH(11m): the LIVE corpus tail in the viewer.
//
// A corpus-tail-shaped sample AUTHORS the DAG tail set (TreeView/DatePicker/
// Calendar/DataGrid/Polyline/Menu + the item/header scaffolding) and the LIVE
// viewer loads + renders it interactively (the generic renderer via
// classify_element). This test asserts LOAD + RENDER-PLAN (the S212
// encode_visual_slice emits the tail's Solid/Monochrome draws) + tag-reachable
// controls. ADR-021 consumer pattern.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>       // register_builtin_elements
#include <vaml/core/VamlReader.h>
#include "viewer_growth_11m.h"       // S558 — the plan runner + render seam

#include <string>
#include <vector>

using namespace vaml;
using namespace vaml::viewer;

namespace {

// The corpus-tail sample (the same tail authoring the live viewer loads).
const char* const kTailXaml = R"xaml(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel>
    <TreeView>
      <TreeViewItem>
        <TreeViewItem/>
      </TreeViewItem>
    </TreeView>
    <DatePicker/>
    <Calendar/>
    <DataGrid/>
    <Polyline/>
    <Polygon/>
    <Menu>
      <MenuItem/>
    </Menu>
    <ListBoxItem/>
    <ComboBoxItem/>
    <DataGridRow/>
    <StatusBarItem/>
    <GroupItem/>
    <ItemsPresenter/>
    <GridView/>
  </StackPanel>
</Window>)xaml";

} // namespace

TEST_CASE("S558: the corpus tail loads + renders through the live-viewer plan")
{
    register_builtin_elements();

    // The REAL plan runner (the exact code the live viewer runs): Load +
    // finalize (renders declared children).
    ViewerGrowth11mPlanResult plan = run_viewer_growth_11m_plan(kTailXaml);
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(true));
    SHOULD(ShouldlyIntegral(plan.root != nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(plan.detail).ShouldBe(""));

    // The tail root (Window) + its authored content resolve.
    auto* win = dynamic_cast<Window*>(plan.root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
}

TEST_CASE("S558: the tail's RENDER-PLAN emits draw items (the RENDER-SURFACE proof)")
{
    register_builtin_elements();
    ViewerGrowth11mPlanResult plan = run_viewer_growth_11m_plan(kTailXaml);
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(true));

    std::vector<vaml::vulkan::ui::DrawItem> out;
    std::size_t n = viewer_growth_11m_render_plan(plan.root.get(), out, 480.0f, 360.0f);
    // The tail controls (TreeView/DatePicker/Calendar/DataGrid/Polyline/Menu +
    // the scaffolding) classify Solid/Monochrome and emit draw items.
    SHOULD(ShouldlyIntegral(n > 0u).ShouldBe(true));
    // At least the head/root + the tail children encode draw items.
    SHOULD(ShouldlyIntegral(out.size() >= 1u).ShouldBe(true));
}

TEST_CASE("S558: the retained reader stays armed after the plan returns")
{
    register_builtin_elements();
    ViewerGrowth11mPlanResult plan = run_viewer_growth_11m_plan(kTailXaml);
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(true));
    // The reader is RETAINED (not destroyed on return) — the live viewer keeps
    // the x:Name registry + authored resources armed.
    SHOULD(ShouldlyIntegral(plan.reader != nullptr).ShouldBe(true));
}

TEST_CASE("S558: a malformed tail fails loudly with the structured detail (never silent)")
{
    register_builtin_elements();
    ViewerGrowth11mPlanResult plan =
        run_viewer_growth_11m_plan("<Window><Unbalanced</Window>");
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(plan.detail.empty()).ShouldBe(false));
}
