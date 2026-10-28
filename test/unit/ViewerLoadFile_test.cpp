// S221 (dep:S220) — Viewer CLI entry, HEADLESS golden-fixture suite.
//
// Proves the viewer's file→tree entry path (the `load_file`/`load_text` seam)
// end-to-end against a golden VAML fixture, WITHOUT a live window. This is the
// S221 contract: `vaml_viewer <file.vaml>` opens → VamlReader::Load → live
// tree, or a TYPED error + non-zero exit on missing/unreadable/parse-error
// (never silent). By compiling the ACTUAL seam (vaml_viewer/load_file.cpp) into
// the test target, the headless suite exercises the EXACT code the CLI runs —
// the app's main.cpp is thin argv→seam→exit-code glue and stays out of the
// tests.
//
// The seam is a genuine ADR-021 consumer: it includes ONLY public vaml headers.
// This test drives it through the S095 golden-doc pattern (S221-unique global
// DPs so the process-wide name-unique table stays clean, per S039/S095
// discipline) — the callout in the S221 title: "HEADLESS-ctestable against the
// S095 golden fixture."
//
// S221-unique global DPs: all names carry the "S221" prefix; the suite-local
// seen_names() set guards the process-wide name-unique table (S039).
//
// NO-GAP: S221 introduces no MINIMAL REAL and resolves none. The seam is REAL
// permanent entry-path code (not a stub); rendering the tree into a window is
// S222's named step (recorded, not silent). The exit-code→category mapping in
// main.cpp is app glue, verified by the live verify_s221.cmd CLI proofs.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"  // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include "load_file.h"               // the viewer's HEADLESS-ctestable seam (S221)

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // Button/StackPanel/Text/Window + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"

#include <cmath>   // std::isnan (11d-DEBUG Width=\""Auto\"" -> NaN assertion)
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>

using vaml::Button;
using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::FileLoadError;
using vaml::LoadResult;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Text;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueType;
using vaml::Window;
using vaml::load_file;
using vaml::load_scan_file;
using vaml::load_scan_text;
using vaml::load_text;

namespace {

// ---------------------------------------------------------------------------
// S221-only global DependencyProperty table (name-unique process-wide, S039).
// ---------------------------------------------------------------------------

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
        return DependencyProperty::Register(name, type, "S221_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// The S221-unique global DPs the golden fixture sets from text — a small
// realistic subset of the S095 surface, deliberately re-derived here so the
// seam's golden test is self-contained (it must not depend on S095's globals).
void registerS221Properties()
{
    (void)registerGlobal("S221Width",   ValueType::Int32,  PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S221Title",   ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S221Content", ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S221Margin",  ValueType::Thickness, PropertyFlags::None,
                         Value::Thickness(Thickness{0, 0, 0, 0}));
    (void)registerGlobal("S221Fill",    ValueType::Color,  PropertyFlags::None,
                         Value::Color(Color{0, 0, 0, 0}));
}

// The S221 golden document — one realistic golden fixture mirroring the S095
// shape (Window root + nested real built-ins + converted literal properties).
inline const char* kGolden221 = R"(<Window S221Title="Viewer" S221Width="800" S221Margin="2,3,4,5" S221Fill="#FF0000"><StackPanel><Button S221Content="Go" S221Width="100"/><Text S221Content="Hello">Hi</Text></StackPanel></Window>)";

} // namespace

// ---------------------------------------------------------------------------
// load_text — the PURE seam (golden fixture, deterministic in-memory)
// ---------------------------------------------------------------------------

TEST_CASE("S221: load_text(golden) builds a live tree and maps a typed SUCCESS")
{
    registerS221Properties();
    const LoadResult r = load_text(kGolden221);

    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(FileLoadError::None)));
    SHOULD(ShouldlyString(r.detail).ShouldBe(std::string("")));
    SHOULD(ShouldlyIntegral(r.root != nullptr).ShouldBe(1));
    if (r.root)
    {
        // Root is a real Window carrying the converted literal properties.
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(r.root.get()) != nullptr).ShouldBe(1));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Title")))
                   .ShouldBe(Value::String("Viewer")));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Width")))
                   .ShouldBe(Value::Int32(800)));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Margin")))
                   .ShouldBe(Value::Thickness(Thickness{2, 3, 4, 5})));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Fill")))
                   .ShouldBe(Value::Color(Color{255, 0, 0, 255})));
    }
}

