#pragma once
// VAML UIElement — the base UI class for the element/scene-graph tree.
// S120 (dep:S097, Sprint 3 — element/scene graph + measure/arrange layout).
//
// S120 title contract (four concern areas, each treated explicitly):
//   1. visibility  — a REAL registered dependency property (this step).
//   2. opacity     — a REAL registered dependency property (this step), NOT
//      inherited (PropertyFlags::None): reads ONLY its own local (default 1.0);
//      the WPF `base.VisualOpacity` SUBTREE result is the S208 encoder's
//      ancestor-chain PRODUCT at encode time (effective_opacity), not DP
//      inheritance (S822 dep:S821).
//   3. transform owner — RESOLVED BY S208. The S120 transform-owner SENTINEL
//      is REMOVED here per its resolution contract: UIElement now owns the REAL
//      per-element transform member/type — the RenderTransform dependency
//      property holding a vaml::Matrix2D (the S208 affine value type) — and
//      S208's renderer performs it (the vulkan encoder CPU-folds each element's
//      Model into the quad vertex positions). Resolution at the feature's own
//      step, not a backfill (NO-GAP). See docs/roadmap.md S208.
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
// property machinery (local ?? default, validate, coercion,
// coalescing). Opacity is NOT inherited and accepts ANY double (WPF has no
// ValidateFunc and no Inherits arm for Opacity, UIElement.cs:2660-2665); its
// observable SUBTREE alpha is reached by the S208 encoder's ancestor-chain
// PRODUCT — not a "WPF does it this way" inheritance copy.
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
// (Note S822: Opacity is NOT one of the Inherits that cascade through the
// edge — it is PropertyFlags::None; the edge still feeds the S208 encoder's
// ancestor-chain opacity product.)

#include "vaml/core/DependencyObject.h"

#include "vaml/core/PropertyId.h" // PropertyId (Set/Get key)
#include "vaml/core/Value.h"      // Value, ValueType

