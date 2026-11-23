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

#include <algorithm>
#include <cmath>
#include <limits>
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
    //
    // S824a (WPF-fidelity boundary gate): mirrors WPF FE.cs:3741-3758 — both
    // DPs carry AffectsArrange (FE.cs:3748) AND a ValidateValueCallback
    // (ValidateHorizontalAlignmentValue / ValidateVerticalAlignmentValue,
    // FE.cs:3751/3783) that accepts ONLY the declared enum range
    // (Left/Top=0..Stretch=3). Authoring an out-of-range value (HorizontalAlignment
    // ="4" or "=-1", a raw Int32 past the enum domain) THROWS
    // InvalidPropertyValueException at the Set boundary (store unchanged) via
    // the S810_1 false-ValidateFunc disposition — matching WPF's
    // ArgumentException(InvalidPropertyValue) at DependencyObject.cs:653.
    // 0/1/2/3 all resolve; Stretch=3 is the in-range default. The
    // Int32-rides-enum mapping (S120/S600/S648) is UNCHANGED — only the WPF
    // validate callback + AffectsArrange flag are added.
    (void)DependencyProperty::Register(
        "HorizontalAlignment", ValueType::Int32, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Int32(static_cast<std::int32_t>(HorizontalAlignment::Stretch)),
                      PropertyFlags::AffectsArrange))
            .with_validate([](const Value& v) {
                const int x = v.get<int>();
                // WPF ValidateHorizontalAlignmentValue (FE.cs:3751-3757):
                // Left(0)/Center(1)/Right(2)/Stretch(3).
                return x == 0 || x == 1 || x == 2 || x == 3;
            }));
    (void)DependencyProperty::Register(
        "VerticalAlignment", ValueType::Int32, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Int32(static_cast<std::int32_t>(VerticalAlignment::Stretch)),
                      PropertyFlags::AffectsArrange))
            .with_validate([](const Value& v) {
                const int x = v.get<int>();
                // WPF ValidateVerticalAlignmentValue (FE.cs:3783-3789):
                // Top(0)/Center(1)/Bottom(2)/Stretch(3).
                return x == 0 || x == 1 || x == 2 || x == 3;
            }));

    // Width / Height (WPF FE.cs:3466/3545): default auto/NaN, NOT inherited,
    // AffectsMeasure, ValidateValueCallback=IsWidthHeightValid (WPF
    // FE.cs:3451/3530). S842a brings the metadata to WPF-exact (the earlier
    // PropertyFlags::None registration was a drift). A Set to NaN or a finite
    // >= 0 (not +Inf) is accepted; a negative or +Inf THROWS
    // InvalidPropertyValueException at the Set boundary (S810_1 disposition).
    (void)DependencyProperty::Register(
        "Width", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Double(std::numeric_limits<double>::quiet_NaN()),
                      PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return std::isnan(x) ||
                       (x >= 0.0 && !std::isinf(x)); // WPF IsWidthHeightValid
            }));
    (void)DependencyProperty::Register(
        "Height", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Double(std::numeric_limits<double>::quiet_NaN()),
                      PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return std::isnan(x) ||
                       (x >= 0.0 && !std::isinf(x)); // WPF IsWidthHeightValid
            }));

    // MinWidth / MinHeight (WPF FE.cs:3492/3571): default 0, AffectsMeasure,
    // IsMinWidthHeightValid (!NaN, >= 0, not +Inf).
    (void)DependencyProperty::Register(
        "MinWidth", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(Value::Double(0.0),
                                                PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return !std::isnan(x) && x >= 0.0 && !std::isinf(x);
            }));
    (void)DependencyProperty::Register(
        "MinHeight", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(Value::Double(0.0),
                                                PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return !std::isnan(x) && x >= 0.0 && !std::isinf(x);
            }));

    // MaxWidth / MaxHeight (WPF FE.cs:3518/3597): default +Inf, AffectsMeasure,
    // IsMaxWidthHeightValid (!NaN, >= 0 — +Inf IS allowed for Max).
    (void)DependencyProperty::Register(
        "MaxWidth", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Double(std::numeric_limits<double>::infinity()),
                      PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return !std::isnan(x) && x >= 0.0; // WPF IsMaxWidthHeightValid
            }));
    (void)DependencyProperty::Register(
        "MaxHeight", ValueType::Double, "FrameworkElement",
        std::move(PropertyMetadata::WithDefault(
                      Value::Double(std::numeric_limits<double>::infinity()),
                      PropertyFlags::AffectsMeasure))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return !std::isnan(x) && x >= 0.0; // WPF IsMaxWidthHeightValid
            }));

    // UseLayoutRounding (WPF FE.cs:~ around :1260): bool, default false,
    // Inherits | AffectsMeasure; the change handler (OnUseLayoutRoundingChanged)
    // mirrors the value into the VisualFlags.UseLayoutRounding bit (handled by
    // UIElement::OnPropertyChanged per the DP id). Registered HERE as the real
    // Bool DP (removed from the ElementDpFidelity Int32 placeholder row).
    (void)DependencyProperty::Register(
        "UseLayoutRounding", ValueType::Bool, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Bool(false),
                                      PropertyFlags::Inherits | PropertyFlags::AffectsMeasure));

    // LayoutTransform (WPF FE.cs:~:1250): Transform OBJECT, default
    // Transform.Identity, AffectsMeasure; the change handler
    // (OnLayoutTransformChanged) sets AreTransformsClean = false (handled by
    // UIElement::OnPropertyChanged). Accepts ANY Transform (WPF has no
    // ValidateValueCallback).
    (void)DependencyProperty::Register(
        "LayoutTransform", ValueType::Transform, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Transform(vaml::Transform::identity()),
                                      PropertyFlags::AffectsMeasure));

    // DoNotApplyLayoutRoundingToMarginsAndBorderThickness (WPF internal
    // FrameworkAppContextSwitch — modeled as an internal Bool DP, default false)
    // and BypassLayoutPolicies (WPF InternalFlags2 — modeled as an internal Bool
    // DP, default false). Both record-only this batch.
    (void)DependencyProperty::Register(
        "DoNotApplyLayoutRoundingToMarginsAndBorderThickness", ValueType::Bool,
        "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Bool(false), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "BypassLayoutPolicies", ValueType::Bool, "FrameworkElement",
        PropertyMetadata::WithDefault(Value::Bool(false), PropertyFlags::None));

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

