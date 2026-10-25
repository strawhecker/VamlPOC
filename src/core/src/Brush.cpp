// S364 — SolidColorBrush definitions. See Brush.h.

#include "vaml/core/Brush.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"

namespace vaml {

namespace {

// Registers the SolidColorBrush "Color" DP once per process (Meyers-static
// guard). Color type, not inherited, default opaque black.
void register_brush_properties()
{
    (void)DependencyProperty::Register(
        "Color", ValueType::Color, "SolidColorBrush",
        PropertyMetadata::WithDefault(Value::Color(vaml::Color{ 0, 0, 0, 255 }),
                                      PropertyFlags::None));
}

const bool kBrushRegistered = [] {
    register_brush_properties();
    return true;
}();

} // namespace

VAML_CORE_EXPORT
SolidColorBrush::SolidColorBrush()
{
    (void)kBrushRegistered; // ensure the global Color DP is registered
}

VAML_CORE_EXPORT
SolidColorBrush::~SolidColorBrush() = default;

VAML_CORE_EXPORT
SetResult SolidColorBrush::set_color(const vaml::Color& value)
{
    return Set(PropertyId::of("Color"), Value::Color(value));
}

VAML_CORE_EXPORT
vaml::Color SolidColorBrush::color() const
{
    const Value v = Get(PropertyId::of("Color"));
    return v.type() == ValueType::Color ? v.get<vaml::Color>() : vaml::Color{ 0, 0, 0, 255 };
}

} // namespace vaml
