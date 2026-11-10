// 11k-9 (dep:11k-8) — DP-FIDELITY CLOSURE IMPLEMENTATION (FULL closure).
//
// The 11k-8 audit RECORDED + VERIFIED the FULL WPF DP/property/EVENT surface
// (base-class surfaces + per-control own surfaces + Window/Border/DockPanel/
// Image/TextBlock + the 11k-4 brush surfaces) against the real WPF source
// (E:\GitRepos\wpf). THIS step MAKES the audit CODE: registers every audited
// DP/property/event that a corpus document could author, so authoring ANY
// WPF-valid property on a supported control RESOLVES (never UnknownProperty).
//
// Registry reality (the C++-ism, declared + pinned by conformance): the VAML
// property registry is NAME-UNIQUE globally (DependencyProperty::Register
// throws PropertyAlreadyRegistered on a duplicate; DependencyProperty::TryGet
// never throws). WPF resolves a DP by type-inheritance (the same DP name on a
// base/derived class is ONE property); the faithful VAML mapping is ONE
// canonical bare registration per property name + the 11j-5 Control-base
// bundle owning the shared display names once. Therefore this closure:
//   * registers the audited DPs that are NOT already in the global table
//     (guarded by TryGet — idempotent, never throws, safe to call at any
//     startup order), and
//   * SKIPS (never re-registers) the ~151 names already owned by a control or
//     the shared base bundle (Value/Text/FontSize/Padding/Orientation/...).
//   This is the WPF-FAITHFUL read of "full closure": every WPF-valid property
//   a corpus doc authors now RESOLVES, either to its existing shared
//   registration or to its newly-registered own-surface DP.
//
// Routed events: registered via RoutedEvent::registered(name, direction)
// (idempotent per the S320 registry convention). Each carries the WPF-faithful
// routing (bubbling unless the WPF source says tunneling).
//
// ADR-018: ZERO new locks (single-threaded startup). NO MINIMAL REAL — every
// DP/event here is real, permanent, resolvable code. The 11k-8 audit table is
// the acceptance checklist: EVERY row now registers + resolves.

#include "vaml/core/Brush.h"              // S643b-1: default_black_brush (the shared paint default)
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/Value.h"

#include <string>

namespace vaml {
namespace detail {

// Guarded registration: register `name` (WPF-faithful ValueType + default) ONLY
// if it is not already in the global table. This is the idempotent, never-
// throwing closure primitive — safe against the shared/base names owned by
// 11j-5. Returns the canonical DependencyProperty (existing or new).
inline const DependencyProperty& dp_register_guard(
    const char* name, ValueType type, std::string_view owner,
    PropertyMetadata metadata)
{
    if (const DependencyProperty* p = DependencyProperty::TryGet(name))
    {
        return *p;
    }
    return DependencyProperty::Register(name, type, owner, std::move(metadata));
}

inline void event_register(const char* name, RouteEventDirection dir)
{
    (void)RoutedEvent::registered(name, dir);
}

// Runs the FULL 11k-9 closure once (Meyers-static guard). Called from
// register_builtin_elements() (Element.cpp) so EVERY process registers the
// audited surface before any corpus load. Idempotent by the TryGet guards.
void run_dp_fidelity_closure()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    // =====================================================================
    // BASE-CLASS SURFACES (the 11k-8 audit table)
    // Shared/Control-base names already owned by 11j-5 (FontSize/FontFamily/
    // Padding/HorizontalContentAlignment/VerticalContentAlignment) are SKIPPED
    // by the TryGet guard. Register the genuinely-missing base DPs.
    // =====================================================================

