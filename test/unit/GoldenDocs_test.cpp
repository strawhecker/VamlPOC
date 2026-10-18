// S095 (dep:S094) — Parser + reader FULL TEST SUITE over GOLDEN VAML documents.
//
// The Sprint-2 capstone: prove the ENTIRE S2 pipe on realistic golden documents
// — `text → parse_vaml → ParsedElement → VamlReader::Load → live tree` — for
// every feature surface authored since S080 at once. The formal full-suite gate
// over the individual per-feature tests (S086 parser round-trip, S091 reader
// live tree, S092 strands, S093 no-hooks integration, S094 error taxonomy).
//
// TEST-ONLY — zero new product symbols (the S093/S091/S086 test-gate precedent).
// Three golden docs + focused variants, each asserted through the WHOLE pipe:
//
//   GOLDEN A (full-feature app view) — ONE realistic doc carrying together:
//     xmlns:x + a prefixed custom-ns element (<vm:Button>), x:Class + x:Name on
//     root + children, attached-style dotted props (S095Dock.Dock / S095Grid.Row
//     / S095Grid.Column), numeric (S095Width="1024"), quoted (S095Title), comma-
//     list (S095Margin="10,10,10,10"), markup extension ({Binding ...}), an
//     event-shaped key (S095Click="OnClick") routed via register_event,
//     x:Key (deferred), self-closing leaves, a text-content leaf, multi-level
//     nesting. Asserted on FOUR axes:
//       Axis 1 PARSER GOLDEN — canonicalize(parse_vaml(doc)) == hand-written
//         expected string (reuses the S086 canonicalize() test-local contract).
//       Axis 2 READER GOLDEN — root type, retained count + order, parent() chain,
//         named()/names()/name_count(), root_class(), and every literal-converted
//         property value read back (Value::Int32/Bool/Color/Thickness/String +
//         attached).
//       Axis 3 DEFERRED — deferred_extensions() captures {Binding ...} verbatim
//         (doc order, target/key/source), x:Key stays deferred (never set, object
//         still built); with register_event("S095Click"), deferred_event_handlers()
//         captures (target, "S095Click", "OnClick").
//       Axis 4 LIVE PARITY — Load(text) and Load(parse_vaml(doc)) produce
//         identical live trees (root type, retained count, values) — the S091
//         parity proof re-run on a golden doc.
//
//   GOLDEN B (error taxonomy on real docs) — 5 defect docs each firing the RIGHT
//     structured exception (never a message catch-all): unknown element →
//     ElementResolutionException(UnknownType); unknown attribute → VamlReader
//     Exception(UnknownProperty); malformed value → ValueConversionException
//     (MalformedValue); duplicate x:Name → VamlReaderException(DuplicateName);
//     x:Class on non-root → VamlReaderException(ClassOnNonRoot). Re-proves the
//     S091/S094 arms through one golden-doc path — no new product code (S048).
//
//   FOCUSED — a clean-minimal control doc proving a no-hooks / no-extensions
//     golden loads with deferred_extensions().empty() &&
//     deferred_event_handlers().empty() (the S093 "no latent wiring" headline
//     re-confirmed at the suite level).
//
// S095-unique global DPs (S039 discipline): all names carry the "S095" prefix;
// the suite-local seen_names() set guards the process-wide name-unique table.
//
// NO-GAP accounting: S095 introduces no MINIMAL REAL and resolves none. Every
// feature the goldens touch is already scheduled at its own sprint (x:Key→S361,
// x:Null→S084, {Binding}→S252, event wiring→S322). Nothing deferred to a silent
// backfill; no non-buildable window.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Color/Thickness/Value/ValueType
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // Button/StackPanel/Text/Grid/Window + register_builtin_elements
#include "vaml/core/ElementResolver.h"    // ElementResolutionException
#include "vaml/core/ParsedElement.h"      // ParsedElement + parse_vaml
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/VamlReader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::Button;
using vaml::Color;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::ElementResolutionError;
using vaml::ElementResolutionException;
using vaml::Grid;
using vaml::ParsedAttribute;
using vaml::ParsedElement;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Text;
using vaml::Thickness;
using vaml::Value;
using vaml::ValueConversionError;
using vaml::ValueConversionException;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::Window;
using vaml::parse_vaml;
using vaml::test::ShouldThrow;

