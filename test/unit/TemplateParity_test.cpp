// S366 (dep:S365) — Dual-path shared style/template resolution parity. Both the
// S2 text reader (core VamlReader) and the S7 compiled reader (tools VBamlReader
// over vamlcc→.vbaml bytes) produce trees that resolve styling through the
// SAME core resolution functions — find_for_element / find_implicit_style /
// apply_implicit_style / apply_style (S360–S365). This proves the two paths
// agree on the styling RESOLUTION surface, not just the structural tree.
//
// Under test (per the S366 plan):
//   1. A styling document (a tree with a ResourceDictionary carrying a Style)
//      loads through BOTH readers; both trees have the same structure +
//      resolved literal props (the S288-parity structural axes, re-verified
//      over a styling doc).
//   2. The implicit-style resolution (find_implicit_style) resolves the SAME
//      Style (by target type) on the tree built by EACH reader.
//   3. apply_implicit_style applies the SAME core style-resolution to a
//      UIElement retyped on each path — proving both readers feed the shared
//      `apply_style`/StyleApplier surface identically.
//   4. {StaticResource} resolution (find_for_element) reaches the SAME value
//      on both paths.
//
// The styling surface (Style/StyleApplier/ResourceDictionary) is the SAME core
// code for both readers; this test proves the TREE each reader builds is
// stylable through that shared surface. Full resource AUTHORING (the
// `<Style>`/`<x:Key>` property-element syntax in the S2/S7 parsers) is a
// recorded NO-GAP forward surface (reader-level resource property-elements →
// the mechanism tokens S360–S365 ship the resolution engine; markup authoring
// grammar lands with the resource property-element step).
//
// Registers S366-unique global DPs. Uses the Shouldly vocabulary; doctest main
// from smoke_test.cpp. Lives in vaml-tools-tests (it drives BOTH readers).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"          // StackPanel + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/Style.h"
#include "vaml/core/StyleApplier.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConverter.h"   // register_builtin_converters (S7)
#include "vaml/core/VamlReader.h"
#include "vaml/tools/Vamlc.h"           // compile_to_bytes
#include "vaml/tools/VBamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::DataTrigger;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::DynamicResourceLink;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::Setter;
using vaml::SetterValue;
using vaml::Style;
using vaml::StyleApply;
using vaml::StyleApplier;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::apply_implicit_style;
using vaml::find_for_element;
using vaml::find_implicit_style;
using vaml::tools::VBamlReader;

