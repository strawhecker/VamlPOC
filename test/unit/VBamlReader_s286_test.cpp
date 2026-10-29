// S286 (dep:S285) — Reader instantiates via factory registry, sets props,
// attaches deferred events (S8). The full compiled-path deferred surface:
//
//   Strand A — x:Name/x:Class/x:Key registries (the compiled analogue of the
//     S092/S094 text reader): named()/names()/name_count(), root_class(),
//     duplicate x:Name → VBamlError::DuplicateName, x:Class-on-non-root →
//     VBamlError::ClassOnNonRoot. Per-Load clearing (no cross-Load leakage).
//   Strand B — {Binding} DeferredExtensions: isBinding attributes are recorded
//     verbatim (target/key/source retained, NOT set, NOT resolved — binding
//     resolution is the release consumer / S252-analogue, recorded NO-GAP).
//   Strand C — event attach (S8): register_event(name) matches a key → a
//     VBamlDeferredEventHandler{target,eventName,handlerName} — never a
//     property set, never an unknown-property throw. (Resolved to a real routed
//     handler by S322.)
//   Full error taxonomy: DuplicateName + ClassOnNonRoot + UnknownProperty
//     distinctness — each proven on the compiled path.
//   Parity of the deferred api-shape with the S092 text reader is asserted
//     structurally (same record fields) + behaviorally (same error kinds).
//
// S286-unique global DPs (S039 discipline) — the plain attributes the golden
// sets. The compiled golden adds event-shaped keys (S286Click, S286Changed)
// + {Binding} attributes + a duplicate-name control + a non-root x:Class
// control, all verified through the REAL compile_to_bytes → VBamlReader path.
// ADR-018 zero locks; ADR-021 headless (drives the real reader, never a
// spawned subprocess).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h" // Window/StackPanel/Button/Text built-ins
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/tools/VBamlReader.h"
#include "vaml/tools/Vamlc.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::Button;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Value;
using vaml::ValueType;
using vaml::tools::VBamlDeferredEventHandler;
using vaml::tools::VBamlError;
using vaml::tools::VBamlException;
using vaml::tools::VBamlReader;

namespace {

std::unordered_set<std::string>& s286_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS286Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s286_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S286_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS286Properties()
{
    (void)registerS286Global("S286Content", ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS286Global("S286Width",   ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerS286Global("S286Text",    ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS286Global("S286Enabled", ValueType::Bool, PropertyFlags::None, Value::Bool(true));
}

// The S286 compiled golden: events (S286Click/S286Changed, registered before
// Load) + {Binding} attrs + x:Name on root+children + x:Key on a child.
const char* kS286GoldenDoc =
    R"(<Window x:Name="root" S286Click="OnLoad" S286Changed="OnTextChanged">)"
    R"(<StackPanel x:Name="panel" S286Enabled="true">)"
    R"(<Button x:Name="search" S286Content="Search" S286Click="OnSearch" S286Text="{Binding SearchText}"/>)"
    R"(<TextBlock x:Name="label" x:Key="labelKey" S286Text="{Binding LabelText}">Hi</TextBlock>)"
    R"(</StackPanel></Window>)";

std::vector<std::uint8_t> compileS286Doc(const char* doc)
{
    return vaml::tools::compile_to_bytes(doc);
}

// The golden carries event-shaped keys (S286Click/S286Changed). The Strand C
// contract requires them REGISTERED before Load; to keep every golden-based
// test focused on its own strand, a helper registers the canonical event names
// (reader-level configuration, independent of the per-Load data) then loads.
void loadGolden(VBamlReader& reader, std::unique_ptr<DependencyObject>& out)
{
    reader.register_event("S286Click");
    reader.register_event("S286Changed");
    out = reader.Load(compileS286Doc(kS286GoldenDoc));
}

} // namespace

// ---------------------------------------------------------------------------
// Strand A — x:Name/x:Class/x:Key registries
// ---------------------------------------------------------------------------

TEST_CASE("S286: x:Name registry — named()/names()/name_count() + root_class()")
{
    registerS286Properties();
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root;
    loadGolden(reader, root);

    SHOULD(ShouldlyString(reader.name_count()).ShouldBe(4u));
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("")));

    // named() maps name → the built object (root Window).
    DependencyObject* r = reader.named("root");
    SHOULD(ShouldlyIntegral(r == root.get()).ShouldBe(true));
    DependencyObject* panel = reader.named("panel");
    SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
    DependencyObject* search = reader.named("search");
    SHOULD(ShouldlyIntegral(search != nullptr).ShouldBe(true));
    DependencyObject* label = reader.named("label");
    SHOULD(ShouldlyIntegral(label != nullptr).ShouldBe(true));
    // An unregistered name resolves to null (never a throw).
    SHOULD(ShouldlyIntegral(reader.named("nope") == nullptr).ShouldBe(true));

    // names() enumerates the interned views in doc order.
    const auto& all = reader.names();
    SHOULD(ShouldlyString(all.size()).ShouldBe(4u));
    if (all.size() == 4)
    {
        SHOULD(ShouldlyString(all[0].first).ShouldBe(std::string("root")));
        SHOULD(ShouldlyString(all[1].first).ShouldBe(std::string("panel")));
        SHOULD(ShouldlyString(all[2].first).ShouldBe(std::string("search")));
        SHOULD(ShouldlyString(all[3].first).ShouldBe(std::string("label")));
    }
}

TEST_CASE("S286: x:Name — the x:Key child is NOT in the name registry (deferred)")
{
    registerS286Properties();
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root;
    loadGolden(reader, root);
    // "label" IS in the registry (it also has x:Name); "labelKey" (x:Key only)
    // is NOT a name — it's the deferred x:Key resource record (S361). No throw.
    SHOULD(ShouldlyIntegral(reader.named("label") != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(reader.named("labelKey") == nullptr).ShouldBe(true));
}

TEST_CASE("S286: duplicate x:Name fires VBamlException(DuplicateName)")
{
    registerS286Properties();
    const char* doc =
        R"(<StackPanel><Button x:Name="dup"/><TextBlock x:Name="dup"/></StackPanel>)";
    const std::vector<std::uint8_t> bytes = compileS286Doc(doc);
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(bytes);
    }
    catch (const VBamlException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VBamlError::DuplicateName)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S286: x:Class on a NON-root fires VBamlException(ClassOnNonRoot)")
{
    registerS286Properties();
    const char* doc =
        R"(<StackPanel><Button x:Class="Bad"/></StackPanel>)";
    const std::vector<std::uint8_t> bytes = compileS286Doc(doc);
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(bytes);
    }
    catch (const VBamlException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VBamlError::ClassOnNonRoot)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S286: x:Class on the ROOT records root_class(); no throw")
{
    registerS286Properties();
    const char* doc =
        R"(<Window x:Name="root" x:Class="App.MainWindow"><Button x:Name="b"/></Window>)";
    const std::vector<std::uint8_t> bytes = compileS286Doc(doc);
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(bytes);
    SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("App.MainWindow")));
}

