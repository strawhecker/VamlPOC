// VAML AutomationProperties — the PresentationCore static attached-property
// class (WPF System.Windows.Automation.AutomationProperties). 11n-1.
//
// REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL). Reuses the
// existing DependencyProperty/PropertyId/SetResult machinery; no new base.
// The five documented head attached DPs (Name/AutomationId/HelpText/ItemStatus/
// IsOffscreenBehavior) are registered globally under owner "AutomationProperties"
// via the S043 attached-aware path (value on any DependencyObject), so a corpus
// doc authoring `AutomationProperties.Name="..."` on a UIElement resolves + reads
// back.
//
// Registration runs exactly once (Meyers-static guard, mirroring Grid/DockPanel/
// Canvas). ADR-018: ZERO new locks.

#include "vaml/core/AutomationProperties.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"

namespace vaml {

namespace {
// Registers the five AutomationProperties attached DPs exactly once per
// process. String DPs default to the empty string; IsOffscreenBehavior defaults
// to Default (0) with a validate gate (0/1/2 — the WPF OffscreenBehavior enum).
void register_automation_properties()
{
    (void)DependencyProperty::Register(
        "AutomationProperties.Name", ValueType::String, "AutomationProperties",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "AutomationProperties.AutomationId", ValueType::String, "AutomationProperties",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "AutomationProperties.HelpText", ValueType::String, "AutomationProperties",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "AutomationProperties.ItemStatus", ValueType::String, "AutomationProperties",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "AutomationProperties.IsOffscreenBehavior", ValueType::Int32, "AutomationProperties",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) {
                const int x = v.get<int>();
                return x == 0 || x == 1 || x == 2;
            }));
}
} // namespace

// Runs register_automation_properties() at most once (first access). A public
// seam so tests/consumers can ensure the set deterministically (the
// register_builtin_elements() precedent).
VAML_CORE_EXPORT
void AutomationProperties::ensure_automation_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_automation_properties();
    }
}

VAML_CORE_EXPORT
SetResult AutomationProperties::set_name(DependencyObject& obj, std::string value)
{
    ensure_automation_properties_registered();
    return obj.Set(PropertyId::of("AutomationProperties.Name"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string AutomationProperties::name(const DependencyObject& obj)
{
    ensure_automation_properties_registered();
    return obj.Get(PropertyId::of("AutomationProperties.Name")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult AutomationProperties::set_automation_id(DependencyObject& obj, std::string value)
{
    ensure_automation_properties_registered();
    return obj.Set(PropertyId::of("AutomationProperties.AutomationId"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string AutomationProperties::automation_id(const DependencyObject& obj)
{
    ensure_automation_properties_registered();
    return obj.Get(PropertyId::of("AutomationProperties.AutomationId")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult AutomationProperties::set_help_text(DependencyObject& obj, std::string value)
{
    ensure_automation_properties_registered();
    return obj.Set(PropertyId::of("AutomationProperties.HelpText"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string AutomationProperties::help_text(const DependencyObject& obj)
{
    ensure_automation_properties_registered();
    return obj.Get(PropertyId::of("AutomationProperties.HelpText")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult AutomationProperties::set_item_status(DependencyObject& obj, std::string value)
{
    ensure_automation_properties_registered();
    return obj.Set(PropertyId::of("AutomationProperties.ItemStatus"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string AutomationProperties::item_status(const DependencyObject& obj)
{
    ensure_automation_properties_registered();
    return obj.Get(PropertyId::of("AutomationProperties.ItemStatus")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult AutomationProperties::set_is_offscreen_behavior(DependencyObject& obj, std::int32_t value)
{
    ensure_automation_properties_registered();
    return obj.Set(PropertyId::of("AutomationProperties.IsOffscreenBehavior"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t AutomationProperties::is_offscreen_behavior(const DependencyObject& obj)
{
    ensure_automation_properties_registered();
    return obj.Get(PropertyId::of("AutomationProperties.IsOffscreenBehavior")).get<std::int32_t>();
}

} // namespace vaml