    // UIElement (beyond the shared Visibility/Opacity/Focusable/RenderTransform
    // already registered) — WPF-faithful base surface.
    dp_register_guard("AllowDrop",           ValueType::Int32,  "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ClipToBounds",        ValueType::Int32,  "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("SnapsToDevicePixels", ValueType::Int32,  "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    // IsHitTestVisible is NOW a REAL UIElement-owned Bool DP registered at S595a
    // (UIElement.cpp, guarded + ValueType::Bool) — it is REMOVED from the fidelity
    // table so the S595a editor surface (Bool, default true, NOT inherited) owns
    // the single canonical registration (a Bool matches the AllowsTransparency/
    // Topmost precedent; the audited authoring `IsHitTestVisible="true"/"false"`
    // resolves through the Bool converter). It stays AUTHORED-resolvable (the 11k-9
    // closure intent) via that real registration.
    dp_register_guard("IsManipulationEnabled", ValueType::Int32, "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("Uid",                 ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("OpacityMask",         ValueType::BrushId, "UIElement",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    dp_register_guard("CacheMode",           ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("BitmapEffect",        ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("BitmapEffectInput",   ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("Effect",              ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("RenderTransformOrigin", ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string("0.5,0.5")), PropertyFlags::None));

    // FrameworkElement (beyond Width/Height/Margin/alignment/Tag already
    // shared) — WPF-faithful base surface.
    dp_register_guard("Name",                ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("DataContext",         ValueType::UntypedPtr, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::UntypedPtr(nullptr), PropertyFlags::None));
    dp_register_guard("Language",            ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("InputScope",          ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("FlowDirection",       ValueType::Int32,  "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("UseLayoutRounding",   ValueType::Int32,  "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("OverridesDefaultStyle", ValueType::Int32, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("FocusVisualStyle",    ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("Cursor",              ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ForceCursor",         ValueType::Int32,  "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ToolTip",             ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ContextMenu",         ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // Control (beyond the shared 11j-5 bundle) — WPF-faithful.
    dp_register_guard("BorderBrush",         ValueType::Brush, "Control",
        PropertyMetadata::WithDefault(Value::Brush(default_black_brush()), PropertyFlags::None));
    dp_register_guard("BorderThickness",     ValueType::Thickness, "Control",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));
    dp_register_guard("Background",          ValueType::Brush, "Control",
        PropertyMetadata::WithDefault(Value::Brush(default_black_brush()), PropertyFlags::None));
    dp_register_guard("TabIndex",            ValueType::Int32,  "Control",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("IsTabStop",           ValueType::Int32,  "Control",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("Template",            ValueType::String, "Control",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ContentControl (the Content DP is object-typed; already registered per
    // owner — register the remaining content surface once under Control).
    dp_register_guard("HasContent",          ValueType::Int32,  "ContentControl",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ContentTemplate",     ValueType::String, "ContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ContentTemplateSelector", ValueType::String, "ContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ContentStringFormat", ValueType::String, "ContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // RangeBase (Value/Min/Max are per-control dotted; register the shared
    // change event + LargeChange/SmallChange ~ the value surface).
    dp_register_guard("LargeChange",         ValueType::Double, "RangeBase",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));
    dp_register_guard("SmallChange",         ValueType::Double, "RangeBase",
        PropertyMetadata::WithDefault(Value::Double(0.1), PropertyFlags::None));

    // ButtonBase (the Command surface — ClickEvent already shared).
    dp_register_guard("Command",             ValueType::String, "ButtonBase",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("CommandParameter",    ValueType::String, "ButtonBase",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("CommandTarget",       ValueType::String, "ButtonBase",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ClickMode",           ValueType::Int32,  "ButtonBase",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Selector (per-control SelectedIndex/Item exist; the shared surface).
    dp_register_guard("IsSelectionActive",   ValueType::Int32,  "Selector",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsSynchronizedWithCurrentItem", ValueType::Int32, "Selector",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("SelectedValue",       ValueType::String, "Selector",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("SelectedValuePath",   ValueType::String, "Selector",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ToggleButton (IsChecked/IsThreeState are per-control dotted; the shared
    // surface is the local set). Register the shared event names.
    event_register("Checked",    RouteEventDirection::Bubble);
    event_register("Unchecked",  RouteEventDirection::Bubble);
    event_register("Indeterminate", RouteEventDirection::Bubble);

    // Thumb.
    dp_register_guard("IsDragging",          ValueType::Int32,  "Thumb",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    event_register("DragStarted",  RouteEventDirection::Bubble);
    event_register("DragDelta",    RouteEventDirection::Bubble);
    event_register("DragCompleted", RouteEventDirection::Bubble);

    // HeaderedContentControl / HeaderedItemsControl (Header already per-owner).
    dp_register_guard("HasHeader",           ValueType::Int32,  "HeaderedContentControl",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("HeaderTemplate",      ValueType::String, "HeaderedContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("HeaderTemplateSelector", ValueType::String, "HeaderedContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("HeaderStringFormat",  ValueType::String, "HeaderedContentControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ItemsControl shared surface.
    dp_register_guard("HasItems",            ValueType::Int32,  "ItemsControl",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("DisplayMemberPath",   ValueType::String, "ItemsControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ItemTemplateSelector", ValueType::String, "ItemsControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ItemStringFormat",    ValueType::String, "ItemsControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("ItemsPanel",          ValueType::String, "ItemsControl",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("IsGrouping",          ValueType::Int32,  "ItemsControl",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // =====================================================================
    // OWN-SURFACE DPs (per the 11k-8 audit) — the genuinely-missing set.
    // =====================================================================

    // ---- Button (ButtonBase + own: IsDefault/IsCancel/IsDefaulted) ----
    dp_register_guard("IsDefault",           ValueType::Int32,  "Button",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsCancel",            ValueType::Int32,  "Button",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsDefaulted",         ValueType::Int32,  "Button",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- TextBox (TextBoxBase + own; Text/AcceptsReturn already owned) ----
    dp_register_guard("TextWrapping",        ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("MinLines",            ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
    dp_register_guard("MaxLines",            ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("CharacterCasing",     ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("MaxLength",           ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("TextAlignment",       ValueType::Int32,  "TextBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("TextDecorations",     ValueType::String, "TextBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- Slider (RangeBase + own; Value/Min/Max already dotted) ----
    // NOTE: Slider's WPF-authentic `Delay`/`Interval` (the repeat-delay prop
    // names) are NOT registered HERE because RepeatButton ALREADY owns the
    // bare `Delay`/`Interval` registrations (11k-2, in its ctor). The name-
    // unique registry forbids a second bare registration; the S043 attached-
    // aware read resolves bare `Delay`/`Interval` against that owner on any
    // object, so a corpus `<Slider Delay=...>` resolves against RepeatButton's
    // registration too (WPF-observable surface preserved; no duplicate).
    dp_register_guard("IsDirectionReversed", ValueType::Int32,  "Slider",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsSnapToTickEnabled", ValueType::Int32,  "Slider",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("TickPlacement",       ValueType::Int32,  "Slider",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("TickFrequency",       ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));
    dp_register_guard("Ticks",               ValueType::String, "Slider",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("IsSelectionRangeEnabled", ValueType::Int32, "Slider",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("SelectionStart",      ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("SelectionEnd",        ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("IsMoveToPointEnabled", ValueType::Int32, "Slider",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- ScrollViewer (own; offset + visibility) ----
    dp_register_guard("HorizontalScrollBarVisibility", ValueType::Int32, "ScrollViewer",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("VerticalScrollBarVisibility",   ValueType::Int32, "ScrollViewer",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("CanContentScroll",    ValueType::Int32,  "ScrollViewer",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsDeferredScrollingEnabled", ValueType::Int32, "ScrollViewer",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("PanningMode",         ValueType::Int32,  "ScrollViewer",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- Label (ContentControl b/; Content/Foreground already owned) ----
    dp_register_guard("Target",              ValueType::String, "Label",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- ProgressBar (RangeBase + own: IsIndeterminate/Orientation) ----
    dp_register_guard("IsIndeterminate",     ValueType::Int32,  "ProgressBar",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- ComboBox (Selector + ItemsControl + own) ----
    dp_register_guard("MaxDropDownHeight",   ValueType::Double, "ComboBox",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("IsEditable",          ValueType::Int32,  "ComboBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("IsReadOnly",          ValueType::Int32,  "ComboBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ShouldPreserveUserEnteredPrefix", ValueType::Int32, "ComboBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("StaysOpenOnEdit",     ValueType::Int32,  "ComboBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- ListBox (Selector + own: SelectionMode) ----
    dp_register_guard("SelectionMode",       ValueType::Int32,  "ListBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- ListView: View ----
    dp_register_guard("View",                ValueType::String, "ListView",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- ScrollBar: Orientation present; nothing more (ViewportSize owned) ----
    // (RangeBase LargeChange/SmallChange registered above under "RangeBase".)

    // ---- RepeatButton: Delay/Interval/IsPressed already owned (11k-2) ----
    // The ButtonBase Command/ClickMode + Control surface now resolve via base.

    // ---- Viewbox: Stretch/StretchDirection already owned (11k-2) ----

    // ---- ToolBar (own: IsOverflowItem/OverflowMode missing beyond 11k-2) ----
    dp_register_guard("IsOverflowItem",      ValueType::Int32,  "ToolBar",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("OverflowMode",        ValueType::Int32,  "ToolBar",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- Expander (ExpandDirection beyond IsExpanded owned) ----
    dp_register_guard("ExpandDirection",     ValueType::Int32,  "Expander",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- GroupBox: header already owned; ContentControl surface via base ----

    // ---- ToolTip (own: HasDropShadow/StaysOpen/PlacementTarget/Rect) ----
    dp_register_guard("HasDropShadow",       ValueType::Int32,  "ToolTip",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("StaysOpen",           ValueType::Int32,  "ToolTip",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("PlacementRectangle",  ValueType::String, "ToolTip",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- Popup (own: PopupAnimation beyond 11k-2 StaysOpen; AllowsTransparency
    //      is NOT registered here — Window ALREADY owns bare AllowsTransparency
    //      (11j-4), and the name-unique registry forbids a second bare
    //      registration; the S043 attached-aware read resolves it on a Popup) ----
    dp_register_guard("PopupAnimation",      ValueType::Int32,  "Popup",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- Window (beyond 11j-4 set; adds Title/SizeToContent/WindowState) ----
    dp_register_guard("Title",               ValueType::String, "Window",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("SizeToContent",       ValueType::Int32,  "Window",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("WindowState",         ValueType::Int32,  "Window",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("ShowInTaskbar",       ValueType::Int32,  "Window",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ShowActivated",       ValueType::Int32,  "Window",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("Top",                 ValueType::Double, "Window",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("Left",                ValueType::Double, "Window",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));

    // ---- TextBlock (own; FontSize/FontFamily/Foreground/Text/Padding shared;
    //       the remaining text DPs register here per the 11k-8 TextBlock row) ----
    dp_register_guard("FontStyle",           ValueType::Int32,  "TextBlock",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("FontStretch",         ValueType::Int32,  "TextBlock",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("LineHeight",          ValueType::Double, "TextBlock",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("LineStackingStrategy", ValueType::Int32, "TextBlock",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("BaselineOffset",      ValueType::Double, "TextBlock",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    dp_register_guard("TextEffects",         ValueType::String, "TextBlock",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("IsHyphenationEnabled", ValueType::Int32, "TextBlock",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    // TextWrapping/TextAlignment/TextTrimming are also shared TextBlock text
    // DPs — register under TextBlock so the unprefixed corpus form resolves.
    dp_register_guard("TextTrimming",        ValueType::Int32,  "TextBlock",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // ---- Border (already owns BorderBrush/Thickness/CornerRadius + Padding
    //      via base; the Border own-surface is complete) ----

    // ---- DockPanel: LastChildFill ----
    dp_register_guard("LastChildFill",       ValueType::Int32,  "DockPanel",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- RadioButton: GroupName ----
    dp_register_guard("GroupName",           ValueType::String, "RadioButton",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- Image: Source/IconKey/Stretch + Width/Height; StretchDirection is
    //      NOT registered here — Viewbox ALREADY owns bare `StretchDirection`
    //      (11k-2), and the name-unique registry forbids a second bare
    //      registration; the S043 attached-aware read resolves it on an Image
    //      too (WPF-observable surface preserved). Image.Stretch itself is
    //      Image-owned (dotted) — both resolve. ----

    // ---- 11k-4 brush surfaces (Geometry/Drawing common Transform already
    //      owned; the remaining 11k-8 brush rows) ----
    dp_register_guard("ClipGeometry",        ValueType::String, "DrawingGroup",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("GuidelineSet",        ValueType::String, "DrawingGroup",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));

    // ---- DataGrid (own; existing dotted set + the remaining 11k-8 rows) ----
    dp_register_guard("CanUserResizeColumns", ValueType::Int32, "DataGrid",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ColumnWidth",          ValueType::String, "DataGrid",
        PropertyMetadata::WithDefault(Value::String(std::string("Auto")), PropertyFlags::None));
    dp_register_guard("GridLinesVisibility",  ValueType::Int32,  "DataGrid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("CanUserSortColumns",   ValueType::Int32,  "DataGrid",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("FrozenColumnCount",    ValueType::Int32,  "DataGrid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("CanUserReorderColumns", ValueType::Int32, "DataGrid",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    dp_register_guard("ClipboardCopyMode",    ValueType::Int32,  "DataGrid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("EnableRowVirtualization", ValueType::Int32, "DataGrid",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- TreeView/TreeViewItem (the 11k-8 audit rows) ----
    dp_register_guard("SelectedItem",         ValueType::String, "TreeView",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("SelectedValue",        ValueType::String, "TreeView",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    event_register("SelectedItemChanged", RouteEventDirection::Bubble);
    dp_register_guard("IsExpanded",           ValueType::Int32,  "TreeViewItem",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- MenuItem own (already owned) + the shared HeaderedItems surface ----
    // ---- DatePicker rows (the 11k-8 audit) ----
    dp_register_guard("SelectedDate",         ValueType::String, "DatePicker",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    dp_register_guard("SelectedDateFormat",   ValueType::Int32,  "DatePicker",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("FirstDayOfWeek",        ValueType::Int32,  "DatePicker",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    dp_register_guard("IsTodayHighlighted",   ValueType::Int32,  "DatePicker",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- GridSplitter own (already owned: ResizeDirection/Behavior/
    //      DragIncrement/KeyboardIncrement); ShowsPreview/PreviewStyle ----
    dp_register_guard("ShowsPreview",         ValueType::Int32,  "GridSplitter",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));

    // ---- PasswordBox (own already owned: Password/MaxLength/PasswordChar) ----
    // ---- StatusBar (own already owned: Dock) ----
    // ---- Menu (own already owned: IsMainMenu) ----

    // =====================================================================
    // ROUTED EVENTS register (the 11k-8 per-control event rows not yet).
    // =====================================================================
    event_register("ValueChanged", RouteEventDirection::Bubble);
    event_register("TextChanged",  RouteEventDirection::Bubble);
    event_register("SelectionChanged", RouteEventDirection::Bubble);
    event_register("ScrollChanged", RouteEventDirection::Bubble);
    event_register("Scroll",       RouteEventDirection::Bubble);
    event_register("Expanded",     RouteEventDirection::Bubble);
    event_register("Collapsed",    RouteEventDirection::Bubble);
    event_register("Opened",       RouteEventDirection::Bubble);
    event_register("Closed",       RouteEventDirection::Bubble);
    event_register("Loaded",       RouteEventDirection::Bubble);
    event_register("Unloaded",     RouteEventDirection::Bubble);
    event_register("GotFocus",     RouteEventDirection::Bubble);
    event_register("LostFocus",    RouteEventDirection::Bubble);
    event_register("SizeChanged",  RouteEventDirection::Bubble);
    event_register("Click",        RouteEventDirection::Bubble);
    event_register("PasswordChanged", RouteEventDirection::Bubble);
    event_register("SelectedDateChanged", RouteEventDirection::Bubble);
}

} // namespace detail

// Public entry — called from register_builtin_elements() (Element.cpp). Defined
// inside `namespace vaml` (the Element.h forward declaration is also in vaml).
void vaml_run_dp_fidelity_closure()
{
    vaml::detail::run_dp_fidelity_closure();
}

} // namespace vaml
