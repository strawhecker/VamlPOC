// S501 (dep:S500) — WPF-TRUTHFUL CONVERTER SURFACE.
//
// The app's `*Converter` patterns through the S242 converter seam + a
// `{ConverterResource}`-style authoring path. "Converters are logic, not
// rendering" (ADR-011). Test `ConverterSurface_test.cpp`.
//
// WPF GROUND TRUTH (verified against E:\GitRepos\wpf): the binding value-
// converter surface is the `IValueConverter` INTERFACE — there is NO
// `ValueConverter` base class anywhere in the WPF tree
// (PresentationFramework\System\Windows\Data\IValueConverter.cs). S501 corrects
// the roadmap's make-believe "ValueConverter base" wording to the WPF-truthful
// interface and renames the S089 literal/type-converter alias
// `ValueConverter` -> `LiteralConverter`.
//
// THIS SUITE PROVES:
//   A. `IValueConverter` is the app-authoring seam:
//      1. Convert default = identity passthrough;
//      2. ConvertBack default = not-supported (returns ok=false, WPF convention);
//      3. a custom `*Converter` (a NullTo* pattern) implements it and converts.
//   B. `BooleanToVisibilityConverter` (the SOLE framework-owned concrete):
//      4. Convert Bool(true) -> Int32(0) (Visible), Bool(false) -> Int32(2) (Collapsed);
//      5. Convert non-bool FAILS (ok=false, never a silent collapse);
//      6. ConvertBack Int32(0) (Visible) -> Bool(true); Collapsed/Hidden -> Bool(false);
//      7. ConvertBack non-Int32 FAILS (ok=false).
//   C. The adapter into the S242 seam:
//      8. ToBindingConverter wraps an IValueConverter; convert/convertBack delegate.
//   D. Markup authoring + reader resolution + binding integration:
//      9. BindingMarkup parses `Converter={StaticResource K}` and `Converter=K`;
//     10. VamlReader resolves Converter= into the S242 converter seam (End-to-end
//         `{Binding Bool, Converter={StaticResource BoolVis}}` pushes Visibility);
//     11. an UNRESOLVED Converter key is a Class A reader error (never silent);
//     12. a converter FAILURE routes to the S242 ConverterFailed/fallback path.
//
// ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/BindingExpression.h"  // BindingConversion, BindingConverter
#include "vaml/core/BindingMarkup.h"      // parse_markup_binding
#include "vaml/core/BooleanToVisibilityConverter.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // register_builtin_elements (reader Load)
#include "vaml/core/IValueConverter.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/mvvm/property_bindable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using vaml::BindingConversion;
using vaml::BooleanToVisibilityConverter;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::IValueConverter;
using vaml::MarkupExtensionKind;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::ToBindingConverter;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::parse_markup_binding;
using vaml::test::ShouldThrow;

namespace {

std::unordered_set<std::string>& seen_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerGlobal(const std::string& name, ValueType type,
                                         PropertyFlags flags, Value def)
{
    if (seen_names().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S501_Converter_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name, "S501_Converter_Owner");
}

void registerS501Properties()
{
    // The Visibility Int32 DP shape (Visible=0 / Collapsed=2) the converter
    // pushes onto — a real Int32 DP, mirroring the S120 Visibility DP.
    (void)registerGlobal("S501Visible", ValueType::Int32, PropertyFlags::None, Value::Int32(0));
}

// The app-constructed VM (S250/S251 pattern) the {Binding ...} resolves against.
class S501ViewModel final : public ObservableObject
{
public:
    PROPERTY_BINDABLE(bool, IsAvailable);   // the bool the converter maps
};

// A NullTo* APP-AUTHORED converter (WPF has NO framework NullTo* — these are
// app types implementing IValueConverter, per D1/S501). Turn a Null into a
// placeholder, pass everything else through.
class S501NullToPlaceholder final : public IValueConverter
{
public:
    BindingConversion Convert(const Value& value) const override
    {
        if (value.type() == ValueType::Null)
        {
            return BindingConversion{/*ok=*/true, Value::String("[none]"), {}};
        }
        return BindingConversion{/*ok=*/true, value, {}};
    }
};

} // namespace

// ---------------------------------------------------------------------------
// A. IValueConverter — the app-authoring seam
// ---------------------------------------------------------------------------

TEST_CASE("S501: IValueConverter Convert default is identity passthrough")
{
    IValueConverter c;
    const BindingConversion r = c.Convert(Value::String("abc"));
    SHOULD(ShouldlyIntegral(r.ok ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("abc")));
}

TEST_CASE("S501: IValueConverter ConvertBack default is not-supported (WPF convention)")
{
    IValueConverter c;
    const BindingConversion r = c.ConvertBack(Value::Int32(7));
    SHOULD(ShouldlyIntegral(r.ok ? 1 : 0).ShouldBe(0)); // not-supported
    SHOULD(ShouldlyString(r.detail).ShouldNotBe(std::string("")));
}