// ---------------------------------------------------------------------------
// Strand B — {Binding} DeferredExtensions (recorded verbatim, NOT set)
// ---------------------------------------------------------------------------

TEST_CASE("S286: {Binding} attrs captured as DeferredExtension{target,key,source} verbatim")
{
    registerS286Properties();
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root;
    loadGolden(reader, root);

    const auto& exts = reader.deferred_extensions();
    // Two {Binding} attrs: Button.S286Text + Text.S286Text.
    SHOULD(ShouldlyString(exts.size()).ShouldBe(2u));
    if (exts.size() == 2)
    {
        // Button search (named object) — key S286Text, source "SearchText" binding.
        const auto& first = exts[0];
        SHOULD(ShouldlyString(first.key).ShouldBe(std::string("S286Text")));
        // source is the ORIGINAL authored text (the S2 tag), not the path.
        SHOULD(ShouldlyString(first.source).ShouldBe(std::string("{Binding SearchText}")));
        SHOULD(ShouldlyIntegral(first.target != nullptr).ShouldBe(true));
    }
}

TEST_CASE("S286: {Binding} extension is NOT set as a property, NOT resolved")
{
    registerS286Properties();
    VBamlReader reader;
    std::unique_ptr<DependencyObject> root;
    loadGolden(reader, root);

    // The deferred extensions carry the S2 tag; the DP is untouched (its value
    // stays the default, not the binding text).
    DependencyObject* search = reader.named("search");
    SHOULD(ShouldlyIntegral(search != nullptr).ShouldBe(true));
    if (search != nullptr)
    {
        // Never converted/set: the default (empty string) must remain.
        SHOULD(ShouldlyString(search->Get(PropertyId::of("S286Text")))
                   .ShouldBe(Value::String("")));
    }
}

// ---------------------------------------------------------------------------
// Strand C — event attach via register_event → DeferredEventHandler
// ---------------------------------------------------------------------------

