// 11j-7 (GAP CLOSE, dep:S454) — ELEMENT-LEVEL `<Owner.Triggers>` authoring.
//
// S454 authored ONLY `<Style.Triggers>`; the corpus authors element-level
// `<Window.Triggers>` / `<UserControl.Triggers>`. 11j-7 resolves element-level
// `.Triggers` property-element authoring: the `<Owner.Triggers>` children
// (PropertyTrigger/DataTrigger + child setters) are parsed into the SAME live
// S362 trigger model a Style carries and attached to the element (a real per-
// element owned trigger set, reader-scoped). Authoring is fully real +
// parseable; element-level EVALUATION (re-evaluate on DP match) is the recorded
// continuation to the control-material/content step.
//
// Headless (Rail 1).

#include "vaml/core/Style.h"        // Trigger / TriggerKind / trigger_kind
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/VamlReader.h"
#include "vaml/core/Element.h"

#include "doctest/doctest.h"

using namespace vaml;

TEST_CASE("11j-7: <Window.Triggers> element-level authoring parses + records triggers")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window>"
        "<Window.Triggers>"
        "<PropertyTrigger Property=\"Opacity\" Value=\"0.4\">"
        "<Setter Property=\"Opacity\" Value=\"0.4\"/>"
        "</PropertyTrigger>"
        "</Window.Triggers>"
        "</Window>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<Window*>(root.get()) != nullptr);

    // The reader recorded the element-level trigger set in per-Load scope.
    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    CHECK(et[0].first == root.get()); // attached to the Window
    REQUIRE(et[0].second.size() == 1u);
    CHECK(trigger_kind(et[0].second[0]) == TriggerKind::Property);
}

TEST_CASE("11j-7: <UserControl.Triggers> with a DataTrigger parses (reuses S240 DataTrigger parse)")
{
    VamlReader reader;
    auto root = reader.Load(
        "<UserControl>"
        "<UserControl.Triggers>"
        "<DataTrigger Binding=\"IsMouseOver\" Value=\"True\">"
        "<Setter Property=\"Opacity\" Value=\"0.9\"/>"
        "</DataTrigger>"
        "</UserControl.Triggers>"
        "<Grid/>"
        "</UserControl>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<UserControl*>(root.get()) != nullptr);

    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    CHECK(et[0].first == root.get());
    REQUIRE(et[0].second.size() == 1u);
    CHECK(trigger_kind(et[0].second[0]) == TriggerKind::Data);
}

TEST_CASE("11j-7: a malformed element-level trigger still fails loud (never silent)")
{
    VamlReader reader;
    bool threw = false;
    try
    {
        // An unknown trigger child is a structured error.
        auto root = reader.Load(
            "<Window>"
            "<Window.Triggers><BogusTrigger/></Window.Triggers>"
            "</Window>");
    }
    catch (const VamlReaderException&)
    {
        threw = true;
    }
    CHECK(threw);
}
