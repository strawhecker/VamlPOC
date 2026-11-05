// S621 (dep:S620) — PLACEHOLDER BRUSH/VALUE SEEDING (Sprint 11q root B2).
// Headless Rail 1 through the REAL viewer seam (vaml_viewer/viewer_seed_context.h,
// ADR-021 — the S256 precedent): the S620 text-only seed is upgraded to be
// TYPE-AWARE on the TARGET DP's declared bound type, so a bound-but-data-empty
// doc whose {Binding} targets a BRUSH/OPACITY/COLOR DP renders VISIBLE (never
// transparent/black):
//   (1) the deterministic seed primitives (FNV name hash -> opaque saturated
//       Color; non-zero BrushId; Opacity -> 1.0);
//   (2) seed_typed_bindings maps a Color-typed target (Background) to an OPAQUE
//       NON-BLACK Color, and a BrushId-typed target (Foreground) to a NON-ZERO id;
//   (3) run_viewer_seed_plan lands the visible Color on the Background DP (the
//       retained-render fix) + the fully-visible Opacity; deterministic (same doc
//       -> same seed, twice).
// VIEWER-ONLY (zero lib change). ADR-018: zero new locks.

#include "doctest/doctest.h"

#include "viewer_seed_context.h"  // the S620+S621 seam (seed_typed_bindings + run_viewer_seed_plan)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Color.h"         // vaml::Color (the resolved Background fill)
#include "vaml/core/Element.h"       // StackPanel / TextBlock (real UIElement subclasses)
#include "vaml/core/UIElement.h"     // UIElement (the renderable root)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <string>

using vaml::viewer::run_viewer_seed_plan;
using vaml::viewer::seed_name_hash;
using vaml::viewer::seed_typed_bindings;
using vaml::viewer::seed_visible_color;

