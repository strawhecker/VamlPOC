#pragma once
// VAML AutomationProperties — the PresentationCore static attached-property
// class (WPF System.Windows.Automation.AutomationProperties). 11n-1.
//
// This is REAL permanent core surface — NOT a STUB / NOT MINIMAL REAL (complete
// on delivery). It reuses the existing DependencyProperty/PropertyId/SetResult
// machinery; it introduces NO new base class.
//
// WPF-faithful surface: `AutomationProperties` is a STATIC class of ATTACHED
// dependency properties (the automation accessibility metadata a control
// author sets on any UIElement). We model the WPF-documented set as attached
// DPs, stored on the target object (the S043 attached-aware path — owner
// "AutomationProperties", value on any DependencyObject):
//   - Name            (String)  — the accessible name;
//   - AutomationId     (String)  — the stable automation identifier;
//   - HelpText         (String)  — the accessible help/tooltip text;
//   - ItemStatus       (String)  — the status of an item within its container;
//   - IsOffscreenBehavior (Int32) — an enum-like placement (off-screen vs
//     on-screen) controlling whether a bound element is treated as off-screen
//     for automation.
//
// (WPF also carries AcceleratorKey, AccessKey, ItemType, LabeledBy, LiveSetting,
// PositionInSet, SizeOfSet, IsRequiredForForm, IsDialog … on AutomationProperties;
// this step registers the documented head set per the 11n-1 title and records
// the remainder as a NO-GAP forward token — never silent. IsOffscreenBehavior is
// represented as Int32 (the WPF OffscreenBehavior enum: Default=0, Offscreen=1,
// Onscreen=2) instead of inventing a new enum type, per ZERO-invented-names.)
//
// A corpus doc authoring `AutomationProperties.Name="..."` on a UIElement
// resolves (the dotted-key reader, S091) + reads back via the typed statics.
//
// Typed statics per property (mirroring Grid.Row / Canvas.Left):
//   AutomationProperties::set_name/name, ::set_automation_id/automation_id,
//   ::set_help_text/help_text, ::set_item_status/item_status,
//   ::set_is_offscreen_behavior/is_offscreen_behavior.
//
// ADR-018: ZERO new locks (static registration via the Meyers-guard pattern +
// S043 attached-aware value path — single-threaded, mirroring the existing
// attached-DP classes). The registration runs at the first property access /
// test (ensured by ensure_automation_properties_registered()).

#include "vaml/core/DependencyObject.h"   // DependencyObject, SetResult
#include "vaml/core/export.h"

#include <string>

namespace vaml {

// The PresentationCore attached-property static class (11n-1).
class VAML_CORE_EXPORT AutomationProperties
{
public:
    // Registers the five AutomationProperties.* attached DPs into the global
    // table exactly once per process (Meyers-static guard). Idempotent; safe to
    // call repeatedly. A public seam so tests/consumers ensure the set
    // deterministically (the register_builtin_elements() precedent) before a
    // corpus doc authors them.
    static void ensure_automation_properties_registered();
    // The WPF OffscreenBehavior enum (Default=0, Offscreen=1, Onscreen=2),
    // represented as an Int32 attached value (ZERO invented names).
    static constexpr std::int32_t OffscreenBehavior_Default   = 0;
    static constexpr std::int32_t OffscreenBehavior_Offscreen = 1;
    static constexpr std::int32_t OffscreenBehavior_Onscreen  = 2;

    // --- Name (String attached DP, default "") ---------------------------
    [[nodiscard]] static SetResult set_name(DependencyObject& obj, std::string value);
    [[nodiscard]] static std::string name(const DependencyObject& obj);

    // --- AutomationId (String attached DP, default "") -------------------
    [[nodiscard]] static SetResult set_automation_id(DependencyObject& obj, std::string value);
    [[nodiscard]] static std::string automation_id(const DependencyObject& obj);

    // --- HelpText (String attached DP, default "") -----------------------
    [[nodiscard]] static SetResult set_help_text(DependencyObject& obj, std::string value);
    [[nodiscard]] static std::string help_text(const DependencyObject& obj);

    // --- ItemStatus (String attached DP, default "") ---------------------
    [[nodiscard]] static SetResult set_item_status(DependencyObject& obj, std::string value);
    [[nodiscard]] static std::string item_status(const DependencyObject& obj);

    // --- IsOffscreenBehavior (Int32 attached DP, default Default=0) ------
    [[nodiscard]] static SetResult set_is_offscreen_behavior(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t is_offscreen_behavior(const DependencyObject& obj);
};

} // namespace vaml
