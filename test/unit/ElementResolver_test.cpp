// S088 (dep:S087) — ElementResolver tests.
//
// Proves the reader-boundary resolution primitive: resolve a ParsedElement into
// a live instance via the S087 ElementFactoryRegistry, with an unknown type
// becoming a STRUCTURED error naming the type + source line:col.
//
// Test plan (Shouldly vocabulary):
//   1. resolve <Button> -> live Button (dynamic_cast non-null).
//   2. custom registry with a registered factory -> resolves that type.
//   3. global path: register_builtin_elements() then resolve -> Button instance.
//   4. unknown type -> ElementResolutionException, kind()==UnknownType,
//      type_name()=="Frobnicator", line()/col() match the element's position.
//   5. prefixed <local:Button> resolves via local_name ("Button").
//   6. deep-doc line accuracy: multi-line source, resolve the offending element,
//      error line == that element's parse line.
//   7. empty type name -> UnknownType error.
//   8. a throwing factory on a REGISTERED type propagates its construction
//      exception (never swallowed into UnknownType).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"               // Button + register_builtin_elements
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/ElementResolver.h"       // resolve_element + exception
#include "vaml/core/Intern.h"                // vaml::intern (S140)
#include "vaml/core/ParsedElement.h"         // ParsedElement + parse_vaml

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

using vaml::Button;
using vaml::DependencyObject;
using vaml::ElementFactoryRegistry;
using vaml::ElementResolutionError;
using vaml::ElementResolutionException;
using vaml::ParsedElement;
using vaml::Text;
using vaml::parse_vaml;
using vaml::register_builtin_elements;
using vaml::resolve_element;
using vaml::test::ShouldThrow;