namespace {

// ---------------------------------------------------------------------------
// S095-only global DependencyProperty table (name-unique process-wide, S039).
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
        return DependencyProperty::Register(name, type, "S095_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// The S095-unique global DPs the S095 golden documents set from text. This is
// the SURFACE the goldens prove end-to-end — a realistic, WPF-faithful set.
void registerS095Properties()
{
    (void)registerGlobal("S095Width",      ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S095Height",     ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S095Title",      ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S095Content",    ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S095Text",       ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S095IsEnabled",  ValueType::Bool, PropertyFlags::None, Value::Bool(true));
    (void)registerGlobal("S095Margin",     ValueType::Thickness, PropertyFlags::None,
                         Value::Thickness(Thickness{0, 0, 0, 0}));
    (void)registerGlobal("S095Fill",       ValueType::Color, PropertyFlags::None,
                         Value::Color(Color{0, 0, 0, 0}));
    // ATTACHED (owner "S095_Owner"; value lands on whatever object Set is called
    // on — S043 attached-aware global resolution by name).
    (void)registerGlobal("S095Dock.Dock",  ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S095Grid.Row",   ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerGlobal("S095Grid.Column",ValueType::Int32, PropertyFlags::None, Value::Int32(0));
}

// ---------------------------------------------------------------------------
// GOLDEN DOC A — the S2 full-feature app view. Every element is a registered
// built-in (Window/StackPanel/Grid/Text/Button — NOT DockPanel, which has no
// seed); every plain attribute is an S095-registered global DP; every
// directive/extension/event is one the reader defers or routes.
// ---------------------------------------------------------------------------

// The x-language namespace declaration (full xmlns:x="..." attribute value).
inline const char* kXamlNs = "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"";

// Elements flush (no inter-element whitespace) so the golden canonical string
// is exact and non-ambiguous (the S086 round-trip discipline).
const char* kGoldenDocA = R"(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" xmlns:vm="urn:vm:app" x:Class="App.MainView" x:Name="root" S095Width="1024" S095Title="Contact Manager" S095Margin="10,10,10,10"><StackPanel S095IsEnabled="true"><vm:Button x:Name="search" S095Content="Search" S095Width="200" S095Height="40" S095Dock.Dock="Top" S095Grid.Row="1" S095Grid.Column="0" S095Click="OnClick" S095Command="{Binding SearchCommand}" S095Fill="#FF0000"/><Text x:Name="label" x:Key="searchLabelKey" S095Text="{Binding SearchText}">Search:</Text><Grid><Button S095Content="Nested" S095Grid.Row="2"/></Grid></StackPanel></Window>)";

// The expected canonicalize(parse(kGoldenDocA)) — the parser GOLDEN. Reproduces
// the authored structure exactly, modulo the xmlns declarations (they are
// namespace DECLARATIONS, never ordinary attributes — S082 — so the faithful
// re-serializer omits them).
const char* kExpectedCanonicalA = R"(<Window x:Class="App.MainView" x:Name="root" S095Width="1024" S095Title="Contact Manager" S095Margin="10,10,10,10"><StackPanel S095IsEnabled="true"><vm:Button x:Name="search" S095Content="Search" S095Width="200" S095Height="40" S095Dock.Dock="Top" S095Grid.Row="1" S095Grid.Column="0" S095Click="OnClick" S095Command="{Binding SearchCommand}" S095Fill="#FF0000"/><Text x:Name="label" x:Key="searchLabelKey" S095Text="{Binding SearchText}">Search:</Text><Grid><Button S095Content="Nested" S095Grid.Row="2"/></Grid></StackPanel></Window>)";

// Test-local re-serializer (S086 contract): attrs in document order, self-closing
// preserved, children then content, no xml namespaces. NOT product code.
std::string canonicalize(const ParsedElement& el)
{
    std::string out = "<" + std::string(el.tagName);
    for (const ParsedAttribute& a : el.attributes)
    {
        out += " " + std::string(a.key) + "=\"" + std::string(a.value) + "\"";
    }
    if (el.self_closing)
    {
        out += "/>";
        return out;
    }
    out += ">";
    for (const ParsedElement& c : el.children)
    {
        out += canonicalize(c);
    }
    out += std::string(el.content);
    out += "</" + std::string(el.tagName) + ">";
    return out;
}

// Convenience: find the attribute with `key` inside `el`, or null when absent.
const ParsedAttribute* findAttr(const ParsedElement& el, const std::string& key)
{
    for (const ParsedAttribute& a : el.attributes)
    {
        if (a.key == key)
        {
            return &a;
        }
    }
    return nullptr;
}

template <typename Concrete>
void assertConcrete(const std::unique_ptr<DependencyObject>& p)
{
    SHOULD(ShouldlyIntegral(p != nullptr).ShouldBe(true));
    if (p)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Concrete*>(p.get()) != nullptr).ShouldBe(true));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// GOLDEN A — AXIS 1: PARSER GOLDEN (text → tree → structure reproduces doc)
// ---------------------------------------------------------------------------

TEST_CASE("S095: golden A parser axis — canonicalize(parse_vaml(doc)) is exact")
{
    const ParsedElement root = parse_vaml(kGoldenDocA);
    SHOULD(ShouldlyString(canonicalize(root)).ShouldBe(std::string(kExpectedCanonicalA)));
}

TEST_CASE("S095: golden A parser axis — the prefixed element + directives are carried")
{
    const ParsedElement root = parse_vaml(kGoldenDocA);

    // Root Window: no xmlns attrs (declarations), x:Class + x:Name + 3 literals.
    SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("Window")));
    SHOULD(ShouldlyIntegral(findAttr(root, "xmlns:x") != nullptr).ShouldBe(false));
    SHOULD(ShouldlyIntegral(findAttr(root, "xmlns:vm") != nullptr).ShouldBe(false));
    SHOULD(ShouldlyString(root.attributes.size()).ShouldBe(static_cast<std::size_t>(5)));

