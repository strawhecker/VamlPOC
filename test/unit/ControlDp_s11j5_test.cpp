// 11j-5 (GAP CLOSE, dep:S400/S480) — the SHARED CONTROL BASE DP-BUNDLE.
//
// The VAML property registry is NAME-UNIQUE globally (DependencyProperty::
// Register throws on a duplicate NAME; owner is a declaration label). WPF
// resolves Padding / HorizontalContentAlignment / VerticalContentAlignment /
// FontSize / FontFamily on a Control/TextElement BASE by type-inheritance; the
// faithful VAML mapping is ONE canonical bare registration (owner "Control") +
// the affected controls' typed accessors re-pointed to that single owner. This
// closes the corpus-wide unprefixed-authoring gap across Button/TextBox/
// TextBlock/…, and MIGRATES the prior conflicting registrations (NO backfill):
//   - TextBlock (11k-1; formerly the S087 `Text`) owned bare "FontSize"
//                                        -> removed, re-pointed to Control
//   - Label owned "Label.FontSize" / "Label.FontFamily" /
//            "Label.HorizontalContentAlignment" -> removed, re-pointed
//   - Border owned "Border.Padding"     -> removed, re-pointed
//
// Headless (Rail 1).

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"     // Button / TextBox / TextBlock / Border / Label
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include "doctest/doctest.h"

#include <string>

using namespace vaml;

namespace {

void expectInt32(DependencyObject& o, const char* name, std::int32_t want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name);
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name), Value::Int32(want));
    CHECK(o.Get(PropertyId::of(name)).get<std::int32_t>() == want);
}

void expectDouble(DependencyObject& o, const char* name, double want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name);
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name), Value::Double(want));
    CHECK(o.Get(PropertyId::of(name)).get<double>() == want);
}

void expectString(DependencyObject& o, const char* name, const std::string& want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name);
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name), Value::String(want));
    CHECK(o.Get(PropertyId::of(name)).get<std::string>() == want);
}

} // namespace

TEST_CASE("11j-5: the shared Control base names resolve UNPREFIXED on a plain DependencyObject (S043 attached)")
{
    // The whole point: these names (no "Label."/"Border." prefix) resolve on ANY
    // element via the S043 attached-aware read, because they are registered ONCE
    // under the single canonical "Control" owner.
    TextBlock text0; // ensure the bundle is registered (Text ctor depends on it)
    DependencyObject o;

    const DependencyProperty* pPadding = DependencyProperty::TryGet("Padding");
    REQUIRE(pPadding != nullptr);
    CHECK(pPadding->metadata().boundType() == ValueType::Thickness);
    (void)o.Set(PropertyId::of("Padding"), Value::Thickness(vaml::Thickness{1, 2, 3, 4}));
    CHECK(o.Get(PropertyId::of("Padding")).get<vaml::Thickness>() == vaml::Thickness{1, 2, 3, 4});

    expectInt32(o, "HorizontalContentAlignment", 2); // Center
    expectInt32(o, "VerticalContentAlignment", 1);   // Top
    expectDouble(o, "FontSize", 22.0);
    expectString(o, "FontFamily", "Segoe UI");
}

TEST_CASE("11j-5: each shared name is registered EXACTLY ONCE (no duplicate/throw at startup)")
{
    // A duplicate Register would throw PropertyAlreadyRegistered when the bundle
    // + any control ctor run. Constructing each control must be a no-throw.
    CHECK_NOTHROW(Button());
    CHECK_NOTHROW(TextBox());
    CHECK_NOTHROW(Border());
    CHECK_NOTHROW(Label());
}

TEST_CASE("11j-5: migrated accessors still round-trip (Label/TextBlock/Border re-proof)")
{
    Label label;
    (void)label.set_font_size(18.0);
    CHECK(label.font_size() == 18.0);
    (void)label.set_font_family("Consolas");
    CHECK(label.font_family() == "Consolas");
    (void)label.set_horizontal_content_alignment(2);
    CHECK(label.horizontal_content_alignment() == 2);

    TextBlock text;
    (void)text.set_font_size(24.0);
    CHECK(text.font_size() == 24.0);

    Border border;
    (void)border.set_padding(vaml::Thickness{5, 6, 7, 8});
    CHECK(border.padding() == vaml::Thickness{5, 6, 7, 8});
}

TEST_CASE("11j-5: the control DEFAULT for FontSize/Padding/HAlignment is the Control bundle's (16/0/Left-Top)")
{
    // WPF-faithful defaults: FontSize 16 em; Padding 0; HorizontalContentAlignment
    // Left (0); VerticalContentAlignment Top (0).
    TextBlock text; // also forces the bundle to exist
    DependencyObject o;
    CHECK(o.Get(PropertyId::of("FontSize")).get<double>() == 16.0);
    CHECK(o.Get(PropertyId::of("Padding")).get<vaml::Thickness>() == vaml::Thickness{});
    CHECK(o.Get(PropertyId::of("HorizontalContentAlignment")).get<std::int32_t>() == 0);
    CHECK(o.Get(PropertyId::of("VerticalContentAlignment")).get<std::int32_t>() == 0);
}