namespace {

// S088: builds an element with the given type identity at line/col. Mirrors the
// fields applyElementIdentity sets: tagName / local_name / line / col.
// S140: tagName/local_name are string_view backed by the intern pool — the
// std::string temporaries die at return, so we MUST intern() to get a stable,
// caller-independent view (the same lifetime contract parse_vaml honors).
ParsedElement makeElement(std::string tagName, std::string localName,
                          std::size_t line, std::size_t col)
{
    ParsedElement el;
    el.tagName = vaml::intern(tagName);
    el.local_name = vaml::intern(localName);
    el.line = line;
    el.col = col;
    return el;
}

// S088: asserts `p` is non-null AND dynamic_cast<Concrete*>(p) is non-null
// (proves the produced object's concrete type, not a generic DependencyObject).
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
// resolve -> live instance (custom registry)
// ---------------------------------------------------------------------------

TEST_CASE("S088: resolve a Button element to a live Button via a custom registry")
{
    ElementFactoryRegistry r;
    r.register_element("Button", [] { return std::make_unique<Button>(); });

    ParsedElement el = makeElement("Button", "Button", 3, 5);
    std::unique_ptr<DependencyObject> o = resolve_element(el, r);

    assertConcrete<Button>(o);
}

TEST_CASE("S088: a registered custom type resolves to that type")
{
    ElementFactoryRegistry r;
    r.register_element("FancyWidget", [] { return std::make_unique<Text>(); });

    ParsedElement el = makeElement("FancyWidget", "FancyWidget", 1, 1);
    std::unique_ptr<DependencyObject> o = resolve_element(el, r);
    assertConcrete<vaml::Text>(o);
}

TEST_CASE("S088: fresh instance per resolve")
{
    ElementFactoryRegistry r;
    r.register_element("Button", [] { return std::make_unique<Button>(); });

    ParsedElement el = makeElement("Button", "Button", 2, 2);
    std::unique_ptr<DependencyObject> a = resolve_element(el, r);
    std::unique_ptr<DependencyObject> b = resolve_element(el, r);

    SHOULD(ShouldlyIntegral(a != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(b != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(a.get() != b.get()).ShouldBe(true));
    assertConcrete<Button>(a);
    assertConcrete<Button>(b);
}

// ---------------------------------------------------------------------------
// global path (S091 reader path)
// ---------------------------------------------------------------------------

TEST_CASE("S088: global path resolves a built-in Button after startup registration")
{
    register_builtin_elements();

    ParsedElement el = makeElement("Button", "Button", 1, 1);
    std::unique_ptr<DependencyObject> o = resolve_element(el); // global() overload
    assertConcrete<Button>(o);
}

// ---------------------------------------------------------------------------
// prefixed element resolves via local_name
// ---------------------------------------------------------------------------

TEST_CASE("S088: a prefixed <local:Button> resolves Button via local_name")
{
    ElementFactoryRegistry r;
    r.register_element("Button", [] { return std::make_unique<Button>(); });

    // tagName is the FULL raw name "local:Button"; local_name is "Button" (the
    // S082 split). The registry key must be the LOCAL name.
    ParsedElement el = makeElement("local:Button", "Button", 4, 7);
    std::unique_ptr<DependencyObject> o = resolve_element(el, r);
    assertConcrete<Button>(o);
}

// ---------------------------------------------------------------------------
// structured error: unknown type naming type + line
// ---------------------------------------------------------------------------

TEST_CASE("S088: unknown type throws ElementResolutionException naming type+line")
{
    ElementFactoryRegistry r;

    ParsedElement el = makeElement("Frobnicator", "Frobnicator", 6, 9);

    bool threw = false;
    try
    {
        (void)resolve_element(el, r);
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ElementResolutionError::UnknownType)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Frobnicator")));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(6u));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBe(9u));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S088: ShouldThrow form for the unknown-type error")
{
    ElementFactoryRegistry r;
    ParsedElement el = makeElement("Nope", "Nope", 2, 3);
    SHOULD(ShouldThrow<ElementResolutionException>([&] {
        (void)resolve_element(el, r);
    }));
}

TEST_CASE("S088: unknown-type what() names the type and the line:col")
{
    ElementFactoryRegistry r;
    ParsedElement el = makeElement("Mystery", "Mystery", 12, 4);

    bool matched = false;
    try
    {
        (void)resolve_element(el, r);
    }
    catch (const ElementResolutionException& e)
    {
        matched = (std::string(e.what()).find("Mystery") != std::string::npos) &&
                  (std::string(e.what()).find("line 12") != std::string::npos);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(matched)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// deep-doc line accuracy (the S088 title "naming type+line" end-to-end)
// ---------------------------------------------------------------------------

TEST_CASE("S088: error line matches the offending element's parse line in real markup")
{
    ElementFactoryRegistry r;
    // Only Button is registered; the doc contains an unknown element so its
    // line is reported from the parsed tree, not guessed.
    r.register_element("Button", [] { return std::make_unique<Button>(); });

    const std::string doc =
        "<Window>\n"      // line 1
        "  <Zorp/>\n"     // line 2 (unknown element)
        "  <Button/>\n"   // line 3
        "</Window>\n";    // line 4

    ParsedElement root = parse_vaml(doc);

    // Walk to the first child (Zorp) and resolve it -> error must report its
    // real parsed line 2 (Zorp is the first child on line 2, 1-based col 3).
    const ParsedElement& zorp = root.children[0];
    SHOULD(ShouldlyString(zorp.tagName).ShouldBe(std::string("Zorp")));

    bool threw = false;
    try
    {
        (void)resolve_element(zorp, r);
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("Zorp")));
        // Zorp opens at line 2, column 3 (2 spaces indent then '<').
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(2u));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBe(3u));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S088: parsed position is carried onto the tree (line/col populated)")
{
    const std::string doc = "<Window>\n  <Button/>\n</Window>\n";
    ParsedElement root = parse_vaml(doc);

    // Root Window at line 1 col 1; Button child at line 2 col 3.
    SHOULD(ShouldlyIntegral(root.line).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(root.col).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(root.children[0].line).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(root.children[0].col).ShouldBe(3u));
}

// ---------------------------------------------------------------------------
// empty type name -> UnknownType
// ---------------------------------------------------------------------------

TEST_CASE("S088: empty type name is an UnknownType error")
{
    ElementFactoryRegistry r;
    // local_name empty AND tagName empty -> no type at all.
    ParsedElement el = makeElement("", "", 1, 1);

    bool threw = false;
    try
    {
        (void)resolve_element(el, r);
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ElementResolutionError::UnknownType)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// a throwing factory propagates (never becomes UnknownType)
// ---------------------------------------------------------------------------

TEST_CASE("S088: a throwing factory's construction exception propagates, not swallowed")
{
    ElementFactoryRegistry r;
    r.register_element("Fragile", []() -> std::unique_ptr<DependencyObject> {
        throw std::runtime_error("fragile element ctor failed");
    });

    ParsedElement el = makeElement("Fragile", "Fragile", 5, 5);

    // The type IS registered; its factory throws at construction. That throw
    // must propagate (it is a live construction failure), NOT be remapped to
    // UnknownType.
    SHOULD(ShouldThrow<std::runtime_error>([&] {
        (void)resolve_element(el, r);
    }));

    // registry still reports it registered — the error was construction, not
    // absence.
    SHOULD(ShouldlyIntegral(r.is_registered("Fragile")).ShouldBe(true));
}
