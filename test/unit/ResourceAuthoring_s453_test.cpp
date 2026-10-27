// S453 (dep:S452) — RESOURCE-DICTIONARY AUTHORING + x:Key (the S366 NO-GAP
// hand-off RESOLVED). The S2 text reader authors <ResourceDictionary> from real
// .vaml:
//   * `<Button.Resources>` / `<Root.Resources>` property-element builds an OWNED
//     ResourceDictionary routed into the S360 side-table (set_element_resources),
//     NOT a plain DP (accepted-decision 3).
//   * an authored child carrying `x:Key` is built + captured via
//     `dict.set(key, owned object)` — an S360 Object resource; authored resources
//     now REACH the S360 store (the S361 x:Key deferral RESOLVED).
//   * `<ResourceDictionary.MergedDictionaries>` with multiple `<ResourceDictionary>`
//     children -> each merges (self-wins + sibling-last-wins, existing S360).
//   * `{StaticResource Key}` on the ELEMENT resolves through the S361
//     three-scope lookup over the AUTHORED dict via find_for_element.
//   * duplicate authored x:Key -> ResourceAlreadyRegistered (dedupe, unchanged);
//     an authored resource with NO x:Key in a dictionary -> structured reader error;
//     a missing authored key via {StaticResource} -> ResourceLookupException(MissingKey).
//
// ADR-018 zero locks; ADR-021 headless (drives the real VamlReader).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Brush.h"                 // SolidColorBrush
#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"               // StackPanel + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/ResourceResolver.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::ResourceAlreadyRegistered;
using vaml::ResourceDictionary;
using vaml::ResourceLookupException;
using vaml::ResourceValue;
using vaml::SolidColorBrush;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::find_for_element;