// --- S842a Min/Max size DPs + UseLayoutRounding + LayoutTransform accessors --

VAML_CORE_EXPORT
SetResult FrameworkElement::set_min_width(double value)
{
    return Set(PropertyId::of("MinWidth", "FrameworkElement"), Value::Double(value));
}
VAML_CORE_EXPORT
double FrameworkElement::min_width() const
{
    return Get(PropertyId::of("MinWidth", "FrameworkElement")).get<double>();
}
VAML_CORE_EXPORT
SetResult FrameworkElement::set_min_height(double value)
{
    return Set(PropertyId::of("MinHeight", "FrameworkElement"), Value::Double(value));
}
VAML_CORE_EXPORT
double FrameworkElement::min_height() const
{
    return Get(PropertyId::of("MinHeight", "FrameworkElement")).get<double>();
}
VAML_CORE_EXPORT
SetResult FrameworkElement::set_max_width(double value)
{
    return Set(PropertyId::of("MaxWidth", "FrameworkElement"), Value::Double(value));
}
VAML_CORE_EXPORT
double FrameworkElement::max_width() const
{
    return Get(PropertyId::of("MaxWidth", "FrameworkElement")).get<double>();
}
VAML_CORE_EXPORT
SetResult FrameworkElement::set_max_height(double value)
{
    return Set(PropertyId::of("MaxHeight", "FrameworkElement"), Value::Double(value));
}
VAML_CORE_EXPORT
double FrameworkElement::max_height() const
{
    return Get(PropertyId::of("MaxHeight", "FrameworkElement")).get<double>();
}
VAML_CORE_EXPORT
SetResult FrameworkElement::set_use_layout_rounding(bool value)
{
    return Set(PropertyId::of("UseLayoutRounding", "FrameworkElement"), Value::Bool(value));
}
VAML_CORE_EXPORT
bool FrameworkElement::use_layout_rounding() const
{
    return Get(PropertyId::of("UseLayoutRounding", "FrameworkElement")).get<bool>();
}
VAML_CORE_EXPORT
SetResult FrameworkElement::set_layout_transform(const std::shared_ptr<vaml::Transform>& value)
{
    return Set(PropertyId::of("LayoutTransform", "FrameworkElement"),
               Value::Transform(value ? value : vaml::Transform::identity()));
}
VAML_CORE_EXPORT
std::shared_ptr<vaml::Transform> FrameworkElement::layout_transform() const
{
    return Get(PropertyId::of("LayoutTransform", "FrameworkElement"))
        .get<std::shared_ptr<vaml::Transform>>();
}

