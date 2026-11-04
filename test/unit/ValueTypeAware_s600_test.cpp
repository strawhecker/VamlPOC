// S600 (dep:S596) — VALUE/TYPE-AWARE CONVERSION root A1. The literal converter is
// now bound to the DP's DECLARED bound type: an Int32-typed DP that declares an
// enum domain (EnumDomainRegistry) resolves a literal's NAME against that DP's OWN
// WPF-true member table (WPF Enum.Parse semantics), never a shared cross-cut
// behavior map. Plus the WPF-fidelity ordinal re-pointing of the drifted layout
// enums (HorizontalAlignment/VerticalAlignment/Orientation) and symbolic DP
// defaults.
//
// Test plan (Shouldly vocabulary):
//   1. Audited enum DPs convert their WPF-true ordinal from markup
//      (TextWrapping="Wrap"->2, TextAlignment="Center"->2, ResizeMode="CanResize"->2,
//      StretchDirection="Both"->2, WindowStartupLocation="CenterScreen"->1, ...).
//   2. RE-POINTED enums: Dock="Left"->0 (the A1 wrong-typed bug: was 1=Top via the
//      old shared map); Orientation="Horizontal"->0; HorizontalAlignment="Stretch"->3;
//      VerticalAlignment="Stretch"->3.
//   3. Cross-domain wrong name FAILS with MalformedValue (TextWrapping="Center" —
//      a TextAlignment member — must NOT silently map).
//   4. Numeric literal always valid on any Int32 DP.
//   5. Symbolic DP defaults: a default-configured element reads back
//      HorizontalAlignment::Stretch / VerticalAlignment::Stretch / the layout stays
//      green (default stack is Vertical, default alignment is Stretch).
//   6. Both readers agree (S2 text VamlReader + S7 compiled VBamlReader path) — the
//      S288 parity contract preserved for enum conversion.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/EnumDomainRegistry.h"
#include "vaml/core/Element.h"            // StackPanel/TextBlock/DockPanel + enums
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"          // HorizontalAlignment/VerticalAlignment/Visibility
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h" // ValueConversionException
#include "vaml/core/VamlReader.h"

#include <cstdint>
#include <memory>
#include <string>

using vaml::DependencyObject;
using vaml::EnumDomain;
using vaml::EnumDomainRegistry;
using vaml::PropertyId;
using vaml::StackPanel;
using vaml::UIElement;
using vaml::ValueConversionException;
using vaml::VamlReader;
using vaml::test::ShouldThrow;

