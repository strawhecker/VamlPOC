// 11k-5 — `EventTrigger` element-level + style authoring: the event-driven
// trigger AUTHORING surface, parsed into the S362 trigger model.
//
// WPF-FAITHFUL: `EventTrigger` is ACTIONS-based (`[ContentProperty("Actions")]`
// of TriggerAction), NOT Setter-based like Property/Data triggers. At 11k-5 only
// the AUTHORING CONTAINER is modelled + parsed: `RoutedEvent` (required) +
// optional `SourceName` → `TriggerKind::Event`. The concrete `TriggerAction`
// payload (BeginStoryboard → Storyboard/DoubleAnimation) + the
// `TriggerAction::Invoke` evaluation are the recorded NO-GAP continuations owned
// by the 11e/11f/11g forward tokens (roadmap + DAG_controls trigger family),
// resolved at S510 — never silent.
//
// The EventTrigger authors in BOTH places a trigger can live:
//   * ELEMENT-LEVEL `<Owner.Triggers>` (the 11j-7 path — the `Control.Triggers`
//     focus of the 11k-5 title), and
//   * `<Style.Triggers>` (the S454 build_style path) — model coherence.
//
// Headless (Rail 1, ADR-021). doftest main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"               // Window/StackPanel + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/ResourceDictionary.h"
#include "vaml/core/Style.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::ResourceDictionary;
using vaml::ResourceValue;
using vaml::StackPanel;
using vaml::Style;
using vaml::Trigger;
using vaml::TriggerKind;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::trigger_kind;

namespace {

// Local helper mirroring the S454 test: register an Int32 DP once.
std::unordered_set<std::string>& k5_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerK5Int(std::string_view name)
{
    if (k5_names().insert(std::string(name)).second)
    {
        return DependencyProperty::Register(name, ValueType::Int32, "11k5_Owner",
                                            PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    }
    return DependencyProperty::Get(name);
}

void registerK5()
{
    (void)registerK5Int("K5Size");
    (void)registerK5Int("K5Hover");
    vaml::register_builtin_elements();
}

const ResourceDictionary* authoredDict(StackPanel* panel)
{
    return vaml::detail::get_element_resources(panel);
}

const Style* findStyle(const ResourceDictionary* dict, std::string_view key)
{
    if (dict == nullptr)
    {
        return nullptr;
    }
    const ResourceValue* v = dict->try_get(key);
    if (v == nullptr || v->kind() != ResourceValue::Kind::Object)
    {
        return nullptr;
    }
    return dynamic_cast<const Style*>(v->object());
}

} // namespace

// ---------------------------------------------------------------------------
// 1. ELEMENT-LEVEL `<Owner.Triggers>` — the `<Control.Triggers>` title focus.
// ---------------------------------------------------------------------------

TEST_CASE("11k-5: element-level <Window.Triggers><EventTrigger RoutedEvent> parses + records kind Event")
{
    registerK5();
    VamlReader reader;
    auto root = reader.Load(
        "<Window>"
        "<Window.Triggers>"
        "<EventTrigger RoutedEvent=\"Button.Click\"/>"
        "</Window.Triggers>"
        "</Window>");
    REQUIRE(root != nullptr);
    REQUIRE(dynamic_cast<vaml::Window*>(root.get()) != nullptr);

    // The reader recorded the element-level trigger set in per-Load scope.
    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    CHECK(et[0].first == root.get()); // attached to the Window
    REQUIRE(et[0].second.size() == 1u);
    // Discrimination seam: kind Event, with a real eventName.
    CHECK(trigger_kind(et[0].second[0]) == TriggerKind::Event);
    CHECK(std::string(et[0].second[0].event.eventName) == "Button.Click");
    // SourceName is optional; it is empty when not authored.
    CHECK(std::string(et[0].second[0].event.sourceName).empty());
}

TEST_CASE("11k-5: element-level EventTrigger with SourceName parses the optional source")
{
    registerK5();
    VamlReader reader;
    auto root = reader.Load(
        "<Window>"
        "<Window.Triggers>"
        "<EventTrigger RoutedEvent=\"ToolTip.Opened\" SourceName=\"Tip\"/>"
        "</Window.Triggers>"
        "</Window>");
    REQUIRE(root != nullptr);

    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    REQUIRE(et[0].second.size() == 1u);
    CHECK(trigger_kind(et[0].second[0]) == TriggerKind::Event);
    CHECK(std::string(et[0].second[0].event.eventName) == "ToolTip.Opened");
    CHECK(std::string(et[0].second[0].event.sourceName) == "Tip");
}

// ---------------------------------------------------------------------------
// 2. `<Style.Triggers>` — model coherence with the S454 build_style path.
// ---------------------------------------------------------------------------

TEST_CASE("11k-5: <Style.Triggers><EventTrigger> authors a kind-Event record into the Style")
{
    registerK5();
    VamlReader reader;
    auto root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"E\" TargetType=\"StackPanel\">"
        "<Style.Triggers>"
        "<EventTrigger RoutedEvent=\"MouseEnter\"/>"
        "</Style.Triggers>"
        "</Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");

