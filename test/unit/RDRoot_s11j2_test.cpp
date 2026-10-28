// 11j-2 (GAP CLOSE, dep:S453) — `ResourceDictionary` as a LOADABLE ROOT.
//
// RESOLVES a corpus blocker: ~100 corpus files are standalone resource docs
// (themes/icons/DataTemplates) whose root is `<ResourceDictionary>`, which the
// reader rejected ("may only appear inside .Resources / .MergedDictionaries").
//
// WPF-FAITHFUL (confirmed from the WPF source, ResourceDictionary.cs): WPF's
// `ResourceDictionary` is a PLAIN class (implements IDictionary — NOT a
// DependencyObject) yet is a legal standalone `.xaml` root. Our Load API
// returns `std::unique_ptr<DependencyObject>`, so a root RD is produced via the
// REAL DependencyObject HOST `ResourceDictionaryHolder` (registered as the
// element name "ResourceDictionary"), which OWNS the built dictionary. This
// reuses S453's build_resource_dictionary capture verbatim (the S360 dup-key /
// no-key / merged semantics are unchanged).
//
// Headless (Rail 1) — no window/runtime.

#include "vaml/core/Element.h"       // ResourceDictionaryHolder
#include "vaml/core/ElementFactoryRegistry.h" // S087
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/VamlReader.h"

#include "doctest/doctest.h"

#include <memory>
#include <string>

using namespace vaml;

TEST_CASE("11j-2: ResourceDictionary resolves as a DependencyObject HOLDER element")
{
    register_builtin_elements();
    ElementFactoryRegistry& factory = ElementFactoryRegistry::global();
    auto created = factory.create("ResourceDictionary");
    REQUIRE(created != nullptr);
    // It must be a DependencyObject (the Load API root type) — the Holder host.
    CHECK(dynamic_cast<DependencyObject*>(created.get()) != nullptr);
    CHECK(dynamic_cast<ResourceDictionaryHolder*>(created.get()) != nullptr);
    // A ResourceDictionary Holder is NOT a UIElement (a resource store, not a
    // visual element — WPF-faithful: RD is a plain non-element class).
    CHECK(dynamic_cast<UIElement*>(created.get()) == nullptr);
}

TEST_CASE("11j-2: a root <ResourceDictionary> with keyed resources loads (no throw) + captures keys")
{
    VamlReader reader;
    auto root = reader.Load(
        "<ResourceDictionary xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<SolidColorBrush x:Key=\"BrushA\" Color=\"Red\"/>"
        "<SolidColorBrush x:Key=\"BrushB\" Color=\"Blue\"/>"
        "</ResourceDictionary>");
    REQUIRE(root != nullptr);
    ResourceDictionaryHolder* holder = dynamic_cast<ResourceDictionaryHolder*>(root.get());
    REQUIRE(holder != nullptr);
    REQUIRE(holder->dictionary() != nullptr);
    // The authored keys were captured (S453 semantics, unchanged for the ROOT).
    CHECK(holder->dictionary()->has("BrushA"));
    CHECK(holder->dictionary()->has("BrushB"));
}

TEST_CASE("11j-2: a duplicate x:Key in a root RD still fails loud (S360 semantics unchanged)")
{
    VamlReader reader;
    bool threw = false;
    try
    {
        auto root = reader.Load(
            "<ResourceDictionary xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<SolidColorBrush x:Key=\"A\" Color=\"Red\"/>"
            "<SolidColorBrush x:Key=\"A\" Color=\"Blue\"/>"
            "</ResourceDictionary>");
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    CHECK(threw);
}