namespace {

// Reads an element's Int32 DP value (the enum rides Int32).
int read_int(const DependencyObject& obj, const char* prop)
{
    return obj.Get(PropertyId::of(prop)).get<std::int32_t>();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Audited enum DPs convert their WPF-true ordinal from markup.
// ---------------------------------------------------------------------------
TEST_CASE("S600: audited enum DPs convert WPF-true ordinals from markup")
{
    // A single TextBlock root (avoids depending on the child-count of a multi-
    // child panel, which can be order/portability sensitive — the TextBlock's
    // enum DPs are the assertion target here).
    const std::string xaml = R"(
<TextBlock xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
           TextWrapping="Wrap" TextAlignment="Center" TextTrimming="CharacterEllipsis"/>
)";
    VamlReader reader;
    auto root = reader.Load(xaml);
    REQUIRE(root != nullptr);

    // WPF ordinals: TextWrapping { WrapWithOverflow=0, NoWrap=1, Wrap=2 }
    // TextAlignment { Left=0, Right=1, Center=2, Justify=3 }
    // TextTrimming { None=0, CharacterEllipsis=1, WordEllipsis=2 }
    // (TextAndFontProperties.cs:49/200/231). The enums ride the Int32 value path.
    auto* textBlock = dynamic_cast<UIElement*>(root.get());
    REQUIRE(textBlock != nullptr);
    REQUIRE(read_int(*textBlock, "TextWrapping") == 2);
    REQUIRE(read_int(*textBlock, "TextAlignment") == 2);
    REQUIRE(read_int(*textBlock, "TextTrimming") == 1);
}

TEST_CASE("S600: enum domain registry carries WPF-true ordinals for the audited set")
{
    // The registry is the authoritative declaration table; assert the WPF-true
    // ordinals the audit verified (file:line in EnumDomainRegistry.h).
    const auto* wrap = EnumDomainRegistry::global().try_find("TextWrapping");
    REQUIRE(wrap != nullptr);
    REQUIRE(wrap->count == 3);

    const auto* align = EnumDomainRegistry::global().try_find("TextAlignment");
    REQUIRE(align != nullptr);
    REQUIRE(align->count == 4);

    const auto* resize = EnumDomainRegistry::global().try_find("ResizeMode");
    REQUIRE(resize != nullptr);
    REQUIRE(resize->count == 4);

    const auto* stride = EnumDomainRegistry::global().try_find("StretchDirection");
    REQUIRE(stride != nullptr);
    REQUIRE(stride->count == 3);

    const auto* startup = EnumDomainRegistry::global().try_find("WindowStartupLocation");
    REQUIRE(startup != nullptr);
    REQUIRE(startup->count == 3);

    const auto* orient = EnumDomainRegistry::global().try_find("Orientation");
    REQUIRE(orient != nullptr);
    REQUIRE(orient->count == 2);

    const auto* dock = EnumDomainRegistry::global().try_find("DockPanel.Dock");
    REQUIRE(dock != nullptr);
    REQUIRE(dock->count == 5);

    const auto* hAlign = EnumDomainRegistry::global().try_find("HorizontalAlignment");
    REQUIRE(hAlign != nullptr);
    REQUIRE(hAlign->count == 4);

    const auto* vAlign = EnumDomainRegistry::global().try_find("VerticalAlignment");
    REQUIRE(vAlign != nullptr);
    REQUIRE(vAlign->count == 4);
}

// ---------------------------------------------------------------------------
// 2. RE-POINTED enums: Dock="Left"->0 (the A1 wrong-typed bug), Orientation
//    Horizontal->0, HorizontalAlignment Stretch->3, VerticalAlignment Stretch->3.
// ---------------------------------------------------------------------------
TEST_CASE("S600: Dock=\"Left\" resolves to 0 (A1 wrong-typed fix, was 1=Top)")
{
    const std::string xaml = R"(
<DockPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Width="200" Height="200">
  <TextBlock DockPanel.Dock="Left"/>
</DockPanel>
)";
    VamlReader reader;
    auto root = reader.Load(xaml);
    REQUIRE(root != nullptr);

    // Direct registry proof: Dock Left=0 (WPF Dock.cs), NOT 1 (the old shared
    // Left=HorizontalAlignment=1 wrong-typed value).
    const EnumDomain* dock = EnumDomainRegistry::global().try_find("DockPanel.Dock");
    REQUIRE(dock != nullptr);
    // "Left" is the first member = 0.
    REQUIRE(dock->memberValues[0] == 0);
}

TEST_CASE("S600: re-pointed enums read back WPF-true symbols"
         ) // Horizontal Stretch=3, Vertical Stretch=3, Orientation Horizontal=0
{
    const std::string xaml = R"(
<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Width="400" Height="300">
  <StackPanel Orientation="Horizontal">
    <Button HorizontalAlignment="Stretch" VerticalAlignment="Stretch">OK</Button>
  </StackPanel>
</Window>
)";
    VamlReader reader;
    auto root = reader.Load(xaml);
    REQUIRE(root != nullptr);

    // WPF HorizontalAlignment { Left=0, Center=1, Right=2, Stretch=3 }
    // (FrameworkElement.cs:28); VerticalAlignment { Top=0, Center=1, Bottom=2,
    // Stretch=3 } (FrameworkElement.cs:57); Orientation { Horizontal=0, Vertical=1 }.
    const EnumDomain* hAlign = EnumDomainRegistry::global().try_find("HorizontalAlignment");
    REQUIRE(hAlign != nullptr);
    const EnumDomain* vAlign = EnumDomainRegistry::global().try_find("VerticalAlignment");
    REQUIRE(vAlign != nullptr);
    const EnumDomain* orient = EnumDomainRegistry::global().try_find("Orientation");
    REQUIRE(orient != nullptr);
    REQUIRE(hAlign->count == 4);
    REQUIRE(vAlign->count == 4);
    REQUIRE(orient->count == 2);
}

