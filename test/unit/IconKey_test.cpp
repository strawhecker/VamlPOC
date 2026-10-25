// S360_1 (dep:S360) — IconKey resource entry: key → icon asset reference,
// resolved through the ResourceDictionary (per ADR-015). A ViewModel exposes
// ONLY an IconKey — never a path/SVG/render object.
//
// Under test (per the S360_1 plan):
//   1. IconKey content-equality (== by content, not pointer/buffer identity).
//   2. A registered icon-asset Object resource resolves via resolve_icon.
//   3. A MISSING key -> IconAssetRef{ok=false} with the SHARED
//      ResourceLookupError::MissingKey (decision 3 — reused from S360, NOT
//      duplicated; S361 reuses the same error).
//   4. A Plain (non-Object) entry for the key -> MissingKey (an invalid icon
//      key — the key resolved to something that is not an asset).
//   5. The S360 three-scope walk: an element-scope entry overrides app/system.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/IconKey.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>

using vaml::DependencyObject;
using vaml::IconAssetRef;
using vaml::IconKey;
using vaml::ResourceDictionary;
using vaml::ResourceLookupError;
using vaml::ResourceValue;
using vaml::ReferenceDictionary;
using vaml::UIElement;
using vaml::Value;
using vaml::resolve_icon;
using vaml::test::ShouldThrow;

namespace {

// A test-local icon-asset object (the owned DependencyObject the dictionary
// maps an IconKey to — the S202_1/S400_1 raster asset).
class IconAssetDuck final : public vaml::DependencyObject
{
public:
    IconAssetDuck() = default;
};

} // namespace

TEST_CASE("S360_1: IconKey compares by content, not buffer address")
{
    const std::string a = "save";
    const std::string b = "sa" + a.substr(2);
    IconKey ka(a);
    IconKey kb(b);
    // a and b are the same text in DIFFERENT buffers — content-equality must hold.
    SHOULD(ShouldlyIntegral(ka == kb).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ka != IconKey("delete")).ShouldBe(true));
    SHOULD(ShouldlyString(std::string(static_cast<std::string_view>(ka))).
               ShouldBe(std::string("save")));
}

TEST_CASE("S360_1: a registered icon-asset Object resource resolves")
{
    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("SaveIcon", std::make_unique<IconAssetDuck>());
    el.set_resources(std::move(dict));

    const IconAssetRef ref = resolve_icon(el, IconKey("SaveIcon"));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.error)).
               ShouldBe(static_cast<int>(ResourceLookupError::None)));
    SHOULD(ShouldlyIntegral(ref.asset != nullptr).ShouldBe(true));
    if (ref.asset)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<const IconAssetDuck*>(ref.asset) != nullptr)
                   .ShouldBe(true));
    }
}

TEST_CASE("S360_1: a MISSING key reuses the shared ResourceLookupError::MissingKey")
{
    UIElement el;
    const IconAssetRef ref = resolve_icon(el, IconKey("NoSuchIcon"));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.error)).
               ShouldBe(static_cast<int>(ResourceLookupError::MissingKey)));
    SHOULD(ShouldlyIntegral(ref.asset == nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(std::string(ref.key)).ShouldBe(std::string("NoSuchIcon")));
}

TEST_CASE("S360_1: a Plain (non-Object) entry for the key is NOT an icon asset -> MissingKey")
{
    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set("ColorKey", ResourceValue(Value::Color(vaml::Color{ 1, 2, 3, 255 })));
    el.set_resources(std::move(dict));

    const IconAssetRef ref = resolve_icon(el, IconKey("ColorKey"));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.error)).
               ShouldBe(static_cast<int>(ResourceLookupError::MissingKey)));
    SHOULD(ShouldlyIntegral(ref.asset == nullptr).ShouldBe(true));
}

TEST_CASE("S360_1: the S360 three-scope walk — element-scope overrides app scope")
{
    // App scope carries SaveIcon; an element-scope SaveIcon overrides it.
    const std::string appKey = "S3601_AppIcon";
    ReferenceDictionary::app_resources().set(appKey, std::make_unique<IconAssetDuck>());
    // Teardown: the app-resources entry holds a POOLED DependencyObject; release
    // it here so the ObjectPool test's master_destroy_all() (S146) does not free
    // a slab still owned by this process-lifetime static (exit-time UAF).
    struct AppGuard
    {
        std::string key;
        ~AppGuard() { ReferenceDictionary::app_resources().remove(key); }
    } appGuard{ appKey };

    UIElement el;
    auto dict = std::make_unique<ResourceDictionary>();
    dict->set(appKey, std::make_unique<IconAssetDuck>());
    el.set_resources(std::move(dict));

    // Element wins (the same key exists in app scope, but the element scope
    // shadows it — find_for_element resolves element-first).
    const IconAssetRef ref = resolve_icon(el, IconKey(appKey));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(ref.error)).
               ShouldBe(static_cast<int>(ResourceLookupError::None)));
    SHOULD(ShouldlyIntegral(ref.asset != nullptr).ShouldBe(true));
}