VAML_CORE_EXPORT
vaml::MinMax FrameworkElement::min_max() const
{
    // WPF `new MinMax(this)`: read the six layout DPs and fold. Width/Height
    // live on UIElement (returns the FE-owner "Width"/"Height" DPs).
    return vaml::MinMax(
        width(), height(),
        Get(PropertyId::of("MinWidth", "FrameworkElement")).get<double>(),
        Get(PropertyId::of("MinHeight", "FrameworkElement")).get<double>(),
        Get(PropertyId::of("MaxWidth", "FrameworkElement")).get<double>(),
        Get(PropertyId::of("MaxHeight", "FrameworkElement")).get<double>());
}

VAML_CORE_EXPORT
bool FrameworkElement::bypass_layout_policies() const
{
    return Get(PropertyId::of("BypassLayoutPolicies", "FrameworkElement")).get<bool>();
}

VAML_CORE_EXPORT
bool FrameworkElement::do_not_apply_layout_rounding_to_margins_border() const
{
    return Get(PropertyId::of("DoNotApplyLayoutRoundingToMarginsAndBorderThickness",
                              "FrameworkElement")).get<bool>();
}

// --- S842.6 OVERRIDE-SEAM: the relocated measure/arrange cores --------------
//
// The bodies below are the logic RELOCATED verbatim from the former UIElement
// inline Measure body (margin strip + Width/Height reconcile + compute_measured_
// desired finalizer) and Arrange compute_final_rect block. WPF-faithful class
// placement (FE owns the cores); behaviour preserved 1:1. The WPF-exact 22-leg/
// 23-leg rewrite + the DesiredSize clamp-pair is the next body sub-chunk.

namespace {

inline double dmax(double a, double b) noexcept { return a > b ? a : b; }
inline double dmin(double a, double b) noexcept { return a < b ? a : b; }

// WPF `LayoutTransformData` (FE.cs:4068-4097): the per-measure snapshot of a
// non-identity LayoutTransform plus the untransformed / transformed-unrounded
// DesiredSize carriers the ArrangeCore re-uses. `has` mirrors the WPF null check.
struct LayoutTransformData
{
    bool     has = false;
    Matrix2D transform{};
    vaml::Size untransformedDS{};
    vaml::Size transformedUnroundedDS{};
};

// WPF `ComputeAlignmentOffset(clientSize, inkSize)` (FE.cs:4805-4857). For the
// vaml void/Size ArrangeOverride, inkSize == arrangeSize on the default path;
// this returns the alignment offset within the clientSize window. A degenerate
// Stretch that has been squeezed (ink > client) degenerates to Left/Top (0).
vaml::Point compute_alignment_offset(HorizontalAlignment hAlign,
                                     VerticalAlignment vAlign,
                                     vaml::Size clientSize,
                                     vaml::Size inkSize,
                                     bool hStretched,
                                     bool vStretched)
{
    double x = 0.0, y = 0.0;
    switch (hAlign)
    {
    case HorizontalAlignment::Center:
        x = (clientSize.width - inkSize.width) * 0.5;
        break;
    case HorizontalAlignment::Right:
        x = clientSize.width - inkSize.width;
        break;
    case HorizontalAlignment::Stretch:
        // Only offset a Stretch element that could not fill (degenerate squeezed
        // case): WPF ComputeAlignmentOffset degenerates Stretch->Left once
        // inkSize > clientSize, so the offset stays 0 there.
        x = 0.0;
        break;
    case HorizontalAlignment::Left:
    default:
        x = 0.0;
        break;
    }
    switch (vAlign)
    {
    case VerticalAlignment::Center:
        y = (clientSize.height - inkSize.height) * 0.5;
        break;
    case VerticalAlignment::Bottom:
        y = clientSize.height - inkSize.height;
        break;
    case VerticalAlignment::Stretch:
    case VerticalAlignment::Top:
    default:
        y = 0.0;
        break;
    }
    (void)hStretched;
    (void)vStretched;
    return vaml::Point{ static_cast<float>(x), static_cast<float>(y) };
}

} // namespace

