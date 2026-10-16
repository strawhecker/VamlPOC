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
//   4. layout slot — RESOLVED BY S122. The real layout geometry (LayoutSlot /
//      DesiredSize / final rect / Margin / alignment) is authored at S122,
//      which owns it: UIElement exposes DesiredSize + final rect (S123/S124
//      write them) and the Margin/HorizontalAlignment/VerticalAlignment DPs,
//      and the alignment algorithm lives in LayoutSlot.h. The layout-slot
//      SENTINEL introduced by S120 is REMOVED here per its resolution contract
//      (resolution at the feature's own step, not a backfill).
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

// S121 (dep:S120) — visual children (create/attach/detach), parent traversal,
// visual-tree walk. SCOPE = structural children + traversal ONLY (the S121
// title). Layout geometry (LayoutSlot/DesiredSize/final rect/Margin/alignment)
// is S122's explicit step; Measure/Arrange is S123+; none of that is fabricated
// here. The visual children list is DISTINCT from the S041 inheritance-scope:
// attaching a child also calls set_parent(this) so the S039/S041 parent link +
// inverse inheritance-scope edge stay coherent (Opacity's cascade flows through
// the visual child via that link — proven by test). A parent OWNS its children
// (unique_ptr), resolving the S091/S120-recorded ownership-transfer note.

#include "vaml/core/DependencyObject.h"

#include "vaml/core/PropertyId.h" // PropertyId (Set/Get key)
#include "vaml/core/Value.h"      // Value, ValueType

#include "vaml/core/Geometry.h"     // Size, Rect
#include "vaml/core/LayoutSlot.h"   // HorizontalAlignment, VerticalAlignment
#include "vaml/core/Thickness.h"    // Thickness (Margin DP)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace vaml {

// WPF-faithful visibility states, stored on the Visibility DP via the Int32
// value path (Value has no enum tag in S030 — the enum rides Int32).
enum class Visibility : std::uint8_t
{
    Visible   = 0, // rendered and hit-testable
    Hidden    = 1, // not rendered, but still occupies layout space
    Collapsed = 2, // neither rendered nor laid out
};