namespace {

std::unordered_set<std::string>& s453_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS453Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s453_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S453_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS453()
{
    (void)registerS453Global("S453Background", ValueType::Color, PropertyFlags::None,
                             Value::Color(Color{0, 0, 0, 0}));
    vaml::register_builtin_elements();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. A `<X.Resources>` property-element routes an owned dictionary into the side-table
// ---------------------------------------------------------------------------

TEST_CASE("S453: <Element.Resources> routes an authored dictionary into the S360 side-table")
{
    registerS453();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<SolidColorBrush x:Key=\"B\" Color=\"#FF0000\"/>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));

    // The authored dict is in the THREE-SCOPE store for the element.
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        SHOULD(ShouldlyIntegral(dict->size()).ShouldBe(1u));
        // The authored SolidColorBrush is captured BY its x:Key as an Object resource.
        const ResourceValue* v = dict->try_get("B");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(v->kind() == ResourceValue::Kind::Object).ShouldBe(true));
            const SolidColorBrush* brush = dynamic_cast<const SolidColorBrush*>(v->object());
            SHOULD(ShouldlyIntegral(brush != nullptr).ShouldBe(true));
            if (brush)
            {
                SHOULD(ShouldlyString(brush->color()).ShouldBe(Color{255, 0, 0, 255}));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 2. `{StaticResource Key}` on a child resolves the AUTHORED dict up the tree
// ---------------------------------------------------------------------------

TEST_CASE("S453: {StaticResource} on a child resolves through the authored dict (parent chain)")
{
    registerS453();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<SolidColorBrush x:Key=\"B\" Color=\"#00FF00\"/>"
        "</StackPanel.Resources>"
        "<StackPanel S453Background=\"{StaticResource B}\"/></StackPanel>");

    // BindResources resolves the deferred {StaticResource B} against the S361
    // three-scope lookup. The KEY IS FOUND in the parent-authored dict, so
    // BindResources must NOT throw (a missing key would throw MissingKey —
    // this proves the authored dict is in the lookup path).
    bool threw = false;
    try
    {
        (void)reader.BindResources();
    }
    catch (const ResourceLookupException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(false));

    // The authored SOLIDCOLORBRUSH OBJECT is resolved through find_for_element
    // (walks the parent's authored dict). This is the S453 dictionary-authoring
    // deliverable. The Object->scalar DP materialization (turning the resolved
    // Brush into a rendered Red) is the S364/S400/S12 renderer boundary —
    // recorded NO-GAP, not silent (an Object resource is not a scalar set on the
    // DP-set path at S453).
    const UIElement* inner = nullptr;
    for (const auto& o : reader.retained_objects())
    {
        if (dynamic_cast<StackPanel*>(o.get()) != nullptr)
        {
            inner = dynamic_cast<StackPanel*>(o.get());
            break;
        }
    }
    SHOULD(ShouldlyIntegral(inner != nullptr).ShouldBe(true));
    if (inner)
    {
        const ResourceValue* v = find_for_element(*inner, "B");
        SHOULD(ShouldlyIntegral(v != nullptr).ShouldBe(true));
        if (v)
        {
            SHOULD(ShouldlyIntegral(v->kind() == ResourceValue::Kind::Object).ShouldBe(true));
            const SolidColorBrush* brush = dynamic_cast<const SolidColorBrush*>(v->object());
            SHOULD(ShouldlyIntegral(brush != nullptr).ShouldBe(true));
        }
    }
}

// ---------------------------------------------------------------------------
// 3. MergedDictionaries with multiple <ResourceDictionary> children
// ---------------------------------------------------------------------------

TEST_CASE("S453: MergedDictionaries merges children; merged keys resolve through the parent")
{
    registerS453();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
        "<ResourceDictionary>"
        "<SolidColorBrush x:Key=\"Shared\" Color=\"#0000FF\"/>"
        "<ResourceDictionary.MergedDictionaries>"
        "<ResourceDictionary><SolidColorBrush x:Key=\"M1\" Color=\"#FF0000\"/></ResourceDictionary>"
        "<ResourceDictionary><SolidColorBrush x:Key=\"M2\" Color=\"#00FF00\"/></ResourceDictionary>"
        "</ResourceDictionary.MergedDictionaries>"
        "</ResourceDictionary>"
        "</StackPanel.Resources></StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
    const ResourceDictionary* dict = vaml::detail::get_element_resources(panel);
    SHOULD(ShouldlyIntegral(dict != nullptr).ShouldBe(true));
    if (dict)
    {
        // Shared is in the self dict; the merged children resolve via merge.
        SHOULD(ShouldlyIntegral(dict->try_get("Shared") != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dict->try_get("M1") != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(dict->try_get("M2") != nullptr).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// 4. Duplicate authored x:Key -> ResourceAlreadyRegistered (dedupe, unchanged)
// ---------------------------------------------------------------------------

TEST_CASE("S453: a duplicate authored x:Key throws ResourceAlreadyRegistered")
{
    registerS453();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
            "<SolidColorBrush x:Key=\"B\" Color=\"#FF0000\"/>"
            "<SolidColorBrush x:Key=\"B\" Color=\"#00FF00\"/>"
            "</StackPanel.Resources></StackPanel>");
    }
    catch (const ResourceAlreadyRegistered&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 5. Authored resource with NO x:Key in a dictionary -> structured reader error
// ---------------------------------------------------------------------------

TEST_CASE("S453: an authored resource with NO x:Key in a dictionary is an authoring error")
{
    registerS453();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources>"
            "<SolidColorBrush Color=\"#FF0000\"/>"
            "</StackPanel.Resources></StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::UnknownProperty)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 6. Missing key via {StaticResource} -> ResourceLookupException(MissingKey)
// ---------------------------------------------------------------------------

TEST_CASE("S453: a missing authored key via {StaticResource} -> MissingKey (Class A)")
{
    registerS453();
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"><StackPanel.Resources></StackPanel.Resources>"
        "<StackPanel S453Background=\"{StaticResource DoesNotExist}\"/></StackPanel>");

    bool threw = false;
    try
    {
        (void)reader.BindResources();
    }
    catch (const ResourceLookupException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(true));
}