    StackPanel* panel = dynamic_cast<StackPanel*>(root.get());
    const Style* s = findStyle(authoredDict(panel), "E");
    REQUIRE(s != nullptr);
    REQUIRE(s->triggers().size() >= 1u);
    CHECK(trigger_kind(s->triggers()[0]) == TriggerKind::Event);
    CHECK(std::string(s->triggers()[0].event.eventName) == "MouseEnter");
    CHECK(std::string(s->triggers()[0].event.sourceName).empty());
}

// ---------------------------------------------------------------------------
// 3. Mixed Property/Data/Event — order preserved, each discriminated correctly.
// ---------------------------------------------------------------------------

TEST_CASE("11k-5: mixed Property/Data/Event in one .Triggers preserves order + discriminates each")
{
    registerK5();
    VamlReader reader;
    auto root = reader.Load(
        "<Window>"
        "<Window.Triggers>"
        "<PropertyTrigger Property=\"K5Hover\" Value=\"1\">"
        "<Setter Property=\"K5Size\" Value=\"2\"/>"
        "</PropertyTrigger>"
        "<DataTrigger Binding=\"Ready\" Value=\"true\"/>"
        "<EventTrigger RoutedEvent=\"Loaded\"/>"
        "</Window.Triggers>"
        "</Window>");
    REQUIRE(root != nullptr);

    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    REQUIRE(et[0].second.size() == 3u);
    CHECK(trigger_kind(et[0].second[0]) == TriggerKind::Property);
    CHECK(trigger_kind(et[0].second[1]) == TriggerKind::Data);
    CHECK(trigger_kind(et[0].second[2]) == TriggerKind::Event);
    CHECK(std::string(et[0].second[2].event.eventName) == "Loaded");
}

// ---------------------------------------------------------------------------
// 4. Enum-stability + no Property/Data regression.
// ---------------------------------------------------------------------------

TEST_CASE("11k-5: TriggerKind is enum-stable (Property=1, Data=2, Event=3) with no regression")
{
    // Property=1, Data=2 unchanged; Event=3 APPENDED (the WPF-faithful Event arm).
    CHECK(static_cast<int>(TriggerKind::Property) == 1);
    CHECK(static_cast<int>(TriggerKind::Data) == 2);
    CHECK(static_cast<int>(TriggerKind::Event) == 3);

    // The factories still discriminate the Property/Data arms exactly as before.
    Trigger p = Trigger::MakeProperty(vaml::PropertyTrigger{});
    Trigger d = Trigger::MakeData(vaml::DataTrigger{});
    Trigger e = Trigger::MakeEvent(vaml::EventTrigger{});
    CHECK(trigger_kind(p) == TriggerKind::Property);
    CHECK(trigger_kind(d) == TriggerKind::Data);
    CHECK(trigger_kind(e) == TriggerKind::Event);
}

// ---------------------------------------------------------------------------
// 5. Malformed authoring fails loud (never silent).
// ---------------------------------------------------------------------------

TEST_CASE("11k-5: an EventTrigger without RoutedEvent fails loud (never silent)")
{
    registerK5();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<Window>"
            "<Window.Triggers><EventTrigger/></Window.Triggers>"
            "</Window>");
    }
    catch (const vaml::VamlReaderException& ex)
    {
        threw = true;
        // The offending attribute/property is named ("RoutedEvent").
        CHECK(ex.property() == "RoutedEvent");
    }
    CHECK(threw);
}

TEST_CASE("11k-5: an unknown trigger child still fails loud (never silent)")
{
    registerK5();
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<Window>"
            "<Window.Triggers><BogusTrigger/></Window.Triggers>"
            "</Window>");
    }
    catch (const vaml::VamlReaderException&)
    {
        threw = true;
    }
    CHECK(threw);
}