VAML_CORE_EXPORT
vaml::Size FrameworkElement::MeasureOverride(const vaml::Size& available)
{
    // Base FrameworkElement has no content -> {0,0}; a leaf (Text) or panel
    // overrides this. (Relocated seam; WPF FrameworkElement.MeasureOverride.)
    (void)available;
    return vaml::Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
vaml::Size FrameworkElement::ArrangeOverride(vaml::Size finalSize)
{
    // Base FrameworkElement ArrangeOverride returns the passed size — the ink
    // (RenderSize) equals the arrange slot by default (WPF FE.cs:5111-5113).
    return finalSize;
}

VAML_CORE_EXPORT
vaml::Size FrameworkElement::MeasureCore(const vaml::Size& availableSize)
{
    // S842.7 — the WPF FrameworkElement.MeasureCore 22-leg body (FE.cs:4284-4503).
    // The clamp-pair is LIVE here: the returned DesiredSize is CLAMPED to the
    // available size (and to MinMax), while the UnclippedDesiredSize (the child's
    // true content extent) is kept separately for ArrangeCore's child-wins math.
    const bool useLayoutRounding = use_layout_rounding();
    const DpiScale dpi = get_dpi();

    // Leg 1: mirror the UseLayoutRounding DP into the VisualFlags bit.
    set_use_layout_rounding_flag(useLayoutRounding);

    // Leg 2: ApplyTemplate (WPF FE.cs:4302) — the [MINIMAL REAL] forward hook
    // (RESOLVED BY S847). No-op default = WPF's no-template behaviour.
    apply_template();

    // Leg 3: BypassLayoutPolicies short-circuit (FE.cs:4305-4308).
    if (bypass_layout_policies())
    {
        const vaml::Size desired = MeasureOverride(availableSize);
        publish_measure_result(desired, desired, false);
        return desired;
    }

    // Legs 4-5: strip the Margin (rounded when UseLayoutRounding and not
    // DoNotApply…; FE.cs:4312-4334), then form frameworkAvailableSize (>= 0).
    const Thickness m = margin();
    double marginWidth  = static_cast<double>(m.left) + static_cast<double>(m.right);
    double marginHeight = static_cast<double>(m.top) + static_cast<double>(m.bottom);
    if (useLayoutRounding && !do_not_apply_layout_rounding_to_margins_border())
    {
        marginWidth  = round_layout_value(marginWidth, dpi.dpiScaleX);
        marginHeight = round_layout_value(marginHeight, dpi.dpiScaleY);
    }
    vaml::Size frameworkAvailableSize{
        static_cast<float>(dmax(availableSize.width - marginWidth, 0.0)),
        static_cast<float>(dmax(availableSize.height - marginHeight, 0.0)) };

    // Leg 6: MinMax fold (rounded when UseLayoutRounding; FE.cs:4340-4348).
    MinMax mm = min_max();
    if (useLayoutRounding)
    {
        mm.minWidth  = round_layout_value(mm.minWidth,  dpi.dpiScaleX);
        mm.maxWidth  = round_layout_value(mm.maxWidth,  dpi.dpiScaleX);
        mm.minHeight = round_layout_value(mm.minHeight, dpi.dpiScaleY);
        mm.maxHeight = round_layout_value(mm.maxHeight, dpi.dpiScaleY);
    }

    // Leg 7: LayoutTransform snapshot (FE.cs:4350-4378).
    LayoutTransformData ltd;
    const std::shared_ptr<vaml::Transform> lt = layout_transform();
    if (lt && !lt->matrix().is_identity())
    {
        ltd.has = true;
        ltd.transform = lt->matrix();
        ltd.untransformedDS = vaml::Size{};
        ltd.transformedUnroundedDS = vaml::Size{};
    }

    // Leg 8: transform the available constraint into local space (FE.cs:4380).
    if (ltd.has)
    {
        frameworkAvailableSize = find_maximal_area_local_space_rect(ltd.transform, frameworkAvailableSize);
    }

    // Leg 9: clamp the framework-available size to [min,max] (FE.cs:4387-4388).
    frameworkAvailableSize.width =
        static_cast<float>(dmax(mm.minWidth, dmin(frameworkAvailableSize.width, mm.maxWidth)));
    frameworkAvailableSize.height =
        static_cast<float>(dmax(mm.minHeight, dmin(frameworkAvailableSize.height, mm.maxHeight)));

    // Leg 10: round the available size (FE.cs:4391-4394).
    if (useLayoutRounding)
    {
        frameworkAvailableSize = round_layout_size(frameworkAvailableSize, dpi);
    }

    // Leg 11: the element's content measurement (FE.cs:4397).
    vaml::Size desiredSize = MeasureOverride(frameworkAvailableSize);

    // Leg 12: apply the min floor (FE.cs:4400-4402).
    desiredSize.width  = static_cast<float>(dmax(desiredSize.width, mm.minWidth));
    desiredSize.height = static_cast<float>(dmax(desiredSize.height, mm.minHeight));

    // Leg 13: capture the un-transformed unclipped desired (FE.cs:4406).
    vaml::Size unclippedDesiredSize = desiredSize;

    // Leg 14: transform the unclipped desired (FE.cs:4408-4419).
    if (ltd.has)
    {
        ltd.untransformedDS = unclippedDesiredSize;
        const vaml::Rect unclippedBoundsTransformed = ltd.transform.rect_transform(
            vaml::Rect{ 0.0f, 0.0f, unclippedDesiredSize.width, unclippedDesiredSize.height });
        unclippedDesiredSize = vaml::Size{ unclippedBoundsTransformed.width,
                                           unclippedBoundsTransformed.height };
    }

    // Leg 15: clip the desired to the max bound (FE.cs:4421-4434).
    bool clipped = false;
    if (desiredSize.width > mm.maxWidth)
    {
        desiredSize.width = static_cast<float>(mm.maxWidth);
        clipped = true;
    }
    if (desiredSize.height > mm.maxHeight)
    {
        desiredSize.height = static_cast<float>(mm.maxHeight);
        clipped = true;
    }

    // Leg 16: transform the (max-clamped) desired (FE.cs:4436-4442).
    if (ltd.has)
    {
        const vaml::Rect childBoundsTransformed = ltd.transform.rect_transform(
            vaml::Rect{ 0.0f, 0.0f, desiredSize.width, desiredSize.height });
        desiredSize = vaml::Size{ childBoundsTransformed.width, childBoundsTransformed.height };
    }

    // Leg 17: add the margin back (FE.cs:4446-4447).
    double clippedDesiredWidth  = static_cast<double>(desiredSize.width) + marginWidth;
    double clippedDesiredHeight = static_cast<double>(desiredSize.height) + marginHeight;

    // Leg 18: clamp to the available size (the clamp-pair; FE.cs:4452-4461).
    if (clippedDesiredWidth > availableSize.width)
    {
        clippedDesiredWidth = availableSize.width;
        clipped = true;
    }
    if (clippedDesiredHeight > availableSize.height)
    {
        clippedDesiredHeight = availableSize.height;
        clipped = true;
    }

    // Leg 19: record the transformed unrounded desired (FE.cs:4464).
    if (ltd.has)
    {
        ltd.transformedUnroundedDS = vaml::Size{
            static_cast<float>(dmax(0.0, clippedDesiredWidth)),
            static_cast<float>(dmax(0.0, clippedDesiredHeight)) };
    }

    // Leg 20: round the clipped desired (FE.cs:4467-4471).
    if (useLayoutRounding)
    {
        clippedDesiredWidth  = round_layout_value(clippedDesiredWidth, dpi.dpiScaleX);
        clippedDesiredHeight = round_layout_value(clippedDesiredHeight, dpi.dpiScaleY);
    }

    // Legs 21-22: store the UnclippedDesiredSize SizeBox when clipped, then
    // return the clamped DesiredSize (FE.cs:4489-4502).
    const vaml::Size clampedDesired{
        static_cast<float>(dmax(0.0, clippedDesiredWidth)),
        static_cast<float>(dmax(0.0, clippedDesiredHeight)) };
    if (clipped || clippedDesiredWidth < 0.0 || clippedDesiredHeight < 0.0)
    {
        publish_measure_result(clampedDesired, unclippedDesiredSize, true);
    }
    else
    {
        publish_measure_result(clampedDesired, vaml::Size{}, false);
    }
    return clampedDesired;
}

VAML_CORE_EXPORT
void FrameworkElement::ArrangeCore(const vaml::Rect& finalRect)
{
    // S842.8 — the WPF FrameworkElement.ArrangeCore 23-leg body (FE.cs:4507-4786).
    // Consumes the UnclippedDesiredSize clamp-pair (child-wins-then-clip), folds
    // the MinMax + LayoutTransform, computes the ink via ArrangeOverride, and
    // publishes final_rect / layout_offset / render_size / needs_clip / clip_rect.
    const bool useLayoutRounding = use_layout_rounding();
    const DpiScale dpi = get_dpi();

    // Leg 1: transform-clean + rounding mirror.
    set_are_transforms_clean(true);

    // Leg 2: BypassLayoutPolicies short-circuit (FE.cs:4523-4531).
    if (bypass_layout_policies())
    {
        const vaml::Size oldRenderSize = render_size();
        const vaml::Size inkSize = ArrangeOverride(vaml::Size{ finalRect.width, finalRect.height });
        publish_arrange_result(finalRect, vaml::Point{ finalRect.x, finalRect.y }, inkSize,
                               false, vaml::Rect{});
        (void)oldRenderSize;
        return;
    }

    // Leg 3: reset NeedsClipBounds (FE.cs:4546).
    bool needsClip = false;

    // Leg 4: arrangeSize from the final-rect size (FE.cs:4551).
    vaml::Size arrangeSize{ finalRect.width, finalRect.height };

    // Legs 5-6: strip the Margin, clamped >= 0 (FE.cs:4553-4563).
    const Thickness m = margin();
    double marginWidth  = static_cast<double>(m.left) + static_cast<double>(m.right);
    double marginHeight = static_cast<double>(m.top) + static_cast<double>(m.bottom);
    if (useLayoutRounding && !do_not_apply_layout_rounding_to_margins_border())
    {
        marginWidth  = round_layout_value(marginWidth, dpi.dpiScaleX);
        marginHeight = round_layout_value(marginHeight, dpi.dpiScaleY);
    }
    arrangeSize.width  = static_cast<float>(dmax(0.0, arrangeSize.width - marginWidth));
    arrangeSize.height = static_cast<float>(dmax(0.0, arrangeSize.height - marginHeight));

    // Leg 7: transform-unrounded desired minus margins (FE.cs:4565-4576).
    vaml::Size transformedUnroundedDS{};
    const std::shared_ptr<vaml::Transform> lt = layout_transform();
    LayoutTransformData ltd;
    if (lt && !lt->matrix().is_identity())
    {
        ltd.has = true;
        ltd.transform = lt->matrix();
    }
    if (useLayoutRounding && ltd.has)
    {
        transformedUnroundedDS = vaml::Size{
            static_cast<float>(dmax(0.0, ltd.transformedUnroundedDS.width - marginWidth)),
            static_cast<float>(dmax(0.0, ltd.transformedUnroundedDS.height - marginHeight)) };
    }

    // Leg 8: the unclipped desired (SizeBox if Measure clamped, else DesiredSize -
    // margin), folded with the transformed-unrounded desired (FE.cs:4579-4594).
    vaml::Size unclippedDesiredSize;
    if (has_unclipped_desired_size())
    {
        unclippedDesiredSize = unclipped_desired_size(); // the SizeBox (content extent)
    }
    else
    {
        unclippedDesiredSize = vaml::Size{
            static_cast<float>(dmax(0.0, desired_size().width - marginWidth)),
            static_cast<float>(dmax(0.0, desired_size().height - marginHeight)) };
        unclippedDesiredSize.width  = static_cast<float>(dmax(unclippedDesiredSize.width, transformedUnroundedDS.width));
        unclippedDesiredSize.height = static_cast<float>(dmax(unclippedDesiredSize.height, transformedUnroundedDS.height));
    }

    // Leg 9: child-wins — the arrange size never falls below the unclipped
    // desired (FE.cs:4596-4607).
    if (layout_double_util::less_than(arrangeSize.width, unclippedDesiredSize.width))
    {
        needsClip = true;
        arrangeSize.width = unclippedDesiredSize.width;
    }
    if (layout_double_util::less_than(arrangeSize.height, unclippedDesiredSize.height))
    {
        needsClip = true;
        arrangeSize.height = unclippedDesiredSize.height;
    }

    // Leg 10: a non-Stretch axis is arranged at the unclipped desired verbatim
    // (FE.cs:4611-4619).
    if (horizontal_alignment() != HorizontalAlignment::Stretch)
    {
        arrangeSize.width = unclippedDesiredSize.width;
    }
    if (vertical_alignment() != VerticalAlignment::Stretch)
    {
        arrangeSize.height = unclippedDesiredSize.height;
    }

    // Leg 11: LayoutTransform arrange-size correction (FE.cs:4621-4661).
    if (ltd.has)
    {
        const vaml::Size potentialArrangeSize =
            find_maximal_area_local_space_rect(ltd.transform, arrangeSize);
        unclippedDesiredSize = ltd.untransformedDS;
        if (potentialArrangeSize.width != 0.0f && potentialArrangeSize.height != 0.0f &&
            unclippedDesiredSize.width != 0.0f && unclippedDesiredSize.height != 0.0f)
        {
            if (layout_double_util::less_than(potentialArrangeSize.width, arrangeSize.width))
            {
                arrangeSize.width = unclippedDesiredSize.width;
            }
            if (layout_double_util::less_than(potentialArrangeSize.height, arrangeSize.height))
            {
                arrangeSize.height = unclippedDesiredSize.height;
            }
            if (layout_double_util::less_than(arrangeSize.width, unclippedDesiredSize.width))
            {
                needsClip = true;
                arrangeSize.width = unclippedDesiredSize.width;
            }
            if (layout_double_util::less_than(arrangeSize.height, unclippedDesiredSize.height))
            {
                needsClip = true;
                arrangeSize.height = unclippedDesiredSize.height;
            }
        }
    }

    // Leg 12: MinMax fold (rounded when UseLayoutRounding; FE.cs:4663-4675).
    MinMax mm = min_max();
    if (useLayoutRounding)
    {
        mm.minWidth  = round_layout_value(mm.minWidth,  dpi.dpiScaleX);
        mm.maxWidth  = round_layout_value(mm.maxWidth,  dpi.dpiScaleX);
        mm.minHeight = round_layout_value(mm.minHeight, dpi.dpiScaleY);
        mm.maxHeight = round_layout_value(mm.maxHeight, dpi.dpiScaleY);
    }

    // Leg 13: effective max clamp (FE.cs:4680-4694).
    const double effectiveMaxWidth = dmax(unclippedDesiredSize.width, mm.maxWidth);
    if (layout_double_util::less_than(effectiveMaxWidth, arrangeSize.width))
    {
        needsClip = true;
        arrangeSize.width = static_cast<float>(effectiveMaxWidth);
    }
    const double effectiveMaxHeight = dmax(unclippedDesiredSize.height, mm.maxHeight);
    if (layout_double_util::less_than(effectiveMaxHeight, arrangeSize.height))
    {
        needsClip = true;
        arrangeSize.height = static_cast<float>(effectiveMaxHeight);
    }

    // Leg 14: round the arrange size (FE.cs:4696-4700).
    if (useLayoutRounding)
    {
        arrangeSize = round_layout_size(arrangeSize, dpi);
    }

    // Client window = finalRect - margin (FE.cs:4752-4764), independent of ink —
    // computed here so the pre-ink arrange origin can be published before
    // ArrangeOverride (VAML panels rebuild their world slot from layout_offset()
    // + the passed size; WPF sets _finalRect/offset before ArrangeOverride,
    // FE.cs:4702/4708-4712).
    vaml::Size clientSize{
        static_cast<float>(dmax(0.0, finalRect.width - marginWidth)),
        static_cast<float>(dmax(0.0, finalRect.height - marginHeight)) };
    if (useLayoutRounding)
    {
        clientSize = round_layout_size(clientSize, dpi);
    }

    // Arrange origin (ComputeAlignmentOffset(clientSize, arrangeSize) + slot +
    // margin; FE.cs:4772-4775 / 4805-4857). This matches the former
    // compute_final_rect origin (alignment offset on the ARRANGE size).
    vaml::Point offset = compute_alignment_offset(
        horizontal_alignment(), vertical_alignment(), clientSize, arrangeSize,
        horizontal_alignment() == HorizontalAlignment::Stretch,
        vertical_alignment() == VerticalAlignment::Stretch);
    offset.x += finalRect.x + static_cast<float>(m.left);
    offset.y += finalRect.y + static_cast<float>(m.top);
    const vaml::Rect finalBox{ offset.x, offset.y, arrangeSize.width, arrangeSize.height };
    const vaml::Rect clipRect{ static_cast<float>(finalRect.x + m.left),
                               static_cast<float>(finalRect.y + m.top),
                               clientSize.width, clientSize.height };

    // Publish the pre-ink arrange state so a panel's ArrangeOverride observes the
    // correct layout_offset()/final_rect()/render_size (placeholder = arrangeSize).
    publish_arrange_result(finalBox, offset, arrangeSize, needsClip, clipRect);

    // Leg 15: the ink via ArrangeOverride (FE.cs:4702-4703).
    const vaml::Size oldRenderSize = render_size();
    vaml::Size innerInkSize = ArrangeOverride(arrangeSize);

    // Leg 16: RenderSize = ink (rounded; FE.cs:4708-4712).
    if (useLayoutRounding)
    {
        innerInkSize = round_layout_size(innerInkSize, dpi);
    }

    // Leg 17: clippedInkSize (max clamp; FE.cs:4717-4726).
    vaml::Size clippedInkSize{
        static_cast<float>(dmin(innerInkSize.width, mm.maxWidth)),
        static_cast<float>(dmin(innerInkSize.height, mm.maxHeight)) };
    if (useLayoutRounding)
    {
        clippedInkSize = round_layout_size(clippedInkSize, dpi);
    }

    // Leg 18: ink exceeds max -> clip (FE.cs:4729-4733).
    if (layout_double_util::less_than(clippedInkSize.width, innerInkSize.width) ||
        layout_double_util::less_than(clippedInkSize.height, innerInkSize.height))
    {
        needsClip = true;
    }

    // Leg 19: transform the clipped ink (FE.cs:4736-4746).
    if (ltd.has)
    {
        const vaml::Rect inkRectTransformed = ltd.transform.rect_transform(
            vaml::Rect{ 0.0f, 0.0f, clippedInkSize.width, clippedInkSize.height });
        clippedInkSize = vaml::Size{ inkRectTransformed.width, inkRectTransformed.height };
        if (useLayoutRounding)
        {
            clippedInkSize = round_layout_size(clippedInkSize, dpi);
        }
    }

    // Leg 21: client smaller than ink -> clip (FE.cs:4766-4770).
    if (layout_double_util::less_than(clientSize.width, clippedInkSize.width) ||
        layout_double_util::less_than(clientSize.height, clippedInkSize.height))
    {
        needsClip = true;
    }

    // Leg 23: re-publish the arrange result with the resolved ink (RenderSize),
    // keeping the arrange box + origin. (final_rect() is the arrange box;
    // render_size() is the ink; ink_rect() composes them — S841.)
    publish_arrange_result(finalBox, offset, innerInkSize, needsClip, clipRect);
    (void)oldRenderSize;
}

VAML_CORE_EXPORT
bool FrameworkElement::get_layout_clip(const vaml::Size& layoutSlotSize, vaml::Rect& clipRect)
{
    // Behaviour-preserving this pass: the arranged slot-client clip (S833) is
    // already stored in clip_rect_ by ArrangeCore::publish_arrange_result; the
    // S208 renderer gates on needs_clip() || clip_to_bounds(). Return it under
    // the same gate. (The full 5-branch FE.cs:4946-5105 rewrite — RTL mirror +
    // ltd inverse intersect — is the S842.9 body sub-chunk.)
    (void)layoutSlotSize;
    if (!needs_clip() && !clip_to_bounds())
    {
        return false;
    }
    clipRect = clip_rect();
    return true;
}

VAML_CORE_EXPORT
bool FrameworkElement::apply_template()
{
    // [MINIMAL REAL] FrameworkElement::apply_template — RESOLVED BY S847.
    // Default: no template (WPF's no-template behaviour). Permanent hook.
    return false;
}

VAML_CORE_EXPORT
vaml::Matrix2D FrameworkElement::get_flow_direction_transform() const
{
    // [MINIMAL REAL] get_flow_direction_transform — RESOLVED BY S846.
    // Default: LTR identity (WPF's LeftToRight identity transform). Permanent hook.
    return vaml::Matrix2D{};
}

void FrameworkElement::on_render_size_changed(vaml::Size previous, vaml::Size newSize)
{
    UIElement::on_render_size_changed(previous, newSize);
    SizeChangedEventArgs args;
    args.previous_size = previous;
    args.new_size = newSize;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("SizeChanged", vaml::RouteEventDirection::Bubble), args);
}
float FrameworkElement::actual_width() const
{
    return has_width_ever_changed() ? render_size().width : 0.0f;
}
float FrameworkElement::actual_height() const
{
    return has_height_ever_changed() ? render_size().height : 0.0f;
}

} // namespace vaml
