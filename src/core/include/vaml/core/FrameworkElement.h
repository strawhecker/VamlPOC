#pragma once
// VAML FrameworkElement — the REAL base (S824, dep:S823, Sprint 11ab), the
// structural divergence closure.
//
// WPF ground truth (E:\GitRepos\wpf — the authority):
//   - FrameworkElement.cs:93   `public partial class FrameworkElement : UIElement, ...`
//   - Control.cs:20            `public class Control : FrameworkElement`
//   - Panel.cs:22              `public abstract class Panel : FrameworkElement`
//   - Decorator.cs:22          `public class Decorator : FrameworkElement`
//   - TextBlock.cs:44          `public class TextBlock : FrameworkElement`
//   - Image.cs:27              `public class Image : FrameworkElement`
//   - UIElement.cs:67          `public partial class UIElement : Visual, ...`
//     -> grep confirms UIElement declares NONE of Width/Margin/alignments/
//        Tag/DataContext/FlowDirection/Name (zero hits).
//   - WidthProperty (FE.cs:3466) `AffectsMeasure` + ValidateValueCallback
//     (IsWidthHeightValid); Min/MaxWidth (3492/3518), Height (3545),
//     Min/MaxHeight (3571/3597), Margin (3710), H/VAlignment (3741/3773).
//   - FE surface on FE: Style (132), DataContext (2680, Inherits), Name (3190),
//     Tag (3218), FlowDirection (3623), FocusVisualStyle (3824), Cursor (3844),
//     ToolTip (5864), ContextMenu (5893).
//   - OnPropertyChanged (FE.cs:2047) — `protected override`, the Affects*
//     invalidation engine (FE.cs:2198-2251) lives on FrameworkElement.
//
// S824 consequence: WPF places the LAYOUT SLOT (Margin/H/VAlignment/Width/
// MinWidth/MaxWidth/Height/MinHeight/MaxHeight) + the FE surface (Name/
// DataContext/Tag/FlowDirection/Style/FocusVisualStyle/Cursor/ToolTip/
// ContextMenu) + the Affects* OnPropertyChanged engine on FrameworkElement,
// NOT on UIElement. Before S824 VAML had collapsed the slot + S500 DataContext
// + 11j-8 Tag onto the flat UIElement base (DP owner "UIElement", every layout
// element `: public UIElement`). S824 introduces this REAL FRAMEWORKELEMENT
// base carrying the moved surface, and re-bases every layout element onto it
// (BOTH the FrameworkElement-branch non-controls AND the Control-branch
// controls — `Control : FrameworkElement`, so every WPF layout element is
// FrameworkElement-derived; there is NO WPF layout element on UIElement).
//
// A BASE TYPE (not a built-in element): `register_builtin_elements()` built-in
// count is UNCHANGED by introducing it. The DP (name,owner) identity (11s,
// S800-S808) re-keys the moved slot + FE-surface DPs under owner
// "FrameworkElement"; the universal-base-owner chain (DependencyProperty.cpp)
// already carries "FrameworkElement", so the bare-name reader + S043
// attached-aware global resolve them on ANY element.
//
// The measure/arrange PASS (Measure/Arrange/MeasureOverride/ArrangeOverride/
// InvalidateMeasure/InvalidateArrange + the reconcile_axis math) stays on
// UIElement (WPF-faithful: UIElement owns the layout pass; FE owns the slot).
// FrameworkElement supplies the SLOT accessors + FE-surface accessors + the
// moved OnPropertyChanged Affects* engine (S818, FE.cs:2198-2251). The
// internal Measure/Arrange bodies in UIElement.cpp read the SLOT via the
// FE-owner PropertyIds (see UIElement.cpp) so the pass consumes the FE-owned
// DPs exactly as WPF's FrameworkElement.MeasureCore applies the slot.
//
// ZERO invented names (WPF-real; 11k-7 closed). ZERO new locks (ADR-018). NO
// MINIMAL REAL stub — this is a permanent base. The MINIMAL REAL seeds
// (Button/StackPanel/...) re-base here at their own resolution sprints; those
// already real (StackPanel S129, Grid S131, TextBlock S400, etc.) are
// re-based onto this base at S824 (their real home).

#include "vaml/core/UIElement.h"