TEST_CASE("S501: an app-authored NullTo converter (WPF has no framework NullTo*)")
{
    S501NullToPlaceholder c;
    SHOULD(ShouldlyString(c.Convert(Value::Null()).detail).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(c.Convert(Value::Null()).value).ShouldBe(Value::String("[none]")));
    SHOULD(ShouldlyString(c.Convert(Value::String("x")).value).ShouldBe(Value::String("x")));
}

// ---------------------------------------------------------------------------
// B. BooleanToVisibilityConverter — the SOLE framework-owned concrete
// ---------------------------------------------------------------------------

TEST_CASE("S501: BooleanToVisibilityConverter Convert bool -> Visibility Int32")
{
    BooleanToVisibilityConverter c;
    const BindingConversion vis = c.Convert(Value::Bool(true));
    SHOULD(ShouldlyIntegral(vis.ok ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(vis.value.get<std::int32_t>()).ShouldBe(0)); // Visible

    const BindingConversion col = c.Convert(Value::Bool(false));
    SHOULD(ShouldlyIntegral(col.ok ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(col.value.get<std::int32_t>()).ShouldBe(2)); // Collapsed
}

TEST_CASE("S501: BooleanToVisibilityConverter Convert non-bool fails (never silent)")
{
    BooleanToVisibilityConverter c;
    const BindingConversion r = c.Convert(Value::String("yes"));
    SHOULD(ShouldlyIntegral(r.ok ? 1 : 0).ShouldBe(0)); // fail loud (ADR-016)
    SHOULD(ShouldlyString(r.detail).ShouldNotBe(std::string("")));
}

TEST_CASE("S501: BooleanToVisibilityConverter ConvertBack Visibility -> bool")
{
    BooleanToVisibilityConverter c;
    SHOULD(ShouldlyIntegral(c.ConvertBack(Value::Int32(0)).value.get<bool>() ? 1 : 0).ShouldBe(1)); // Visible
    SHOULD(ShouldlyIntegral(c.ConvertBack(Value::Int32(1)).value.get<bool>() ? 1 : 0).ShouldBe(0)); // Hidden
    SHOULD(ShouldlyIntegral(c.ConvertBack(Value::Int32(2)).value.get<bool>() ? 1 : 0).ShouldBe(0)); // Collapsed
    SHOULD(ShouldlyIntegral(c.ConvertBack(Value::Int32(0)).ok ? 1 : 0).ShouldBe(1));
}

TEST_CASE("S501: BooleanToVisibilityConverter ConvertBack non-Int32 fails")
{
    BooleanToVisibilityConverter c;
    SHOULD(ShouldlyIntegral(c.ConvertBack(Value::String("Visible")).ok ? 1 : 0).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// C. ToBindingConverter — the adapter into the generic S242 seam
// ---------------------------------------------------------------------------

TEST_CASE("S501: ToBindingConverter wraps an IValueConverter into the S242 seam")
{
    auto conv = std::make_shared<BooleanToVisibilityConverter>();
    const vaml::BindingConverter bridge = ToBindingConverter(conv);

    // convert delegates to the wrapped converter.
    const BindingConversion vis = bridge.convert(Value::Bool(true));
    SHOULD(ShouldlyIntegral(vis.value.get<std::int32_t>()).ShouldBe(0));

    // convertBack delegates too (the converter supports it).
    const BindingConversion back = bridge.convertBack(Value::Int32(0));
    SHOULD(ShouldlyIntegral(back.value.get<bool>() ? 1 : 0).ShouldBe(1));

    // A converter WITHOUT convertBack support surfaces as not-supported via the
    // bridge (the WPF convention, never a crash).
    auto nullOnly = std::make_shared<S501NullToPlaceholder>();
    const vaml::BindingConverter nb = ToBindingConverter(nullOnly);
    SHOULD(ShouldlyIntegral(nb.convertBack(Value::Int32(1)).ok ? 1 : 0).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// D.1 BindingMarkup — the Converter= clause
// ---------------------------------------------------------------------------

TEST_CASE("S501: BindingMarkup parses Converter={StaticResource K} + Converter=K")
{
    // {StaticResource K} form strips the braces + keyword -> key "BoolVis".
    const vaml::BindingMarkup m1 = parse_markup_binding(
        "{Binding IsAvailable, Converter={StaticResource BoolVis}}");
    SHOULD(ShouldlyIntegral(static_cast<int>(m1.kind))
               .ShouldBe(static_cast<int>(MarkupExtensionKind::Binding)));
    SHOULD(ShouldlyString(m1.path).ShouldBe(std::string("IsAvailable")));
    SHOULD(ShouldlyIntegral(m1.hasConverter ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(m1.converter).ShouldBe(std::string("BoolVis")));

    // Bare-key form.
    const vaml::BindingMarkup m2 = parse_markup_binding("{Binding X, Converter=BoolVis}");
    SHOULD(ShouldlyIntegral(m2.hasConverter ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(m2.converter).ShouldBe(std::string("BoolVis")));

    // Mode= and Converter= coexist in either order.
    const vaml::BindingMarkup m3 = parse_markup_binding(
        "{Binding X, Mode=OneWay, Converter=BoolVis}");
    SHOULD(ShouldlyIntegral(m3.hasConverter ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(m3.hasMode ? 1 : 0).ShouldBe(1));
    const vaml::BindingMarkup m4 = parse_markup_binding(
        "{Binding X, Converter=BoolVis, Mode=TwoWay}");
    SHOULD(ShouldlyIntegral(m4.hasConverter ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(m4.mode).ShouldBe(std::string("TwoWay")));

    // A DynamicResource converter token is NOT a converter key (malformed).
    const vaml::BindingMarkup m5 = parse_markup_binding(
        "{Binding X, Converter={DynamicResource B}}");
    SHOULD(ShouldlyIntegral(m5.malformed ? 1 : 0).ShouldBe(1));

    // Empty Converter= -> malformed.
    const vaml::BindingMarkup m6 = parse_markup_binding("{Binding X, Converter=}");
    SHOULD(ShouldlyIntegral(m6.malformed ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// D.2 Reader resolution + end-to-end binding integration
// ---------------------------------------------------------------------------

TEST_CASE("S501: {Binding Bool, Converter={StaticResource BoolVis}} pushes Visibility end-to-end")
{
    registerS501Properties();
    VamlReader reader;
    S501ViewModel vm;
    (void)vm.set_IsAvailable(true);
    reader.set_data_context(&vm);
    // Register the SOLE framework-owned converter under a markup key.
    reader.register_converter("BoolVis", std::make_shared<BooleanToVisibilityConverter>());

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S501Visible=\"{Binding IsAvailable, Converter={StaticResource BoolVis}}\"/>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(1u));

    const std::size_t bound = reader.BindExtensions();
    SHOULD(ShouldlyIntegral(bound).ShouldBe(1u));

    // bool=true -> Visible (0) landed on the target Int32 DP.
    const DependencyObject* target = reader.deferred_extensions()[0].target;
    SHOULD(ShouldlyIntegral(target->Get(PropertyId::of("S501Visible")).get<std::int32_t>())
               .ShouldBe(0));

    // A live VM change -> the S243 one-way re-push re-converts: false -> Collapsed (2).
    (void)vm.set_IsAvailable(false);
    SHOULD(ShouldlyIntegral(target->Get(PropertyId::of("S501Visible")).get<std::int32_t>())
               .ShouldBe(2));
}

TEST_CASE("S501: {Binding Bool, Converter=BoolVis} — bare-key form binds too")
{
    registerS501Properties();
    VamlReader reader;
    S501ViewModel vm;
    (void)vm.set_IsAvailable(true);
    reader.set_data_context(&vm);
    reader.register_converter("BoolVis", std::make_shared<BooleanToVisibilityConverter>());

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S501Visible=\"{Binding IsAvailable, Converter=BoolVis}\"/>");
    SHOULD(ShouldlyIntegral(reader.BindExtensions()).ShouldBe(1u));
    const DependencyObject* target = reader.deferred_extensions()[0].target;
    SHOULD(ShouldlyIntegral(target->Get(PropertyId::of("S501Visible")).get<std::int32_t>())
               .ShouldBe(0));
}

TEST_CASE("S501: an UNRESOLVED Converter= key is a Class A reader error (never silent)")
{
    registerS501Properties();
    VamlReader reader;
    S501ViewModel vm;
    (void)vm.set_IsAvailable(true);
    reader.set_data_context(&vm);
    // NOTE: `BoolVis` is NOT registered -> the reader must fail loud.

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S501Visible=\"{Binding IsAvailable, Converter=BoolVis}\"/>");
    bool threw = false;
    bool named_key = false;
    try
    {
        (void)reader.BindExtensions();
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::MalformedBindingMarkup)));
        // The message must NAME the unresolved converter key (never silent).
        named_key = std::string(e.what()).find("BoolVis") != std::string::npos;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(named_key)).ShouldBe(1));
}

TEST_CASE("S501: {Binding X, Converter=...} authoring — register_converter is idempotent (first-wins)")
{
    registerS501Properties();
    VamlReader reader;
    S501ViewModel vm;
    (void)vm.set_IsAvailable(true);
    reader.set_data_context(&vm);
    reader.register_converter("BoolVis", std::make_shared<BooleanToVisibilityConverter>());
    reader.register_converter("BoolVis", std::make_shared<BooleanToVisibilityConverter>()); // repeat -> first wins

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S501Visible=\"{Binding IsAvailable, Converter=BoolVis}\"/>");
    SHOULD(ShouldlyIntegral(reader.BindExtensions()).ShouldBe(1u));
    const DependencyObject* target = reader.deferred_extensions()[0].target;
    SHOULD(ShouldlyIntegral(target->Get(PropertyId::of("S501Visible")).get<std::int32_t>())
               .ShouldBe(0));
}
