// S440 (dep:S408) — the dedicated Window RE-BASE test (the app-shell root).
//
// Sprint 11 = the in-repo v1 viewer deliverable; S440 re-bases `Window` onto
// `UIElement` (it was a MINIMAL REAL DependencyObject seed) so the app-shell
// root is a REAL renderable/layoutable container: the S087 sentinel
// `[MINIMAL REAL] Element Window — RESOLVED BY S440` is REMOVED here per its
// resolution contract (resolution at the feature's own step, not a backfill —
// NO-GAP). This TU proves the RE-BASE at the core level:
//
//   1. Window IS-A UIElement (the re-base — participates in the visual tree +
//      Measure/Arrange + hit-testing + the S121 renderable-children contract).
//   2. Window resolves through the S087 ElementFactoryRegistry by its
//      registered type name "Window" (the reader/consumer produces a REAL
//      Window), and dynamic-casts back to Window.
//   3. Window hosts EXACTLY ONE content child (the WPF-faithful single-content
//      slot): set_content adopts one; a second set_content REPLACES it; the
//      child is stored/owned via the parent's S121 visual-children list (one
//      ownership edge). content() returns the non-owning observer.
//   4. Window lays out its single content child to FILL its own box via the
//      implicit single-child Measure/Arrange path (the S123/S124 contract):
//      Measure measures the child; Arrange arranges it into the Window's full
//      final rect (so a Window root renders its declared content — the S440/
//      app-shell headline at the core level).
//
// The VAML-LOAD path (finalize_visual_tree closing the S120/S121 gap so a
// loaded Window renders children) is proven headlessly by AppShellPlan_s440_test
// (the consumer seam). THIS TU is the dedicated core Window-rebase proof.
//
// Headless (Rail 1) — no window/runtime, no Vulkan (the standing S160+
// precedent): the REAL core Window algorithms are pure + deterministic.

#include "vaml/core/Element.h"     // Window / Grid / UIElement (the re-base targets)
#include "vaml/core/ElementFactoryRegistry.h" // S087: the registered type-name registry

#include "doctest/doctest.h"

#include <memory>

using namespace vaml;

TEST_CASE("S440: Window is a real UIElement (the re-base onto UIElement)")
{
    // The S440 Window re-base: Window derives UIElement, so it participates in
    // the visual tree + the S121 children + Measure/Arrange + hit-testing (the
    // Sprint 11 app-shell root is a REAL renderable container, not a bare
    // DependencyObject seed).
    Window win;
    CHECK(dynamic_cast<UIElement*>(&win) != nullptr);
    CHECK(dynamic_cast<DependencyObject*>(&win) != nullptr);
    // The re-base must NOT remove the DependencyObject lineage (Window is still
    // a dependency object — it hosts DPs through the inherited store).
    CHECK(win.child_count() == 0u);              // a fresh Window has no content
    CHECK(win.content() == nullptr);
}

TEST_CASE("S440: Window resolves through the S087 factory registry by name")
{
    // Register the STARTUP built-in element set (S087/S092: register_builtin_
    // elements() populates the process-wide singleton once). This is the same
    // STARTUP GATE the existing ElementFactoryRegistry test performs before
    // create("Button"); without it the registry is empty and create throws
    // ElementNotFound.
    register_builtin_elements();
    ElementFactoryRegistry& factory = ElementFactoryRegistry::global();
    std::unique_ptr<DependencyObject> created = factory.create("Window");
    REQUIRE(created != nullptr);
    // It must be a real Window (re-based onto UIElement) — the S440 headline.
    CHECK(dynamic_cast<Window*>(created.get()) != nullptr);
    CHECK(dynamic_cast<UIElement*>(created.get()) != nullptr);
}

TEST_CASE("S440: Window hosts exactly one content child (the single-content slot)")
{
    Window win;
    auto child1 = std::make_unique<Grid>();
    Grid* raw1 = child1.get();
    win.set_content(std::move(child1));

    // Exactly ONE child (WPF-faithful single-content composition).
    CHECK(win.child_count() == 1u);
    CHECK(win.content() != nullptr);
    CHECK(win.content() == raw1);
    // The child is owned by the Window's S121 visual-children list (the parent
    // link is wired — Single-parent ownership, one edge).
    CHECK(win.child_at(0) == raw1);
    CHECK(raw1->parent() == &win);

    // A SECOND set_content REPLACES the first (the single-content slot, not a
    // panel list): the old child is released, the new one wins.
    auto child2 = std::make_unique<Grid>();
    Grid* raw2 = child2.get();
    win.set_content(std::move(child2));
    CHECK(win.child_count() == 1u);
    CHECK(win.content() == raw2);
    CHECK(win.child_at(0) == raw2);
    CHECK(raw2->parent() == &win);
}

TEST_CASE("S440: Window lays out its content child to fill (implicit single-child layout)")
{
    Window win;
    win.set_content(std::make_unique<Grid>());
    Grid* grid = static_cast<Grid*>(win.content());
    REQUIRE(grid != nullptr);

    // Run the S123/S124 contract over the Window root: Measure with an offered
    // box, then Arrange into a real final rect. The Window's implicit
    // single-child layout measures + arranges its content child into the
    // Window's box (the "a Window RENDERS its declared content" contract at the
    // core level).
    const vaml::Size offered(400.0, 300.0);
    win.Measure(offered);
    const vaml::Rect slot(0.0, 0.0, 400.0, 300.0);
    win.Arrange(slot);

    // The Window's final rect equals the arranged slot.
    CHECK(win.final_rect().x == doctest::Approx(0.0));
    CHECK(win.final_rect().y == doctest::Approx(0.0));
    CHECK(win.final_rect().width == doctest::Approx(400.0));
    CHECK(win.final_rect().height == doctest::Approx(300.0));

    // The content child is arranged to FILL the Window's box (the implicit
    // single-child fill — a Window root renders its declared content).
    CHECK(grid->final_rect().width == doctest::Approx(400.0));
    CHECK(grid->final_rect().height == doctest::Approx(300.0));
}
