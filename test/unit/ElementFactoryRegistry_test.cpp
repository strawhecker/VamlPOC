// S087 (dep:S086) — ElementFactoryRegistry tests.
//
// Proves the typeName -> factory<unique_ptr<DependencyObject>> registry + the
// startup registration (register_builtin_elements):
//   * register + create returns a live DependencyObject of the right CONCRETE
//     type (dynamic_cast<Button*> non-null -> typeName dispatches to a real
//     type, not a generic blob);
//   * a registered factory produces a FRESH instance per create() call;
//   * duplicate registration throws ElementAlreadyRegistered and the registry
//     is UNCHANGED (name-unique dedupe, S032 discipline);
//   * create() on an unknown name throws ElementNotFound;
//   * try_create() on an unknown name -> nullptr (never throws);
//   * is_registered / size / empty;
//   * registered_names() returns INSERTION ORDER deterministically;
//   * STARTUP GATE — register_builtin_elements() then create("Button"),
//     create("StackPanel"), ... yields the concrete type for every built-in
//     element name (the S087 headline verified end-to-end).
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/DependencyObject.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

using vaml::Button;
using vaml::Canvas;
using vaml::CheckBox;
using vaml::DependencyObject;
using vaml::ElementAlreadyRegistered;
using vaml::ElementFactory;
using vaml::ElementFactoryRegistry;
using vaml::ElementNotFound;
using vaml::Grid;
using vaml::ScrollViewer;
using vaml::Slider;
using vaml::StackPanel;
using vaml::Text;
using vaml::TextBox;
using vaml::Window;
using vaml::WrapPanel;
using vaml::register_builtin_elements;
using vaml::test::ShouldThrow;

namespace {

// Assert `p` is non-null AND dynamic_cast<Concrete*>(p) is non-null (proves the
// produced object's concrete type, not a generic DependencyObject).
template <typename Concrete>
void assertConcrete(const std::unique_ptr<DependencyObject>& p)
{
    SHOULD(ShouldlyIntegral(p != nullptr).ShouldBe(true));
    if (p)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Concrete*>(p.get()) != nullptr).ShouldBe(true));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// register + create -> concrete instance
// ---------------------------------------------------------------------------

TEST_CASE("S087: register+create returns a live object of the concrete type")
{
    ElementFactoryRegistry r;
    r.register_element("ButtonS087", [] { return std::make_unique<Button>(); });

    std::unique_ptr<DependencyObject> o = r.create("ButtonS087");
    assertConcrete<Button>(o);
}

TEST_CASE("S087: register+create returns a live object of the concrete type (WrapPanel)")
{
    // S134: a WrapPanel concrete instance resolves from the registry (the
    // typeName -> concrete-type dispatch is real, not a generic blob).
    ElementFactoryRegistry r;
    r.register_element("WrapPanelS134", [] { return std::make_unique<WrapPanel>(); });
    std::unique_ptr<DependencyObject> o = r.create("WrapPanelS134");
    assertConcrete<WrapPanel>(o);
}

TEST_CASE("S087: register+create returns a live object of the concrete type (Canvas)")
{
    // S136: a Canvas concrete instance resolves from the registry (the
    // typeName -> concrete-type dispatch is real, not a generic blob).
    ElementFactoryRegistry r;
    r.register_element("CanvasS136", [] { return std::make_unique<Canvas>(); });
    std::unique_ptr<DependencyObject> o = r.create("CanvasS136");
    assertConcrete<Canvas>(o);
}

