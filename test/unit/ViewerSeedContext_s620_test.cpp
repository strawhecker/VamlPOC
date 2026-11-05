// S620 (dep:S605) — VIEWER SEED CONTEXT (Sprint 11q root B1). Headless Rail 1
// through the REAL viewer seam (vaml_viewer/viewer_seed_context.h, ADR-021 — the
// S256 precedent, compiled from the vaml_viewer include into vaml-core-tests):
//   (1) the SEED data-context (a plain ObservableObject whose properties are
//       derived deterministically from the authored {Binding} paths — the
//       property name AS TEXT) is injected BEFORE BindExtensions so a bound-but-
//       data-empty document renders visible placeholder content;
//   (2) the PURE seed-render decision table (viewer_seed_render_action);
//   (3) the REAL plan runner (run_viewer_seed_plan) Loads the text through a
//       RETAINED VamlReader, seeds the single-segment {Binding} paths, sets the
//       data context + BindExtensions, and the S252 first-transfer lands the
//       seed TEXT on a String-target DP — the visible placeholder.
//
// VIEWER-ONLY: the lib's binding engine is UNTOUCHED. ZERO new locks (ADR-018).

#include "doctest/doctest.h"

#include "viewer_seed_context.h"  // the S620 seam (SeedObservableObject + run_viewer_seed_plan)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/BindingMarkup.h" // parse_markup_binding (the seed path grammar)
#include "vaml/core/Element.h"       // TextBlock (a real UIElement subclass exposing the Text DP)
#include "vaml/core/UIElement.h"     // UIElement + the S120 Opacity DP (effective_value)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <string>

using vaml::viewer::SeedRenderAction;
using vaml::viewer::seed_string_bindings;
using vaml::viewer::run_viewer_seed_plan;
using vaml::viewer::viewer_seed_render_action;

namespace {

// A bound-but-data-empty TextBlock doc: `Text` is {Binding}-backed with NO
// data context. Before S620 this resolved nothing -> the Text DP stayed empty
// (black/transparent glyph-less). After seeding, `{Binding Name}` seeds the
// leaf "Name" -> the Text DP renders "Name".
const char* kSeededDoc =
    "<TextBlock Text=\"{Binding Name}\"/>";

// A doc with a multi-hop path (S620-recorded seed limitation — NOT seeded).
const char* kMultiHopDoc =
    "<TextBlock Text=\"{Binding Employee.Name}\"/>";

std::string text_of(vaml::UIElement& el)
{
    const vaml::Value v = el.Get(vaml::PropertyId::of("Text"));
    return v.isNull() ? std::string() : v.get<std::string>();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. The PURE seed-render decision table
// ---------------------------------------------------------------------------

TEST_CASE("S620: the pure seed-render decision table")
{
    // A clean load+seed -> Render (the seeded bound tree re-renders in the window).
    SHOULD(ShouldlyString(static_cast<int>(viewer_seed_render_action(true)))
               .ShouldBe(static_cast<int>(SeedRenderAction::Render)));
    // A failed load -> Report (keep the last-good tree, never blank).
    SHOULD(ShouldlyString(static_cast<int>(viewer_seed_render_action(false)))
               .ShouldBe(static_cast<int>(SeedRenderAction::Report)));
}

// ---------------------------------------------------------------------------
// 2. The seed function: single-segment {Binding} -> property-name-as-text
// ---------------------------------------------------------------------------

TEST_CASE("S620: seed_string_bindings seeds a leaf {Binding} path as its name")
{
    vaml::VamlReader reader;
    auto root = reader.Load(kSeededDoc);
    SHOULD(ShouldlyIntegral((bool)root).ShouldBe(1));
    vaml::ObservableObject vm;
    // Only the single-segment `{Binding Name}` is seeded (one path).
    const std::size_t n = seed_string_bindings(reader, vm);
    SHOULD(ShouldlyIntegral(n == 1u).ShouldBe(1));
    // The leaf property name was seeded AS TEXT (the S620 deterministic value).
    SHOULD(ShouldlyString(vm.Get(vaml::PropertyId::of("Name")).get<std::string>())
               .ShouldBe(std::string("Name")));
}

TEST_CASE("S620: a multi-hop binding path is NOT seeded (recorded scope)")
{
    vaml::VamlReader reader;
    auto root = reader.Load(kMultiHopDoc);
    SHOULD(ShouldlyIntegral((bool)root).ShouldBe(1));
    vaml::ObservableObject vm;
    const std::size_t n = seed_string_bindings(reader, vm);
    // The multi-hop `Employee.Name` is deliberately out of S620 scope -> 0.
    SHOULD(ShouldlyIntegral(n == 0u).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. The REAL plan runner: the seeded bound tree renders visible placeholder text
// ---------------------------------------------------------------------------

TEST_CASE("S620: run_viewer_seed_plan seeds + binds {Binding Name} onto Text")
{
    auto plan = run_viewer_seed_plan(kSeededDoc, /*seedEnabled=*/true);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.seeded == 1u).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.bound == 1u).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.seedVm).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));

    // The root must be a UIElement (the renderable tree the S208 encoder folds).
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));
    // The S252 FIRST TRANSFER landed the seeded TEXT ("Name") onto the target
    // Text DP — the visible placeholder (previously black/transparent).
    SHOULD(ShouldlyString(text_of(*uiRoot)).ShouldBe(std::string("Name")));
}

TEST_CASE("S620: seeding disabled keeps the hardwired data-context path unchanged")
{
    // seedEnabled=false must NOT inject the seed context — it behaves like the
    // pre-S620 S256 binding path (a null data context -> no visible text).
    auto plan = run_viewer_seed_plan(kSeededDoc, /*seedEnabled=*/false);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.seeded == 0u).ShouldBe(1));
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));
    // Without a seed, the bound {Binding Name} resolves against a null context ->
    // a Class B NotReadable -> the Text DP stays at its (empty) default. This is
    // the EXACT pre-S620 black/transparent behavior the seed is meant to fix, and
    // proves the seed is the thing that changes the outcome (no lib change).
    SHOULD(ShouldlyString(text_of(*uiRoot)).ShouldBe(std::string("")));
}

// ---------------------------------------------------------------------------
// 4. A document whose load fails reports (never silent) + keeps last-good
// ---------------------------------------------------------------------------

TEST_CASE("S620: a document whose load fails reports a structured detail")
{
    const std::string bad = "<UnknownElement/>";   // unknown element -> reader error
    auto plan = run_viewer_seed_plan(bad, /*seedEnabled=*/true);
    // The plan reports the failure (never silent), keeps no root, and the pure
    // decision says Report.
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(0));
    SHOULD(ShouldlyString(std::string(plan.detail)).ShouldNotBe(std::string("")));
    SHOULD(ShouldlyString(static_cast<int>(viewer_seed_render_action(plan.ok)))
               .ShouldBe(static_cast<int>(SeedRenderAction::Report)));
}
