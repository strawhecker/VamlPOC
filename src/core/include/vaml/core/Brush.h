#pragma once
// VAML Brush — core data brushes + Color DP.
// S364 (dep:S363) — Sprint 9.
//
// ADR-011 IS HELD: a Brush is CORE DATA (a DependencyObject owning a Color
// dependency property) — NOT a render object. The vulkan/presentation layers
// consume the brush's Color at render time (S12 / renderer), never the reverse.
// This keeps brushes pure data, testable headlessly, and layer-conformant.
//
// SolidColorBrush is the single concrete brush delivered at S364 (the visual-
// state "brush swap" primitive the ControlTemplate state engine applies). A
// SolidColorBrush:
//   - IS a DependencyObject (so it is a valid ResourceValue::Object owned
//     resource, produced via ElementFactoryRegistry in real authoring),
//   - owns a REAL "Color" dependency property (registered globally, Color
//     type, default opaque black),
//   - exposes typed set_color()/color() + the standard Set/Get path on
//     PropertyId("Color").
//
// NO-GAP -> S12: the renderer maps a brush's Color to a Color4 fill (solid-rect
// path today, honest current-surface rendering). Real per-control materials
// are S400/S401. Recorded, not silent.

#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <vector>

namespace vaml {

// S460 (dep:S456) — BRUSH AUTHORING. A GRADIENT STOP: one authored
// {Color, Offset} pair inside a linear-gradient brush. `offset` is the
// fractional position along the gradient axis in [0,1] (the WPF
// GradientStop.Offset convention). Pure value data (NOT a DependencyObject —
// a stop is a sub-part of a brush, not an independently-named resource).
struct VAML_CORE_EXPORT GradientStop
{
    Color  color;
    double offset = 0.0;
};

// S460 — the AUTHORABLE `<GradientStop>` element: a DependencyObject carrying
// the authored Color + Offset DPs (the SolidColorBrush pattern). The reader
// builds each authored `<GradientStop Color= Offset=/>` into this object and
// folds it into the owning LinearGradientBrush's `std::vector<GradientStop>`
// value list. Registered under the element name "GradientStop".
class VAML_CORE_EXPORT GradientStopHost : public DependencyObject
{
public:
    GradientStopHost();
    ~GradientStopHost() override;

    // Color (Color DP, default opaque black) — the stop's color.
    [[nodiscard]] SetResult set_color(const vaml::Color& value);
    [[nodiscard]] vaml::Color color() const;

    // Offset (Double DP, default 0) — the stop's fractional position [0,1].
    [[nodiscard]] SetResult set_offset(double value);
    [[nodiscard]] double offset() const;
};

// S460 (dep:S456) — BRUSH AUTHORING: LinearGradientBrush.
//
// RESOLVED BY S465 (gradient render): the S460 MINIMAL REAL seed was KEPT +
// EXTENDED — the authored gradient data (stops + StartPoint/EndPoint) is REAL
// at S460, and S465 delivers the gradient RENDER via resolve_gradient_fill (the
// vertex-sampled tint over the solid-rect path). The sentinel is REMOVED (NO-GAP:
// the resolution is part of S465's own step, never a backfill).
//
// ADR-011 IS HELD (mirrors SolidColorBrush): LinearGradientBrush is CORE DATA —
// a DependencyObject owning its authored gradient stops + StartPoint/EndPoint.
// The vulkan/presentation layers consume the authored gradient AT RENDER TIME
// (S465 resolves this seed by emitting a gradient-tinted solid over the S206
// solid-rect path — vertex-sampled interpolation across the element box, NO new
// shader). The REAL core data delivered HERE: authored stops + Start/EndPoint +
// the color-resolution seam that proves a gradient reaches `resolve_foreground_`
// `color` by id. NO gradient RENDER at S460 — that is S465's obligation.
//
// A LinearGradientBrush is registered under the SAME "Brush:<id>" convention as
// SolidColorBrush (a scalar BrushId -> registered brush resource -> color), so
// `<Button.Background>` (the BrushId path) can consume an authored gradient.
class VAML_CORE_EXPORT LinearGradientBrush : public DependencyObject
{
public:
    LinearGradientBrush();
    ~LinearGradientBrush() override;

    // --- Authored gradient data (REAL at S460) --------------------------
    // The ordered gradient stops (first = offset 0 end, last = offset 1 end).
    void add_stop(GradientStop stop);
    void set_stops(std::vector<GradientStop> stops);
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept;

    // StartPoint / EndPoint — the gradient axis endpoints (normalized [0,1]
    // coordinates in element space; default the WPF-faithful diagonal 0,0 -> 1,1).
    void set_start_point(double x, double y);
    [[nodiscard]] double start_x() const noexcept;
    [[nodiscard]] double start_y() const noexcept;
    void set_end_point(double x, double y);
    [[nodiscard]] double end_x() const noexcept;
    [[nodiscard]] double end_y() const noexcept;

    // --- Color resolution seam (S460 REAL core) -------------------------
    // Resolves TWO gradient stops (start/end) into a single representative
    // opaque color for the scalar-foreground path (S465 upgrades this to the
    // full vertex-sampled gradient). Uses the FIRST stop when only one exists;
    // the LAST when only two; the average of the two endpoint stops otherwise.
    [[nodiscard]] Color representative_color() const noexcept;

private:
    std::vector<GradientStop> stops_;
    double startX_ = 0.0;
    double startY_ = 0.0;
    double endX_   = 1.0;
    double endY_   = 1.0;
};

class VAML_CORE_EXPORT SolidColorBrush : public DependencyObject
{
public:
    SolidColorBrush();
    // Self-registers the Color DP into the global table idempotently; the dtor
    // must be out-of-line (default) so the registration guard is complete here.
    ~SolidColorBrush() override;

    // --- Color (REAL DP; Color type, default opaque black) ---------------
    // Typed round-trip on the Color value path; also settable via
    // Set/Get(PropertyId("Color")).
    [[nodiscard]] SetResult set_color(const vaml::Color& value);
    [[nodiscard]] vaml::Color color() const;
};

} // namespace vaml
