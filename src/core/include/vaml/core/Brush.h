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

namespace vaml {

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
