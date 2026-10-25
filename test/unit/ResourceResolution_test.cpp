// S361 (dep:S360) — StaticResource/DynamicResource resolution; merge precedence;
// missing-key errors. RESOLVES the S092 x:Key -> S361 deferral + the S252/S253
// MarkupExtensionKind::None -> StaticResource/DynamicResource deferral.
//
// Under test (per the S361 plan):
//   1. parse_resource_markup — {StaticResource Key} -> Static, {DynamicResource Key}
//      -> Dynamic; a binding/x:Null source -> recognized=false; a malformed
//      empty-key -> malformed=true. static_resource_markup/dynamic_resource_markup.
//   2. Static resolution — a {StaticResource <key>} resolves from the element's
//      Resources / app / system (the S360 walk) ONCE and applies to the target
//      DP. A MISSING static key throws ResourceLookupException(MissingKey) —
//      the SHARED decision-3 error.
//   3. Merge precedence through markup — a parent's merged dictionary supplies
//      a key the element resolves (the S360 self-wins/sibling-last-wins
//      precedence is exercised through the reader path).
//   4. Dynamic — resolves now, records a link, reapply_dynamic_resources
//      RE-PUSHES on a source change; NEVER throws on a transiently-missing key
//      (holds, via the S250_1 diagnostics channel).
//   5. Duplicate x:Key authoring under <X.Resources> captures into the element's
//      Resources (the S092 x:Key -> S361 resolution; tested via the S092-unique
//      DP pattern at the reader level in VamlReader_s361-suite-adjacent, but the
//      markup-level seams are proven here headlessly).
//
// Uses the Shouldly-for-doctest vocabulary. doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/ResourceMarkup.h"
#include "vaml/core/ResourceResolver.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::DynamicResourceLink;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::ResourceDictionary;
using vaml::ResourceMarkup;
using vaml::ResourceReferenceKind;
using vaml::ResourceResolver;
using vaml::ResourceLookupError;
using vaml::ResourceLookupException;
using vaml::ResourceValue;
using vaml::ReferenceDictionary;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::dynamic_resource_markup;
using vaml::parse_resource_markup;
using vaml::static_resource_markup;
using vaml::test::ShouldThrow;

