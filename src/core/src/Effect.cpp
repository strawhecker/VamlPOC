// S511 — Effect out-of-line definitions: the validated setters + the pure
// shadow offset/color seams. See Effect.h for the WPF-faithful semantics.
//
// ADR-018: ZERO new locks (single-threaded UI-thread; the setters write plain
// fields). Validation rejects negative physical resources (Blur/SDepth/Radius)
// and out-of-[0,1] Effect.Opacity — the S025/S208 non-finite/out-of-range
// rejection precedent. Direct field storage (not DP-backed): the effect is a
// side-table payload (S145/S360/S500 discipline), not a registered dependency
// property.

#include "vaml/core/Effect.h"

#include <cmath>

namespace vaml {

// ---------------------------------------------------------------------------
// DropShadowEffect
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
SetResult DropShadowEffect::set_color(const vaml::Color& c) noexcept
{
    color_ = c;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Color DropShadowEffect::color() const noexcept
{
    return color_;
}

VAML_CORE_EXPORT
SetResult DropShadowEffect::set_blur_radius(double r) noexcept
{
    if (!std::isfinite(r) || r < 0.0)
    {
        return SetResult::REJECTED; // negative/non-finite physical radius is malformed
    }
    blurRadius_ = r;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
double DropShadowEffect::blur_radius() const noexcept
{
    return blurRadius_;
}

VAML_CORE_EXPORT
SetResult DropShadowEffect::set_shadow_depth(double d) noexcept
{
    if (!std::isfinite(d) || d < 0.0)
    {
        return SetResult::REJECTED;
    }
    shadowDepth_ = d;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
double DropShadowEffect::shadow_depth() const noexcept
{
    return shadowDepth_;
}

VAML_CORE_EXPORT
SetResult DropShadowEffect::set_direction(double deg) noexcept
{
    if (!std::isfinite(deg))
    {
        return SetResult::REJECTED;
    }
    direction_ = deg;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
double DropShadowEffect::direction() const noexcept
{
    return direction_;
}

VAML_CORE_EXPORT
SetResult DropShadowEffect::set_opacity(double o) noexcept
{
    if (!std::isfinite(o) || o < 0.0 || o > 1.0)
    {
        return SetResult::REJECTED;
    }
    opacity_ = o;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
double DropShadowEffect::opacity() const noexcept
{
    return opacity_;
}

// ---------------------------------------------------------------------------
// BlurEffect
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
SetResult BlurEffect::set_radius(double r) noexcept
{
    if (!std::isfinite(r) || r < 0.0)
    {
        return SetResult::REJECTED;
    }
    radius_ = r;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
double BlurEffect::radius() const noexcept
{
    return radius_;
}

// ---------------------------------------------------------------------------
// Pure seams
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void drop_shadow_offset(double directionDeg, double depth,
                        double& outDx, double& outDy) noexcept
{
    // WPF-faithful: "Direction" is a compass angle in degrees (0-360) FROM
    // which the light shines; the shadow is cast AWAY from it. WPF's default
    // Direction=315 casts the shadow toward the LOWER-RIGHT (+x,+y in the
    // down-positive screen convention). The convention:
    //   (dx, dy) = ( -sin(θ), cos(θ) ) * depth
    // Direction 0 -> straight down (+y); 90 -> left (-x); 315 -> lower-right.
    const double rad = directionDeg * (3.14159265358979323846 / 180.0);
    outDx = -std::sin(rad) * depth;
    outDy =  std::cos(rad) * depth;
}

VAML_CORE_EXPORT
vaml::Color drop_shadow_color(const vaml::Color& color, double opacity) noexcept
{
    const double a = color.a * (opacity < 0.0 ? 0.0 : (opacity > 1.0 ? 1.0 : opacity));
    const int ia = static_cast<int>(a + 0.5);
    vaml::Color c = color;
    c.a = static_cast<std::uint8_t>(ia < 0 ? 0 : (ia > 255 ? 255 : ia));
    return c;
}

} // namespace vaml
