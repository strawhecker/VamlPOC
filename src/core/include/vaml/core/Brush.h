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
#include "vaml/core/Freezable.h" // S643b-4 (D3): Brush : Freezable (WPF-true base)
#include "vaml/core/Value.h"

#include <cstdint>
#include <vector>

namespace vaml {

// S643b-1 (closes D1) + S643b-4 (D3) — the WPF-faithful BRUSH base. WPF's
// `Brush` (Brush.cs) is `Brush : Freezable : DependencyObject` — the abstract
// common base of every concrete brush (SolidColorBrush, LinearGradientBrush,
// ImageBrush, DrawingBrush...). VAML's paint DPs now hold a
// `std::shared_ptr<Brush>` — the "the DP holds the brush OBJECT" read
// (Control.cs:108-124 `(Brush) GetValue(BackgroundProperty)`) — so every brush
// derives from this base. It is a Freezable (WPF-true), so a brush can be
// frozen + shared (D3). ADR-011 HELD: a Brush is CORE DATA (a DependencyObject),
// consumed at render time, never the reverse. It carries the common color-
// resolution surface the renderer uses (see color()).
class VAML_CORE_EXPORT Brush : public Freezable
{
public:
    ~Brush() override;

    // The brush's representative opaque color (the render seam). SolidColorBrush
    // returns its Color; LinearGradientBrush returns its representative_color;
    // a non-color brush (ImageBrush/DrawingBrush) returns opaque black via the
    // base (the S465/S649 material surfaces upgrade the real read, recorded
    // NO-GAP not silent). Pure + headless-testable.
    [[nodiscard]] virtual Color color() const = 0;

protected:
    Brush() = default;
};

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
// S643b-1: derives the shared `Brush` base (object paint payload).
class VAML_CORE_EXPORT LinearGradientBrush : public Brush
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

    // Brush::color override — the representative color (the scalar render seam).
    [[nodiscard]] Color color() const override { return representative_color(); }

private:
    std::vector<GradientStop> stops_;
    double startX_ = 0.0;
    double startY_ = 0.0;
    double endX_   = 1.0;
    double endY_   = 1.0;
};

class VAML_CORE_EXPORT SolidColorBrush : public Brush
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
    [[nodiscard]] vaml::Color color() const override;
};

// S643b-4 (D3 — SHARED FROZEN, no waiver) — the WPF `IFreezeFreezables.
// TryGetFreezable` analogue (Brush.cs:55-77): the SAME color literal resolves to
// a SINGLE SHARED, FROZEN SolidColorBrush instance (WPF shares identical frozen
// brushes). `shared_frozen_solid_brush(color)` returns a process-lifetime cached
// frozen brush, creating + caching the first time a given color is requested. A
// frozen SolidColorBrush is immutable (writes throw) — the safe shared read the
// renderer uses. ADR-018 zero new locks (single-threaded cache map / Meyer's).
[[nodiscard]] VAML_CORE_EXPORT std::shared_ptr<SolidColorBrush>
shared_frozen_solid_brush(const vaml::Color& color);

// S643b-4 — freezes `brush` (WPF Freeze); a frozen brush is immutable. Used by
// the shared pool; a caller may also freeze a brush it owns. Freeze must be
// callable on the affinity thread (ADR-018 single-threaded affinity).
VAML_CORE_EXPORT void freeze_solid_brush(SolidColorBrush& brush);

// S643b-1 — the shared default paint brush (opaque black — the WPF default
// foreground/background/... brush default). A single shared instance (WPF's
// default values are shared objects). The renderer reads its color() directly.
[[nodiscard]] VAML_CORE_EXPORT const std::shared_ptr<SolidColorBrush>&
default_black_brush() noexcept;

// S643b-2 (D2 confinement) — the auto brush id BASE. Auto-ids (from a COLOR
// literal or a Style/visual-state brush) start at this HIGH base so they can
// never collide with app-registered small ids. Declared here (Brush.h) as the
// single source; VamlReader.h / VBamlReader.h reference it (the S7 reader's own
// legacy mirror is removed).
inline constexpr std::int32_t kAutoBrushIdBase = 1000000;

// S643b-2 (D2 confinement) — the SINGLE SOURCE OF TRUTH for the auto brush id.
// A process-lifetime monotonic atomic allocator (seeded at kAutoBrushIdBase, so
// it can never collide with app-registered small ids). EVERY consumer — the S2
// text reader + S7 compiled reader auto-id path and register_element_brush
// (Style setter / visual-state) — draws from THIS one allocator, so ids are
// unique across the whole process, NOT reset per Load. This declares the seam
// divergence explicitly (D2): the ids are a monotonic namespace, never a dedup
// map. ADR-018 zero new locks (a single std::atomic<std::int32_t> fetch_add).
[[nodiscard]] VAML_CORE_EXPORT std::int32_t next_auto_brush_id() noexcept;

} // namespace vaml