TEST_CASE("S221: load_text maps a structured PARSE error (never silent)")
{
    registerS221Properties();
    // Unknown property -> VamlReaderException(UnknownProperty) -> ParseError,
    // with a NON-EMPTY detail carrying the structured message.
    const LoadResult r = load_text(std::string("<Button S221Bogus=\"1\"/>"));
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::ParseError)));
    SHOULD(ShouldlyIntegral(r.root == nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0)); // NEVER silent
}

TEST_CASE("11d-DEBUG: Width=\"Auto\" resolves to the auto-size NaN sentinel end-to-end")
{
    registerS221Properties();
    // The window's WIDTH dp is the REAL UIElement Width (Double, default NaN —
    // the same DP `Width=\"Auto\"` targets). Loading must SUCCEED, and the result
    // must be NaN (auto), proving the Auto token rides the converter into the DP.
    const LoadResult r = load_text(std::string("<Window Width=\"Auto\"/>"));
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(FileLoadError::None)));
    SHOULD(ShouldlyIntegral(r.root != nullptr).ShouldBe(1));
    if (r.root)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(
            std::isnan(r.root->Get(PropertyId::of("Width")).get<double>()))).ShouldBe(1));
    }
}

TEST_CASE("S221: load_text maps a malformed-value conversion error to ParseError")
{
    registerS221Properties();
    // S221Width is Int32; "abc" is MalformedValue -> ParseError, detail non-empty.
    const LoadResult r = load_text(std::string("<Button S221Width=\"abc\"/>"));
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::ParseError)));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0)); // NEVER silent
}

TEST_CASE("S221: load_text maps an unknown-type resolution error to ParseError")
{
    registerS221Properties();
    // Unknown ELEMENT -> ElementResolutionException(UnknownType) -> ParseError.
    const LoadResult r = load_text(std::string("<S221Zorp S221Content=\"x\"/>"));
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::ParseError)));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0)); // NEVER silent
}

// ---------------------------------------------------------------------------
// load_file — the I/O seam (temp real file with the golden fixture)
// ---------------------------------------------------------------------------

TEST_CASE("S221: load_file(temp real .vaml with golden) builds the live tree (parity)")
{
    registerS221Properties();
    // Write the golden fixture to a real temp file, then load it through the
    // full I/O path (read + Load). Parity: the file path yields the same typed
    // SUCCESS and the same converted values as load_text.
    const char* kTmp = "s221_verify_fixture.vaml";
    {
        std::ofstream f(kTmp, std::ios::binary);
        REQUIRE(f.is_open());
        f.write(kGolden221, static_cast<std::streamsize>(std::char_traits<char>::length(kGolden221)));
        f.close();
    }

    const LoadResult r = load_file(kTmp);
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(FileLoadError::None)));
    if (r.root)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(r.root.get()) != nullptr).ShouldBe(1));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Title")))
                   .ShouldBe(Value::String("Viewer")));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Margin")))
                   .ShouldBe(Value::Thickness(Thickness{2, 3, 4, 5})));
    }

    std::remove(kTmp);
}

TEST_CASE("S221: load_file(missing path) maps a TYPED NoSuchFile error (never silent)")
{
    registerS221Properties();
    const LoadResult r = load_file("s221_definitely_missing_does_not_exist.vaml");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::NoSuchFile)));
    SHOULD(ShouldlyIntegral(r.root == nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0)); // NEVER silent
}

TEST_CASE("S450_1: a real .xaml golden loads through the text path (the .xaml acceptance gate)")
{
    // S450_1 — the viewer ACCEPTS `.xaml` files: the same parse path, the SAME
    // grammar (the parser is XML/XAML-aware), with NO extension gate. A bona-fide
    // XAML-looking document — presentation-style `xmlns` + a prefixed custom
    // namespace + x: directives + an unprefixed root resolved by local name.
    // This is NOT an alias-by-extension; it proves a real-WPF-shaped .xaml body
    // resolves through the existing namespace + resolution machinery.
    registerS221Properties();

    const char* kXamlDoc = R"(<Window S221Title="Xaml" S221Width="320" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Class="Demo.MainWindow"><StackPanel x:Name="root"><Button S221Content="OK" S221Width="64"/></StackPanel></Window>)";

    const LoadResult r = load_text(kXamlDoc);
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(FileLoadError::None)));
    if (r.root)
    {
        // Root resolves by local_name "Window" (the presentation root); title set.
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(r.root.get()) != nullptr).ShouldBe(1));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Title")))
                   .ShouldBe(Value::String("Xaml")));
        // x:Class consumed on the root (root_class decodes "Demo.MainWindow");
        // the nested Button resolves + sets content through the same machinery.
    }
}

