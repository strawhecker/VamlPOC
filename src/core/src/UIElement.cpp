// S120 — UIElement out-of-line definitions + self-registration.
//
// Visibility + Opacity are REAL registered dependency properties usable
// through the full S1 property machinery (S043 attached-aware globals:
// an object that does NOT locally register them still resolves the global
// descriptor). Self-registration mirrors register_builtin_elements(): the
// first UIElement construction registers both DPs into the global
// DependencyProperty table once, idempotently.
//
// The transform-owner + layout slots are SENTINEL-ONLY (see UIElement.h) —
// no member, no fabricated geometry type; each is resolved at its named
// sprint (S208 / S122).

#include "vaml/core/UIElement.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyFlags.h"

#include <utility>

namespace vaml {

namespace {

// Registers the two REAL S120 DPs into the global table exactly once per
// process (Meyers-static guard; a second call is a no-op). Visibility and
// Opacity are GLOBALLY registered so they resolve on ANY UIElement — or on
// any DependencyObject via S043 attached-aware resolution — without per-
// object registration.
void register_uielement_properties()
{
    // Visibility: default Visible, NOT inherited (per-element render state).
    // The Value payload rides Int32 (no enum tag in S030).
    (void)DependencyProperty::Register(
        "Visibility", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Opacity: default 1.0, INHERITED (S040/S041 cascade to descendants),
    // bounded [0,1] via validate (out-of-range Set -> REJECTED, unchanged).
    (void)DependencyProperty::Register(
        "Opacity", ValueType::Double, "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::Inherits))
            .with_validate([](const Value& v) {
                const double x = v.get<double>();
                return x >= 0.0 && x <= 1.0;
            }));
}

// Runs register_uielement_properties() at most once (first UIElement ctor).
// Function-local static guard => no static-init-order hazard (the underlying
// global tables are themselves function-local Meyers statics).
void ensure_uielement_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_uielement_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
UIElement::UIElement()
{
    ensure_uielement_properties_registered();
}

VAML_CORE_EXPORT
SetResult UIElement::set_visibility(Visibility v)
{
    return Set(PropertyId::of("Visibility"), Value::Int32(static_cast<std::int32_t>(v)));
}

VAML_CORE_EXPORT
Visibility UIElement::visibility() const
{
    return static_cast<Visibility>(Get(PropertyId::of("Visibility")).get<std::int32_t>());
}

VAML_CORE_EXPORT
SetResult UIElement::set_opacity(double value)
{
    return Set(PropertyId::of("Opacity"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::opacity() const
{
    return Get(PropertyId::of("Opacity")).get<double>();
}

} // namespace vaml