    // The prefixed custom-ns element keeps its full "vm:Button" tagName (S082).
    const ParsedElement& sp = root.children[0];
    SHOULD(ShouldlyString(sp.tagName).ShouldBe(std::string("StackPanel")));
    const ParsedElement& vmBtn = sp.children[0];
    SHOULD(ShouldlyString(vmBtn.tagName).ShouldBe(std::string("vm:Button")));
    SHOULD(ShouldlyString(vmBtn.prefix).ShouldBe(std::string("vm")));
    SHOULD(ShouldlyString(vmBtn.local_name).ShouldBe(std::string("Button")));

    // The text-content leaf carries its authored content; x:Key kept as directive.
    const ParsedElement& text = sp.children[1];
    SHOULD(ShouldlyString(text.content).ShouldBe(std::string("Search:")));
    SHOULD(ShouldlyIntegral(text.self_closing).ShouldBe(0));
    SHOULD(ShouldlyIntegral(findAttr(text, "x:Key") != nullptr).ShouldBe(true));

    // Markup-extension values tagged on the parsed attributes (S084/S085).
    const ParsedAttribute* cmd = findAttr(vmBtn, "S095Command");
    SHOULD(ShouldlyIntegral(cmd != nullptr).ShouldBe(true));
    if (cmd)
    {
        SHOULD(ShouldlyIntegral(cmd->isExtension).ShouldBe(1));
        SHOULD(ShouldlyString(cmd->value).ShouldBe(std::string("{Binding SearchCommand}")));
    }
}

// ---------------------------------------------------------------------------
// GOLDEN A — AXIS 2: READER GOLDEN (live tree + converted values + names)
// ---------------------------------------------------------------------------