namespace {

// Registers an S361-unique global DP "S361Tint" (an Int32 scalar a resource
// can bind onto) — the S039/S092-unique-names discipline.
std::unordered_set<std::string>& s361_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS361Tint()
{
    if (s361_names().insert("S361Tint").second)
    {
        return DependencyProperty::Register("S361Tint", ValueType::Int32, "S361_Owner",
                                            PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    }
    return DependencyProperty::Get("S361Tint");
}

} // namespace

TEST_CASE("S361: parse_resource_markup recognizes {StaticResource}/{DynamicResource}")
{
    const ResourceMarkup s = parse_resource_markup("{StaticResource MyBrush}");
    SHOULD(ShouldlyIntegral(static_cast<int>(s.recognized)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(s.kind)).
               ShouldBe(static_cast<int>(ResourceReferenceKind::Static)));
    SHOULD(ShouldlyIntegral(static_cast<int>(s.malformed)).ShouldBe(0));
    SHOULD(ShouldlyString(std::string(s.key)).ShouldBe(std::string("MyBrush")));
    SHOULD(ShouldlyIntegral(static_cast<int>(static_resource_markup("{StaticResource MyBrush}"))).ShouldBe(1));

    const ResourceMarkup d = parse_resource_markup("{DynamicResource MyBrush}");
    SHOULD(ShouldlyIntegral(static_cast<int>(d.recognized)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(d.kind)).
               ShouldBe(static_cast<int>(ResourceReferenceKind::Dynamic)));
    SHOULD(ShouldlyString(std::string(d.key)).ShouldBe(std::string("MyBrush")));
    SHOULD(ShouldlyIntegral(static_cast<int>(dynamic_resource_markup("{DynamicResource MyBrush}"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(static_resource_markup("{DynamicResource MyBrush}"))).ShouldBe(0));
}

TEST_CASE("S361: a binding/x:Null source is NOT a resource reference (recognized=false)")
{
    const ResourceMarkup b = parse_resource_markup("{Binding Title}");
    SHOULD(ShouldlyIntegral(static_cast<int>(b.recognized)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(b.kind)).
               ShouldBe(static_cast<int>(ResourceReferenceKind::None)));
    SHOULD(ShouldlyIntegral(static_cast<int>(b.malformed)).ShouldBe(0));

    const ResourceMarkup x = parse_resource_markup("{x:Null}");
    SHOULD(ShouldlyIntegral(static_cast<int>(x.recognized)).ShouldBe(0));
}

TEST_CASE("S361: a malformed {StaticResource } (empty key) is reported malformed")
{
    const ResourceMarkup m = parse_resource_markup("{StaticResource }");
    SHOULD(ShouldlyIntegral(static_cast<int>(m.recognized)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(m.malformed)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(static_resource_markup("{StaticResource }"))).ShouldBe(0));
}

TEST_CASE("S361: Static resolution applies a Plain resource to the target DP once")
{
    registerS361Tint();
    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("TintFive", ResourceValue(Value::Int32(5)));
    el.set_resources(std::move(dict));

    ResourceResolver resolver;
    std::vector<DynamicResourceLink> links;
    const std::size_t applied = resolver.apply_markup(el, "S361Tint", "{StaticResource TintFive}", links);

    SHOULD(ShouldlyString(applied).ShouldBe(1u));
    SHOULD(ShouldlyString(links.size()).ShouldBe(0u)); // static records no dynamic link
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S361Tint"))).ShouldBe(Value::Int32(5)));
}

TEST_CASE("S361: a MISSING static key throws the SHARED ResourceLookupException(MissingKey)")
{
    registerS361Tint();
    UIElement el; // no resources

    ResourceResolver resolver;
    std::vector<DynamicResourceLink> links;
    bool threw = false;
    try
    {
        (void)resolver.apply_markup(el, "S361Tint", "{StaticResource NoSuch}", links);
    }
    catch (const ResourceLookupException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind())).
                   ShouldBe(static_cast<int>(ResourceLookupError::MissingKey)));
        SHOULD(ShouldlyString(e.key()).ShouldBe(std::string("NoSuch")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S361: merge precedence through markup — a merged child supplies the key")
{
    registerS361Tint();
    ResourceDictionary merged;
    merged.set("MergedKey", ResourceValue(Value::Int32(9)));

    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->merge_with(merged); // the element dict merges a child that has the key
    el.set_resources(std::move(dict));

    ResourceResolver resolver;
    std::vector<DynamicResourceLink> links;
    const std::size_t applied = resolver.apply_markup(el, "S361Tint", "{StaticResource MergedKey}", links);
    SHOULD(ShouldlyString(applied).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S361Tint"))).ShouldBe(Value::Int32(9)));
}

TEST_CASE("S361: Dynamic applies now + records a link; reapply re-pushes on a source change")
{
    registerS361Tint();
    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("DynTint", ResourceValue(Value::Int32(1)));
    el.set_resources(std::move(dict));

    ResourceResolver resolver;
    std::vector<DynamicResourceLink> links;
    const std::size_t first = resolver.apply_markup(el, "S361Tint", "{DynamicResource DynTint}", links);
    SHOULD(ShouldlyString(first).ShouldBe(1u));
    SHOULD(ShouldlyString(links.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S361Tint"))).ShouldBe(Value::Int32(1)));

    // Change the source dictionary key -> reapply re-pushes the new value.
    // (The S361-approved re-evaluation seam: the viewer/app calls reapply after
    // a dictionary key change; the element's Resources slot is mutated as the
    // source — a fresh dictionary ownership swap is the honest simulate.)
    auto fresh = std::make_unique<ResourceDictionary>();
    fresh->set("DynTint", ResourceValue(Value::Int32(7)));
    el.set_resources(std::move(fresh));

    const std::size_t reapplied = resolver.reapply_dynamic_resources(links);
    SHOULD(ShouldlyString(reapplied).ShouldBe(1u));
    SHOULD(ShouldlyString(el.Get(PropertyId::of("S361Tint"))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S361: Dynamic NEVER throws on a transiently-missing key (holds)")
{
    registerS361Tint();
    UIElement el; // no resources — the key is missing

    ResourceResolver resolver;
    std::vector<DynamicResourceLink> links;
    // Must NOT throw (dynamic holds on a transiently-missing source).
    const std::size_t applied = resolver.apply_markup(el, "S361Tint", "{DynamicResource Gone}", links);
    SHOULD(ShouldlyString(applied).ShouldBe(0u));
    SHOULD(ShouldlyString(links.size()).ShouldBe(1u)); // the link is still recorded

    // reapply over the missing key is also non-throwing + applies 0.
    const std::size_t reapplied = resolver.reapply_dynamic_resources(links);
    SHOULD(ShouldlyString(reapplied).ShouldBe(0u));
}