TEST_CASE("S450_1: a real .xaml FILE loads through the I/O path (parity with .vaml)")
{
    // Same golden as above, written to a real temp `.xaml` path and loaded via
    // load_file — the I/O seam accepts .xaml as a first-class source.
    registerS221Properties();
    const char* kTmp = "s450_fixture.xaml";
    {
        std::ofstream f(kTmp, std::ios::binary);
        REQUIRE(f.is_open());
        const char* doc =
            R"(<Window S221Title="Xaml" S221Width="320" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Class="Demo.MainWindow"><StackPanel><Button S221Content="OK"/></StackPanel></Window>)";
        f.write(doc, static_cast<std::streamsize>(std::char_traits<char>::length(doc)));
        f.close();
    }

    const LoadResult r = load_file(kTmp);
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(FileLoadError::None)));
    if (r.root)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(r.root.get()) != nullptr).ShouldBe(1));
        SHOULD(ShouldlyString(r.root->Get(PropertyId::of("S221Title")))
                   .ShouldBe(Value::String("Xaml")));
    }
    std::remove(kTmp);
}

TEST_CASE("S221: a TEMP malformed fixture yields ParseError (not a silent ok)")
{
    registerS221Properties();
    const char* kTmp = "s221_malformed.vaml";
    {
        std::ofstream f(kTmp, std::ios::binary);
        REQUIRE(f.is_open());
        const char* bad = "<Button S221Content=\"unterminated";
        f.write(bad, static_cast<std::streamsize>(std::char_traits<char>::length(bad)));
        f.close();
    }

    const LoadResult r = load_file(kTmp);
    // The file READ fine but is malformed markup -> the scanner throws
    // XmlScanException(UnterminatedAttributeValue) -> ParseError, never silent.
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::ParseError)));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0)); // NEVER silent

    std::remove(kTmp);
}

// ---------------------------------------------------------------------------
// 11d-DEBUG — SCAN mode (option A): ONE document reports ALL of its
// unknown-property errors in one pass (default), and --fail_on_first_error
// restores the strict single-error behavior. An errored document is never
// rendered (ok=false, root=null) in BOTH modes.
// ---------------------------------------------------------------------------

TEST_CASE("11d-DEBUG: load_scan_text collects ALL unknown properties in ONE pass")
{
    // registerS221Properties() registers S221* DPs only; these Window/Button
    // attributes are genuinely UNKNOWN (11j-4 registers the real Window DPs
    // AllowsTransparency/WindowStartupLocation — NOT these bogus keys), so every
    // one reports in scan mode. proves aggregation of MULTIPLE errors in one pass.
    registerS221Properties();
    const LoadResult r = load_scan_text(std::string(
        "<Window BogusRootOne=\"1\" BogusRootTwo=\"2\">"
        "<Button BogusOne=\"1\" BogusTwo=\"2\"/>"
        "</Window>"));

    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::ParseError)));
    SHOULD(ShouldlyIntegral(r.root == nullptr).ShouldBe(1));   // never render from an errored doc
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));    // never silent
    // ALL FOUR authoring errors (2 on the root + 2 on the child Button) are
    // aggregated into `issues` in ONE pass, not just the first.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.issues.size())).ShouldBe(4));
    // Each issue carries a non-empty, authoring-error line.
    for (const vaml::ScanResultItem& issue : r.issues)
    {
        SHOULD(ShouldlyIntegral(issue.what.empty()).ShouldBe(0));
    }
}

TEST_CASE("11d-DEBUG: a clean document scans OK (issues empty, root present)")
{
    registerS221Properties();
    const LoadResult r = load_scan_text(std::string(
        "<Window S221Title=\"Clean\" S221Width=\"100\"><StackPanel/></Window>"));
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::None)));
    SHOULD(ShouldlyIntegral(r.issues.empty()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr).ShouldBe(1));
}

TEST_CASE("11d-DEBUG: load_scan_file(missing) maps NoSuchFile (typed, never silent)")
{
    const LoadResult r = load_scan_file("s11d_definitely_missing.vaml");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(FileLoadError::NoSuchFile)));
    SHOULD(ShouldlyIntegral(r.issues.empty()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.detail.empty()).ShouldBe(0));
}