// [MINIMAL REAL] UIElement transform owner — RESOLVED BY S208 (Sprint 5, renamed to include RenderTransform). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT UIElement : public DependencyObject
{
public:
    UIElement();
    // S121: the pimpl owns the visual children (unique_ptr<UIElement>), so the
    // destructor must be out-of-line (default) for the Impl to be complete here.
    ~UIElement() override;

    // --- Layout slot (RESOLVED BY S122) ------------------------------------
    // DesiredSize + final rect are the S123/S124 WRITE targets, exposed
    // read-side now so the layout contract is visible. desired_size() is the
    // field S123's Measure writes (default 0x0); final_rect() is the field
    // S124's Arrange writes (default 0). Margin + Horizontal/VerticalAlignment
    // are REAL registered DPs (S043 attached-aware), defaulting to 0/Stretch.

    // The child's measured DesiredSize (written by Measure at S123). Default
    // 0x0 until the first Measure pass.
    [[nodiscard]] const vaml::Size& desired_size() const noexcept;

    // The child's arranged final rect (written by Arrange at S124). Default 0
    // until the first Arrange pass.
    [[nodiscard]] const vaml::Rect& final_rect() const noexcept;

    // --- Measure contract (S123) -------------------------------------------
    // Measure(available) -> DesiredSize, infinity-aware stretch-vs-wrap.
    // The S122-desired_size field is the WRITE target; the computed DesiredSize
    // is exposed via desired_size() immediately after a Measure pass. `available`
    // is the bounded or unbounded (Size::Infinite()) bound a parent offers; an
    // unbounded axis never stretches (the DesiredSize WRAPS to content) and
    // Size::Infinite() never escapes into a DesiredSize (always finite).
    //
    // RESOLUTION SCRIPT (recorded, NO-GAP — not silent):
    //   - S125 filled the protected measure_natural_size() override on the Text
    //     leaf + panels (the content-extent seam this contract called first).
    //   - S127 (RESOLVED) upgraded measure_natural_size into the canonical
    //     MeasureOverride/ArrangeOverride virtual pair (a NAMED upgrade at the
    //     pair's own step, not silent churn). measure_natural_size no longer
    //     exists; MeasureOverride is the seam Measure now calls. Do NOT extend
    //     this seam silently.
    void Measure(const vaml::Size& available);

    // The element's un-finalized CONTENT extent for a given available bound
    // (the S127 MeasureOverride seam). Base UIElement has no content -> {0,0};
    // S129/S131 panels and S400 Text override it. Protected so only the layout
    // machinery invokes it; Measure() is the public entry that calls it.
    [[nodiscard]] virtual vaml::Size MeasureOverride(const vaml::Size& available) const;

    // --- Arrange contract (S124) -------------------------------------------
    // Arrange(arrangeSlot) -> final_rect(). The write-side pair to Measure:
    // consumes the S122 compute_final_rect(slot, desired, margin, hAlign,
    // vAlign) with the element's own desired_size() (written by S123's
    // Measure), margin(), horizontal_alignment(), vertical_alignment(); writes
    // the S122 final_rect() field. Repeated Arrange with a new slot RECOMPUTES
    // (recompute, never cumulate — same discipline as Measure/S123).
    //
    // RENDER SLOT == CLIP BOUNDS: final_rect() IS the render slot where the
    // renderer (S208) draws the element, AND the extent that clip honors —
    // content overflowing final_rect() is not rendered. The rectangular clip
    // op the renderer + ScrollViewer (S405) consume is vaml::clip_to_bounds
    // (LayoutSlot.h); the base content (empty) fits any slot, so a base
    // UIElement needs no override clip (see the Arrange test for the slot
    // equality). S127 (RESOLVED) added the ArrangeOverride virtual here, so
    // Arrange now auto-invokes a panel's ArrangeOverride; the arrange-subgraph
    // auto-drive (LayoutManager -> root Arrange -> panel ArrangeOverride ->
    // children arranged) completes on this seam. Do NOT extend this seam
    // silently.
    //
    // RESOLUTION SCRIPT (recorded, NO-GAP — not silent):
    //   - S125 called Arrange on children after measuring them (panels arrange
    //     each child into its arranged slot).
    //   - S126 (LayoutManager) drives the Measure+Arrange passes over the dirty
    //     subgraph.
    //   - S127 (RESOLVED) upgraded measure_natural_size into the canonical
    //     MeasureOverride/ArrangeOverride virtual pair (a NAMED upgrade at the
    //     pair's own step, not silent churn). ArrangeScope: Arrange now
    //     auto-invokes this element's ArrangeOverride(final_rect).
    //   - S129 (StackPanel) + S131 (Grid) adopt MeasureOverride/ArrangeOverride
    //     for their real panel algorithms.
    //   - S208 (renderer) consumes final_rect() as the render slot + applies
    //     clip_to_bounds; S405 (ScrollViewer) clips its viewport via
    //     clip_to_bounds.
    void Arrange(const vaml::Rect& arrangeSlot);

    // The element's arrange-side virtual (S127). Receives the element's FULL
    // final render slot (final_rect(), computed by Arrange: margin + alignment
    // against the arrangeSlot) so a panel lays out its children into its own
    // render slot. Default no-op (a base leaf has nothing to place); panels
    // (S129/S131) override it. Protected so only the layout machinery invokes
    // it; Arrange() is the public entry that calls it after computing
    // final_rect(). This is the seam that completes the arrange-subgraph
    // auto-drive (LayoutManager -> root Arrange -> panel ArrangeOverride ->
    // children arranged).
    virtual void ArrangeOverride(const vaml::Rect& finalSlot);

    // --- Layout invalidation (S126) ----------------------------------------
    // WPF-faithful dirty flags: each element knows whether it needs a Measure
    // pass (measureDirty) and/or an Arrange pass (arrangeDirty). A fresh
    // element starts dirty on BOTH axes (never laid out -> measured/arranged
    // on its first pass). InvalidateMeasure/InvalidateArrange SET the local
    // flag (short-circuited when already set, so a redundant invalidation does
    // NOT re-walk the chain) and PROPAGATE it up the parent chain: a parent's
    // DesiredSize is a function of its children, so a child invalidation
    // dirties every ancestor to the root. The S126 LayoutManager consumes these
    // gates (UpdateLayout runs a measure pass iff the root is measure-dirty, an
    // arrange pass iff the root is arrange-dirty) so a clean UpdateLayout is a
    // true no-op, and a panel re-measures/re-arranges only its DIRTY children.

    // Marks this element (and every ancestor to the root) as needing a Measure
    // pass. No-op if this element is already measure-dirty (coalescing: many
    // invalidations collapse to one pass). Wiring: the S123/S124 contract writes
    // the S122 DesiredSize/final rect; dirty flags are the change-driven trigger
    // that lets the LayoutManager (and panels) skip clean subgraphs.
    void InvalidateMeasure();

    // Marks this element (and every ancestor to the root) as needing an Arrange
    // pass. Independent of the measure axis (see tests). No-op if already
    // arrange-dirty.
    void InvalidateArrange();

    // True if this element needs a Measure pass (never measured, or re-
    // invalidated since the last measure).
    [[nodiscard]] bool is_measure_dirty() const noexcept;

    // True if this element needs an Arrange pass (never arranged, or re-
    // invalidated since the last arrange).
    [[nodiscard]] bool is_arrange_dirty() const noexcept;

    // True if EITHER axis is dirty (needs a measure OR an arrange pass).
    [[nodiscard]] bool is_layout_dirty() const noexcept;

    // --- Margin (REAL DP) --------------------------------------------------
    // Default 0. The child's outer inset removed from its parent's available
    // bounds to form its LayoutSlot (S122/WPF-faithful).
    [[nodiscard]] SetResult set_margin(const vaml::Thickness& value);
    [[nodiscard]] vaml::Thickness margin() const;

    // --- HorizontalAlignment (REAL DP; default Stretch) --------------------
    [[nodiscard]] SetResult set_horizontal_alignment(vaml::HorizontalAlignment value);
    [[nodiscard]] vaml::HorizontalAlignment horizontal_alignment() const;

    // --- VerticalAlignment (REAL DP; default Stretch) ----------------------
    [[nodiscard]] SetResult set_vertical_alignment(vaml::VerticalAlignment value);
    [[nodiscard]] vaml::VerticalAlignment vertical_alignment() const;

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

    // --- Visual children (S121: create/attach/detach + parent traversal) ----
    // A parent OWNS its children (std::unique_ptr). Attaching a child ALSO
    // calls set_parent(this), wiring the S039 parent link + the S041 inverse
    // inheritance-scope edge (so Inherits like Opacity cascade through the
    // visual child). The visual children list is DISTINCT from the S041
    // inheritance-scope: set_parent registers the inheritance edge for ANY
    // DependencyObject; the visual list below is UIElement-only structure.

    // Creates a child of type T (default-constructible, derives UIElement) and
    // attaches it as the LAST child. Returns a NON-OWNING pointer to the
    // appended child (the parent owns it). T must construct with the given
    // args; the resolved type must be UIElement (static_assert).
    template <typename T = UIElement, typename... Args>
    [[nodiscard]] T* create_child(Args&&... args)
    {
        static_assert(std::is_base_of<UIElement, T>::value,
                      "create_child<T>: T must derive from UIElement");
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = child.get();
        attach_child(std::move(child));
        return raw;
    }

    // Attaches an already-constructed owned child as the LAST child, wiring the
    // parent link + inheritance scope edge (set_parent(this)). The parent takes
    // ownership. After attach, `child` is owned by this element.
    void add_child(std::unique_ptr<UIElement> child);

    // Detaches + DESTROYS the child at `index`. Removes it from the visual
    // children AND clears its parent link (set_parent(nullptr) unwinds the S041
    // inheritance-scope edge), so no dangling scope edge survives. Returns the
    // number of children still remaining (convenience). Throws std::out_of_range
    // for an index >= child_count().
    [[nodiscard]] std::size_t remove_child(std::size_t index);

    // Detaches the child at `index` and TRANSFERS OWNERSHIP back to the caller
    // (the parent OWNS its children, so the only ownership-safe way to keep a
    // child alive across a detach is to return the unique_ptr). Clears the
    // parent link (set_parent(nullptr) unwinds the S041 inheritance-scope edge).
    // The removed child's parent() becomes nullptr and it is no longer in the
    // visual children. Throws std::out_of_range for an index >= child_count().
    [[nodiscard]] std::unique_ptr<UIElement> take_child(std::size_t index);

    // --- Parent traversal / visual-tree walk ------------------------------
    [[nodiscard]] std::size_t child_count() const noexcept;
    // NON-OWNING pointer to the child at `index` (nullptr for an out-of-range
    // index, so a walker can bound-check without throwing).
    [[nodiscard]] UIElement* child_at(std::size_t index) const noexcept;
    // NON-OWNING read-only view of the children, in index (z/visual) order.
    [[nodiscard]] const std::vector<UIElement*> children() const noexcept;
    // Depth-first pre-order walk over THIS element and all descendants in the
    // visual tree, invoking `visitor(UIElement&)` on each visited element
    // (this first, then children in order). Non-recursive internals are guarded
    // against deep trees; the visitor may read owner-free (non-owning) state.
    void walk(const std::function<void(UIElement&)>& visitor) const;

    // --- Reparent discipline ------------------------------------------------
    // A child owned by parent A cannot attach to parent B: add_child detects a
    // child that ALREADY has a non-null parent and throws std::logic_error
    // (single-parent ownership invariant). Detach first (take_child/remove_child)
    // to move a child between parents.

private:
    void attach_child(std::unique_ptr<UIElement> child);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
