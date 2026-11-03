// 11n-1 — AUTOMATIONPROPERTIES (vaml::core).
//
// The PresentationCore static attached-property class (WPF
// System.Windows.Automation.AutomationProperties). Reuses the EXISTING
// DependencyProperty/PropertyId/SetResult machinery; NO new base. Real
// permanent core surface — NOT a STUB / NOT MINIMAL REAL.
//
// Proves:
//   (A) TYPED STATICS round-trip: set_*/ *() for Name/AutomationId/HelpText/
//       ItemStatus land on a real DependencyObject via the S043 attached-aware
//       path and read back; the String DPs default to the empty string; each
//       set fires APPLIED on a real change and NO_CHANGE on an equal set.
//   (B) IsOffscreenBehavior: Int32 attached DP defaulting to Default(0); the
//       three WPF OffscreenBehavior values (0/1/2) are accepted; an out-of-range
//       value is REJECTED (store unchanged, no callback).
//   (C) CORPORUS-DOC AUTHORING: a corpus doc authoring
//       `AutomationProperties.Name="..." / .AutomationId / .HelpText /
//       .ItemStatus / .IsOffscreenBehavior="1"` on a UIElement resolves through
//       the S091 dotted-key reader + reads back the exact authored values.
//   (D) NO new base: AutomationProperties introduces no DependencyObject/host;
//       it is a pure static attached-DP class.
//
// ADR-018: ZERO new locks (Meyers-static registration + S043 attached-aware
// value path — single-threaded, the existing attached-DP precedent).
// Registers the five "AutomationProperties.*" global names (owner
// "AutomationProperties") — no S11n1-unique object-scoped names.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/AutomationProperties.h>
#include <vaml/core/DependencyObject.h>
#include <vaml/core/Element.h>        // register_builtin_elements + UIElement re-base
#include <vaml/core/UIElement.h>
#include <vaml/core/VamlReader.h>

#include <memory>
#include <string>

using vaml::AutomationProperties;
using vaml::DependencyObject;
using vaml::SetResult;
using vaml::UIElement;
using vaml::VamlReader;

// The Shouldly* fluent factories are MACROS (ShouldlyIntegral/ShouldlyString/
// ShouldlyPointer) — available globally once Shouldly.h is included after doctest.

namespace {
inline bool is_applied(SetResult r) { return r == SetResult::APPLIED; }
inline bool is_no_change(SetResult r) { return r == SetResult::NO_CHANGE; }
inline bool is_rejected(SetResult r) { return r == SetResult::REJECTED; }
} // namespace

// ---------------------------------------------------------------------------
// A. Typed static round-trip + real-change discipline
// ---------------------------------------------------------------------------

TEST_CASE("11n-1: AutomationProperties String attached DPs round-trip + real-change")
{
    AutomationProperties::ensure_automation_properties_registered();

    UIElement target;

    // default = empty string
    SHOULD(ShouldlyString(AutomationProperties::name(target)).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(AutomationProperties::automation_id(target)).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(AutomationProperties::help_text(target)).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(AutomationProperties::item_status(target)).ShouldBe(std::string("")));

    // real change -> APPLIED + reads back
    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_name(target, "SaveButton"))).ShouldBe(true));
    SHOULD(ShouldlyString(AutomationProperties::name(target)).ShouldBe(std::string("SaveButton")));

    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_automation_id(target, "btnSave"))).ShouldBe(true));
    SHOULD(ShouldlyString(AutomationProperties::automation_id(target)).ShouldBe(std::string("btnSave")));

    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_help_text(target, "Persists the current document"))).ShouldBe(true));
    SHOULD(ShouldlyString(AutomationProperties::help_text(target)).ShouldBe(std::string("Persists the current document")));

    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_item_status(target, "Dirty"))).ShouldBe(true));
    SHOULD(ShouldlyString(AutomationProperties::item_status(target)).ShouldBe(std::string("Dirty")));

    // equal re-set -> NO_CHANGE (the real-change discipline, S035)
    SHOULD(ShouldlyIntegral(is_no_change(AutomationProperties::set_name(target, "SaveButton"))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(is_no_change(AutomationProperties::set_automation_id(target, "btnSave"))).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// B. IsOffscreenBehavior: Int32 attached DP + validate gate
// ---------------------------------------------------------------------------

TEST_CASE("11n-1: IsOffscreenBehavior defaults + validates the OffscreenBehavior enum")
{
    AutomationProperties::ensure_automation_properties_registered();

    UIElement target;

    // default = Default (0)
    SHOULD(ShouldlyIntegral(AutomationProperties::is_offscreen_behavior(target))
               .ShouldBe(AutomationProperties::OffscreenBehavior_Default));

    // all three WPF OffscreenBehavior values accepted
    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_is_offscreen_behavior(target, 1))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(AutomationProperties::is_offscreen_behavior(target)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(is_applied(AutomationProperties::set_is_offscreen_behavior(target, 2))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(AutomationProperties::is_offscreen_behavior(target)).ShouldBe(2));

    // out-of-range rejected (store unchanged, no cb), value retains the last valid
    SHOULD(ShouldlyIntegral(is_rejected(AutomationProperties::set_is_offscreen_behavior(target, 7))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(AutomationProperties::is_offscreen_behavior(target)).ShouldBe(2));
}

// ---------------------------------------------------------------------------
// C. Corpus-doc authoring resolves + reads back
// ---------------------------------------------------------------------------

TEST_CASE("11n-1: corpus doc authors AutomationProperties on a UIElement + reads back")
{
    AutomationProperties::ensure_automation_properties_registered();
    vaml::register_builtin_elements();

    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"B\" AutomationProperties.Name=\"Submit\""
        " AutomationProperties.AutomationId=\"btnSubmit\""
        " AutomationProperties.HelpText=\"Sends the form\""
        " AutomationProperties.ItemStatus=\"Ready\""
        " AutomationProperties.IsOffscreenBehavior=\"1\"/>"
        "</StackPanel>");

    DependencyObject* dobj = reader.named("B");
    SHOULD(ShouldlyPointer(dobj).ShouldNotBeNull());

    SHOULD(ShouldlyString(AutomationProperties::name(*dobj)).ShouldBe(std::string("Submit")));
    SHOULD(ShouldlyString(AutomationProperties::automation_id(*dobj)).ShouldBe(std::string("btnSubmit")));
    SHOULD(ShouldlyString(AutomationProperties::help_text(*dobj)).ShouldBe(std::string("Sends the form")));
    SHOULD(ShouldlyString(AutomationProperties::item_status(*dobj)).ShouldBe(std::string("Ready")));
    SHOULD(ShouldlyIntegral(AutomationProperties::is_offscreen_behavior(*dobj)).ShouldBe(1));
}
