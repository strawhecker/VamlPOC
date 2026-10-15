#pragma once
// VAML UIElement — the base UI class for the element/scene-graph tree.
// S120 (dep:S097, Sprint 3 — element/scene graph + measure/arrange layout).
//
// S120 title contract (four concern areas, each treated explicitly):
//   1. visibility  — a REAL registered dependency property (this step).
//   2. opacity     — a REAL registered dependency property (this step),
//      inherited so a parent's opacity cascades to descendants (S040/S041).
//   3. transform owner slot — a MINIMAL REAL SENTINEL ONLY (comment-level;
//      no member, no DP, no fabricated geometry type). UIElement is designated
//      the transform OWNER. This is NOT lazy — NO-GAP is satisfied because the
//      sentinel names an explicit, scheduled resolution step (S208, renamed
//      to include RenderTransform) that introduces the real member/type and
//      performs per-element transforms at the renderer. Deliberately deferred:
//      no Transform/Matrix type exists in core, and no consumer exists until
//      S208 — building a transform DP now would invent an API against zero
//      consumers (heuristic: don't commit detailed rules before behavior is
//      known). See docs/roadmap.md S208.
//   4. layout slot — a MINIMAL REAL SENTINEL ONLY (comment-level). The real
//      layout geometry (LayoutSlot / DesiredSize / final rect / Margin /
//      alignment) is authored at S122, which explicitly owns it. S120 only
//      designates UIElement as the layout host; no member is fabricated here.
//
// WHY REAL DPs FOR VISIBILITY + OPACITY (and not slots): both are WPF-faithful
// per-element render state that a real consumer honors at the renderer
// (S208 honors Opacity + RenderTransform; Visibility is consumed by layout +
// hit-testing sprints). They are testable NOW through the S1 dependency-
// property machinery (local ?? inherited ?? default, validate, coercion,
// coalescing, descendant push). Opacity's inheritance is REAL behavior we can
// verify end-to-end via the existing S040/S041 push pipeline — not a blind
// "WPF does it this way" copy.
//
// SELF-REGISTRATION: UIElement's constructor lazily registers the two REAL DPs
// (Visibility, Opacity) into the global DependencyProperty table, idempotently
// (Meyers-static guard — first construction does the work; later ones are a
// no-op), mirroring register_builtin_elements(). Any UIElement instance makes
// both properties globally resolvable (S043 attached-aware read/write).
//
// SCOPE BOUNDARY (recorded, NOT silent):
//   - Visual children list / create-attach-detach + walk = S121 (distinct from
//     the DependencyObject inheritance-scope; composes via set_parent).
//   - Layout geometry from the "layout slot" = S122.
//   - Transform API + per-element Model matrix = S208.
//   - The existing MINIMAL REAL control seeds (Button/StackPanel/...) are NOT
//     re-based onto UIElement here — their own sentinels forbid silent
//     extension; each re-bases at its own resolution sprint (StackPanel->S129,
//     Grid->S131, etc.). S121 builds/tests the visual tree on UIElement
//     directly.

#include "vaml/core/DependencyObject.h"

#include "vaml/core/PropertyId.h" // PropertyId (Set/Get key)
#include "vaml/core/Value.h"      // Value, ValueType

#include <cstdint>

namespace vaml {

// WPF-faithful visibility states, stored on the Visibility DP via the Int32
// value path (Value has no enum tag in S030 — the enum rides Int32).
enum class Visibility : std::uint8_t
{
    Visible   = 0, // rendered and hit-testable
    Hidden    = 1, // not rendered, but still occupies layout space
    Collapsed = 2, // neither rendered nor laid out
};

// [MINIMAL REAL] UIElement layout slot — RESOLVED BY S122 (Sprint 3). Do NOT extend silently; do NOT backfill.
// [MINIMAL REAL] UIElement transform owner — RESOLVED BY S208 (Sprint 5, renamed to include RenderTransform). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT UIElement : public DependencyObject
{
public:
    UIElement();

    // --- Visibility (REAL DP; not inherited) ----------------------------
    // Default Visible. Per-element render state (NOT inherited — WPF-faithful).
    // Exposed both via the typed accessors and the standard Set/Get path on
    // PropertyId("Visibility").
    [[nodiscard]] SetResult set_visibility(Visibility v);
    [[nodiscard]] Visibility visibility() const;

    // --- Opacity (REAL DP; inherited, validated [0,1]) ------------------
    // Default 1.0. Inherited (cascades to descendants) + bounded [0,1]
    // (out-of-range Set is REJECTED, store unchanged). A parent's opacity
    // flows to children; a child's own set wins locally. Consumed by S208.
    [[nodiscard]] SetResult set_opacity(double value);
    [[nodiscard]] double opacity() const;
};

} // namespace vaml