// The layout-slot value types (Thickness, HorizontalAlignment,
// VerticalAlignment) and the FE-surface value types (Value, ObservableObject)
// are already brought in by UIElement.h (Thickness.h/LayoutSlot.h/Value.h).

#include <cstdint>
#include <string>

namespace vaml {

// Arms the FE-surface + layout-slot DP registrations (owner "FrameworkElement")
// at most once (Meyers-static guard, mirroring ensure_uielement_properties_
// registered). Declared here so the UIElement ctor (which keeps the layout-
// slot accessors, reading FE-owner IDs) can arm the FE DPs too — a bare
// UIElement that reads/writes the slot must find the registered FE-owner DP.
void VAML_CORE_EXPORT ensure_framework_element_properties_registered();

class VAML_CORE_EXPORT FrameworkElement : public UIElement
{
public:
    FrameworkElement();
    // FrameworkElement adds no owning member beyond the base (the visual-
    // children/desired-size/rect/dirty layout state lives in UIElement; the
    // slot + surface DPs live in the global DP store; DataContext lives in the
    // S500 side-table), but the out-of-line dtor mirrors UIElement's so the
    // base's Impl is complete here if it ever grows. Default.
    ~FrameworkElement() override;

    // --- Layout slot (RESOLVED — owner "FrameworkElement") -----------------
    // Note on accessor placement: the layout-slot typed accessors
    // (margin/alignments/width/height) and the DataContext + OnPropertyChanged
    // engine LIVE on the base UIElement (see UIElement.h), which runs the
    // Measure/Arrange pass and must read the slot. Their DP OWNER is re-keyed
    // to "FrameworkElement" (the WPF observable contract); the accessors read
    ///
    //     PropertyId::of(name, "FrameworkElement")
    ///
    // so the FE-owned DPs are the single source, and a FrameworkElement (or
    // any FE-derived element) reads its WPF-true slot. FrameworkElement is the
    // re-base TARGET for every element class (Control : FrameworkElement in
    // WPF, so the whole element tree here derives FrameworkElement).

    // --- FE surface (owner "FrameworkElement") -----------------------------
    // Name (FE.cs:3190): the element's x:Name / string identifier (String DP).
    [[nodiscard]] SetResult set_name(const std::string& value);
    [[nodiscard]] std::string name() const;

    // FlowDirection (FE.cs:3623): Int32; 0 = LeftToRight, 1 = RightToLeft.
    [[nodiscard]] SetResult set_flow_direction(std::int32_t value);
    [[nodiscard]] std::int32_t flow_direction() const;

    // Tag (FE.cs:3218, 11j-8): object-slot DP on the element base, registered
    // with a String bound type (default "" = no tag) so a literal attribute
    // `Tag="abc"` converts through the String converter; the property-element
    // OBJECT branch calls Set(..., Value::UntypedPtr) directly (see the 11j-8
    // contract). The typed accessor reads the stored Value BY TAG.
    [[nodiscard]] SetResult set_tag(const std::string& value);
    [[nodiscard]] std::string tag() const;

    // --- S842a layout DPs (WPF FE.cs:3452-3665; owner "FrameworkElement") ---
    // The Min/Max size DPs + UseLayoutRounding + LayoutTransform. Width/Height
    // already live on UIElement (default auto/NaN, owner "FrameworkElement",
    // now AffectsMeasure + IsWidthHeightValid per S842a). These accessors read/
    // write the FE-owned DPs (the single canonical registration). Registered in
    // register_framework_element_properties().
    [[nodiscard]] SetResult set_min_width(double value);
    [[nodiscard]] double min_width() const;
    [[nodiscard]] SetResult set_min_height(double value);
    [[nodiscard]] double min_height() const;
    [[nodiscard]] SetResult set_max_width(double value);
    [[nodiscard]] double max_width() const;
    [[nodiscard]] SetResult set_max_height(double value);
    [[nodiscard]] double max_height() const;
    [[nodiscard]] SetResult set_use_layout_rounding(bool value);
    [[nodiscard]] bool use_layout_rounding() const;
    [[nodiscard]] SetResult set_layout_transform(const std::shared_ptr<vaml::Transform>& value);
    [[nodiscard]] std::shared_ptr<vaml::Transform> layout_transform() const;