TEST_CASE("S095: golden A reader axis — root type, retained order, parent chain")
{
    registerS095Properties();
    VamlReader reader;
    reader.register_event("S095Click"); // golden A carries an event-shaped key
    std::unique_ptr<DependencyObject> root = reader.Load(kGoldenDocA);

    assertConcrete<Window>(root);
    const auto& retained = reader.retained_objects();
    // Post-order depth-first retained order: [0]=search Button (vm:Button),
    // [1]=Text, [2]=nested Button (inside Grid), [3]=Grid, [4]=StackPanel.
    SHOULD(ShouldlyString(retained.size()).ShouldBe(5u));

    if (retained.size() == 5)
    {
        assertConcrete<Button>(retained[0]);
        assertConcrete<Text>(retained[1]);
        assertConcrete<Button>(retained[2]);
        assertConcrete<Grid>(retained[3]);
        assertConcrete<StackPanel>(retained[4]);

        // Parent chain: nested Button → Grid; search Button/Text/Grid → StackPanel;
        // StackPanel → root Window.
        SHOULD(ShouldlyIntegral(retained[0]->parent() == retained[4].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(retained[1]->parent() == retained[4].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(retained[2]->parent() == retained[3].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(retained[3]->parent() == retained[4].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(retained[4]->parent() == root.get()).ShouldBe(true));
    }
}

TEST_CASE("S095: golden A reader axis — every literal property is converted + set")
{
    registerS095Properties();
    VamlReader reader;
    reader.register_event("S095Click"); // golden A carries an event-shaped key
    std::unique_ptr<DependencyObject> root = reader.Load(kGoldenDocA);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(5u));
    if (retained.size() < 5)
    {
        return; // guard (should not happen); keeps the case null-safe on failure
    }

    // Root Window: numeric Int32, quoted String, comma-list Thickness.
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S095Width")))
               .ShouldBe(Value::Int32(1024)));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S095Title")))
               .ShouldBe(Value::String("Contact Manager")));
    SHOULD(ShouldlyString(root->Get(PropertyId::of("S095Margin")))
               .ShouldBe(Value::Thickness(Thickness{10, 10, 10, 10})));

    // StackPanel: Bool.
    SHOULD(ShouldlyString(retained[4]->Get(PropertyId::of("S095IsEnabled")))
               .ShouldBe(Value::Bool(true)));

    // search Button (retained[0]): String/Int32 + attached + Color.
    const auto& btn = retained[0];
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Content")))
               .ShouldBe(Value::String("Search")));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Width")))
               .ShouldBe(Value::Int32(200)));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Height")))
               .ShouldBe(Value::Int32(40)));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Dock.Dock")))
               .ShouldBe(Value::String("Top")));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Grid.Row")))
               .ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Grid.Column")))
               .ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(btn->Get(PropertyId::of("S095Fill")))
               .ShouldBe(Value::Color(Color{255, 0, 0, 255})));

    // nested Button (retained[2]): String + attached Int32.
    const auto& nested = retained[2];
    SHOULD(ShouldlyString(nested->Get(PropertyId::of("S095Content")))
               .ShouldBe(Value::String("Nested")));
    SHOULD(ShouldlyString(nested->Get(PropertyId::of("S095Grid.Row")))
               .ShouldBe(Value::Int32(2)));
}

TEST_CASE("S095: golden A reader axis — x:Name registry + root_class resolve the doc")
{
    registerS095Properties();
    VamlReader reader;
    reader.register_event("S095Click"); // golden A carries an event-shaped key
    std::unique_ptr<DependencyObject> root = reader.Load(kGoldenDocA);

    // Root + both named children resolve to live objects via named().
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("App.MainView")));
    SHOULD(ShouldlyIntegral(reader.named("root") == root.get()).ShouldBe(true));

    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(5u));
    if (retained.size() == 5)
    {
        SHOULD(ShouldlyIntegral(reader.named("search") == retained[0].get()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(reader.named("label") == retained[1].get()).ShouldBe(true));
    }

    // names() in document (registration) order: root, search, label.
    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(3u));
    const auto& names = reader.names();
    SHOULD(ShouldlyString(names.size()).ShouldBe(3u));
    if (names.size() == 3)
    {
        SHOULD(ShouldlyString(std::string(names[0].first)).ShouldBe(std::string("root")));
        SHOULD(ShouldlyString(std::string(names[1].first)).ShouldBe(std::string("search")));
        SHOULD(ShouldlyString(std::string(names[2].first)).ShouldBe(std::string("label")));
    }
}