TEST_CASE("S087: create produces a fresh instance per call")
{
    ElementFactoryRegistry r;
    r.register_element("TextS087", [] { return std::make_unique<Text>(); });

    std::unique_ptr<DependencyObject> a = r.create("TextS087");
    std::unique_ptr<DependencyObject> b = r.create("TextS087");
    SHOULD(ShouldlyIntegral(a != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(b != nullptr).ShouldBe(true));
    // Two distinct objects, both concrete Text.
    SHOULD(ShouldlyIntegral(a.get() != b.get()).ShouldBe(true));
    assertConcrete<Text>(a);
    assertConcrete<Text>(b);
}

// ---------------------------------------------------------------------------
// dedupe
// ---------------------------------------------------------------------------

TEST_CASE("S087: duplicate registration throws ElementAlreadyRegistered; registry unchanged")
{
    ElementFactoryRegistry r;
    r.register_element("ButtonS087", [] { return std::make_unique<Button>(); });
    const std::size_t before = r.size();

    bool threw = false;
    try
    {
        // Registering the same NAME with a DIFFERENT factory must be rejected.
        r.register_element("ButtonS087", [] { return std::make_unique<Grid>(); });
    }
    catch (const ElementAlreadyRegistered& e)
    {
        threw = true;
        SHOULD(ShouldlyString(std::string(e.what())).ShouldNotBe(std::string("")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // Registry unchanged: size identical AND the ORIGINAL factory still resolves
    // to the Button (a rejected insert never swaps the factory).
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(before));
    assertConcrete<Button>(r.create("ButtonS087"));
}

TEST_CASE("S087: ShouldThrow form of dedupe rejection")
{
    ElementFactoryRegistry r;
    r.register_element("WindowS087", [] { return std::make_unique<Window>(); });
    // Same name -> ElementAlreadyRegistered via the typed-exception assertion.
    SHOULD(ShouldThrow<ElementAlreadyRegistered>([&] {
        r.register_element("WindowS087", [] { return std::make_unique<Window>(); });
    }));
    // Still the Window, still registered exactly once.
    assertConcrete<Window>(r.create("WindowS087"));
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// lookup failures
// ---------------------------------------------------------------------------

TEST_CASE("S087: create on an unknown name throws ElementNotFound")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldThrow<ElementNotFound>([&] {
        (void)r.create("DoesNotExistS087");
    }));
}

TEST_CASE("S087: try_create on an unknown name returns nullptr (never throws)")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldlyIntegral(r.try_create("DoesNotExistS087") == nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// is_registered / size / empty / insertion order
// ---------------------------------------------------------------------------

TEST_CASE("S087: is_registered + size + empty reflect registration")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldlyIntegral(r.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(r.is_registered("X_S087")).ShouldBe(false));

    r.register_element("X_S087", [] { return std::make_unique<Button>(); });
    SHOULD(ShouldlyIntegral(r.empty()).ShouldBe(false));
    SHOULD(ShouldlyIntegral(r.is_registered("X_S087")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(r.is_registered("Y_S087")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(1u));
}

TEST_CASE("S087: registered_names preserves insertion order deterministically")
{
    ElementFactoryRegistry r;
    r.register_element("FirstS087", [] { return std::make_unique<Button>(); });
    r.register_element("SecondS087", [] { return std::make_unique<Text>(); });
    r.register_element("ThirdS087", [] { return std::make_unique<Grid>(); });

    // Insertion order, not hash order — deterministic across runs.
    const std::vector<std::string> names = r.registered_names();
    SHOULD(ShouldlyIntegral(names.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(names[0]).ShouldBe(std::string("FirstS087")));
    SHOULD(ShouldlyString(names[1]).ShouldBe(std::string("SecondS087")));
    SHOULD(ShouldlyString(names[2]).ShouldBe(std::string("ThirdS087")));
}

// ---------------------------------------------------------------------------
// STARTUP GATE — register_builtin_elements() + create() for every built-in
// ---------------------------------------------------------------------------

TEST_CASE("S087: startup registration — every built-in element resolves to its concrete type")
{
    // register_builtin_elements() populates the process-wide singleton.
    register_builtin_elements();

    ElementFactoryRegistry& g = ElementFactoryRegistry::global();

    SHOULD(ShouldlyIntegral(g.is_registered("Button")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("StackPanel")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Grid")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Text")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CheckBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Slider")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Window")).ShouldBe(true));
    // S134: WrapPanel is a real built-in (registered at S134, not a seed).
    SHOULD(ShouldlyIntegral(g.is_registered("WrapPanel")).ShouldBe(true));
    // S136: Canvas is a real built-in (registered at S136, not a seed).
    SHOULD(ShouldlyIntegral(g.is_registered("Canvas")).ShouldBe(true));

    // The S087 headline: create("Button") etc. yields the CONCRETE type, not a
    // generic blob — the typeName -> type dispatch is real end-to-end.
    assertConcrete<Button>(g.create("Button"));
    assertConcrete<StackPanel>(g.create("StackPanel"));
    assertConcrete<Grid>(g.create("Grid"));
    assertConcrete<Text>(g.create("Text"));
    assertConcrete<TextBox>(g.create("TextBox"));
    assertConcrete<CheckBox>(g.create("CheckBox"));
    assertConcrete<Slider>(g.create("Slider"));
    assertConcrete<ScrollViewer>(g.create("ScrollViewer"));
    assertConcrete<Window>(g.create("Window"));
    // S134: the registry-resolve assertion is authored at THIS step (the
    // element was registered here, not silently backfilled later).
    assertConcrete<WrapPanel>(g.create("WrapPanel"));
    // S136: the registry-resolve assertion is authored at THIS step.
    assertConcrete<Canvas>(g.create("Canvas"));

    // Size matches the full built-in set (11 elements: 9 seeds + WrapPanel + Canvas).
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(11u));
}

TEST_CASE("S087: register_builtin_elements is idempotent (second call is a no-op, not a throw)")
{
    register_builtin_elements();
    register_builtin_elements(); // second population must NOT throw
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    // Exactly the built-in set, no duplicates (a duplicate would have thrown or
    // inflated size). S134: WrapPanel adds the 10th built-in; S136: Canvas the 11th.
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(11u));
    assertConcrete<Button>(g.create("Button"));
}