    // WPF `new MinMax(this)` (FE.cs:4036-4066): the folded min/max derived from
    // the six layout DPs (Width/Height NaN => auto). Returns the S842a MinMax.
    [[nodiscard]] vaml::MinMax min_max() const;

    // --- S842.3/7 internal layout switches (WPF BypassLayoutPolicies /
    //     DoNotApplyLayoutRoundingToMarginsAndBorderThickness) ----------------
    // Read the internal Bool DPs registered in register_framework_element_
    // properties(). Consumed by MeasureCore/ArrangeCore (FE.cs:4305/4523/4315).
    [[nodiscard]] bool bypass_layout_policies() const;
    [[nodiscard]] bool do_not_apply_layout_rounding_to_margins_border() const;

    // --- S842.6 OVERRIDE-SEAM (WPF FE.cs:4284 MeasureCore / :4507 ArrangeCore) -
    // WPF places the REAL measure/arrange bodies on FrameworkElement, NOT
    // UIElement (UIElement.MeasureCore returns new Size(); UIElement.ArrangeCore
    // writes only the RenderSize/offset). These are the FE cores, `final` (no
    // further override — a derived element customizes MeasureOverride/
    // ArrangeOverride, never the core). UIElement::Measure/Arrange run the pass
    // protocol and delegate to these.
    //
    // Behavior note (S842.6): the FE cores reproduce, 1:1, the logic relocated
    // from the former UIElement inline bodies + compute_final_rect — the
    // 22-leg/23-leg WPF-exact rewrite + the DesiredSize clamp-pair (Unclipped-
    // DesiredSize separate) is the NEXT atomic body sub-chunk, tracked in
    // session-context; landing it flips DesiredSize to clamped and re-baselines
    // the measure tests. This pass is behavior-preserving.
    vaml::Size MeasureCore(const vaml::Size& availableSize) final;
    void ArrangeCore(const vaml::Rect& finalRect) final;

    // The element's CONTENT seams (relocated from UIElement — WPF has no
    // UIElement.MeasureOverride/ArrangeOverride). Base MeasureOverride {0,0};
    // base ArrangeOverride returns the passed size (ink == slot). Panels/Text
    // override these. NEW virtuals on FrameworkElement (there is no UIElement
    // base to override — WPF-faithful placement), so they carry `virtual`, not
    // `override`.
    virtual vaml::Size MeasureOverride(const vaml::Size& available);
    virtual vaml::Size ArrangeOverride(vaml::Size finalSize);
    // S844 (G3): WPF FrameworkElement OnRenderSizeChanged -- updates
    // ActualWidth/ActualHeight and raises the SizeChanged routed event.
    void on_render_size_changed(vaml::Size previous, vaml::Size newSize) override;
    [[nodiscard]] float actual_width() const;
    [[nodiscard]] float actual_height() const;

    // --- S842.6: GetLayoutClip (WPF FE.cs:4946-5105) ------------------------
    // The 5-branch slot-client clip (NeedsClipBounds||ClipToBounds → local/
    // slot/combined, RTL mirror, ltd inverse intersect). Behavior-preserving
    // this pass (reproduces the S833 slot-client rect the S208 renderer
    // scissors to).
    bool get_layout_clip(const vaml::Size& layoutSlotSize, vaml::Rect& clipRect) override;

    // --- [MINIMAL REAL] forward hooks (S842.6) ------------------------------
    // ApplyTemplate (WPF FE.ApplyTemplate, FE.cs:~) and
    // GetFlowDirectionTransform (WPF FE.cs:~). Both default to exactly WPF's
    // no-template / LTR behaviour and are SEEDED here for the template (S847)
    // and flow-direction (S846) features to extend at their own resolution
    // steps. They are permanent virtual hooks, NOT throwaway stubs.
    //
    // [MINIMAL REAL] FrameworkElement::apply_template — RESOLVED BY S847. Do NOT extend silently; do NOT backfill.
    // [MINIMAL REAL] get_flow_direction_transform — RESOLVED BY S846. Do NOT extend silently; do NOT backfill.
    virtual bool apply_template();
    [[nodiscard]] virtual vaml::Matrix2D get_flow_direction_transform() const;
};

} // namespace vaml
