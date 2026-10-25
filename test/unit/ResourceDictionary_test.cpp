// S360 (dep:S329) — ResourceDictionary + merged dictionaries + three-scope
// lookup (tree + app + system). The Sprint-9 styling/resource foundation.
//
// Under test (per the S360 plan + the accepted Sprint-9 decisions):
//   1. set/try_get/get round-trip — Object (an owned built DependencyObject),
//      Plain (a Value), and the dedupe-throws-unchanged contract
//      (ResourceAlreadyRegistered, store unchanged — S032/S039 discipline).
//   2. get() throws ResourceLookupException(MissingKey) naming the key (the
//      SHARED missing-key error also reused by S360_1 icon + S361 StaticResource).
//   3. merge precedence — self-WINS over a merged child; among merged children
//      the LAST merged wins (the S361 precedence contract authored + proven here).
//   4. find_for_element — the element's own Resources -> each UIElement ancestor's
//      Resources -> app (ReferenceDictionary::app_resources) -> system
//      (ReferenceDictionary::system_resources), with shadowing at each scope.
//   5. keys() insertion order; move-only; `resources()` slot default absent.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <string_view>

using vaml::DependencyObject;
using vaml::ResourceAlreadyRegistered;
using vaml::ResourceDictionary;
using vaml::ResourceLookupError;
using vaml::ResourceLookupException;
using vaml::ResourceValue;
using vaml::ReferenceDictionary;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::find_for_element;
using vaml::find_resource;
using vaml::test::ShouldThrow;

namespace {

// A test-local owned resource object (a real DependencyObject subclass). S360
// resources are owned DependencyObjects (built via the S087 factory registry in
// real authoring; a hand-constructed object stands in here).
class ResourceDuck final : public vaml::DependencyObject
{
public:
    ResourceDuck() = default;
};

} // namespace

TEST_CASE("S360: ResourceValue wraps an owned DependencyObject (Object kind)")
{
    ResourceDictionary dict;
    auto duck = std::make_unique<ResourceDuck>();
    ResourceDuck* raw = duck.get();
    dict.set("DuckKey", std::move(duck));

    const ResourceValue* v = dict.try_get("DuckKey");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(v->kind())).
                   ShouldBe(static_cast<int>(ResourceValue::Kind::Object)));
        SHOULD(ShouldlyIntegral(v->object() == raw).ShouldBe(true));
        // non-owning read still points at the LIVE object
        SHOULD(ShouldlyIntegral(v->object() != nullptr).ShouldBe(true));
    }
}

TEST_CASE("S360: ResourceValue wraps a plain Value (Plain kind)")
{
    ResourceDictionary dict;
    dict.set("ColorKey", ResourceValue(Value::Color(vaml::Color{ 10, 20, 30, 255 })));

    const ResourceValue* v = dict.try_get("ColorKey");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(v->kind())).
                   ShouldBe(static_cast<int>(ResourceValue::Kind::Plain)));
        SHOULD(ShouldlyIntegral(static_cast<int>(v->value().type())).
                   ShouldBe(static_cast<int>(ValueType::Color)));
        const vaml::Color c = v->value().get<vaml::Color>();
        SHOULD(ShouldlyIntegral(static_cast<int>(c.r)).ShouldBe(10));
        SHOULD(ShouldlyIntegral(static_cast<int>(c.g)).ShouldBe(20));
    }
}

TEST_CASE("S360: try_get returns nullptr for an absent key; has() is false")
{
    ResourceDictionary dict;
    dict.set("K", ResourceValue(Value::Int32(7)));
    SHOULD(ShouldlyIntegral(dict.try_get("Missing") == nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(dict.has("K"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(dict.has("Missing"))).ShouldBe(0));
}

TEST_CASE("S360: get() throws ResourceLookupException(MissingKey) naming the key")
{
    ResourceDictionary dict;
    dict.set("K", ResourceValue(Value::Int32(7)));
    bool threw = false;
    try
    {
        (void)dict.get("NotFound");
    }
    catch (const ResourceLookupException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind())).
                   ShouldBe(static_cast<int>(ResourceLookupError::MissingKey)));
        SHOULD(ShouldlyString(e.key()).ShouldBe(std::string("NotFound")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S360: a duplicate key throws ResourceAlreadyRegistered and leaves the store unchanged")
{
    ResourceDictionary dict;
    dict.set("K", ResourceValue(Value::Int32(1)));

    bool threw = false;
    try
    {
        dict.set("K", ResourceValue(Value::Int32(99)));
    }
    catch (const ResourceAlreadyRegistered&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // The store is UNCHANGED — the ORIGINAL value 1 is retained (never last-wins).
    const ResourceValue* v = dict.try_get("K");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(1)));
    }
}

TEST_CASE("S360: keys() returns insertion order; size() counts self entries")
{
    ResourceDictionary dict;
    dict.set("A", ResourceValue(Value::Int32(1)));
    dict.set("B", ResourceValue(Value::Int32(2)));
    dict.set("C", ResourceValue(Value::Int32(3)));

    const auto keys = dict.keys();
    SHOULD(ShouldlyString(keys.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(std::string(keys[0])).ShouldBe(std::string("A")));
    SHOULD(ShouldlyString(std::string(keys[1])).ShouldBe(std::string("B")));
    SHOULD(ShouldlyString(std::string(keys[2])).ShouldBe(std::string("C")));
    SHOULD(ShouldlyString(dict.size()).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(static_cast<int>(dict.empty())).ShouldBe(0));
}

TEST_CASE("S360: merge — self WINS over a merged child for the same key")
{
    ResourceDictionary parent;
    ResourceDictionary child;
    parent.set("K", ResourceValue(Value::Int32(1)));
    child.set("K", ResourceValue(Value::Int32(999)));
    parent.merge_with(child);

    // SELF-WINS: the parent's own 1 beats the merged child's 999.
    const ResourceValue* v = find_resource(parent, "K");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(1)));
    }
}

