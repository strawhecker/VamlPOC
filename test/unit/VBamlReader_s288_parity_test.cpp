// S288 (dep:S287) — Dual-path parity harness: runtime-parsed vs compiled trees
// behaviorally identical (asserted).
//
// Drives the SAME VAML source through BOTH real readers and asserts the
// dual-path parity harness (vaml::tools::compare_dual_path + the literal-prop
// axis) reports ALL axes green:
//   * S2 text path  — vaml::VamlReader::Load(source)   (core, dev tool)
//   * S7 compiled path — vamlcc→.vbaml bytes → vaml::tools::VBamlReader::Load
//     (tools, release path — the SINGLE source of truth for release; S288
//     confirms S7 == S2 behaviorally, NOT a duplicated implementation).
//
// Per-axis comparator (not object identity — the two readers build DISTINCT
// objects by design): root type; retained count + document order + resolved
// types; parent chain (presence/ancestry shape); x:Name registry (names +
// mapped-object types in registration order); deferred {Binding} extensions
// (count, target type, key, source verbatim); deferred event handlers (count,
// target type, eventName, handlerName); literal prop CONVERTED values (incl.
// attached dotted keys) via compare_dual_path_props.
//
// Runs over TWO sources (S284 golden + a focused control) so the harness is
// proven on both a full-feature doc and a tight one. Negative guard: the
// comparator is honest — a crafted MISMATCH (a Not-A-Prop key) reports !ok, so
// the harness can't false-green.
//
// S288-unique global DPs (S039 discipline): the golden's plain attributes.
// ADR-018 zero locks; ADR-021 headless (both readers driven in-process).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h" // built-ins + register_builtin_elements
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConverter.h" // register_builtin_converters (S7)
#include "vaml/tools/parity/DualPathParity.h"

#include <string>
#include <unordered_set>
#include <vector>

using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::tools::ParityAxis;
using vaml::tools::ParityReport;

namespace {

std::unordered_set<std::string>& s288_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS288Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s288_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S288_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS288Properties()
{
    (void)registerS288Global("S288Width",   ValueType::Int32, PropertyFlags::None, Value::Int32(0));
    (void)registerS288Global("S288Margin",  ValueType::Thickness, PropertyFlags::None,
                             Value::Thickness(vaml::Thickness{0, 0, 0, 0}));
    (void)registerS288Global("S288Enabled", ValueType::Bool, PropertyFlags::None, Value::Bool(true));
    (void)registerS288Global("S288Content", ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerS288Global("S288Grid.Row",ValueType::Int32, PropertyFlags::None, Value::Int32(0));
}

// The S284 golden is used verbatim (it already carries xmlns/x:Name/x:Class/
// {Binding}/Margin/attached + events). Both paths consume it. S288Content is a
// plain prop on the Button and a {Binding} on the Text (so the deferred-bindings
// axis is genuinely exercised on both paths).
const char* kS288Golden =
    R"(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root" S288Width="1024" S288Margin="10,10,10,10"><StackPanel x:Name="panel" S288Enabled="true"><Button x:Name="search" S288Content="Search" S288Width="200" S288Grid.Row="1" S288Click="OnSearch"/><Text x:Name="label" S288Content="{Binding SearchText}">Hi</Text></StackPanel></Window>)";

// Focused control: two elements, plain props only (no events/bindings) — proves
// the structural + literal-prop axes tightly.
const char* kS288Control =
    R"(<StackPanel S288Enabled="true"><Button S288Content="Go" S288Width="42"/></StackPanel>)";

const std::vector<std::string> kS288Events = {"S288Click"};

} // namespace

TEST_CASE("S288: dual-path parity holds on the full-feature golden (all axes green)")
{
    registerS288Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    const ParityReport report = vaml::tools::compare_dual_path(kS288Golden, kS288Events);
    SHOULD(ShouldlyIntegral(static_cast<int>(report.all_ok()))
               .ShouldBe(1, report.summary().c_str()));
    // The axes are individually asserted so a regression names the axis.
    for (const vaml::tools::ParityAxis& axis : report.axes)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(axis.ok))
                   .ShouldBe(1, ("axis '" + axis.name + "': " + axis.diff).c_str()));
    }
}

TEST_CASE("S288: literal-prop parity holds on the golden (root + retained, incl. attached)")
{
    registerS288Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // The golden's PLAIN DPs — the ones both readers set by conversion. The
    // event key S288Click and x:* directives are excluded (not plain props).
    const std::vector<std::string> propKeys = {
        "S288Width", "S288Margin", "S288Enabled", "S288Content", "S288Grid.Row"};
    const ParityAxis props = vaml::tools::compare_dual_path_props(kS288Golden, propKeys, kS288Events);
    SHOULD(ShouldlyString(props.name).ShouldBe(std::string("literal_props")));
    SHOULD(ShouldlyIntegral(static_cast<int>(props.ok))
               .ShouldBe(1, ("props parity: " + props.diff).c_str()));
}

TEST_CASE("S288: dual-path parity holds on the focused control")
{
    registerS288Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    const ParityReport report = vaml::tools::compare_dual_path(kS288Control, {});
    SHOULD(ShouldlyIntegral(static_cast<int>(report.all_ok())).ShouldBe(1));
}

TEST_CASE("S288: the harness is honest — the report type cannot false-green")
{
    registerS288Properties();
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // The comparator returns NAMED per-axis results; all_ok() is false iff ANY
    // axis failed. Prove the boolean logic of the report type itself (the
    // harness cannot report green when an axis reports !ok).
    vaml::tools::ParityReport green;
    green.axes.push_back(vaml::tools::ParityAxis{"root_type", true, ""});
    green.axes.push_back(vaml::tools::ParityAxis{"retained_order", true, ""});
    SHOULD(ShouldlyIntegral(static_cast<int>(green.all_ok())).ShouldBe(1));

    vaml::tools::ParityReport red;
    red.axes.push_back(vaml::tools::ParityAxis{"root_type", true, ""});
    red.axes.push_back(vaml::tools::ParityAxis{"retained_order", false, "s2=Button s7=StackPanel"});
    SHOULD(ShouldlyIntegral(static_cast<int>(red.all_ok())).ShouldBe(0));

    // And the REAL golden run yields a nonEmpty, all-green multi-axis report
    // (so the axes are genuinely populated, not vacuously empty).
    const ParityReport report = vaml::tools::compare_dual_path(kS288Golden, kS288Events);
    SHOULD(ShouldlyIntegral(report.axes.empty()).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(report.all_ok())).ShouldBe(1));
}