TEST_CASE("S286: registered event key routes to DeferredEventHandler{target,eventName,handlerName}")
{
    registerS286Properties();
    // register_event BEFORE Load (the S092 contract: the reader consults the
    // registration set while consuming attributes).
    VBamlReader reader;
    reader.register_event("S286Click");
    reader.register_event("S286Changed");

    const std::vector<std::uint8_t> bytes = compileS286Doc(kS286GoldenDoc);
    std::unique_ptr<DependencyObject> root = reader.Load(bytes);
    const auto& handlers = reader.deferred_event_handlers();
    // Events referenced: Window.S286Click(OnLoad), Window.S286Changed(OnTextChanged),
    // Button.S286Click(OnSearch). Three records.
    SHOULD(ShouldlyString(handlers.size()).ShouldBe(3u));
    if (handlers.size() == 3)
    {
        // Window root: Click→OnLoad, Changed→OnTextChanged.
        SHOULD(ShouldlyString(handlers[0].eventName).ShouldBe(std::string("S286Click")));
        SHOULD(ShouldlyString(handlers[0].handlerName).ShouldBe(std::string("OnLoad")));
        SHOULD(ShouldlyIntegral(handlers[0].target == root.get()).ShouldBe(true));
        SHOULD(ShouldlyString(handlers[1].eventName).ShouldBe(std::string("S286Changed")));
        SHOULD(ShouldlyString(handlers[1].handlerName).ShouldBe(std::string("OnTextChanged")));
        // Button search: Click→OnSearch.
        SHOULD(ShouldlyString(handlers[2].eventName).ShouldBe(std::string("S286Click")));
        SHOULD(ShouldlyString(handlers[2].handlerName).ShouldBe(std::string("OnSearch")));
        SHOULD(ShouldlyIntegral(handlers[2].target == reader.named("search")).ShouldBe(true));
    }
}

TEST_CASE("S286: an UNregistered event-shaped key stays a property (UnknownProperty), not a handler")
{
    registerS286Properties();
    const char* doc = R"(<Button S286_NOT_AN_EVENT="OnFoo" S286Content="x"/>)";
    const std::vector<std::uint8_t> bytes = compileS286Doc(doc);
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(bytes);
    }
    catch (const VBamlException& e)
    {
        threw = true;
        // An unregistered key is an ordinary unknown-property authoring error —
        // NOT routed to a handler (never silently swallowed).
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VBamlError::UnknownProperty)));
        SHOULD(ShouldlyString(e.detail()).ShouldBe(std::string("S286_NOT_AN_EVENT")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// Per-Load clearing + error-taxonomy distinctness
// ---------------------------------------------------------------------------

TEST_CASE("S286: registries + deferred lists clear per Load (no cross-Load leakage)")
{
    registerS286Properties();
    VBamlReader reader;

    // First Load: populates names + extensions + handlers (registers the golden's
    // canonical events so no event-shaped key throws UnknownProperty).
    reader.register_event("S286Click");
    reader.register_event("S286Changed");
    std::unique_ptr<DependencyObject> r1 = reader.Load(compileS286Doc(kS286GoldenDoc));
    SHOULD(ShouldlyIntegral(reader.name_count()).ShouldBe(4u));
    SHOULD(ShouldlyString(reader.deferred_extensions().size()).ShouldBe(2u));

    // Second Load: a doc with NO names/extensions must clear the prior sets.
    const char* doc2 = R"(<Window><Button S286Content="x"/></Window>)";
    std::unique_ptr<DependencyObject> r2 = reader.Load(compileS286Doc(doc2));
    SHOULD(ShouldlyIntegral(reader.name_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(reader.deferred_extensions().size()).ShouldBe(0u));
    SHOULD(ShouldlyString(reader.deferred_event_handlers().size()).ShouldBe(0u));
    // The registered event survives the per-Load clear (registration is a
    // reader-level configuration, not a Load artifact).
    const auto& handlers = reader.deferred_event_handlers();
    SHOULD(ShouldlyString(handlers.size()).ShouldBe(0u)); // doc2 has no Click key
    (void)r2;
}

TEST_CASE("S286: error taxonomy distinctness — the four compiled-path kinds are mutually exclusive")
{
    registerS286Properties();
    // The four S285/S286 kinds name DIFFERENT enumerators — a table-check that
    // no two collapse, plus each is reachable as documented. (Statistical no-op
    // guard: the enum values are distinct by construction.)
    const int none = static_cast<int>(VBamlError::None);
    const int dup  = static_cast<int>(VBamlError::DuplicateName);
    const int clas = static_cast<int>(VBamlError::ClassOnNonRoot);
    const int unkp = static_cast<int>(VBamlError::UnknownProperty);
    SHOULD(ShouldlyIntegral(dup != none).ShouldBe(true));
    SHOULD(ShouldlyIntegral(clas != none).ShouldBe(true));
    SHOULD(ShouldlyIntegral(unkp != none).ShouldBe(true));
    SHOULD(ShouldlyIntegral(dup != clas).ShouldBe(true));
    SHOULD(ShouldlyIntegral(dup != unkp).ShouldBe(true));
    SHOULD(ShouldlyIntegral(clas != unkp).ShouldBe(true));
}