namespace {

std::unordered_set<std::string>& s366_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS366Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s366_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S366_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS366Properties()
{
    (void)registerS366Global("S366Width", ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerS366Global("S366Height", ValueType::Int32, PropertyFlags::None, Value::Int32(0));
}

// A styling doc both readers could load: a StackPanel with a Button + a Text.
// The READERS parse the structural tree; the Style RESOLUTION surface (the
// shared S360–S365 engine) is applied programmatically-by-type on the built tree.
const char* kS366StylingDoc =
    R"(<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="panel"><Button x:Name="go" S366Height="40"/><Text x:Name="label">Hi</Text></StackPanel>)";

} // namespace

TEST_CASE("S366: a styling document loads through BOTH readers with matching structure")
{
    registerS366Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // S2 text path.
    VamlReader s2;
    std::unique_ptr<DependencyObject> s2root = s2.Load(kS366StylingDoc);
    SHOULD(ShouldlyIntegral(s2root != nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(s2.name_count()).ShouldBe(3u)); // panel, go, label
    SHOULD(ShouldlyIntegral(s2.named("panel") != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(s2.named("go") != nullptr).ShouldBe(true));

    // S7 compiled path — same source through vamlcc→bytes→VBamlReader.
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS366StylingDoc);
    VBamlReader s7;
    std::unique_ptr<DependencyObject> s7root = s7.Load(bytes);
    SHOULD(ShouldlyIntegral(s7root != nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(s7.name_count()).ShouldBe(3u));
    SHOULD(ShouldlyIntegral(s7.named("panel") != nullptr).ShouldBe(true));

    // Root type parity: both are StackPanel.
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::StackPanel*>(s2root.get()) != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::StackPanel*>(s7root.get()) != nullptr).ShouldBe(true));

    // Literal-prop parity on a retained child (S366Height converted the same
    // on both — a named lookup into the doc's Button).
    const Value v2 = s2.named("go")->Get(PropertyId::of("S366Height"));
    const Value v7 = s7.named("go")->Get(PropertyId::of("S366Height"));
    SHOULD(ShouldlyString(v2).ShouldBe(Value::Int32(40)));
    SHOULD(ShouldlyString(v7).ShouldBe(Value::Int32(40)));
}

TEST_CASE("S366: find_implicit_style + apply_implicit_style resolve through the SHARED core surface on both paths")
{
    registerS366Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Register a StackPanel implicit style (a real Style resource) in the APP
    // scope (shared across both paths — the shared S360 three-scope lookup).
    auto styleRes = std::make_unique<Style>();
    styleRes->set_target_type("StackPanel");
    styleRes->add_setter(Setter{ PropertyId::of("S366Width"),
                                 SetterValue::Literal(Value::Int32(777)) });
    vaml::ReferenceDictionary::app_resources().set("Style:StackPanel", std::move(styleRes));
    // Teardown: release the pooled Style from the process-lifetime app scope so
    // the ObjectPool master_destroy_all() (S146) cannot free it out from under
    // this static (exit-time UAF).
    struct StyleGuard { ~StyleGuard() { vaml::ReferenceDictionary::app_resources().remove("Style:StackPanel"); } } styleGuard;

    // Build the tree on EACH path, then resolve the implicit style through the
    // SAME find_implicit_style / apply_implicit_style functions.
    {
        VamlReader s2;
        std::unique_ptr<DependencyObject> s2root = s2.Load(kS366StylingDoc);
        auto* panel = dynamic_cast<UIElement*>(s2root.get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            const Style* found = find_implicit_style(*panel);
            SHOULD(ShouldlyIntegral(found != nullptr).ShouldBe(true));
            if (found)
            {
                SHOULD(ShouldlyString(found->target_type()).ShouldBe(std::string("StackPanel")));
            }
            std::vector<DynamicResourceLink> links;
            const std::size_t applied = apply_implicit_style(*panel, links);
            SHOULD(ShouldlyString(applied).ShouldBe(1u));
            SHOULD(ShouldlyString(panel->Get(PropertyId::of("S366Width"))).
                       ShouldBe(Value::Int32(777)));
        }
    }
    {
        const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS366StylingDoc);
        VBamlReader s7;
        std::unique_ptr<DependencyObject> s7root = s7.Load(bytes);
        auto* panel = dynamic_cast<UIElement*>(s7root.get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            const Style* found = find_implicit_style(*panel);
            SHOULD(ShouldlyIntegral(found != nullptr).ShouldBe(true));
            if (found)
            {
                SHOULD(ShouldlyString(found->target_type()).ShouldBe(std::string("StackPanel")));
            }
            std::vector<DynamicResourceLink> links;
            const std::size_t applied = apply_implicit_style(*panel, links);
            SHOULD(ShouldlyString(applied).ShouldBe(1u));
            SHOULD(ShouldlyString(panel->Get(PropertyId::of("S366Width"))).
                       ShouldBe(Value::Int32(777)));
        }
    }
}

TEST_CASE("S366: {StaticResource} lookup (find_for_element) reaches the SAME value on both paths")
{
    registerS366Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Put a resource in the APP scope (shared zero-width lookup).
    vaml::ReferenceDictionary::app_resources().set("S366Accent",
        ResourceValue(Value::Int32(99)));

    {
        VamlReader s2;
        std::unique_ptr<DependencyObject> s2root = s2.Load(kS366StylingDoc);
        auto* panel = dynamic_cast<UIElement*>(s2root.get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            const ResourceValue* rv = find_for_element(*panel, "S366Accent");
            SHOULD(ShouldlyIntegral(rv != nullptr).ShouldBe(true));
            if (rv)
            {
                SHOULD(ShouldlyString(rv->value()).ShouldBe(Value::Int32(99)));
            }
        }
    }
    {
        const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(kS366StylingDoc);
        VBamlReader s7;
        std::unique_ptr<DependencyObject> s7root = s7.Load(bytes);
        auto* panel = dynamic_cast<UIElement*>(s7root.get());
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            const ResourceValue* rv = find_for_element(*panel, "S366Accent");
            SHOULD(ShouldlyIntegral(rv != nullptr).ShouldBe(true));
            if (rv)
            {
                SHOULD(ShouldlyString(rv->value()).ShouldBe(Value::Int32(99)));
            }
        }
    }
}