// ---------------------------------------------------------------------------
// GOLDEN A — AXIS 3: DEFERRED surfaces (the S2 tag) on the golden doc
// ---------------------------------------------------------------------------

TEST_CASE("S095: golden A deferred axis — extensions + event handler captured, x:Key deferred")
{
    registerS095Properties();
    VamlReader reader;
    reader.register_event("S095Click");
    std::unique_ptr<DependencyObject> root = reader.Load(kGoldenDocA);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(5u));
    if (retained.size() < 5)
    {
        return; // guard (should not happen)
    }

    // The two {Binding} extensions, document order, with the ORIGINAL source.
    const auto& ext = reader.deferred_extensions();
    SHOULD(ShouldlyString(ext.size()).ShouldBe(2u));
    if (ext.size() == 2)
    {
        // ext[0] = the search Button's S095Command; ext[1] = the Text's S095Text.
        SHOULD(ShouldlyIntegral(ext[0].target == retained[0].get()).ShouldBe(true));
        SHOULD(ShouldlyString(ext[0].key).ShouldBe(std::string("S095Command")));
        SHOULD(ShouldlyString(ext[0].source).ShouldBe(std::string("{Binding SearchCommand}")));

        SHOULD(ShouldlyIntegral(ext[1].target == retained[1].get()).ShouldBe(true));
        SHOULD(ShouldlyString(ext[1].key).ShouldBe(std::string("S095Text")));
        SHOULD(ShouldlyString(ext[1].source).ShouldBe(std::string("{Binding SearchText}")));
    }

    // The single registered event routed to a deferred handler on the search Button.
    const auto& handlers = reader.deferred_event_handlers();
    SHOULD(ShouldlyString(handlers.size()).ShouldBe(1u));
    if (handlers.size() == 1)
    {
        SHOULD(ShouldlyIntegral(handlers[0].target == retained[0].get()).ShouldBe(true));
        SHOULD(ShouldlyString(handlers[0].eventName).ShouldBe(std::string("S095Click")));
        SHOULD(ShouldlyString(handlers[0].handlerName).ShouldBe(std::string("OnClick")));
    }

    // x:Key on the Text stays DEFERRED (S361): never set as a property, but the
    // object is still built (not thrown). The Text's S095Text is an extension
    // (not set); x:Name is a Name directive (registered), never a property.
    SHOULD(ShouldlyString(retained[1]->Get(PropertyId::of("S095Text")))
               .ShouldBe(Value::String("")));
}

// ---------------------------------------------------------------------------
// GOLDEN A — AXIS 4: LIVE ROUND-TRIP parity (text path == parsed path)
// ---------------------------------------------------------------------------

TEST_CASE("S095: golden A parity axis — Load(text) and Load(parse_vaml(doc)) agree")
{
    registerS095Properties();
    VamlReader r1;
    r1.register_event("S095Click"); // golden A carries an event-shaped key
    std::unique_ptr<DependencyObject> root1 = r1.Load(kGoldenDocA);

    ParsedElement parsed = parse_vaml(kGoldenDocA);
    VamlReader r2;
    r2.register_event("S095Click");
    std::unique_ptr<DependencyObject> root2 = r2.Load(parsed);

    // Same root type + same retained count.
    assertConcrete<Window>(root1);
    assertConcrete<Window>(root2);
    SHOULD(ShouldlyString(r1.retained_objects().size()).ShouldBe(r2.retained_objects().size()));
    SHOULD(ShouldlyString(r1.retained_objects().size()).ShouldBe(5u));

    // Same converted values on the corresponding nodes.
    SHOULD(ShouldlyString(root1->Get(PropertyId::of("S095Width")))
               .ShouldBe(root2->Get(PropertyId::of("S095Width"))));
    SHOULD(ShouldlyString(r1.retained_objects()[0]->Get(PropertyId::of("S095Content")))
               .ShouldBe(r2.retained_objects()[0]->Get(PropertyId::of("S095Content"))));
    SHOULD(ShouldlyString(r1.retained_objects()[0]->Get(PropertyId::of("S095Grid.Row")))
               .ShouldBe(r2.retained_objects()[0]->Get(PropertyId::of("S095Grid.Row"))));
    SHOULD(ShouldlyString(r1.retained_objects()[1]->Get(PropertyId::of("S095Text")))
               .ShouldBe(r2.retained_objects()[1]->Get(PropertyId::of("S095Text"))));

    // Same name registry (document order) + root_class.
    SHOULD(ShouldlyString(r1.name_count()).ShouldBe(r2.name_count()));
    SHOULD(ShouldlyString(r1.root_class()).ShouldBe(r2.root_class()));
}