#include "vaml/core/Geometry.h"     // Size, Rect
#include "vaml/core/LayoutSlot.h"   // HorizontalAlignment, VerticalAlignment
#include "vaml/core/Thickness.h"    // Thickness (Margin DP)
#include "vaml/core/Transform.h"    // Transform + Matrix2D (S823 RenderTransform OBJECT DP)
#include "vaml/core/Signal.h"       // Signal<T> (S320 — per-event routed-handler lists)
#include "vaml/core/RoutedEvent.h"  // RoutedEvent / RoutedEventArgs (S320) — routed-handler surface

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vaml {

// Forward declaration (the full ResourceDictionary is defined in
// ResourceDictionary.h; UIElement holds an owned unique_ptr to it, so a
// forward declaration is sufficient here and avoids an include cycle).
class ResourceDictionary;

// Forward declaration (the full ObservableObject is defined in
// ObservableObject.h; UIElement holds a NON-OWNING pointer to it as its
// DataContext, so a forward declaration avoids an include cycle and keeps the
// pointer cheap — a VM is a data SOURCE, never a UIElement-owned visual).
class ObservableObject;

// Forward declaration (the full Effect is defined in Effect.h; UIElement holds
// an OWNED unique_ptr to it as its per-element visual effect, so this forward
// declaration avoids an include cycle — mirroring ResourceDictionary).
class Effect;

// WPF-faithful visibility states, stored on the Visibility DP via the Int32
// value path (Value has no enum tag in S030 — the enum rides Int32).
enum class Visibility : std::uint8_t
{
    Visible   = 0, // rendered and hit-testable
    Hidden    = 1, // not rendered, but still occupies layout space
    Collapsed = 2, // neither rendered nor laid out
};

class VAML_CORE_EXPORT UIElement : public DependencyObject
{
public:
    UIElement();
    // S121: the pimpl owns the visual children (unique_ptr<UIElement>), so the
    // destructor must be out-of-line (default) for the Impl to be complete here.
    ~UIElement() override;

    // --- Resources slot (S360) ---------------------------------------------
    // The element's OWN ResourceDictionary (WPF FrameworkElement.Resources
    // analogue). OPTIONAL — the default is no resources. The S360 three-scope
    // lookup (find_for_element) reads this slot first, then walks the parent
    // chain, then app + system. `set_resources` takes OWNERSHIP; `resources()`
    // returns the non-owning pointer (nullptr when none).
    //
    // S360 OBJECT-POOL BUDGET: the ResourceDictionary is stored OUT-OF-LINE in
    // a process-wide side-table keyed by the element's address (NOT an inline
    // unique_ptr member), so a UIElement/grid that never declares resources
    // does not grow — the S145 slot-size invariant is preserved. An element
    // with resources has ONE owner (the dictionary) + one lifetime (torn down
    // when the element is destroyed / the resource is removed / the side-table
    // is reset). See ResourceDictionary.cpp.
    void set_resources(std::unique_ptr<ResourceDictionary> resources);
    [[nodiscard]] const ResourceDictionary* resources() const noexcept;
    [[nodiscard]] ResourceDictionary* resources() noexcept;

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

    // --- needs_clip() — WPF NeedsClipBounds mirror (S832) ------------------
    // True iff the last Arrange arranged this element LARGER than its slot on
    // an axis (the child-wins-then-clip contract of FrameworkElement.ArrangeCore
    // FE.cs:4597-4606). final_rect() may then EXCEED the arrange slot; the
    // conditional clip scissor that honors this flag is S833 (GetLayoutClip,
    // FE.cs:4862-4960). Re-computed on every Arrange (WPF resets NeedsClipBounds
    // false each ArrangeCore, FE.cs:4546). Default false until the first Arrange.
    [[nodiscard]] bool needs_clip() const noexcept;

    // --- clip_rect() — the WPF GetLayoutClip rectangular result (S833) -----
    // The SLOT-CLIENT clip window the S208 renderer scissors to when
    // needs_clip() is true (FrameworkElement.GetLayoutClip, FE.cs:4916-4929:
    // `Rect(-offset, layoutSlotSize - Margin)` in world coords =
    // `{slot.x+ml, slot.y+mt, inset, inset}`). This is NOT final_rect() (which
    // may EXCEED the slot); clipping an overflowing child to final_rect would be
    // a no-op. Only meaningful when needs_clip() is true (a fitting element
    // produces no clip). Set by Arrange from compute_final_rect().clip_rect.
    // S834 will OR a `ClipToBounds` DP into the trigger; the rect math is the
    // pure value-domain result here.
    [[nodiscard]] const vaml::Rect& clip_rect() const noexcept;

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

    // --- S830: OnChildDesiredSizeChanged (WPF UIElement.cs:323-330) ---------
    // WPF `protected virtual void OnChildDesiredSizeChanged(UIElement child)`
    // with the default body `if (IsMeasureValid) InvalidateMeasure();` — "the
    // default implementation simply calls InvalidateMeasure(), assuming that
    // layout of a parent should be updated after child changed its size".
    // Declared public here to mirror this class's existing convention for the
    // layout virtals (MeasureOverride/ArrangeOverride are likewise declared in
    // the public block with their WPF `protected` visibility recorded).
    //
    // Fired by TWO WPF-real call sites, both mirrored in Measure():
    //   1. the measure-side waterfall (UIElement.cs:687-700) — a completed
    //      Measure whose DesiredSize CHANGED signals its parent, unless the
    //      parent is itself inside a Measure pass (`!p.MeasureInProgress`);
    //   2. the Visibility transition (UIElement.cs:2984-3034) —
    //      `ensureVisible`/`ensureInvisible` call `signalDesiredSizeChange()`
    //      on the Collapsed<->non-Collapsed edges, which routes here.
    // It is the ONLY measure signal a Visibility change produces: Visibility
    // carries NO AffectsMeasure in WPF (plain PropertyMetadata +
    // ValidateValueCallback, UIElement.cs:2938-2954) and none here either.
    virtual void OnChildDesiredSizeChanged(UIElement* child);

    // --- Arrange contract (S124) -------------------------------------------
    // Arrange(arrangeSlot) -> final_rect(). The write-side pair to Measure:
    // consumes the S122 compute_final_rect(slot, desired, margin, hAlign,
    // vAlign) with the element's own desired_size() (written by S123's
    // Measure), margin(), horizontal_alignment(), vertical_alignment(); writes
    // the S122 final_rect() field. Repeated Arrange with a new slot RECOMPUTES
    // (recompute, never cumulate — same discipline as Measure/S123).
    //
    // RENDER SLOT == CLIP BOUNDS: final_rect() IS the render slot where the
    // renderer (S208) draws the element. S832 (Sprint 11ad — child-wins-then-
    // clip): a child whose unclipped desired exceeds its slot is arranged at its
    // FULL desired size — final_rect() may EXCEED the slot — and needs_clip()
    // reports the overflow (WPF NeedsClipBounds, FE.cs:4597-4606). S833
    // (RESOLVED) stores the GetLayoutClip rectangular result in clip_rect_ (the
    // slot-client window, FE.cs:4916-4929) and the S208 renderer scissors to it
    // CONDITIONALLY — only when needs_clip() (a fitting element stays unclipped,
    // unchanged batching). The base clip op the renderer + ScrollViewer (S405)
    // consume remains vaml::clip_to_bounds (LayoutSlot.h). S127 (RESOLVED) added
    // the ArrangeOverride virtual here, so Arrange now auto-invokes a panel's
    // ArrangeOverride and hands it the UNCLIPPED final_rect() (the child receives
    // the size it measured for, FE.cs:4651/4697). Do NOT extend this seam silently.
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

    // --- S818 render invalidation (InvalidateVisual) ---------------------
    // WPF `UIElement.InvalidateVisual()`: marks THIS element as needing its
    // visual re-rendered. In VAML it sets the render-dirty flag consumed by the
    // S208/S210 render-dirty seam (a repaint is scheduled). WPF-faithful scoping:
    // InvalidateVisual marks ONLY THIS element (it does NOT walk to ancestors —
    // ancestor repaint is the AffectsParent* / AffectsRender-up-chain job). Set
    // via `AffectsRender` on a real value change (FE.cs:2251), gated by
    // `SubPropertiesDoNotAffectRender` for non-value changes.
    void InvalidateVisual();

    // True if this element needs its visual re-rendered (set by InvalidateVisual
    // or an AffectsRender change; consumed by the render-dirty seam).
    [[nodiscard]] bool is_render_dirty() const noexcept;

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

    // --- Width / Height (WPF-style layout DPs; default auto/NaN) -----------
    // WPF-faithful explicit-size DPs on the element base (the .xaml SOURE-import
    // line): an explicit positive double constrains that axis; NaN (default) or
    // 0 means AUTO — the element's natural/content extent governs. Consumed by
    // UIElement::Measure (see reconcile_width_height below), NOT a dead
    // registration. Degree-0 type is Double.
    [[nodiscard]] SetResult set_width(double value);
    [[nodiscard]] double width() const;
    [[nodiscard]] SetResult set_height(double value);
    [[nodiscard]] double height() const;

    // --- Visibility (REAL DP; not inherited) ----------------------------
    // Default Visible. Per-element render state (NOT inherited — WPF-faithful).
    // Exposed both via the typed accessors and the standard Set/Get path on
    // PropertyId("Visibility").
    //
    // S821 (WPF-fidelity boundary gate): the DP's ValidateFunc mirrors WPF
    // ValidateVisibility (UIElement.cs:2960) — only Visible(0)/Hidden(1)/
    // Collapsed(2) resolve. Any other value (e.g. 3 or -1, authored as a raw
    // Int32 past the enum domain) is REJECTED at the Set boundary (store
    // unchanged) and THROWS InvalidPropertyValueException (naming the property +
    // the offending value) per the S810_1 false-ValidateFunc disposition.
    [[nodiscard]] SetResult set_visibility(Visibility v);
    [[nodiscard]] Visibility visibility() const;

    // --- Focusable (REAL DP; S325) -------------------------------------
    // Default true. Declares whether THIS element can take keyboard focus.
    // WPF-faithful: a focusable leaf defaults true; a container/panel is
    // typically set false by its author (or a future control default). The
    // S325 FocusManager's tab-order skips non-Focusable elements. Exposed both
    // via the typed accessor and the standard Set/Get path on
    // PropertyId("Focusable") (Int32 0/1).
    [[nodiscard]] SetResult set_focusable(bool value);
    [[nodiscard]] bool focusable() const;

    // --- IsHitTestVisible (REAL DP; S595a) -------------------------------
    // Bool, default TRUE, NOT inherited. PER-ELEMENT input hit-test
    // transparency: when FALSE the element STILL RENDERS but HitTestAt SKIPS it
    // (the same skip axis WPF uses for Visibility, but DECOUPLED — an element
    // can render "100% transparent" overlay chrome while passing clicks through,
    // and toggle to hit-testable to CAPTURE clicks/"handled"). Exposed both via
    // the typed accessor and Set/Get on PropertyId("IsHitTestVisible") (Bool —
    // the AllowsTransparency/Topmost precedent). ZERO new locks (ADR-018).
    [[nodiscard]] SetResult set_is_hit_test_visible(bool value);
    [[nodiscard]] bool is_hit_test_visible() const;

    // --- Opacity (REAL DP; NOT inherited, NO [0,1] validation) ---------
    // Default 1.0. NOT inherited (PropertyFlags::None) — opacity() reads ONLY
    // this element's OWN local, never an inherited DP value (WPF-faithful:
    // UIElement.cs:2660-2665 has no Inherits/GetValueEntry arm for Opacity, and
    // no OverrideMetadata(..., Inherits) exists anywhere; see S822 dep:S821).
    // Accepts ANY double — `Opacity="1.5"`, `"-2"`, `NaN` all resolve exactly
    // as WPF (there is NO ValidateFunc, so the S810_1 throw-on-invalid does NOT
    // apply; accept is the WPF contract). The SUBTREE alpha (a child under an
    // Opacity=0.5 parent renders at 0.5 × its own opacity, the WPF
    // `base.VisualOpacity` composition, UIElement.cs:2689-2691) is reached by
    // the S208 encoder's effective_opacity — the visual-ancestor chain PRODUCT —
    // NOT by DP inheritance.
    [[nodiscard]] SetResult set_opacity(double value);
    [[nodiscard]] double opacity() const;

    // --- RenderTransform (REAL DP; the S823 Transform-object migration) ----    // The per-element 2D affine model transform, WPF-FAITHFUL OBJECT semantics:
    // the DP holds a `Transform` OBJECT (Value::Transform(shared_ptr<Transform>),
    // default `Transform::identity()` — the `Transform.Identity` default,
    // UIElement.cs:1335-1340 is typeof(Transform) with NO ValidateValueCallback).
    // NOT inherited (PropertyFlags::None, each element owns its own, like Margin).
    // WPF accepts ANY Transform — there is NO finite-validity rejection at the
    // DP Set (a NaN/Inf matrix is WPF-legal); the finite-validity gate VAML
    // needs stays ONLY at the S208 raster/encode boundary (`is_transform_valid`
    // re-scoped there), NEVER here. FrameworkElement renders at its layout slot
    // (final_rect) and maps its content through this transform; S208's encoder
    // resolves the authored Transform's matrix() and CPU-folds it into the quad
    // vertex positions BEFORE the S200 MVP projection, then honors
    // RenderTransformOrigin. Set triggers InvalidateArrange() (a visual change →
    // the encoder re-emits).
    [[nodiscard]] SetResult set_render_transform(const std::shared_ptr<vaml::Transform>& value);
    [[nodiscard]] std::shared_ptr<vaml::Transform> render_transform() const;

    // --- RenderTransformOrigin (REAL DP; S823) ----------------------------
    // WPF-faithful (UIElement.cs:1378): a Point (default (0,0)), NOT inherited
    // (PropertyFlags::None). The center (as a normalized FRACTION of the layout
    // slot: origin.x × width, origin.y × height) the RenderTransform scales/
    // rotates about — the S208 fold `T(−originPx)·M·T(+originPx)`,
    // `originPx = (fr.width·origin.x, fr.height·origin.y)`, exactly WPF
    // ArrangeCore (UIElement.cs:1253-1296: identity -> NO fold, zero origin ->
    // NO origin terms; any real fraction incl. <0/>1 is legal — only NaN/±Inf
    // is rejected by IsRenderTransformOriginValid, UIElement.cs:1389-1396).
    [[nodiscard]] SetResult set_render_transform_origin(vaml::Point value);
    [[nodiscard]] vaml::Point render_transform_origin() const;

    // --- Effect (S511 — per-element visual effect) ---------------------
    // WPF-faithful (UIElement.Effect): OPTIONAL visual effect applied when the
    // element's subtree renders. Stored OUT-OF-LINE in a process-wide side-table
    // keyed by element address (S145 object-pool budget — the S360/Q500
    // precedent: UIElement is a fixed object-pool slot size, so NO inline member
    // may grow it). An element that never sets an effect has no side-table entry.
    // The entry is torn down on destruction (see UIElement.cpp).
    //
    // `set_effect` takes OWNERSHIP of a heap Effect (unique_ptr<Effect>); the
    // element becomes its owner. Passing nullptr CLEARS the effect. `effect()`
    // returns the non-owning pointer (nullptr when none).
    //
    // ADR-018: ZERO new locks (single-threaded UI-thread set/destroy; the S360
    // side-table discipline). The render consumption is the vaml-vulkan
    // UiSceneEncoder arm (RENDER_SURFACE-conformant at S511).
    void set_effect(std::unique_ptr<vaml::Effect> effect);
    [[nodiscard]] const vaml::Effect* effect() const noexcept;
    [[nodiscard]] vaml::Effect* effect() noexcept;


    // --- Visual children (S121: create/attach/detach + parent traversal) ----
    // A parent OWNS its children (std::unique_ptr). Attaching a child ALSO
    // calls set_parent(this), wiring the S039 parent link + the S041 inverse
    // inheritance-scope edge (so truly-Inherits properties like DataContext
    // cascade through the visual child). Opacity is NOT one of those (S822 —
    // PropertyFlags::None); the visual children list is DISTINCT from the S041
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
    // Writes the child pointers into `out` (S144 — no return-by-value copy; the
    // caller supplies the sink). For a no-alloc read, use child_count() +
    // child_at().
    void children(std::vector<UIElement*>& out) const noexcept;
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

    // --- DataContext (S500 — per-element inheritable context) --------------
    // WPF-faithful (FrameworkElement.DataContext IS inheritable, per the WPF
    // source review of FrameworkElement.cs: FrameworkPropertyMetadataOptions.
    // Inherits). A parent's DataContext inherits to its descendants unless a
    // descendant carries its OWN local DataContext (local-wins). The classic
    // S500 consumer: a per-item ContentPresenter sets DataContext = item so the
    // template content it hosts inherits the item, and a `{Binding Path}` inside
    // the template resolves against the inherited context (the per-item
    // DataContext-inheritance mechanism).
    //
    // S145 OBJECT-POOL BUDGET (recorded, NOT silent): the DataContext is stored
    // OUT-OF-LINE in a process-wide side-table keyed by element address (the
    // S360 ResourceDictionary precedent — UIElement is a fixed object-pool slot
    // size under the S145 test gate, so NO inline member may grow it). An
    // element that never carries a local DataContext has no side-table entry.
    // The entry is torn down when the element is destroyed (see UIElement.cpp).
    //
    // `set_data_context` sets a LOCAL context (nullptr clears the local
    // override -> inherits the parent's). `effective_data_context()` walks the
    // parent chain and returns the NEAREST local context (this's local if set,
    // else the first ancestor with a local context, else the ultimate fallback
    // passed in — the reader's document data context).
    [[nodiscard]] ::vaml::SetResult set_data_context(::vaml::ObservableObject* ctx) noexcept;
    // Returns THIS element's LOCAL DataContext (nullptr when it does NOT carry
    // a local override — i.e. it inherits from its parent).
    [[nodiscard]] ::vaml::ObservableObject* data_context() const noexcept;
    // Walks the parent chain for the nearest local DataContext, falling back to
    // `documentContext` (the reader's document data context) when no ancestor
    // carries a local one. This is the value a `{Binding}` resolves against.
    [[nodiscard]] ::vaml::ObservableObject* effective_data_context(
        ::vaml::ObservableObject* documentContext) const noexcept;

    // --- Routed events (S321) --------------------------------------------
    // The per-event routed-handler surface built on the S320 Signal<T>
    // primitive. `add_routed_handler` registers a handler for `event` on THIS
    // element (fired in registration order when a raise routes through it);
    // `remove_routed_handler` removes by token (reentrancy-safe). `RaiseEvent`
    // (below) is the engine: bubbling (source -> root, stop on `handled`) and
    // tunneling (root -> source, the preview). The handler may set
    // `args.handled = true` to stop the route — WPF-faithful.
    using RoutedHandler    = std::function<void(RoutedEventArgs&)>;
    using RoutedHandlerToken = SignalToken;

    // Registers `handler` for `event` on this element; returns a removal token.
    // Lazy per-event list (zero cost when the element subscribes nothing).
    [[nodiscard]] RoutedHandlerToken add_routed_handler(const RoutedEvent& event,
                                                        RoutedHandler handler);

    // Removes the routed handler by token; true iff it was present.
    [[nodiscard]] bool remove_routed_handler(const RoutedEvent& event,
                                             RoutedHandlerToken token);

    // RAISES `event` on this element with `args`. `args.source` is expected to
    // be THIS element (the element that raised). The route travels per the
    // event's strategy (bubble/tunnel), honoring `args.handled`. Public so a
    // control / the input-marshaller can raise a routed event from an element.
    void RaiseEvent(const RoutedEvent& event, RoutedEventArgs& args);

    // --- Hit-testing (S323) -----------------------------------------------
    // Returns the DEEPEST TOPMOST element whose final_rect() contains (x, y),
    // or nullptr when the point hits no element. TOPMOST = reverse child order
    // (the LAST child is on top, z-order — later children are tested first);
    // DEEPEST = a hit child wins over its ancestors. Honors hit-testability:
    //     - Visibility::Collapsed / Hidden are NOT hit-testable (skipped);
    //     - a child CLIPPED by an ancestor's final_rect (overflow) is not hit
    //       (the point must lie within every ancestor's final_rect too).
    // The walk uses the layout final_rect() (the S122/S124 render slot) — the
    // S208 render transform is a render concern and NOT folded here (a recorded
    // future refinement: Model-aware hit-testing). ADR-018: zero locks (pure
    // tree walk on the borrowed tree; caller owns the tree's lifetime).
    [[nodiscard]] UIElement* HitTestAt(float x, float y);
    // Same, but a CONST overload returning a non-owning const pointer.
    [[nodiscard]] const UIElement* HitTestAt(float x, float y) const;

private:
    // --- S818 OnPropertyChanged override (the FE.cs Affects* engine) -------
    // Overrides the DO.cs:2039 change call-out to run the WPF
    // `FrameworkElement.OnPropertyChanged` Affects* invalidation
    // (FE.cs:2198-2251): AffectsParentMeasure/Arrange -> nearest UIElement
    // layout parent's InvalidateMeasure/InvalidateArrange (parent-chain walk);
    // AffectsMeasure -> InvalidateMeasure; AffectsArrange -> InvalidateArrange;
    // AffectsRender -> InvalidateVisual (gated on
    // SubPropertiesDoNotAffectRender for non-value changes). Invoked from Set()
    // on a real change only.
    void OnPropertyChanged(
        PropertyId id,
        const Value& oldValue,
        const Value& newValue,
        PropertyFlags flags) override;

    void attach_child(std::unique_ptr<UIElement> child);

    // --- S830: signalDesiredSizeChange (WPF UIElement.cs:3043-3056) ---------
    // `GetUIParentOrICH` → `p.OnChildDesiredSizeChanged(this)`. VAML has no
    // IContentHost branch (FrameworkContentElement hosts are not a measure
    // participant in this engine — recorded, not silent); the walk therefore
    // stops at the NEAREST UIElement ancestor, skipping non-layout ancestors.
    // Private in WPF and private here.
    void signal_desired_size_change();

    // Fires `event`'s handlers on ONE element (sets args.source = element),
    // in registration order. A handler may set handled=true (checked by the
    // RaiseEvent loop). A private member so it can read `routed_handlers_`.
    void _raise_at(UIElement* element, const RoutedEvent& event, RoutedEventArgs& args);

    // --- S321 routed-handler storage -------------------------------------
    // The per-event handler lists live on the heap in a lazily-allocated store
    // (a SINGLE pointer member keeps UIElement small — an inline map would bloat
    // it past the S145 object-pool slot). Zero cost until an element subscribes
    // a routed handler. Single-threaded (UI/affinity thread) — ADR-018 ZERO new
    // locks. Routed handlers are a DIFFERENT surface from the S041 property
    // subscribers.
    struct RoutedHandlerStore; // defined in RoutedEvent.cpp
    std::unique_ptr<RoutedHandlerStore> routedHandlers_; // lazily built on first add

    // ------------------------------------------------------------------
    // S142: the pimpl is COLLAPSED into one block-resident unit with the base's
    // Store — UIElement's layout data composes with DependencyObject's inline
    // store as subobjects of the SAME allocation (no per-object heap hop for
    // the layout state either). pimpl removed, not preserved (ADR-003/ADR-019).
    // ------------------------------------------------------------------
    std::vector<std::unique_ptr<UIElement>> children_;      // S121 owned, ordered visual children
    vaml::Size desired_size_   = {};                        // S122/S123 DesiredSize (Measure writes)
    vaml::Rect final_rect_     = {};                        // S122/S124 final rect (Arrange writes)
    // --- S833 GetLayoutClip rectangular result ---------------------------
    // The SLOT-CLIENT clip window computed by Arrange (compute_final_rect's
    // clip_rect). Core layout geometry (sibling of final_rect_), set on EVERY
    // arrange — meaningful only when needs_clip_. The S208 renderer scissors to
    // this when needs_clip() (WPF caches its Clip on the UIElement after
    // ensureClip; this is the faithful inline store). Not a side-table: it is
    // not optional feature data — it is the element's arrange result.
    vaml::Rect clip_rect_      = {};
    bool       measure_dirty_  = true;                      // S126 layout-dirty flag (measure axis)
    bool       arrange_dirty_  = true;                      // S126 layout-dirty flag (arrange axis)
    // --- S832 NeedsClipBounds mirror -------------------------------------
    // True iff the last Arrange arranged this element LARGER than its slot on
    // an axis (the WPF FrameworkElement.NeedsClipBounds, FE.cs:4546/4596/4603).
    // Re-computed on every Arrange; consumed by the S833 conditional clip
    // scissor (S208 scissors to clip_rect_ when this is true). Default false
    // until the first Arrange pass.
    bool       needs_clip_     = false;                     // S832 NeedsClipBounds mirror
    // --- S829 measure-entry fidelity (WPF UIElement.Measure) -----------------
    // measure_in_progress_: reentrancy guard (WPF MeasureInProgress /
    //   Dispatcher.DisableProcessing — "We cannot survive reentrancy"). A
    //   recursive Measure on this element bails instead of recursing.
    // never_measured_: WPF NeverMeasured — true until the first real Measure
    //   completes; blocks the clean early-out so the first pass always runs.
    //   (Consumed by S835/11ad's arrange `MeasureDirty || NeverMeasured` too.)
    // previous_available_: WPF _previousAvailableSize (default (0,0), paired
    //   with never_measured_ exactly like WPF); a clean Measure at a close
    //   constraint returns the cached DesiredSize without recomputing.
    // Single-threaded (UI/affinity thread) — ADR-018 ZERO new locks.
    bool       measure_in_progress_ = false;                // S829 reentrancy guard
    bool       never_measured_      = true;                 // S829 WPF NeverMeasured
    vaml::Size previous_available_  = {};                   // S829 WPF _previousAvailableSize
    // S818: render-dirty flag (InvalidateVisual / AffectsRender). Default
    // FALSE — a fresh element is not render-dirty until its first
    // InvalidateVisual or an AffectsRender change (unlike measure/arrange,
    // which start true because a fresh element is never laid out). Consumed by
    // the S208/S210 render-dirty seam (a repaint of this element is scheduled).
    bool       render_dirty_   = false;                     // S818 render-dirty flag (InvalidateVisual)
    // RenderTransform lives in the DP store (the S034/S043 effective-value
    // machinery), NOT as a second inline member — no extra member beyond the
    // S122/S126 state above.
};

} // namespace vaml