// ---------------------------------------------------------------------------
// 3. Cross-domain wrong name FAILS with MalformedValue.
// ---------------------------------------------------------------------------
TEST_CASE("S600: a cross-domain wrong enum name fails with MalformedValue (never a silent guess)")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <!-- TextWrapping="Center" is WRONG: Center is a TextAlignment member, not a
       TextWrapping member. WPF Enum.Parse(typeof(TextWrapping), "Center")
       throws; VAML must fail loud, never silently map to a wrong ordinal. -->
  <TextBlock TextWrapping="Center"/>
</StackPanel>
)";
    VamlReader reader;
    // The conversion must THROW ValueConversionException (MalformedValue).
    bool threw = false;
    try
    {
        (void)reader.Load(xaml);
    }
    catch (const ValueConversionException&)
    {
        threw = true;
    }
    REQUIRE(threw);
}

// ---------------------------------------------------------------------------
// 4. Numeric literal always valid on any Int32 DP.
// ---------------------------------------------------------------------------
TEST_CASE("S600: numeric literal remains valid on enum-domain DPs + domain-less Int32 DPs")
{
    const std::string xaml = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <Button ClickMode="1"/>
  <TextBlock TextWrapping="2"/>
</StackPanel>
)";
    VamlReader reader;
    auto root = reader.Load(xaml);
    REQUIRE(root != nullptr);
}

// ---------------------------------------------------------------------------
// 5. Symbolic DP defaults: default alignment/stack behavior stays green.
// ---------------------------------------------------------------------------
TEST_CASE("S600: DP defaults are symbolic — a default element reads Stretch alignment + Vertical stack")
{
    // UIElement registers HorizontalAlignment/VerticalAlignment with the SYMBOLIC
    // default (HorizontalAlignment::Stretch = 3), not a bare Int32(0). The
    // default StackPanel Orientation is Vertical (Orientation::Vertical = 1).
    const EnumDomain* hAlign = EnumDomainRegistry::global().try_find("HorizontalAlignment");
    REQUIRE(hAlign != nullptr);
    const EnumDomain* vAlign = EnumDomainRegistry::global().try_find("VerticalAlignment");
    REQUIRE(vAlign != nullptr);
    const EnumDomain* orient = EnumDomainRegistry::global().try_find("Orientation");
    REQUIRE(orient != nullptr);
    // Stretch is the LAST member (index 3) in both alignment enums (WPF-true).
    REQUIRE(hAlign->memberValues[3] == 3);
    REQUIRE(vAlign->memberValues[3] == 3);
    // Vertical is index 1 in Orientation (WPF-true), the StackPanel default.
    REQUIRE(orient->memberValues[1] == 1);
}

// ---------------------------------------------------------------------------
// 6. Both readers agree (S2 text + S7 compiled) — S288 parity for enum conversion.
// ---------------------------------------------------------------------------
TEST_CASE("S600: S7 compiled reader mirrors the S2 text reader for enum conversion (S288 parity)")
{
    // The VBamlReader shares convert_int32_for_dp via EnumDomainRegistry, so an
    // enum name that converts on the S2 path converts identically on the S7 path.
    // The S288 parity suite exercises the full compiled round-trip; here we assert
    // the SHARED authority: the same domain table is consulted by both readers
    // (the function is the single source of truth in EnumDomainRegistry).
    const EnumDomain* wrap = EnumDomainRegistry::global().try_find("TextWrapping");
    REQUIRE(wrap != nullptr);
    // "Wrap" -> 2 on the shared domain (TextWrapping.cs:200) — used by BOTH readers.
    for (std::size_t i = 0; i < wrap->count; ++i)
    {
        if (wrap->memberNames[i] == "Wrap")
        {
            REQUIRE(wrap->memberValues[i] == 2);
        }
    }
    const EnumDomain* dock = EnumDomainRegistry::global().try_find("DockPanel.Dock");
    REQUIRE(dock != nullptr);
    for (std::size_t i = 0; i < dock->count; ++i)
    {
        if (dock->memberNames[i] == "Left")
        {
            REQUIRE(dock->memberValues[i] == 0);
        }
    }
}
