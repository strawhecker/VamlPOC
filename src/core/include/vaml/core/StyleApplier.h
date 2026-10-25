#pragma once
// VAML StyleApplier — runtime application of Style to typed elements.
// S363 (dep:S362) — Sprint 9.
//
// Takes a real `Style` (S362) and a real `UIElement` and:
//   1. RESOLVES + APPLIES the style's setters onto the element's DPs, walking
//      the BasedOn chain BASE-FIRST (a base style's setters land first, the
//      derived style's overrides land after — child-overrides-base), then the
//      trigger setters.
//   2. Honors LOCAL-WINS: an element with an AUTHORED local value for a
//      property is NOT clobbered by a style setter for that property
//      (WPF-faithful).
//   3. ADVANCED setter kinds:
//        - StaticResource/DynamicResource values resolve through the S360/S361
//          three-scope lookup (a Static setter is applied once; a Dynamic
//          setter registers a DynamicResourceLink so S361 reapply can re-push).
//        - TemplateBinding values are resolved against the templated control's
//          DP by S365 (not here — recorded NO-GAP, not silent).
//   4. TRIGGERS: PropertyTrigger fires when the styled element's DP equals its
//      match value; DataTrigger fires when the S240 path resolved against the
//      data context equals its match value. Applying a style SUBSCRIBES the
//      element's DP (for Property triggers) so a later change re-fires /
//      un-fires (S041 add_property_changed). True -> apply trigger setters;
//      on un-fire -> re-apply the style base setters.
//
// IMPLICIT STYLE (WPF by-target-type): `find_implicit_style(element)` resolves
// the Style whose target_type() equals the element's TYPE NAME from the
// element's own Resources / the parent chain / app / system (the S360 walk).
// `apply_implicit_style(element)` applies it if found.
//
// SEAMS (headless):
//   - style_valid gate (S362), setter_kind discrimination (S362), trigger_kind
//     (S362), resolved-value application to a DP.
//
// ADR-018: ZERO new locks (single-threaded UI use; S041 subscribers = this
// object's existing DP subscriptions).

#include "vaml/core/ResourceResolver.h" // DynamicResourceLink (dynamic setter re-push)
#include "vaml/core/Style.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vaml {

// ONE resolved property-write the applier produced (allocated to DP id +
// resolved value). Used to surface what a style application landed on.
struct StyleApply
{
    PropertyId  property;
    Value       value = Value::Null();
    bool        applied = false;  // true when the Set actually wrote a local
};

class VAML_CORE_EXPORT StyleApplier
{
public:
    // Applies `style` to `element`. Honors local-wins; walks BasedOn base-first;
    // applies trigger setters for currently-firing triggers; records dynamic
    // resource links (for S361 re-push). Returns the resolved + applied
    // property writes (for tests/introspection).
    [[nodiscard]] std::vector<StyleApply>
    apply_style(UIElement& element, const Style& style,
                std::vector<DynamicResourceLink>& dynamicLinks);

    // Re-applies the style BASE setters (used to revert a fired trigger's
    // overrides). Does NOT re-add trigger setters.
    [[nodiscard]] std::vector<StyleApply>
    apply_style_base(UIElement& element, const Style& style,
                     std::vector<DynamicResourceLink>& dynamicLinks);
};

// Finds the implicit Style (WPF target-type implicit style) for `element` by
// resolving the Style whose target_type() == the element's type name through
// the element's Resources -> parent chain -> app -> system. Returns nullptr
// when none. NOTE: type-name lookup uses a dedicated resource KEY of the form
// "Style:{typeName}" (the reader registers an element's implicit style under
// that key in the appropriate dictionary). Never throws.
[[nodiscard]] VAML_CORE_EXPORT const Style* find_implicit_style(const UIElement& element) noexcept;

// Applies the implicit style for `element` (via find_implicit_style +
// apply_style) if one exists. Returns the number of applied style setters
// (0 when no implicit style).
[[nodiscard]] VAML_CORE_EXPORT std::size_t
apply_implicit_style(UIElement& element, std::vector<DynamicResourceLink>& dynamicLinks);

} // namespace vaml