TEST_CASE("S360: merge — among merged children the LAST merged wins")
{
    ResourceDictionary parent;
    ResourceDictionary childA;
    ResourceDictionary childB;
    childA.set("Shared", ResourceValue(Value::Int32(10)));
    childB.set("Shared", ResourceValue(Value::Int32(20)));
    parent.merge_with(childA);
    parent.merge_with(childB);

    // Sibling merge: LAST merged wins (20).
    const ResourceValue* v = find_resource(parent, "Shared");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(20)));
    }
}

TEST_CASE("S360: merge — a key only in a child resolves; absent everywhere is nullptr")
{
    ResourceDictionary parent;
    ResourceDictionary child;
    child.set("OnlyChild", ResourceValue(Value::Int32(5)));
    parent.merge_with(child);

    const ResourceValue* v = find_resource(parent, "OnlyChild");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(5)));
    }
    SHOULD(ShouldlyIntegral(find_resource(parent, "Absent") == nullptr).ShouldBe(true));
    // merge is idempotent (adding the same child twice is a no-op)
    parent.merge_with(child);
    SHOULD(ShouldlyIntegral(parent.merged().size()).ShouldBe(1u));
}

TEST_CASE("S360: find_for_element — element own resources shadow parent")
{
    ResourceDictionary parentRes;
    parentRes.set("K", ResourceValue(Value::Int32(1)));
    ResourceDictionary childRes;
    childRes.set("K", ResourceValue(Value::Int32(2)));

    UIElement parentEl;
    parentEl.set_resources(std::make_unique<ResourceDictionary>(std::move(parentRes)));
    auto childOwned = std::make_unique<UIElement>();
    childOwned->set_resources(std::make_unique<ResourceDictionary>(std::move(childRes)));
    UIElement* childPtr = childOwned.get();
    parentEl.add_child(std::move(childOwned));

    // The child's OWN K=2 shadows the parent's K=1 (tree walk resolves child-first).
    const ResourceValue* v = find_for_element(*childPtr, "K");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(2)));
    }
    // The parent's own K=1 resolves from the parent.
    const ResourceValue* pv = find_for_element(parentEl, "K");
    SHOULD(ShouldlyIntegral(pv != nullptr).ShouldBe(true));
    if (pv)
    {
        SHOULD(ShouldlyString(pv->value()).ShouldBe(Value::Int32(1)));
    }
}

TEST_CASE("S360: find_for_element — parent chain then app then system (shadowing at each scope)")
{
    // Build a 3-level tree root -> mid -> leaf, no own resources on any node.
    UIElement root;
    auto mid = std::make_unique<UIElement>();
    UIElement* midPtr = mid.get();
    auto leaf = std::make_unique<UIElement>();
    UIElement* leafPtr = leaf.get();
    midPtr->add_child(std::move(leaf));
    root.add_child(std::move(mid));

    // Use a unique key per test to avoid cross-test pollution of the process-
    // wide app singleton.
    const std::string appKey = "S360_AppOnly_7";

    // (1) No tree resource -> resolve from APP scope.
    auto& appGlobal = ReferenceDictionary::app_resources();
    appGlobal.set(appKey, ResourceValue(Value::Int32(42)));
    const ResourceValue* v = find_for_element(*leafPtr, appKey);
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyString(v->value()).ShouldBe(Value::Int32(42)));
    }

    // (2) SYSTEM scope: a standard system key resolves when neither tree nor
    // app provides it.
    const ResourceValue* sys = find_for_element(*leafPtr, "SystemWindowBackgroundColor");
    SHOULD(ShouldlyIntegral(sys != nullptr).ShouldBe(true));

    // (3) Tree shadows app: put the key on the ROOT; the leaf walk reaches it
    // through the parent chain (mid, then root) BEFORE app scope.
    ResourceDictionary rootRes;
    rootRes.set(appKey, ResourceValue(Value::Int32(7)));
    root.set_resources(std::make_unique<ResourceDictionary>(std::move(rootRes)));
    const ResourceValue* v2 = find_for_element(*leafPtr, appKey);
    SHOULD(ShouldlyIntegral(v2 != nullptr).ShouldBe(true));
    if (v2)
    {
        SHOULD(ShouldlyString(v2->value()).ShouldBe(Value::Int32(7)));
    }
}

TEST_CASE("S360: a fresh UIElement has no Resources by default")
{
    UIElement el;
    SHOULD(ShouldlyIntegral(el.resources() == nullptr).ShouldBe(true));

    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("K", ResourceValue(Value::Int32(3)));
    el.set_resources(std::move(dict));
    SHOULD(ShouldlyIntegral(el.resources() != nullptr).ShouldBe(true));
    if (el.resources())
    {
        SHOULD(ShouldlyString(el.resources()->size()).ShouldBe(1u));
    }
}

TEST_CASE("S360: ResourceValue supports a deferred reference (S361/S365 carry it)")
{
    ResourceDictionary dict;
    dict.set("DynKey", ResourceValue::DeferredRef(ResourceValue::RefKind::DynamicResource, "Target"));

    const ResourceValue* v = dict.try_get("DynKey");
    SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
    if (v)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(v->kind())).
                   ShouldBe(static_cast<int>(ResourceValue::Kind::Deferred)));
        SHOULD(ShouldlyIntegral(static_cast<int>(v->ref_kind())).
                   ShouldBe(static_cast<int>(ResourceValue::RefKind::DynamicResource)));
        SHOULD(ShouldlyString(std::string(v->ref_key())).ShouldBe(std::string("Target")));
    }
}
