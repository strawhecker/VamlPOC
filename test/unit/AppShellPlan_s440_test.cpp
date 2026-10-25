// S440 (dep:S408) — APP-SHELL: the authored MainView.vaml + the S120/S121 gap
// closure rendered via the public surface. HEADLESS Rail 1 — proves the S11
// headline WITHOUT a live Vulkan runtime, through the S440 seam
// (vaml_viewer/app_shell.h, the ADR-021 consumer path — compiled from the
// vaml_viewer include into the presentation-tests target):
//   (1) the pure app-shell decision table (app_shell_plan: RenderShell/Report);
//   (2) run_app_shell_plan — the REAL plan runner Loads MainView.vaml through a
//       RETAINED VamlReader and calls finalize_visual_tree(), the S440 core
//       hand-off that TRANSFERS every declared child into its parent's SI21
//       visual-children list — so the Window root RENDERS its declared
//       Grid/StackPanel/Text/TextBox/Button/Slider/CheckBox subtree (NOT just
//       the root). This is the "make all S10 controls functional with children
//       defined in a running app" directive of sprint 11, closed at its real
//       home (the reader) instead of a viewer workaround.
//   (3) the S440 REAL Window (re-based onto UIElement — resolves the S087
//       MINIMAL REAL seed) participates in the visual tree: its single content
//       child (the RootGrid) is a UIElement child with the declared controls.
//
// The LIVE window proof remains [both apps, not CI] + feeds S445 (the end-to-end
// click/type/slide/check vertical slice). ADR-018 zero locks.

#include "doctest/doctest.h"

#include "app_shell.h"   // the S440 seam (app_shell_plan + run_app_shell_plan)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include "vaml/core/UIElement.h"     // UIElement + the S121 child API (child_count/child_at)
#include "vaml/core/Element.h"       // Window / Grid / StackPanel / Text / Button / TextBox / Slider / CheckBox
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <string>

using vaml::viewer::AppShellAction;
using vaml::viewer::run_app_shell_plan;
using vaml::viewer::app_shell_plan;

namespace {

// Walk the finalized visual tree, counting every child in an S121 list. A
// finalized VAML tree must have child_count() > 0 at the root (the Window -> its
// Grid) and the Grid must host its declared controls as S121 children. This is
// the S120/S121 gap-closure proof.
std::size_t depth_first_total_children(vaml::UIElement& el)
{
    std::size_t total = 0;
    for (std::size_t i = 0; i < el.child_count(); ++i)
    {
        ++total;
        if (vaml::UIElement* c = el.child_at(i))
        {
            total += depth_first_total_children(*c);
        }
    }
    return total;
}

} // namespace

TEST_CASE("S440 app-shell: pure decision table")
{
    // A clean load+finalize -> RenderShell; a failed load -> Report.
    SHOULD(ShouldlyIntegral(
               static_cast<int>(app_shell_plan(true)) == static_cast<int>(AppShellAction::RenderShell))
               .ShouldBe(1));
    SHOULD(ShouldlyIntegral(
               static_cast<int>(app_shell_plan(false)) == static_cast<int>(AppShellAction::Report))
               .ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(AppShellAction::RenderShell)).ShouldBe(0));
}

TEST_CASE("S440 app-shell: MainView.vaml parses + resolves every element (live tree)")
{
    auto plan = run_app_shell_plan();
    if (plan.ok == false)
    {
        MESSAGE("S440 app-shell plan failed: " << plan.detail);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));

    // The root is a UIElement (the renderable tree the S208 encoder folds).
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));

    // The root must be the S440 REAL Window (re-based onto UIElement — the S087
    // seed RESOLVED at its own sprint).
    auto* win = dynamic_cast<vaml::Window*>(uiRoot);
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(1));

    // The Window hosts its single content child (the RootGrid) — the S440
    // single-content slot + the finalized S121 visual tree.
    SHOULD(ShouldlyIntegral(win->child_count() == 1u).ShouldBe(1));
    auto* grid = dynamic_cast<vaml::Grid*>(win->child_at(0));
    SHOULD(ShouldlyIntegral(grid != nullptr).ShouldBe(1));
}

TEST_CASE("S440 app-shell: the declared controls are S121 children (S120/S121 gap closed)")
{
    auto plan = run_app_shell_plan();
    // The root is the S440 REAL Window; its single content child is the RootGrid
    // (the app-shell's root panel). The grid is NOT the root — it is the
    // Window's one content child (child_at(0)).
    auto* win = dynamic_cast<vaml::Window*>(plan.root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(1));
    auto* grid = (win != nullptr) ? dynamic_cast<vaml::Grid*>(win->child_at(0)) : nullptr;
    SHOULD(ShouldlyIntegral(grid != nullptr).ShouldBe(1));

    // The Grid must host its three declared StackPanels as S121 children. This
    // is the heart of the S440 closure: WITHOUT finalize_visual_tree() the Grid
    // would have child_count() == 0 (children were reader-owned, not visuals).
    SHOULD(ShouldlyIntegral(grid->child_count() >= 3u).ShouldBe(1));

    // And walking the whole finalized tree, every declared control is present as
    // an S121 child somewhere beneath the root (the Window + 3 StackPanels + the
    // 3 Text + Button + CheckBox + TextBox + Slider + the nested Text/SliderLabel
    // texts). The exact total is asserted structurally: at least the Window's
    // grid + the 3 panels + the 7 sibling controls.
    std::size_t total = depth_first_total_children(*grid);
    SHOULD(ShouldlyIntegral(total >= 10u).ShouldBe(1));
}

TEST_CASE("S440 app-shell: the AppShellPlanResult reports transferred (never silent)")
{
    auto plan = run_app_shell_plan();
    // Every declared child reached the visual tree (the never-silent contract):
    // a zero would mean "declared UI would not render" and the plan reports it.
    SHOULD(ShouldlyIntegral(plan.transferred > 0).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(plan.detail)).ShouldBe(std::string("")));
}