// ---------------------------------------------------------------------------
// GOLDEN B — the structured-error taxonomy on real documents (S091/S094 re-proof)
// ---------------------------------------------------------------------------

TEST_CASE("S095: golden B — unknown ELEMENT fires ElementResolutionException(UnknownType)")
{
    registerS095Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load("<StackPanel><S095Zorp S095Content=\"x\"/></StackPanel>");
    }
    catch (const ElementResolutionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ElementResolutionError::UnknownType)));
        SHOULD(ShouldlyString(e.type_name()).ShouldBe(std::string("S095Zorp")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S095: golden B — unknown ATTRIBUTE fires VamlReaderException(UnknownProperty)")
{
    registerS095Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load("<Button S095_NoSuchProp=\"1\"/>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::UnknownProperty)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("S095_NoSuchProp")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S095: golden B — malformed VALUE fires ValueConversionException(MalformedValue)")
{
    registerS095Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load("<Button S095Width=\"abc\"/>"); // Int32, not a number
    }
    catch (const ValueConversionException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(ValueConversionError::MalformedValue)));
        SHOULD(ShouldlyString(e.literal()).ShouldBe(std::string("abc")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S095: golden B — duplicate x:Name fires VamlReaderException(DuplicateName)")
{
    registerS095Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            std::string("<StackPanel ") + kXamlNs + ">"
            "  <Button x:Name=\"dup\"/><Text x:Name=\"dup\"/>"
            "</StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::DuplicateName)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("dup")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S095: golden B — x:Class on a NON-root fires VamlReaderException(ClassOnNonRoot)")
{
    registerS095Properties();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            std::string("<StackPanel ") + kXamlNs + " x:Class=\"Root\">"
            "  <Button x:Class=\"Child\"/>"
            "</StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::ClassOnNonRoot)));
        SHOULD(ShouldlyString(e.property()).ShouldBe(std::string("Child")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// FOCUSED — clean-minimal control doc: NO latent wiring on a real golden doc
// ---------------------------------------------------------------------------

TEST_CASE("S095: clean-minimal golden loads with no extensions and no event handlers")
{
    registerS095Properties();
    VamlReader reader; // NO register_event call
    std::unique_ptr<DependencyObject> root = reader.Load(
        std::string("<StackPanel ") + kXamlNs + " x:Name=\"root\">"
        "  <Button x:Name=\"go\" S095Content=\"Go\" S095Width=\"80\" S095IsEnabled=\"true\"/>"
        "</StackPanel>");

    assertConcrete<StackPanel>(root);
    const auto& retained = reader.retained_objects();
    SHOULD(ShouldlyString(retained.size()).ShouldBe(1u));
    if (retained.size() == 1)
    {
        assertConcrete<Button>(retained[0]);
        // Pure-text tree: no latent wiring behind the reader's back.
        SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(0u));
        SHOULD(ShouldlyIntegral(reader.deferred_event_handlers().size()).ShouldBe(0u));
        // The plain-literal property still converted + set.
        SHOULD(ShouldlyString(retained[0]->Get(PropertyId::of("S095Content")))
                   .ShouldBe(Value::String("Go")));
    }
}