namespace {

// A bound-but-data-empty BRUSH doc: a StackPanel whose Background + Opacity are
// {Binding}-backed with NO data context. Before S621, the Color-typed
// Background resolved nothing -> stayed at the DP default (the classifier's
// neutral fill, and the panel's opaque background is lost) + many brush-bound
// elements rendered black/transparent. After the typed seed, Background renders
// an OPAQUE NON-BLACK visible fill at full Opacity.
const char* kBrushDoc =
    "<StackPanel Background=\"{Binding Bg}\" Opacity=\"{Binding Op}\"/>";

// A binding that targets a BrushId-typed DP (Foreground, TextBlock).
const char* kBrushIdDoc =
    "<TextBlock Text=\"{Binding Txt}\" Foreground=\"{Binding Fg}\"/>";

vaml::Color background_of(vaml::UIElement& el)
{
    const vaml::Value v = el.Get(vaml::PropertyId::of("Background"));
    return v.type() == vaml::ValueType::Color ? v.get<vaml::Color>()
                                              : vaml::Color{ 0, 0, 0, 0 };
}

double opacity_of(vaml::UIElement& el)
{
    return el.Get(vaml::PropertyId::of("Opacity")).get<double>();
}

int32_t foreground_of(vaml::UIElement& el)
{
    return el.Get(vaml::PropertyId::of("Foreground")).get<std::int32_t>();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. The deterministic seed primitives
// ---------------------------------------------------------------------------

TEST_CASE("S621: seed_visible_color is opaque + deterministic + never black")
{
    // The same hash -> the same opaque color.
    const std::uint32_t h = seed_name_hash("Bg", 0x01u);
    const vaml::Color a = seed_visible_color(h);
    const vaml::Color b = seed_visible_color(seed_name_hash("Bg", 0x01u));
    SHOULD(ShouldlyIntegral(a.r == b.r).ShouldBe(1));
    SHOULD(ShouldlyIntegral(a.g == b.g).ShouldBe(1));
    SHOULD(ShouldlyIntegral(a.b == b.b).ShouldBe(1));
    // FULL opacity — the S621 "never transparent" contract.
    SHOULD(ShouldlyIntegral(static_cast<int>(a.a) == 255).ShouldBe(1));
    // NOT black (a visible placeholder, never pure black #000000).
    SHOULD(ShouldlyIntegral((a.r > 0) || (a.g > 0) || (a.b > 0)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. seed_typed_bindings: the type-aware mapping (Color vs BrushId vs Double)
// ---------------------------------------------------------------------------

TEST_CASE("S621: seed_typed_bindings seeds a Color-typed Background opaque non-black")
{
    vaml::VamlReader reader;
    auto root = reader.Load(kBrushDoc);
    SHOULD(ShouldlyIntegral((bool)root).ShouldBe(1));
    vaml::ObservableObject vm;
    const std::size_t n = seed_typed_bindings(reader, vm);
    // Two leaf {Binding} paths authored (Bg + Op) -> two seeds.
    SHOULD(ShouldlyIntegral(n == 2u).ShouldBe(1));
    // Bg (a Color-typed target) -> a Color value.
    const vaml::Value bv = vm.Get(vaml::PropertyId::of("Bg"));
    SHOULD(ShouldlyIntegral(bv.type() == vaml::ValueType::Color).ShouldBe(1));
    const vaml::Color c = bv.get<vaml::Color>();
    SHOULD(ShouldlyIntegral(static_cast<int>(c.a) == 255).ShouldBe(1));   // opaque
    SHOULD(ShouldlyIntegral((c.r > 0) || (c.g > 0) || (c.b > 0)).ShouldBe(1)); // non-black
    // Op (a Double-typed target) -> a Double seed.
    SHOULD(ShouldlyIntegral(vm.Get(vaml::PropertyId::of("Op")).type() == vaml::ValueType::Double)
               .ShouldBe(1));
}

TEST_CASE("S621: a BrushId-typed target (Foreground) seeds a NON-ZERO id")
{
    vaml::VamlReader reader;
    auto root = reader.Load(kBrushIdDoc);
    SHOULD(ShouldlyIntegral((bool)root).ShouldBe(1));
    vaml::ObservableObject vm;
    const std::size_t n = seed_typed_bindings(reader, vm);
    // Txt (String) + Fg (BrushId) -> two seeds.
    SHOULD(ShouldlyIntegral(n == 2u).ShouldBe(1));
    const vaml::Value fv = vm.Get(vaml::PropertyId::of("Fg"));
    SHOULD(ShouldlyIntegral(fv.type() == vaml::ValueType::BrushId).ShouldBe(1));
    // NON-ZERO — never the transparent/black id 0.
    SHOULD(ShouldlyIntegral(fv.get<std::int32_t>() != 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 3. The REAL plan runner: the seeded bound tree renders a visible fill
// ---------------------------------------------------------------------------

TEST_CASE("S621: run_viewer_seed_plan lands an opaque non-black Background + full Opacity")
{
    auto plan = run_viewer_seed_plan(kBrushDoc, /*seedEnabled=*/true);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.seeded == 2u).ShouldBe(1));

    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));
    // The S252 first-transfer landed the type-aware seed: the Background DP now
    // reads an OPAQUE NON-BLACK Color (previously the neutral/transparent
    // default) — the retained-render visible-fill fix.
    const vaml::Color bg = background_of(*uiRoot);
    SHOULD(ShouldlyIntegral(static_cast<int>(bg.a) == 255).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bg.r > 0) || (bg.g > 0) || (bg.b > 0)).ShouldBe(1));
    // The Opacity DP is fully visible (1.0) — the S621 "visible not transparent"
    // contract for an Opacity-bound element.
    SHOULD(ShouldlyString(std::to_string(opacity_of(*uiRoot)))
               .ShouldBe(std::to_string(1.0)));
}

TEST_CASE("S621: the seed is deterministic — the same doc yields the same color twice")
{
    auto p1 = run_viewer_seed_plan(kBrushDoc, true);
    auto p2 = run_viewer_seed_plan(kBrushDoc, true);
    auto* r1 = dynamic_cast<vaml::UIElement*>(p1.root.get());
    auto* r2 = dynamic_cast<vaml::UIElement*>(p2.root.get());
    SHOULD(ShouldlyIntegral(r1 != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r2 != nullptr).ShouldBe(1));
    const vaml::Color c1 = background_of(*r1);
    const vaml::Color c2 = background_of(*r2);
    SHOULD(ShouldlyIntegral(c1.r == c2.r).ShouldBe(1));
    SHOULD(ShouldlyIntegral(c1.g == c2.g).ShouldBe(1));
    SHOULD(ShouldlyIntegral(c1.b == c2.b).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. A BrushId-typed target round-trips through the plan runner
// ---------------------------------------------------------------------------

TEST_CASE("S621: a Foreground {Binding} seeds a NON-ZERO BrushId via the plan")
{
    auto plan = run_viewer_seed_plan(kBrushIdDoc, /*seedEnabled=*/true);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.seeded == 2u).ShouldBe(1));
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr).ShouldBe(1));
    // The Text target (String-typed) got the property-name text (S620), and the
    // Foreground target (BrushId-typed) got a NON-ZERO id — never the default 0.
    SHOULD(ShouldlyString(uiRoot->Get(vaml::PropertyId::of("Text")).get<std::string>())
               .ShouldBe(std::string("Txt")));
    SHOULD(ShouldlyIntegral(foreground_of(*uiRoot) != 0).ShouldBe(1));
}
