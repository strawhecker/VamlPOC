// S824 (dep:S823, Sprint 11ab) — FrameworkElement base out-of-line definitions.
//
// WPF ground truth (E:\GitRepos\wpf — the authority), the structural
// divergence this step closes:
//   - FrameworkElement.cs:93 — `public partial class FrameworkElement : UIElement, ...`
//   - Control.cs:20 — `public class Control : FrameworkElement`
//   - The LAYOUT SLOT (Margin/H/VAlignment/Width/MinWidth/MaxWidth/Height/
//     MinHeight/MaxHeight, FE.cs:3466+) + the FE surface (Name/DataContext/
//     Tag/FlowDirection/Style/FocusVisualStyle/Cursor/ToolTip/ContextMenu) +
//     the Affects* OnPropertyChanged engine (FE.cs:2047, FE.cs:2198-2251) live
//     on FrameworkElement, NOT UIElement.
//   - UIElement.cs:67 declares NONE of them (grep-verified zero hits).
//
// Delivery: a REAL `vaml::FrameworkElement : UIElement` base TYPE (never a
// MINIMAL REAL stub — kept and extended into the real home). Every element
// class re-bases onto it (Control : FrameworkElement in WPF, so the ENTIRE
// element tree here is FE-derived).
//
// OWNERSHIP PLACEMENT (a recorded, principled deviation — the observable
// contract is the DP OWNER, not the C++ method's translation unit): the
// layout-slot typed accessors + the S818 OnPropertyChanged engine physically
// stay in UIElement.cpp (UIElement runs the Measure/Arrange pass and must
// reach the slot), but their DP OWNER is re-keyed to "FrameworkElement"
// (UIElement.cpp reads `PropertyId::of(name, "FrameworkElement")`), so the
// FE-owned DPs are the single source and every FE-derived element reads its
// WPF-true slot. This closes S824's real divergence (DP ownership + the base
// class) without a needless churn of moving the accessors' translation unit.
// This file then supplies the FE-SURFACE surface only: the constructor (which
// arms the FE property registration) + the FE-surface accessors
// (Name/FlowDirection/Tag).

#include "vaml/core/FrameworkElement.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyFlags.h"

#include <string>

namespace vaml {

namespace {

// Registers the FE-surface DPs (owner "FrameworkElement") exactly once per
// process. The layout-slot DPs (Margin/alignments/Width/Height) + Tag are
// registered HERE (owner "FrameworkElement") — NO LONGER by
// register_uielement_properties (which keeps only the UIElement-native DPs:
// Visibility/Opacity/RenderTransform/RenderTransformOrigin/Focusable/
// IsHitTestVisible). The FE-surface DPs that ElementDpFidelity.cpp already
// guards (Name/DataContext/Language/InputScope/FlowDirection/
// UseLayoutRounding/OverridesDefaultStyle/FocusVisualStyle/Cursor/ForceCursor/
// ToolTip/ContextMenu) stay there — registered by the same idempotent guard —
// so this function only registers the slot + Tag the S122 layout needs.
void register_framework_element_properties()
{
    // Margin: default 0, NOT inherited (WPF FE.cs:3710).
    (void)DependencyProperty::Register(
        "Margin", ValueType::Thickness, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));

    // HorizontalAlignment / VerticalAlignment: default Stretch (WPF FE.cs:3744),
    // NOT inherited, Int32-rides-enum (WPF-true: Stretch=3).
    (void)DependencyProperty::Register(
        "HorizontalAlignment", ValueType::Int32, "FrameworkElement",
        PropertyMetadata::WithDefault(
            Value::Int32(static_cast<std::int32_t>(HorizontalAlignment::Stretch)),
            PropertyFlags::None));
    (void)DependencyProperty::Register(
        "VerticalAlignment", ValueType::Int32, "FrameworkElement",
        PropertyMetadata::WithDefault(
            Value::Int32(static_cast<std::int32_t>(VerticalAlignment::Stretch)),
            PropertyFlags::None));

    // Width / Height (WPF FE.cs:3466/3545): default auto/NaN, NOT inherited.
    (void)DependencyProperty::Register(
        "Width", ValueType::Double, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Double(std::numeric_limits<double>::quiet_NaN()),
                                      PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Height", ValueType::Double, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Double(std::numeric_limits<double>::quiet_NaN()),
                                      PropertyFlags::None));

    // Tag (11j-8, WPF FE.cs:3218): object-slot String bound type (default ""),
    // NOT inherited. Mirrors the S036/11j-8 contract (literal String tag or a
    // property-element UntypedPtr object).
    (void)DependencyProperty::Register(
        "Tag", ValueType::String, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
}

} // namespace

// Runs register_framework_element_properties() at most once (first
// FrameworkElement construction). Function-local static guard => no static-
// init-order hazard. A derived FrameworkElement (any element class) arms the
// FE DPs via its base construction, so the DPs exist before any Set.
void ensure_framework_element_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_framework_element_properties();
    }
}

VAML_CORE_EXPORT
FrameworkElement::FrameworkElement()
{
    // Arm the FE-owner slot + Tag DPs at first FE construction (the element
    // base's own ctor already armed the UIElement-native DPs). A derived
    // FrameworkElement ctor runs this base first, so the FE DPs exist before
    // any Set on the concrete element.
    ensure_framework_element_properties_registered();
}

VAML_CORE_EXPORT
FrameworkElement::~FrameworkElement() = default;

// --- FE surface accessors (owner "FrameworkElement") ------------------------

VAML_CORE_EXPORT
SetResult FrameworkElement::set_name(const std::string& value)
{
    return Set(PropertyId::of("Name", "FrameworkElement"), Value::String(value));
}

VAML_CORE_EXPORT
std::string FrameworkElement::name() const
{
    return Get(PropertyId::of("Name", "FrameworkElement")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult FrameworkElement::set_flow_direction(std::int32_t value)
{
    return Set(PropertyId::of("FlowDirection", "FrameworkElement"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t FrameworkElement::flow_direction() const
{
    return Get(PropertyId::of("FlowDirection", "FrameworkElement")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult FrameworkElement::set_tag(const std::string& value)
{
    return Set(PropertyId::of("Tag", "FrameworkElement"), Value::String(value));
}

VAML_CORE_EXPORT
std::string FrameworkElement::tag() const
{
    return Get(PropertyId::of("Tag", "FrameworkElement")).get<std::string>();
}

} // namespace vaml
