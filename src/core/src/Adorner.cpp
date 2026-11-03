// VAML Adorner — the WPF-faithful abstract adorner base (11n-6).
//
// Adorner : UIElement decorates a NON-OWNING AdornedElement, measures to the
// adorned element's extent by default, and exposes the WPF GetDesiredTransform
// seam + the IsClipEnabled DP. ADR-018: ZERO new locks (single affinity
// thread; the DP registration is a Meyers-static guard).

#include "vaml/core/Adorner.h"

#include "vaml/core/DependencyProperty.h" // DependencyProperty::Register
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"

namespace vaml {

namespace {
// Registers the Adorner.IsClipEnabled Bool-via-Int32 DP (default FALSE, not
// inherited) exactly once per process (Meyers-static guard).
void register_adorner_properties()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "Adorner.IsClipEnabled", ValueType::Int32, "Adorner",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
Adorner::Adorner(UIElement* adornedElement) noexcept
    : adorned_(adornedElement)
{
    register_adorner_properties();
}

VAML_CORE_EXPORT
vaml::Matrix2D Adorner::GetDesiredTransform(const vaml::Matrix2D& offered) const
{
    // The WPF passthrough default: an adorner renders exactly at the transform
    // the layer offers (the adorned element's composed Model). A derived
    // adorner overrides to filter/adjust (e.g. an offset for an overhang).
    return offered;
}

VAML_CORE_EXPORT
SetResult Adorner::set_is_clip_enabled(bool value)
{
    return Set(PropertyId::of("Adorner.IsClipEnabled"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool Adorner::is_clip_enabled() const
{
    return Get(PropertyId::of("Adorner.IsClipEnabled")).get<int>() != 0;
}

VAML_CORE_EXPORT
vaml::Size Adorner::MeasureOverride(const vaml::Size& /*available*/) const
{
    // The WPF adorner measure default: measure to the ADORNED element's extent
    // (an overlay matches its decorated box). Null / un-arranged adorned ->
    // a zero box.
    if (!adorned_)
        return vaml::Size{ 0.0f, 0.0f };
    const vaml::Rect r = adorned_->final_rect();
    return vaml::Size{ r.width, r.height };
}

} // namespace vaml
