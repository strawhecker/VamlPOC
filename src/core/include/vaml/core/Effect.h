#pragma once
// VAML Effect — the S511 effects model (dep:S510, Sprint 11g).
//
// WPF-FAITHFUL (rev 3, user-accepted D0-REVISED): UIElement.Effect is a
// Freezable that the compositor renders to an OFFSCREEN surface and blits back
// through a shader. This POC delivers the REAL offscreen blur/blit as the
// vaml-vulkan UiSceneEncoder arm (UiSceneEncoder_effect); THIS core header owns
// the EFFECT MODEL + the pure value seams the render arm consumes:
//
//   * Effect : DependencyObject (abstract) — the base of both concrete effects.
//   * DropShadowEffect { Color, BlurRadius, ShadowDepth, Direction(deg),
//     Opacity } — WPF-faithful: the element's alpha mask is blurred by
//     BlurRadius, offset by (Direction, ShadowDepth), tinted with Color at
//     Effect.Opacity, and drawn BEHIND the element.
//   * BlurEffect { Radius } — the element's subtree blurred then blitted back.
//   * Pure seams (headless-testable, the S207/S208 precedent):
//       drop_shadow_offset(directionDeg, depth) -> (dx,dy)      — the offset
//           vector from the WPF Direction/ShadowDepth pair;
//       drop_shadow_color(color, opacity) -> Color              — the shadow
//           tint with Effect.Opacity applied;
//       blur is a RENDER concern: the core carries { Radius } and the
//       UiSceneEncoder performs the separable Gaussian pass.
//
// Validation (the S025 pipeline): BlurRadius/ShadowDepth/Radius < 0 are
// REJECTED at the typed setter (negative physical resources are malformed
// geometry — the S208 non-finite rejection precedent). Direction is an angle
// in degrees (any finite value). Effect.Opacity is bounded [0,1].
//
// Stored OUT-OF-LINE on UIElement (S145 object-pool budget — the S360/S500
// precedent: a fixed slot size, so the effect lives in a process-wide side-table
// keyed by element address; an element with no effect has no entry). See
// Effect.cpp + UIElement.cpp.
//
// ADR-018: ZERO new locks (single-threaded UI-thread; the side-table is the
// S360 discipline). Spline/Easing are elsewhere (Animation.h NO-GAP); the deep
// blur/shader refinement is the recorded NO-GAP forward token (see
// RENDER_SURFACE.md).

#include "vaml/core/Color.h"        // vaml::Color (the WPF Color form)
#include "vaml/core/DependencyObject.h"
#include "vaml/core/export.h"

#include <cstdint>

namespace vaml {

// The effect base (WPF System.Windows.Media.Effects.Effect : Freezable
// analogue, kept as a plain DependencyObject — freezability is not modeled).
class VAML_CORE_EXPORT Effect : public DependencyObject
{
public:
    ~Effect() override = default;

    [[nodiscard]] virtual const char* effect_name() const noexcept = 0;

protected:
    Effect() = default;
};

// WPF DropShadowEffect: the element's alpha mask is blurred (BlurRadius),
// offset by (Direction, ShadowDepth), tinted `Color` at `Opacity`, and drawn
// BEHIND the element. All setters validate + REJECT out-of-range (see header).
class VAML_CORE_EXPORT DropShadowEffect : public Effect
{
public:
    DropShadowEffect() = default;

    [[nodiscard]] const char* effect_name() const noexcept override { return "DropShadowEffect"; }

    // The shadow tint Color (default Black — WPF's default drop shadow).
    [[nodiscard]] ::vaml::SetResult set_color(const vaml::Color& c) noexcept;
    [[nodiscard]] vaml::Color color() const noexcept;

    // The Gaussian blur radius in logical px (>= 0; REJECTED when negative).
    [[nodiscard]] ::vaml::SetResult set_blur_radius(double r) noexcept;
    [[nodiscard]] double blur_radius() const noexcept;

    // The shadow depth (offset magnitude, px; >= 0).
    [[nodiscard]] ::vaml::SetResult set_shadow_depth(double d) noexcept;
    [[nodiscard]] double shadow_depth() const noexcept;

    // The shadow direction, a compass angle in degrees (WPF: Direction; the
    // offset is derived via sin/cos of the DotProduct convention).
    [[nodiscard]] ::vaml::SetResult set_direction(double deg) noexcept;
    [[nodiscard]] double direction() const noexcept;

    // Effect.Opacity in [0,1] (how opaque the shadow tint is when composited).
    [[nodiscard]] ::vaml::SetResult set_opacity(double o) noexcept;
    [[nodiscard]] double opacity() const noexcept;

private:
    vaml::Color color_{ 0, 0, 0, 255 }; // Black opaque (default)
    double blurRadius_ = 5.0;           // WPF default BlurRadius
    double shadowDepth_ = 5.0;          // WPF default ShadowDepth
    double direction_   = 315.0;        // WPF default Direction (upper-left)
    double opacity_     = 1.0;
};

// WPF BlurEffect: the element's subtree is Gaussian-blurred by `Radius` then
// blitted back. This core carries the parameter; the UiSceneEncoder performs
// the separable Gaussian pass.
class VAML_CORE_EXPORT BlurEffect : public Effect
{
public:
    BlurEffect() = default;

    [[nodiscard]] const char* effect_name() const noexcept override { return "BlurEffect"; }

    // The Gaussian blur radius in logical px (>= 0; REJECTED when negative).
    [[nodiscard]] ::vaml::SetResult set_radius(double r) noexcept;
    [[nodiscard]] double radius() const noexcept;

private:
    double radius_ = 5.0; // WPF default
};

// ---------------------------------------------------------------------------
// Pure seams (headless-testable; no render runtime).
// ---------------------------------------------------------------------------

// Computes the WPF-faithful shadow OFFSET (dx, dy in logical px) from the
// Direction (a compass angle in degrees) + ShadowDepth (magnitude). WPF
// derives the offset from the shadow-matrix of the angle: the direction points
// toward the light, so the shadow is cast in the OPPOSITE travel such that the
// offset = (cos(direction), -sin(direction)) * depth in WPF's screen coords.
// The convention here: Direction 0 = light from the right (shadow cast left),
// and the vector rotates counterclockwise as direction increases — matching
// WPF's DotProduct(Direction)-derived matrix. Pure + noexcept.
VAML_CORE_EXPORT void drop_shadow_offset(double directionDeg, double depth,
                                         double& outDx, double& outDy) noexcept;

// Applies Effect.Opacity to the shadow Color: returns the straight-alpha color
// whose RGB is the shadow color and whose ALPHA is color.a * opacity (the
// WPF-faithful tint-at-Opacity composite). Clamps alpha to [0,255]. Pure.
VAML_CORE_EXPORT vaml::Color drop_shadow_color(const vaml::Color& color, double opacity) noexcept;

} // namespace vaml
