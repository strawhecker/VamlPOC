#pragma once
// VAML Adorner — the WPF-faithful abstract adorner base (WPF
// Documents\Adorner.cs). 11n-6.
//
// In WPF, `Adorner : FrameworkElement` is an abstract element that renders
// ABOVE the element it adorns (the `AdornedElement`): it is a visual overlay
// hosted on an `AdornerLayer`, measured/arranged to the adorned element's
// box, drawn topmost, and never clipped to the adorned element's bounds (an
// adorner legitimately overhangs — e.g. selection handles, resize grips).
// Concrete adorners (a selection border, a drag hint, a focus ring) derive
// it and override MeasureOverride + (optionally) GetDesiredTransform.
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned to the WPF source):
//   * `Adorner : UIElement` — the framework-element branch IS UIElement in
//     VAML (the S129/S131 precedent), so Adorner derives UIElement. It
//     participates in the visual tree as an OVERLAY node (hosted by the
//     AdornerLayer, not the adorned element).
//   * `AdornedElement` — the element this adorner decorates. NON-OWNING
//     pointer (const/observable): the adorned element's lifetime outlives the
//     adorner (the layer + the layout OWN it); the AdornerLayer owns the
//     adorner, so there is no second ownership edge (the Window/Border
//     non-owning-observer precedent).
//   * `MeasureOverride` DEFAULT = the adorned element's SIZE (WPF: an
//     Adorner's measure returns the adorned element's RenderSize). A derived
//     adorner overrides to shape itself (a selection ring may measure a
//     fixed grow-margin).
//   * `GetDesiredTransform(transform)` — the WPF virtual that lets an adorner
//     FILTER/ADJUST the transform the layer offers. The default returns the
//     offered transform unchanged (identity fold). This is the WPF
//     proposed-transform seam, mirroring the S208 per-element Model compose.
//   * `IsClipEnabled` — Bool DP, default FALSE (WPF: an adorner is not clipped
//     by default; adorners may overhang the adorned element).
//
// SCOPE (recorded, NOT silent): the RENDER of adorners (the second-pass
// encoder emission above the normal tree) is the GPU/[both apps, not CI] lane
// of 11n-6 (encode_adorner_tree in the vulkan encoder). The CORE Adorner here
// carries the WPF-faithful data surface + measure contract, fully
// headless-testable (this file + Adorner.cpp). The hit-test of adorners
// (AdornerHitTest, overlaying the S323 HitTestAt) is a recorded NO-GAP forward
// token, not silent.
//
// ADR-011 held (this is core DATA + measure; the "how to render" stays in the
// vulkan classifier). ADR-018: ZERO new locks. ZERO MINIMAL REAL.

#include "vaml/core/UIElement.h"      // UIElement (the VAML visual-node base)
#include "vaml/core/Transform.h"      // Matrix2D (GetDesiredTransform)
#include "vaml/core/export.h"

namespace vaml {

// The WPF-faithful abstract adorner base (11n-6). An Adorner decorates a
// NON-OWNING `AdornedElement` and is hosted (owned) by an AdornerLayer. It
// measures to the adorned element's size by default and may override
// GetDesiredTransform + (via UIElement's protected seam) MeasureOverride.
class VAML_CORE_EXPORT Adorner : public UIElement
{
public:
    // Constructs an adorner wired to `adornedElement` (the NON-OWNING
    // decorated element — must outlive this adorner; the layer/owner keeps it
    // alive, so the pointer may be nullptr only transiently before attach).
    explicit Adorner(UIElement* adornedElement) noexcept;

    // The decorated element (non-owning). nullptr when the adorner is not yet
    // bound. Matches WPF Adorner.AdornedElement (read-only).
    [[nodiscard]] UIElement* AdornedElement() const noexcept { return adorned_; }

    // The WPF proposed-transform seam: the transform the layer OFFERS this
    // adorner (the adorned element's composed Model). A derived adorner may
    // FILTER/ADJUST it (return it unchanged to render exactly at the adorned
    // element's transform, or compose an offset for an overhanging handle).
    // Default: returns the offered transform UNCHANGED (conformance-pinned —
    // WPF's GetDesiredTransform passthrough). Pure + headless-testable.
    [[nodiscard]] virtual vaml::Matrix2D GetDesiredTransform(
        const vaml::Matrix2D& offered) const;

    // --- IsClipEnabled (REAL Bool-via-Int32 DP, default FALSE) ------------
    // WPF-faithful: an adorner is NOT clipped by default (adorners overhang
    // the adorned element). When true, the render lane clips the adorner to
    // its own arranged box.
    [[nodiscard]] SetResult set_is_clip_enabled(bool value);
    [[nodiscard]] bool is_clip_enabled() const;

protected:
    // The WPF adorner measure default: measure to the ADORNED element's
    // extent (WPF: an Adorner's Measure returns the adorned element's
    // RenderSize — the overlay matches its decorated box). A derived adorner
    // overrides to shape itself. When the adorned element is null / not yet
    // arranged, returns {0,0}.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    UIElement* adorned_ = nullptr; // NON-OWNING decorated element (lifecycle: the owner)
};

} // namespace vaml
