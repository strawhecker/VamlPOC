// ============================================================================
// vaml_test_brush.h — the S643a WPF paint-DP compliance seam for tests.
//
// WPF ground truth (E:\GitRepos\wpf): Background/Foreground/BorderBrush are ALL
// `typeof(Brush)` (a brush OBJECT, one DP identity with AddOwner parent/child).
// VAML's render currency is the BrushId int that resolve_foreground_color walks
// through find_for_element("Brush:<id>") to the registered SolidColorBrush — the
// VAML mapping of WPF's Brush object. The pre-11 scalar-`Color` paint surface is
// untrustworthy and is brought into compliance here:
//   * WRITE a paint DP  : register_element_brush(el, color) -> BrushId on the DP
//   * READ a paint DP   : resolve_foreground_color(el, brushId) -> the Color
// These two helpers centralize that seam so every test/consumer site stays
// WPF-faithful and consistent (NO lazy scalar-Color writes anywhere).
// ============================================================================
#ifndef VAML_TEST_BRUSH_H
#define VAML_TEST_BRUSH_H

#include "vaml/core/Brush.h"
#include "vaml/core/Color.h"
#include "vaml/core/ResourceDictionary.h" // register_element_brush
#include "vaml/core/TextLayout.h"         // resolve_foreground_color
#include "vaml/core/UIElement.h"

namespace vaml::test {

// S643a — WRITE a paint DP WPF-faithfully: register the color as a fresh
// SolidColorBrush under "Brush:<autoId>" on the element and return the BrushId
// the caller stores on the paint DP (Background/Foreground/BorderBrush/Fill/...).
[[nodiscard]] inline std::int32_t register_brush(UIElement& el, const Color& color)
{
    return register_element_brush(el, color);
}

// S643a — READ a paint DP WPF-faithfully: given the element + the BrushId the
// paint DP holds, resolve it through the find_for_element walk to the registered
// SolidColorBrush's color. Mirrors the renderer's resolve_foreground_color.
[[nodiscard]] inline Color resolve_brush(UIElement& el, std::int32_t brushId)
{
    return resolve_foreground_color(el, brushId);
}

// S643b-1 (D1) — build a fresh SolidColorBrush shared_ptr holding `color`.
[[nodiscard]] inline std::shared_ptr<SolidColorBrush> make_brush(const Color& color)
{
    auto b = std::make_shared<SolidColorBrush>();
    (void)b->set_color(color);
    return b;
}

// S643b-1 (D1) — a Value::Brush object payload (fresh SolidColorBrush) for a
// paint DP written WPF-faithfully (the DP holds the Brush object, not an id).
[[nodiscard]] inline Value brush_value(const Color& color)
{
    return Value::Brush(make_brush(color));
}

} // namespace vaml::test

#endif // VAML_TEST_BRUSH_H
