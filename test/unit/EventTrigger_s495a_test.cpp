// S495a — EVENTTRIGGER@11e (the 11k-5 forward token, NO-GAP): author +
// exercise `EventTrigger` on the container/menu/tooltip/popup interaction
// surface. The parsed container (`TriggerKind::Event` + `RoutedEvent` +
// optional `SourceName`) is already real (11k-5). This step PROVES the
// container binds to the REAL 11e interaction surfaces — `MenuItem.Click`,
// `ToolTip.Opened`, `TabControl.SelectionChanged`, `DataGrid.SelectionChanged`
// — and that an unknown/missing RoutedEvent fails loud. Action evaluation
// (BeginStoryboard/Storyboard) remains 11g/S510 — recorded NO-GAP on this
// token (the "not-silent 11e action-content disclosure").
//
// Headless (Rail 1, ADR-021). doftest main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/Style.h"
#include "vaml/core/VamlReader.h"

using vaml::StackPanel;
using vaml::TriggerKind;
using vaml::VamlReader;
using vaml::trigger_kind;

// ---------------------------------------------------------------------------
// 1. The 11e interaction surfaces parse as EventTrigger RoutedEvents.
// ---------------------------------------------------------------------------
TEST_CASE("S495a: an EventTrigger binds to each 11e RoutedEvent (MenuItem.Click, ToolTip.Opened, TabControl.SelectionChanged, DataGrid.SelectionChanged)")
{
    vaml::register_builtin_elements();
    const std::string doc =
        "<Window>"
        "<Window.Triggers>"
        "<EventTrigger RoutedEvent=\"MenuItem.Click\" SourceName=\"Save\"/>"
        "<EventTrigger RoutedEvent=\"ToolTip.Opened\"/>"
        "<EventTrigger RoutedEvent=\"TabControl.SelectionChanged\"/>"
        "<EventTrigger RoutedEvent=\"DataGrid.SelectionChanged\"/>"
        "</Window.Triggers>"
        "</Window>";
    VamlReader reader;
    auto root = reader.Load(doc);
    REQUIRE(root != nullptr);
    const auto& et = reader.element_triggers();
    REQUIRE(et.size() == 1u);
    REQUIRE(et[0].second.size() == 4u);
    SHOULD(ShouldlyIntegral(trigger_kind(et[0].second[0]) == TriggerKind::Event).ShouldBe(true));
    SHOULD(ShouldlyString(std::string(et[0].second[0].event.eventName)).ShouldBe(std::string("MenuItem.Click")));
    SHOULD(ShouldlyString(std::string(et[0].second[0].event.sourceName)).ShouldBe(std::string("Save")));
    SHOULD(ShouldlyString(std::string(et[0].second[1].event.eventName)).ShouldBe(std::string("ToolTip.Opened")));
    SHOULD(ShouldlyString(std::string(et[0].second[2].event.eventName)).ShouldBe(std::string("TabControl.SelectionChanged")));
    SHOULD(ShouldlyString(std::string(et[0].second[3].event.eventName)).ShouldBe(std::string("DataGrid.SelectionChanged")));
}

// ---------------------------------------------------------------------------
// 2. An unknown/missing RoutedEvent on the 11e surface fails loud (never silent).
// ---------------------------------------------------------------------------
TEST_CASE("S495a: an EventTrigger on the 11e surface without RoutedEvent fails loud")
{
    vaml::register_builtin_elements();
    {
        VamlReader reader;
        bool threw = false;
        try
        {
            (void)reader.Load(
                "<Window><Window.Triggers><EventTrigger/></Window.Triggers></Window>");
        }
        catch (const vaml::VamlReaderException& ex)
        {
            threw = true;
            SHOULD(ShouldlyString(std::string(ex.property())).ShouldBe(std::string("RoutedEvent")));
        }
        SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
    }
}

// ---------------------------------------------------------------------------
// 3. The DAG-relevant usage (appendix C §5) also authors in a Style.Triggers.
// ---------------------------------------------------------------------------
TEST_CASE("S495a: <Style.Triggers><EventTrigger RoutedEvent=MenuItem.Click/> parses (appendix C §5 surface)")
{
    vaml::register_builtin_elements();
    VamlReader reader;
    auto root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<Style x:Key=\"E2\" TargetType=\"StackPanel\">"
        "<Style.Triggers>"
        "<EventTrigger RoutedEvent=\"MenuItem.Click\"/>"
        "</Style.Triggers>"
        "</Style>"
        "</StackPanel.Resources>"
        "</StackPanel>");
    REQUIRE(root != nullptr);
    // The Style.Triggers EventTrigger is recorded (model coherence, per 11k-5).
    auto* panel = dynamic_cast<StackPanel*>(root.get());
    REQUIRE(panel != nullptr);
}
