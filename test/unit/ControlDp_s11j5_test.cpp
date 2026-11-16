// 11j-5 (GAP CLOSE, dep:S400/S480) — the SHARED CONTROL BASE DP-BUNDLE.
//
// 11s SUPERSEDE NOTE (S167_1 corrective-token): the 11j-5 migration (below)
// that forced the shared Control-bundle to be the SINGLE owner for Nameonly-
// unique FontSize/Padding/content-align names is **SUPERSEDED BY 11s**. The
// composite (name, owner) key houses SAME-NAME/different-OWNER DPs, so the
// controls whose real WPF owner is a DIFFERENT type than Control now own their
// DPs again: TextBlock.FontSize (TextBlock.cs:776 AddOwner) and Border.Padding
// (Border.cs:142 Register) coexist with Control.FontSize / Control.Padding.
// The 11j-5 bundle remains for the genuine Control base (Control.cs:157/182/365
// — Control DOES own FontSize/Padding), which Label (ContentControl-derived,
// Label.cs:30, no own Font* DPs in WPF) correctly inherits. This file now
// ASSERTs the coexistence (the drift's forced disambiguation is abolished).
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

// 11j-5 — S803 re-key: the shared Control-base DPs are registered ONCE under the
// composite owner "Control". Composite identity (name, owner) is the product
// contract, so these "unprefixed" names are now expressed with their declaring
// owner "Control" explicitly (there is no bare-name global lookup; a bare id on
// a plain DependencyObject would resolve to nothing). ResolveForElement walks
// the element owner-chain when the concrete element type is known.
void expectInt32(DependencyObject& o, const char* name, std::int32_t want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name, "Control");
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name, "Control"), Value::Int32(want));
    CHECK(o.Get(PropertyId::of(name, "Control")).get<std::int32_t>() == want);
}

void expectDouble(DependencyObject& o, const char* name, double want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name, "Control");
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name, "Control"), Value::Double(want));
    CHECK(o.Get(PropertyId::of(name, "Control")).get<double>() == want);
}

void expectString(DependencyObject& o, const char* name, const std::string& want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name, "Control");
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name, "Control"), Value::String(want));
    CHECK(o.Get(PropertyId::of(name, "Control")).get<std::string>() == want);
}

} // namespace

TEST_CASE("11j-5: the shared Control base names resolve UNPREFIXED COMPOSITE on a plain DependencyObject (S043 attached, S803)")
{
    // S803: the Control-base DPs are registered ONCE under composite owner
    // "Control". They resolve on ANY element via the S043-owner-chain
    // attached-aware read (ResolveForElement walks each element's owner chain to
    // owner "Control"). On a bare DependencyObject the calling form is the
    // composite id with the declaring owner.
    TextBlock text0; // ensure the bundle is registered (Text ctor depends on it)
    DependencyObject o;

    const DependencyProperty* pPadding = DependencyProperty::TryGet("Padding", "Control");
    REQUIRE(pPadding != nullptr);
    CHECK(pPadding->metadata().boundType() == ValueType::Thickness);
    (void)o.Set(PropertyId::of("Padding", "Control"), Value::Thickness(vaml::Thickness{1, 2, 3, 4}));
    CHECK(o.Get(PropertyId::of("Padding", "Control")).get<vaml::Thickness>() == vaml::Thickness{1, 2, 3, 4});

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

TEST_CASE("11j-5: the control DEFAULT for FontSize/Padding/HAlignment is the Control bundle's (12/0/Left-Top)")
{
    // WPF-faithful defaults: FontSize 12.0 (SystemFonts.ThemeMessageFontSize at
    // 96 DPI — the standard message-font size, not 16); Padding 0;
    // HorizontalContentAlignment Left (0); VerticalContentAlignment Top (0).
    TextBlock text; // also forces the bundle to exist
    DependencyObject o;
    CHECK(o.Get(PropertyId::of("FontSize", "Control")).get<double>() == 12.0);
    CHECK(o.Get(PropertyId::of("Padding", "Control")).get<vaml::Thickness>() == vaml::Thickness{});
    CHECK(o.Get(PropertyId::of("HorizontalContentAlignment", "Control")).get<std::int32_t>() == 0);
    CHECK(o.Get(PropertyId::of("VerticalContentAlignment", "Control")).get<std::int32_t>() == 0);
}

TEST_CASE("11j-5/11s: the Control bundle COEXISTS with the per-owner re-additions")
{
    // 11s — the drift's forced disambiguation is abolished: TextBlock.FontSize
    // and Border.Padding are each under their real WPF owner, distinct from the
    // Control bundle. Both resolve; the per-object stores are independent.
    TextBlock text;
    Border border;

    // TextBlock owns ITS FontSize (TextBlock.cs:776) — distinct from Control's.
    CHECK(DependencyProperty::TryGet("FontSize", "TextBlock") != nullptr);
    CHECK(DependencyProperty::TryGet("FontSize", "Control") != nullptr);
    CHECK(DependencyProperty::TryGet("FontSize", "TextBlock") !=
          DependencyProperty::TryGet("FontSize", "Control"));
    // The TextBlock accessor writes+reads its OWN owner (independent of Control).
    (void)text.set_font_size(33.0);
    CHECK(text.font_size() == 33.0);
    // A plain DependencyObject still reads the Control-bundle default (12.0) —
    // the two owners are physically separate slots.
    DependencyObject o;
    CHECK(o.Get(PropertyId::of("FontSize", "Control")).get<double>() == 12.0);

    // Border owns ITS Padding (Border.cs:142) — distinct from Control's.
    CHECK(DependencyProperty::TryGet("Padding", "Border") != nullptr);
    CHECK(DependencyProperty::TryGet("Padding", "Control") != nullptr);
    CHECK(DependencyProperty::TryGet("Padding", "Border") !=
          DependencyProperty::TryGet("Padding", "Control"));
    (void)border.set_padding(vaml::Thickness{2, 3, 4, 5});
    CHECK(border.padding() == vaml::Thickness{2, 3, 4, 5});
    // Control's own Padding default (0) is independent of Border's.
    CHECK(o.Get(PropertyId::of("Padding", "Control")).get<vaml::Thickness>() == vaml::Thickness{});

    // Label (ContentControl-derived, NO own Font* DPs in WPF) still inherits
    // the Control bundle — the WPF-faithful read (Label.cs:30 : ContentControl
    // with no FontSizeProperty). Its accessor writes Control.FontSize.
    Label label;
    (void)label.set_font_size(20.5);
    CHECK(label.font_size() == 20.5);
    // Each DependencyObject carries its OWN per-(name,owner) value: an unrelated
    // DependencyObject reads the Control FontSize DEFAULT (12.0), unaffected by
    // Label's write; and TextBlock's FontSize is a DIFFERENT owner (33.0 above)
    // — the coexistence proof: two controls, two FontSize owners, no cross-talk.
    CHECK(o.Get(PropertyId::of("FontSize", "Control")).get<double>() == 12.0);
    CHECK(text.font_size() == 33.0);   // TextBlock's own owner, unaffected
}
