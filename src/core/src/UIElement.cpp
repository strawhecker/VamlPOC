// S120 — UIElement out-of-line definitions + self-registration.
//
// Visibility + Opacity are REAL registered dependency properties usable
// through the full S1 property machinery (S043 attached-aware globals:
// an object that does NOT locally register them still resolves the global
// descriptor). Self-registration mirrors register_builtin_elements(): the
// first UIElement construction registers both DPs into the global
// DependencyProperty table once, idempotently.
//
// The layout slot is RESOLVED BY S122 (Geometry.h/LayoutSlot.h real geometry +
// DesiredSize/final_rect/Margin/alignment DPs). The transform-owner is
// RESOLVED BY S208 (the Transform.h Matrix2D affine value type as the REAL
// UIElement RenderTransform DP, performed by the vulkan encoder; the S120
// transform-owner sentinel is removed there — see UIElement.h + Transform.h).
//
// S142 (dep:S141): the pimpl is COLLAPSED. UIElement's layout data (children,
// desired size, final rect, dirty flags) now lives directly as inline members,
// composing with the base DependencyObject's block-resident Store into ONE
// block-resident unit (pimpl removed, not preserved; ADR-003 revised +
// ADR-019). Behavior is unchanged.

#include "vaml/core/UIElement.h"
#include "vaml/core/FrameworkElement.h"  // S824 — ensure_framework_element_properties_registered (the FE-owner slot DPs)
#include "vaml/core/LayoutContext.h"      // S839 — the per-thread layout need-queue
#include "vaml/core/LayoutManager.h"      // S843 — the INTERNAL ContextLayoutManager analogue backing UpdateLayout()

#include "vaml/core/BehaviorCollection.h"   // S502 — remove_element_behaviors (side-table teardown)
#include "vaml/core/ResourceDictionary.h" // S360 — the complete Resources slot type (dtor + accessors)
#include "vaml/core/Effect.h"           // S511 — the complete Effect type (side-table payload)
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/RoutedEvent.h"   // S321 routed handlers
#include "vaml/core/Signal.h"        // S320 Signal<T>

#include <algorithm>
#include <cmath>    // std::fabs / std::isnan / std::isinf (S829 measure guard + S835 arrange guard)
#include <stdexcept>
#include <string>   // std::to_string (S835 Arrange non-finite diagnostic)
#include <unordered_map>
#include <utility>
#include <vector>
#include <limits>   // std::numeric_limits (Width/Height DP auto-NaN sentinel)

namespace vaml {

// --- S321 routed-handler store (complete type for the unique_ptr) -----------
struct UIElement::RoutedHandlerStore
{
    // Per-event-name handler lists, keyed by the event's interned name (a
    // STABLE pool-backed view — see the S140 discipline), each a heap-owned
    // Signal<T>. Built lazily on first subscribe. Single-threaded (UI/affinity);
    // ADR-018 ZERO new locks.
    std::unordered_map<std::string_view, std::unique_ptr<Signal<void(RoutedEventArgs&)>>>
        byEvent;
};

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
    //
    // S821 (WPF-fidelity boundary gate): mirrors WPF `ValidateVisibility`
    // (UIElement.cs:2960) — the DP boundary REJECTS any value outside
    // Visible(0)/Hidden(1)/Collapsed(2). Authoring Visibility="3" or Visibility
    // ="-1" (a raw Int32 past the enum domain) THROWS InvalidPropertyValueException
    // at the Set boundary (store unchanged) via the S810_1 false-ValidateFunc
    // disposition — matching WPF's ArgumentException(InvalidPropertyValue) at
    // DependencyObject.cs:653. 0/1/2 all resolve. The Int32-rides-enum mapping
    // (S120/S600/S648) is UNCHANGED — only the WPF validate callback is added.
    (void)DependencyProperty::Register(
        "Visibility", ValueType::Int32, "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) {
                const int x = v.get<int>();
                return x == 0 || x == 1 || x == 2; // Visible/Hidden/Collapsed
            }));

    // Opacity: default 1.0, NOT inherited, NO [0,1] validation (PropertyFlags::
    // None). Reads ONLY its OWN local (default 1.0) — a descendant's opacity()
    // NEVER reads an inherited DP value. WPF-faithful: `EvaluateEffectiveValue`/
    // `GetValueEntry` (UIElement.cs:2660-2665) has NO Inherits arm for Opacity,
    // and grep-verified there is NO `OverrideMetadata(..., Inherits)` anywhere,
    // so Opacity never inherits. There is also NO ValidateFunc for Opacity —
    // ANY double resolves (`Opacity="1.5"`, `"-2"`, `NaN` all accepted exactly
    // as WPF; the S810_1 throw-on-invalid does NOT apply — accept is the WPF
    // contract). The WPF `base.VisualOpacity` SUBTREE result (UIElement.cs:
    // 2689-2691 — a child under an Opacity=0.5 panel renders at 0.5 × its OWN
    // opacity) is reached by the S208 encoder's ancestor-chain PRODUCT at
    // encode time (effective_opacity), NOT by DP inheritance.
    (void)DependencyProperty::Register(
        "Opacity", ValueType::Double, "UIElement",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));

    // S824 (dep:S823): the layout-slot DPs (Margin/alignments/Width/Height)
    // + Tag are NO LONGER registered here — they are re-based onto the REAL
    // FrameworkElement base and re-keyed under owner "FrameworkElement"
    // (register_framework_element_properties, FrameworkElement.cpp) per the
    // WPF ground truth (FE.cs:3466+ owns the layout slot; FE.cs:3218 owns
    // Tag). The typed accessors remain on UIElement (which runs the
    // Measure/Arrange pass) but read PropertyId::of(name, "FrameworkElement"),
    // so the FE-owned DPs are the single source and the observable WPF
    // contract (layout-slot DP ownership on FrameworkElement) is closed.

    // RenderTransform (S823 dep:S822 — the S208 Matrix2D DP migrated onto the
    // WPF-faithful TRANSFORM OBJECT). WPF `RenderTransformProperty` is
    // `typeof(Transform)`, default `Transform.Identity`, with NO ValidateValueCallback
    // (UIElement.cs:1335-1340) — so the DP holds a `Transform` object and
    // ACCEPTS ANY Transform at Set (even a NaN/Inf matrix — WPF-legal). The
    // finite-validity gate `is_transform_valid` is RE-SCOPED to the S208 raster/
    // encode boundary only (never here). NOT inherited (PropertyFlags::None).
    // Set triggers InvalidateArrange (see accessor).
    (void)DependencyProperty::Register(
        "RenderTransform", ValueType::Transform, "UIElement",
        PropertyMetadata::WithDefault(
            Value::Transform(vaml::Transform::identity()), PropertyFlags::None));

    // RenderTransformOrigin (S823, UIElement.cs:1378): a Point, default (0,0),
    // NOT inherited. The normalized-fraction center the transform operates about.
    (void)DependencyProperty::Register(
        "RenderTransformOrigin", ValueType::Point, "UIElement",
        PropertyMetadata::WithDefault(Value::Point(vaml::Point{}), PropertyFlags::None));

    // Focusable (S325): default true, NOT inherited. Declares whether THIS
    // element can take keyboard focus (WPF-faithful: a focusable leaf is true;
    // a container/panel is set false by its author or default). The Int32
    // value path (Bool rides Int32? — Value uses Int32 as the closest to a
    // bool tag; keep 0/1 semantics).
    (void)DependencyProperty::Register(
        "Focusable", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));

    // IsHitTestVisible (S595a / dep:S595): WPF-faithful UIElement.IsHitTestVisible.
    // Bool, default TRUE, NOT inherited (per-element input hit-test transparency).
    // DECOUPLED from Visibility: an element with this false still RENDERS but does
    // NOT intercept input (click pass-through / transparent overlay chrome). The
    // HitTestAt skip is in UIElement::HitTestAt (const). Bool payload so the corpus
    // authoring `IsHitTestVisible="true"/"false"` converts via the Bool converter
    // (the AllowsTransparency/Topmost precedent, Element.cpp). ZERO new locks.
    (void)DependencyProperty::Register(
        "IsHitTestVisible", ValueType::Bool, "UIElement",
        PropertyMetadata::WithDefault(Value::Bool(true), PropertyFlags::None));

    // ClipToBounds (S834 / dep:S833): WPF-faithful UIElement.ClipToBounds.
    // Bool, default FALSE, NOT inherited, AffectsArrange. WPF ground truth —
    // UIElement.cs:3338-3345 registers `"ClipToBounds"` (typeof(bool),
    // typeof(UIElement), BooleanBoxes.FalseBox default); the change callback
    // ClipToBounds_Changed (UIElement.cs:3347-3356) calls InvalidateArrange().
    // There is NO FrameworkPropertyMetadata.Inherits and NO ValidateFunc — so
    // this is a plain Bool DP with PropertyFlags::AffectsArrange, which the S818
    // OnPropertyChanged engine turns into the exact WPF InvalidateArrange() on
    // change. FrameworkElement.GetLayoutClip (FE.cs:4874 `NeedsClipBounds ||
    // ClipToBounds`) FORCES the layout-slot clip EVEN IF the content fits (the
    // forced-mode / closure-audit case, distinct from the S832 conditional
    // overflow mode); the S833 render-slot clip gate becomes
    // `needs_clip() || clip_to_bounds()`. Bool payload so the corpus authoring
    // `ClipToBounds="true"/"false"` converts via the Bool converter. ZERO new
    // locks.
    (void)DependencyProperty::Register(
        "ClipToBounds", ValueType::Bool, "UIElement",
        PropertyMetadata::WithDefault(Value::Bool(false), PropertyFlags::AffectsArrange));

    // Tag (11j-8) has been MOVED to the FrameworkElement base (owner
    // "FrameworkElement", register_framework_element_properties) per the WPF
    // ground truth (FE.cs:3218). It is NOT a UIElement-owned DP.
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

// ---------------------------------------------------------------------------
// S500 per-element DataContext SIDE-TABLE (stored OUT-OF-LINE).
// The WPF-faithful inheritable per-element DataContext is stored in this
// process-wide side-table keyed by element address — NOT an inline member —
// so an element/grid that never carries a local DataContext does not grow and
// the S145 object-pool slot-size invariant is preserved (the S360
// ResourceDictionary precedent). An element with a local DataContext has ONE
// entry; the dtor releases it (remove_element_data_context) so no dangling key
// survives. ADR-018: single-threaded UI use; no lock (the S360 registry
// discipline).
namespace detail {

using ElementDataContextTable = std::unordered_map<const UIElement*, ObservableObject*>;

VAML_CORE_EXPORT
ElementDataContextTable& element_data_context_table() noexcept
{
    static ElementDataContextTable table;
    return table;
}

VAML_CORE_EXPORT
void set_element_data_context(UIElement* element, ObservableObject* ctx) noexcept
{
    if (element == nullptr)
    {
        return;
    }
    if (ctx == nullptr)
    {
        element_data_context_table().erase(element);
        return;
    }
    // A local override replacing a prior one: direct assignment on the key.
    element_data_context_table()[element] = ctx;
}

VAML_CORE_EXPORT
ObservableObject* get_element_data_context(const UIElement* element) noexcept
{
    if (element == nullptr)
    {
        return nullptr;
    }
    const auto& table = element_data_context_table();
    const auto it = table.find(element);
    return it == table.end() ? nullptr : it->second;
}

VAML_CORE_EXPORT
void remove_element_data_context(UIElement* element) noexcept
{
    if (element != nullptr)
    {
        element_data_context_table().erase(element);
    }
}

VAML_CORE_EXPORT
void element_data_context_master_reset() noexcept
{
    element_data_context_table().clear();
}

} // namespace detail

// --- S511 Effect slot (stored OUT-OF-LINE; S145 budget) ----------------------
// Mirror of the DataContext/Resources side-table discipline: UIElement is a
// fixed object-pool slot size (S145 test gate), so the per-element Effect lives
// in a process-wide table keyed by element address. An element with no effect
// has no entry; the owner unique_ptr is torn down on destruction so no dangling
// key survives. ADR-018: single-threaded UI use; no lock.
namespace detail {

using ElementEffectTable = std::unordered_map<const UIElement*, std::unique_ptr<Effect>>;

VAML_CORE_EXPORT
ElementEffectTable& element_effect_table() noexcept
{
    static ElementEffectTable table;
    return table;
}

VAML_CORE_EXPORT
void set_element_effect(UIElement* element, std::unique_ptr<Effect> effect) noexcept
{
    if (element == nullptr)
    {
        return;
    }
    if (!effect)
    {
        element_effect_table().erase(element);
        return;
    }
    element_effect_table()[element] = std::move(effect);
}

VAML_CORE_EXPORT
Effect* get_element_effect(const UIElement* element) noexcept
{
    if (element == nullptr)
    {
        return nullptr;
    }
    const auto& table = element_effect_table();
    const auto it = table.find(element);
    return it == table.end() ? nullptr : it->second.get();
}

VAML_CORE_EXPORT
void remove_element_effect(UIElement* element) noexcept
{
    if (element != nullptr)
    {
        element_effect_table().erase(element);
    }
}

} // namespace detail

VAML_CORE_EXPORT
UIElement::UIElement()
{
    ensure_uielement_properties_registered();
    // S824: arm the FrameworkElement-owner slot DPs too, so a UIElement that
    // reads/writes the slot (the accessors live here) finds the FE-owned DPs
    // registered. A concrete element (FrameworkElement-derived) arms the same
    // set via its base — the guard makes the second call a no-op.
    ensure_framework_element_properties_registered();
}

VAML_CORE_EXPORT
UIElement::~UIElement()
{
    // S839: drop this element from the per-thread layout need-queue so no
    // dangling pointer survives a layout that drains later (the faithful
    // "queue node cleared on teardown"). Done FIRST — before the other side
    // tables — so a queued element never outlives its storage.
    vaml::detail::LayoutContext::current().detach(this);
    // S360 lifetime: an element that owned a ResourceDictionary releases it on
    // destruction so the side-table holds no dangling key (the S245 detach-on-
    // destroy discipline applied to the resource side-table).
    vaml::detail::remove_element_resources(this);
    // S500 lifetime: an element that carried a LOCAL DataContext releases its
    // side-table entry on destruction so no dangling key survives (the same
    // side-table teardown discipline as the resource table).
    vaml::detail::remove_element_data_context(this);
    // S511 lifetime: an element that owned an Effect releases it on destruction
    // (the owner unique_ptr is torn down) so no dangling key survives — the same
    // side-table teardown discipline as the resource/DataContext tables.
    vaml::detail::remove_element_effect(this);
    // S502 lifetime: an element that carried a BehaviorCollection releases it
    // (detaching each behavior) so no dangling key survives into static
    // teardown. This runs at the START of ~UIElement — while routedHandlers_
    // (S321) is still alive — so the EventTrigger's remove_routed_handler in
    // on_detach never touches a freed handler store.
    remove_element_behaviors(this);
}

// --- S360 Resources slot (stored OUT-OF-LINE; see ResourceDictionary.cpp) ----

VAML_CORE_EXPORT
void UIElement::set_resources(std::unique_ptr<ResourceDictionary> resources)
{
    // The side-table is populated/owned by ResourceDictionary.cpp (the only TU
    // that knows the complete ResourceDictionary type).
    vaml::detail::set_element_resources(this, std::move(resources));
}

// --- S511 Effect slot ------------------------------------------------

VAML_CORE_EXPORT
void UIElement::set_effect(std::unique_ptr<vaml::Effect> effect)
{
    // The side-table is populated/owned by UIElement.cpp (the only TU that
    // knows the complete Effect type + the side-table).
    vaml::detail::set_element_effect(this, std::move(effect));
}

VAML_CORE_EXPORT
const vaml::Effect* UIElement::effect() const noexcept
{
    return vaml::detail::get_element_effect(this);
}

VAML_CORE_EXPORT
vaml::Effect* UIElement::effect() noexcept
{
    return vaml::detail::get_element_effect(this);
}

VAML_CORE_EXPORT
const ResourceDictionary* UIElement::resources() const noexcept
{
    return vaml::detail::get_element_resources(this);
}

VAML_CORE_EXPORT
ResourceDictionary* UIElement::resources() noexcept
{
    return const_cast<ResourceDictionary*>(
        vaml::detail::get_element_resources(const_cast<const UIElement*>(this)));
}

VAML_CORE_EXPORT
SetResult UIElement::set_visibility(Visibility v)
{
    return Set(PropertyId::of("Visibility", "UIElement"), Value::Int32(static_cast<std::int32_t>(v)));
}

VAML_CORE_EXPORT
Visibility UIElement::visibility() const
{
    return static_cast<Visibility>(Get(PropertyId::of("Visibility", "UIElement")).get<std::int32_t>());
}

VAML_CORE_EXPORT
SetResult UIElement::set_focusable(bool value)
{
    return Set(PropertyId::of("Focusable", "UIElement"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool UIElement::focusable() const
{
    return Get(PropertyId::of("Focusable", "UIElement")).get<std::int32_t>() != 0;
}

// S595a — IsHitTestVisible (Bool DP, default true, NOT inherited). Mirrors the
// set_focusable/focusable pattern; the Bool payload (Value::Bool) is the
// AllowsTransparency/Topmost precedent so the typed accessors + the standard
// Set/Get path on PropertyId("IsHitTestVisible") agree.
VAML_CORE_EXPORT
SetResult UIElement::set_is_hit_test_visible(bool value)
{
    return Set(PropertyId::of("IsHitTestVisible", "UIElement"), Value::Bool(value));
}

VAML_CORE_EXPORT
bool UIElement::is_hit_test_visible() const
{
    return Get(PropertyId::of("IsHitTestVisible", "UIElement")).get<bool>();
}

// S834 — ClipToBounds (Bool DP, default false, NOT inherited, AffectsArrange).
// Mirrors the set_is_hit_test_visible/is_hit_test_visible pattern; the Bool
// payload (Value::Bool) keeps the typed accessors and the standard Set/Get path
// on PropertyId("ClipToBounds") in agreement. WPF ground truth
// UIElement.cs:3338-3356 (owner UIElement, default false, InvalidateArrange on
// change, no Inherits, no ValidateFunc).
VAML_CORE_EXPORT
SetResult UIElement::set_clip_to_bounds(bool value)
{
    return Set(PropertyId::of("ClipToBounds", "UIElement"), Value::Bool(value));
}

VAML_CORE_EXPORT
bool UIElement::clip_to_bounds() const
{
    return Get(PropertyId::of("ClipToBounds", "UIElement")).get<bool>();
}

VAML_CORE_EXPORT
SetResult UIElement::set_opacity(double value)
{
    // Accepts ANY double (no [0,1] ValidateFunc — WPF contract; 1.5, -2, NaN
    // all resolve). The S208 encoder folds it into the visual-ancestor product.
    return Set(PropertyId::of("Opacity", "UIElement"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::opacity() const
{
    // Reads ONLY this element's OWN local Opacity (default 1.0) — Opacity is
    // NOT inherited. The subtree alpha (a child under an Opacity=0.5 parent) is
    // the S208 encoder's ancestor-chain PRODUCT, not a DP-inherited read.
    return Get(PropertyId::of("Opacity", "UIElement")).get<double>();
}

// --- S823 RenderTransform (the S120 transform-owner resolution, Transform-OBJECT) ---

VAML_CORE_EXPORT
SetResult UIElement::set_render_transform(const std::shared_ptr<vaml::Transform>& value)
{
    // S823 (dep:S822): accepts ANY Transform (WPF has no ValidateValueCallback —
    // a non-finite matrix is WPF-legal; the finite-validity gate stays ONLY at
    // the S208 encode boundary, never the DP Set). Set triggers InvalidateArrange:
    // a render transform is a visual change the encoder re-emits, so the
    // arrange-dirty flag (plus its ancestor chain) marks the element for a fresh
    // encode pass.
    const SetResult r = Set(PropertyId::of("RenderTransform", "UIElement"),
                            Value::Transform(value ? value : vaml::Transform::identity()));
    if (r == SetResult::APPLIED)
    {
        InvalidateArrange();
    }
    return r;
}

VAML_CORE_EXPORT
std::shared_ptr<vaml::Transform> UIElement::render_transform() const
{
    return Get(PropertyId::of("RenderTransform", "UIElement"))
        .get<std::shared_ptr<vaml::Transform>>();
}

// --- S823 RenderTransformOrigin (Point, default (0,0), NOT inherited) --------

VAML_CORE_EXPORT
SetResult UIElement::set_render_transform_origin(vaml::Point value)
{
    // A normalized-fraction center (UIElement.cs:1378). Any real point accepted
    // incl. <0/>1 (only NaN/±Inf rejected by WPF IsRenderTransformOriginValid —
    // the DP metadata carries no validate per WPF; the encode boundary folds
    // defensively). NOT inherited (PropertyFlags::None). A visual change → the
    // encoder re-emits.
    const SetResult r = Set(PropertyId::of("RenderTransformOrigin", "UIElement"), Value::Point(value));
    if (r == SetResult::APPLIED)
    {
        InvalidateArrange();
    }
    return r;
}

VAML_CORE_EXPORT
vaml::Point UIElement::render_transform_origin() const
{
    return Get(PropertyId::of("RenderTransformOrigin", "UIElement")).get<vaml::Point>();
}

// --- S500 DataContext (per-element inheritable context, WPF-faithful) --------
//
// S145 OBJECT-POOL BUDGET: the per-element DataContext is stored OUT-OF-LINE in
// a process-wide side-table keyed by element address (the S360 ResourceDictionary
// precedent — UIElement is a fixed object-pool slot size under the S145 test
// gate, so NO inline member may grow it). An element that never carries a local
// DataContext has no side-table entry. A destroyed element is released via
// detail::remove_element_data_context (see the dtor), so no dangling key survives.

VAML_CORE_EXPORT
SetResult UIElement::set_data_context(::vaml::ObservableObject* ctx) noexcept
{
    // Local override semantics (WPF-faithful Inherits): a non-null ctx installs
    // a LOCAL context; nullptr CLEARS the local override (the element then
    // inherits from its parent again). Comparing the resolved current-local with
    // the new value avoids a redundant erase/re-set + the NO_CHANGE return.
    if (detail::get_element_data_context(this) == ctx)
    {
        return SetResult::NO_CHANGE;
    }
    detail::set_element_data_context(this, ctx);
    InvalidateMeasure();
    InvalidateArrange();
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
::vaml::ObservableObject* UIElement::data_context() const noexcept
{
    // The LOCAL context (the side-table entry). nullptr when the element does
    // NOT carry a local override (it inherits from its parent).
    return detail::get_element_data_context(this);
}

VAML_CORE_EXPORT
::vaml::ObservableObject* UIElement::effective_data_context(
    ::vaml::ObservableObject* documentContext) const noexcept
{
    // Walk the parent chain to the NEAREST local DataContext (WPF Inherits with
    // local-wins). This element's own local (if any) wins first; else the first
    // ancestor carrying a local context; else the caller-supplied document
    // context (the reader's app-constructed VM). A `{Binding}` resolves its
    // source against this value.
    for (const DependencyObject* cur = this; cur != nullptr; cur = cur->parent())
    {
        const auto* ue = dynamic_cast<const UIElement*>(cur);
        if (ue != nullptr)
        {
            ObservableObject* local = detail::get_element_data_context(ue);
            if (local != nullptr)
            {
                return local;
            }
        }
    }
    return documentContext;
}

// --- S122 layout slot (RESOLVED BY S122) ------------------------------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::desired_size() const noexcept
{
    return desired_size_;
}

// --- S842.7 UnclippedDesiredSize (the clamp-pair) ---------------------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::unclipped_desired_size() const noexcept
{
    return unclipped_desired_size_;
}

VAML_CORE_EXPORT
bool UIElement::has_unclipped_desired_size() const noexcept
{
    return has_unclipped_desired_size_;
}

VAML_CORE_EXPORT
void UIElement::publish_measure_result(const vaml::Size& desired,
                                       const vaml::Size& unclipped,
                                       bool hasUnclipped)
{
    // The WPF MeasureCore writes: the clamped DesiredSize + the UnclippedDesired
    // SizeBox. FrameworkElement::MeasureCore (final) is the only caller.
    desired_size_              = desired;
    unclipped_desired_size_    = unclipped;
    has_unclipped_desired_size_ = hasUnclipped;
}

VAML_CORE_EXPORT
const vaml::Rect& UIElement::final_rect() const noexcept
{
    return final_rect_;
}

// --- S841 ink (RenderSize) + anchor (layout offset) accessors ---------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::render_size() const noexcept
{
    // The INK — ArrangeOverride's returned size (WPF RenderSize). Set by Arrange
    // from the override's return value; default == final_rect().size because the
    // base ArrangeOverride returns the passed size.
    return render_size_;
}

VAML_CORE_EXPORT
vaml::Point UIElement::layout_offset() const noexcept
{
    // The ANCHOR — the arrange-rect origin (WPF layout offset, SetLayoutOffset
    // analogue FE.cs:4788). Tracked DISTINCT from the ink: the render is placed
    // at this offset, never folded into render_size().
    return layout_offset_;
}

VAML_CORE_EXPORT
vaml::Rect UIElement::ink_rect() const noexcept
{
    // The DRAWABLE box = the ink placed at the anchor (WPF RenderSize at the
    // layout offset). The S208 encoder draws the content here; clip + hit-test
    // read it. == final_rect() by default (ink == slot).
    return vaml::Rect{ layout_offset_.x, layout_offset_.y,
                       render_size_.width, render_size_.height };
}

VAML_CORE_EXPORT
bool UIElement::needs_clip() const noexcept
{
    // S832: the WPF NeedsClipBounds mirror — true iff the last Arrange arranged
    // this element larger than its slot (child-wins-then-clip). The S833
    // conditional scissor (S208) consumes this.
    return needs_clip_;
}

VAML_CORE_EXPORT
const vaml::Rect& UIElement::clip_rect() const noexcept
{
    // S833: the WPF GetLayoutClip rectangular result (the slot-client window).
    // Written by Arrange from compute_final_rect().clip_rect; meaningful only
    // when needs_clip() (the S208 renderer scissors to it then). final_rect_ —
    // NOT this — is the draw box; this is the clip bounds.
    return clip_rect_;
}

VAML_CORE_EXPORT
const vaml::Rect& UIElement::arrange_slot() const noexcept
{
    // S835: the WPF `_finalRect` mirror — the arrange slot the parent last
    // handed this element. Written on EVERY Arrange including the Collapsed
    // early-return (where final_rect_ is deliberately NOT recomputed and
    // arrange_dirty_ stays set). Read-side only (incremental-arrange cache).
    return arrange_slot_;
}

VAML_CORE_EXPORT
SetResult UIElement::set_margin(const vaml::Thickness& value)
{
    // S824: the slot DP owner is "FrameworkElement" (the WPF-true home).
    return Set(PropertyId::of("Margin", "FrameworkElement"), Value::Thickness(value));
}

VAML_CORE_EXPORT
vaml::Thickness UIElement::margin() const
{
    return Get(PropertyId::of("Margin", "FrameworkElement")).get<vaml::Thickness>();
}

VAML_CORE_EXPORT
SetResult UIElement::set_horizontal_alignment(vaml::HorizontalAlignment value)
{
    return Set(PropertyId::of("HorizontalAlignment", "FrameworkElement"), Value::Int32(static_cast<std::int32_t>(value)));
}

VAML_CORE_EXPORT
vaml::HorizontalAlignment UIElement::horizontal_alignment() const
{
    return static_cast<vaml::HorizontalAlignment>(
        Get(PropertyId::of("HorizontalAlignment", "FrameworkElement")).get<std::int32_t>());
}

VAML_CORE_EXPORT
SetResult UIElement::set_vertical_alignment(vaml::VerticalAlignment value)
{
    return Set(PropertyId::of("VerticalAlignment", "FrameworkElement"), Value::Int32(static_cast<std::int32_t>(value)));
}

VAML_CORE_EXPORT
vaml::VerticalAlignment UIElement::vertical_alignment() const
{
    return static_cast<vaml::VerticalAlignment>(
        Get(PropertyId::of("VerticalAlignment", "FrameworkElement")).get<std::int32_t>());
}

// --- Width / Height accessors (WPF-style explicit size DPs) -----------------

VAML_CORE_EXPORT
SetResult UIElement::set_width(double value)
{
    return Set(PropertyId::of("Width", "FrameworkElement"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::width() const
{
    return Get(PropertyId::of("Width", "FrameworkElement")).get<double>();
}

VAML_CORE_EXPORT
SetResult UIElement::set_height(double value)
{
    return Set(PropertyId::of("Height", "FrameworkElement"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::height() const
{
    return Get(PropertyId::of("Height", "FrameworkElement")).get<double>();
}

// --- S842.6: reconcile_axis MOVED to FrameworkElement.cpp (the FE::MeasureCore
// body consumes it; UIElement no longer measures content — its core returns
// {0,0}). ---

// --- S123 Measure contract ------------------------------------------------

VAML_CORE_EXPORT
Size UIElement::MeasureCore(const Size& availableSize)
{
    // S842.6 seam split (WPF UIElement.cs:1030-1033): the UIElement base
    // MeasureCore returns `new Size()` — the element has no content. The REAL
    // measure body (margin strip + Width/Height/Min/Max reconcile + the
    // finalizer) lives on FrameworkElement::MeasureCore (final), which every
    // concrete element derives. Measure() is the pass protocol that calls this.
    (void)availableSize;
    return Size{ 0.0f, 0.0f };
}

// --- S829: WPF "close enough" available-size compare ------------------------
// Mirrors WPF DoubleUtil.AreClose for the measure guard's
// `isCloseToPreviousMeasure` check (UIElement.cs:648-651). Any NaN (the
// un-measured `previous_available_` sentinel, or an explicit-size NaN) compares
// NOT-close, so the first Measure always runs. Single-threaded; no locking
// (ADR-018).
namespace {
bool sizes_close(float a, float b) noexcept
{
    if (std::isnan(a) || std::isnan(b))
    {
        return false; // NaN never close (previous_available_ starts { NaN, NaN })
    }
    if (std::isinf(a) || std::isinf(b))
    {
        return a == b; // only inf==inf counts as close
    }
    return std::fabs(a - b) <= 1e-4f;
}

bool sizes_close(const Size& a, const Size& b) noexcept
{
    return sizes_close(a.width, b.width) && sizes_close(a.height, b.height);
}

// RAII reentrancy-flag guard (exception-safe reset of measure_in_progress_).
struct MeasureReentrancyGuard
{
    bool& flag;
    explicit MeasureReentrancyGuard(bool& f) noexcept : flag(f) { flag = true; }
    ~MeasureReentrancyGuard() noexcept { flag = false; }
    MeasureReentrancyGuard(const MeasureReentrancyGuard&) = delete;
    MeasureReentrancyGuard& operator=(const MeasureReentrancyGuard&) = delete;
};
} // namespace

VAML_CORE_EXPORT
void UIElement::Measure(const Size& available)
{
    // S827/S828/S829 — the WPF-faithful Measure contract.
    //
    // Entry order mirrors UIElement.Measure (UIElement.cs:561-676):
    //   (a) reentrancy guard  (WPF: using(Dispatcher.DisableProcessing()) +
    //       MeasureInProgress — "We cannot survive reentrancy in these paths");
    //   (b) Collapsed skip    (skip measure, KEEP measure_dirty_ set so a reveal
    //       re-measures — UIElement.cs:661-676);
    //   (c) clean early-out   (already-measured + unchanged constraint returns
    //       the cached DesiredSize — UIElement.cs:648-651).
    //
    // Body (S827/S828): the offered `available` is shrunk by Margin (content
    // wraps within its INNER constraint, WPF MeasureCore); MeasureOverride sees
    // that inner size; the finalizer returns the UN-CLAMPED content extent
    // (alignment is arrange-only, compute_measured_desired(natural)); the margin
    // is added back. DesiredSize may therefore EXCEED `available` (WPF
    // soft-constraint / unclippedDesiredSize) — the render slot follows the S832
    // (11ad) child-wins-then-clip contract in compute_final_rect (the child may
    // exceed its slot and reports needs_clip; the clip scissor is the deferred
    // S833).
    if (measure_in_progress_)
    {
        // Reentrancy: a Measure must not recursively re-enter itself. WPF makes
        // this impossible via DisableProcessing; single-threaded VAML guards it
        // with a flag and bails (do NOT recurse, do NOT clear dirty here).
        return;
    }

    const bool isCloseToPrevious = sizes_close(available, previous_available_);

    if (visibility() == Visibility::Collapsed)
    {
        // WPF: a Collapsed element is not measured; KEEP measure_dirty_ set so
        // the next revealing Measure runs. Track a changed constraint so a
        // reveal re-measures at the right size (UIElement.cs:661-676).
        if (!isCloseToPrevious)
        {
            // S830 (WPF `InvalidateMeasureInternal()` at UIElement.cs:601-611,
            // "this will ensure that element will be actually re-measured at
            // the new available size later when it becomes visible"): re-arm
            // THIS element's measure-dirty flag locally. WPF's
            // InvalidateMeasureInternal sets the local dirty bit + drops the
            // pending request WITHOUT propagating to ancestors — so this is a
            // direct flag write, not InvalidateMeasure() (which would walk up).
            measure_dirty_ = true;
            previous_available_ = available;
        }
        return;
    }

    if (!measure_dirty_ && !never_measured_ && isCloseToPrevious)
    {
        // Clean + same constraint: keep the cached DesiredSize, don't recompute
        // (WPF IsMeasureValid && !neverMeasured && isCloseToPreviousMeasure).
        return;
    }

    // S839 (D2 — RESOLVES the S126 "independent axes" property): a real Measure
    // pass ALWAYS re-arms arrange — "we always want to be arranged, ensure
    // arrange request" (WPF UIElement.cs:629-631, issued before MeasureCore).
    // This VOIDS the S126 claim that measure and arrange are independent axes:
    // an element that re-measures is always re-arranged. (The clean early-out
    // above has already returned, so this fires only on an actual measure.)
    // S840 (WPF UIElement.cs:626): clear NeverMeasured BEFORE entering the
    // MeasureCore body. Source order is bypass -> NeverMeasured=false ->
    // InvalidateArrange -> MeasureInProgress=true, so a REENTRANT read during
    // the pass already observes neverMeasured==false.
    never_measured_ = false;
    InvalidateArrange();

    const MeasureReentrancyGuard reentrancy_guard(measure_in_progress_);

    // S842.7: capture the DesiredSize BEFORE this pass — the WPF `Size prevSize =
    // _desiredSize;` (UIElement.cs:620) that feeds the measure-side waterfall.
    // MUST be read before MeasureCore (which publishes the new clamped size).
    const Size prevSize = desired_size_;

    // S842.6 seam split: the REAL measure body (the 22-leg WPF MeasureCore) now
    // lives on FrameworkElement::MeasureCore (final). UIElement::Measure runs the
    // WPF pass protocol (guards above) and delegates here. MeasureCore publishes
    // the clamped DesiredSize + the UnclippedDesiredSize SizeBox (S842.7).
    (void)MeasureCore(available);

    previous_available_ = available;
    // A completed Measure pass clears this element's measure-dirty flag (S126).
    // (NeverMeasured was already cleared BEFORE the body -- S840 / UIElement.cs:626.)
    measure_dirty_ = false;
    vaml::detail::LayoutContext::current().remove_measure(this);
    // S839: drop the pending measure queue slot now the pass completed (WPF
    // MeasureQueue.Remove(this)). A no-op when not queued (a host-driven
    // measure). The unconditional InvalidateArrange() above re-queued arrange.

    // S830 (WPF UIElement.cs:687-700 — "notify parent if our desired size
    // changed (waterfall effect)"): a Measure that CHANGED the DesiredSize must
    // dirty the parent, because the parent's own DesiredSize derives from it.
    //   * `!MeasureDuringArrange` — VAML's Arrange never re-enters Measure (the
    //     S127 ArrangeOverride drives children by Arrange only), so that guard
    //     has no analogue here; recorded, not silently dropped.
    //   * `!p.MeasureInProgress` — the load-bearing guard: during the parent's
    //     OWN MeasureOverride the parent is already in progress and therefore
    //     already recomputing, so signalling it would be redundant (and, on the
    //     FIRST pass, would wrongly leave the whole chain dirty).
    // The comparison is WPF's `!DoubleUtil.AreClose(prevSize, desiredSize)`.
    if (!sizes_close(prevSize, desired_size_))
    {
        // S839: the ARRANGE request that a changed DesiredSize implies is now
        // issued UNCONDITIONALLY above (before MeasureOverride) — the S126
        // conditional InvalidateArrange that lived here is REMOVED (D2 voided;
        // the arrange re-arm is no longer gated on a size change). What remains
        // here is ONLY the measure-side waterfall: notify the nearest UIElement
        // ancestor so its own DesiredSize recomputes (UIElement.cs:686-700).
        for (DependencyObject* p = parent(); p != nullptr; p = p->parent())
        {
            auto* ue = dynamic_cast<UIElement*>(p);
            if (ue == nullptr)
            {
                continue; // not a layout participant — keep walking up
            }
            if (!ue->measure_in_progress_)
            {
                ue->OnChildDesiredSizeChanged(this);
            }
            break; // nearest UIElement ancestor only (WPF GetUIParentOrICH)
        }
    }
}

// --- S830: OnChildDesiredSizeChanged / signalDesiredSizeChange --------------

VAML_CORE_EXPORT
void UIElement::OnChildDesiredSizeChanged(UIElement* child)
{
    // WPF UIElement.cs:323-330 — the default body verbatim:
    //     if (IsMeasureValid) { InvalidateMeasure(); }
    // `IsMeasureValid` == !MeasureDirty, so this re-dirties an ancestor that is
    // currently clean (an already-dirty ancestor needs no re-walk — the chain
    // above it is dirty from when it was first dirtied).
    (void)child;
    if (!measure_dirty_)
    {
        InvalidateMeasure();
    }
}

VAML_CORE_EXPORT
void UIElement::signal_desired_size_change()
{
    // WPF UIElement.cs:3043-3056 — `GetUIParentOrICH` then
    // `p.OnChildDesiredSizeChanged(this)`. Unlike the measure-side waterfall
    // there is NO `MeasureInProgress` guard here (WPF says so explicitly in the
    // UIElement.cs:687-700 comment: "this is what differs this code from
    // signalDesiredSizeChange()"), so the call is unconditional.
    for (DependencyObject* p = parent(); p != nullptr; p = p->parent())
    {
        auto* ue = dynamic_cast<UIElement*>(p);
        if (ue == nullptr)
        {
            continue; // not a layout participant — keep walking up
        }
        ue->OnChildDesiredSizeChanged(this);
        break; // nearest UIElement ancestor only
    }
}

// --- S124 Arrange contract --------------------------------------------------

VAML_CORE_EXPORT
void UIElement::ArrangeCore(const Rect& finalRect)
{
    // S842.6 seam split (WPF UIElement.cs ArrangeCore base): the UIElement base
    // arrange writes only the minimal RenderSize/offset state — the element
    // fills its slot by default. The REAL arrange body (child-wins-then-clip
    // final-rect math + margin/alignment/ink/offset) lives on
    // FrameworkElement::ArrangeCore (final), which every concrete element
    // derives. Behavior preserved 1:1 for a bare UIElement (== the old
    // compute_final_rect result for a zero-desired Stretch element: fills the
    // slot, no clip).
    final_rect_    = finalRect;
    layout_offset_ = vaml::Point{ finalRect.x, finalRect.y };
    render_size_   = vaml::Size{ finalRect.width, finalRect.height };
    needs_clip_    = false;
    clip_rect_     = vaml::Rect{};
}

VAML_CORE_EXPORT
bool UIElement::get_layout_clip(const vaml::Size& layoutSlotSize, vaml::Rect& clipRect)
{
    // S842.6 seam split (WPF UIElement.GetLayoutClip, UIElement.cs:3322-3333):
    //   if (ClipToBounds) return new RectangleGeometry(new Rect(RenderSize));
    //   else return null;
    // The FrameworkElement override (FE.cs:4946-5105) supplies the 5-branch
    // slot-client clip. Base produces a clip only under ClipToBounds.
    (void)layoutSlotSize;
    if (clip_to_bounds())
    {
        clipRect = vaml::Rect{ 0.0f, 0.0f, render_size_.width, render_size_.height };
        return true;
    }
    return false;
}

VAML_CORE_EXPORT
void UIElement::Arrange(const Rect& arrangeSlot)
{
    // S835 (dep:S834) — the WPF Arrange guard spectrum (UIElement.cs:786-906),
    // mirroring the S829 measure-entry sibling (11ac):
    //   (a) Inf/NaN slot     -> throw std::invalid_argument (WPF
    //       InvalidOperationException; S810_1 diagnostic form).
    //   (b) Visibility::Collapsed -> store the slot (_finalRect), KEEP dirty,
    //       return WITHOUT arranging (re-arranged on reveal).
    //   (c) MeasureDirty || NeverMeasured -> backfill Measure first (833-862).
    //   (d) end-of-arrange ensureClip — refresh the render-slot clip per the
    //       S833/S834 gate (ensureClip analogue).
    //
    // The S124 contract is the body BELOW the guards: Arrange(arrangeSlot) ->
    // final_rect(). final_rect() is BOTH the render slot (where S208 draws)
    // and the basis for clip bounds. S127 auto-drive: after computing
    // final_rect(), Arrange invokes ArrangeOverride(final_rect()) so a panel
    // places its children into its OWN render slot (LayoutManager -> root
    // Arrange -> panel ArrangeOverride -> children arranged).

    // (a) Guard: a layout slot must be finite. WPF rejects +Infinity and NaN on
    //     Width/Height only (UIElement.cs:798-809 — IsPositiveInfinity/IsNaN);
    //     it does NOT test -Infinity — faithfully mirrored (a -Inf extent
    //     degenerates to a 0-size slot client in compute_final_rect because the
    //     inset clamps >= 0, so no -Inf escapes the extent). Named property +
    //     offending value per the S810_1 disposition.
    // S839: ArrangeInProgress reentrancy guard (RAII — resets on every return /
    // throw path). Mirrors WPF ArrangeInProgress; consumed by the arrange-axis
    // canRelyOnParentRecalc in LayoutContext.
    const MeasureReentrancyGuard arrange_guard(arrange_in_progress_);

    if (std::isnan(arrangeSlot.width) || (std::isinf(arrangeSlot.width) && arrangeSlot.width > 0.0f) ||
        std::isnan(arrangeSlot.height) || (std::isinf(arrangeSlot.height) && arrangeSlot.height > 0.0f))
    {
        throw std::invalid_argument(
            "UIElement::Arrange: arrangeSlot has a non-finite extent (width=" +
            std::to_string(arrangeSlot.width) + ", height=" +
            std::to_string(arrangeSlot.height) +
            "); a layout slot must be a finite Rect "
            "(WPF UIElement.Arrange InvalidOperationException)");
    }

    // (b) Collapsed: store the slot, KEEP the dirty bit, commit NO layout
    //     (UIElement.cs:811-828). No ArrangeOverride, no final_rect_ /
    //     needs_clip_ / clip_rect_ change — the element is re-measured /
    //     re-arranged on reveal. WPF: `_finalRect = finalRect; return;`.
    if (visibility() == Visibility::Collapsed)
    {
        arrange_slot_ = arrangeSlot;
        return;
    }

    // (c) Backfill Measure when measure-dirty or never measured
    //     (UIElement.cs:833-862). NeverMeasured -> Measure(the slot size, the
    //     WPF "set size" arrange-only path); otherwise -> Measure(the cached
    //     PreviousConstraint). This mirrors WPF's ArrangeCore prerequisite so a
    //     never-measured element arranged directly still gets a real DesiredSize.
    if (measure_dirty_ || never_measured_)
    {
        if (never_measured_)
        {
            Measure(Size{ arrangeSlot.width, arrangeSlot.height });
        }
        else
        {
            Measure(previous_available_);
        }
    }

    // S842.6 seam split: the REAL arrange body (child-wins-then-clip
    // compute_final_rect + margin/alignment/ink/offset + the GetLayoutClip
    // slot-client rect) now lives on FrameworkElement::ArrangeCore (final),
    // which publishes final_rect_/layout_offset_/render_size_/needs_clip_/
    // clip_rect_ atomically. UIElement::Arrange runs the WPF pass protocol
    // (guards above + tail below) and delegates. Behavior preserved 1:1.
    ArrangeCore(arrangeSlot);

    // (d) ensureClip (WPF UIElement.cs:884 -> 3056): ArrangeCore already
    //     published the slot-client clip into clip_rect_; this refresh keeps the
    //     WPF "ensureClip(finalRect.Size)" call site in Arrange (idempotent). The
    //     S208 renderer applies it only when needs_clip() || clip_to_bounds().
    ensure_clip(clip_rect());

    // WPF tail: `_finalRect = finalRect; ArrangeDirty = false;`.
    arrange_slot_ = arrangeSlot;
    arrange_dirty_ = false;
    // S839: clear the NeverArranged flag + drop the pending arrange queue slot
    // (WPF ArrangeQueue.Remove(this)). A no-op when not queued (host-driven).
    never_arranged_ = false;
    vaml::detail::LayoutContext::current().remove_arrange(this);
}

VAML_CORE_EXPORT
void UIElement::ensure_clip(const vaml::Rect& clipRect)
{
    // The WPF ensureClip(finalRect.Size) analogue (UIElement.cs:884 -> 3056,
    // GetLayoutClip FE.cs:4862-4960): the slot-client clip window computed by
    // compute_final_rect. Refreshed on EVERY arrange so a prior slot's clip
    // never lingers; the S208 renderer scissors to it ONLY when
    // needs_clip() || clip_to_bounds() (S833/S834).
    clip_rect_ = clipRect;
}

// --- S126 layout invalidation ------------------------------------------------

VAML_CORE_EXPORT
void UIElement::InvalidateMeasure()
{
    // S839 (D1 — RESOLVES the S126 unconditional ancestor walk): set ONLY the
    // local dirty bit and enlist in the per-thread layout context (WPF
    // UIElement.cs:255-266). There is NO parent walk here. Ancestors are dirtied
    // lazily by the measure waterfall (OnChildDesiredSizeChanged) when a child's
    // DesiredSize actually changes. `!never_measured_` mirrors WPF: a fresh
    // element is not enqueued (the host measures it); its local bit is already
    // set from construction.
    // S840 (D1, WPF UIElement.cs:249-274): coalesce AND guard reentrancy — do
    // NOT re-dirty / re-enqueue while a Measure pass is in progress on this
    // element (the source guard is `if (!MeasureDirty && !MeasureInProgress)`;
    // its negation is exactly this early-return).
    if (measure_dirty_ || measure_in_progress_)
    {
        return; // already dirty/queued, or currently measuring (suppress)
    }
    measure_dirty_ = true;
    if (!never_measured_)
    {
        vaml::detail::LayoutContext::current().add_measure(this);
    }
}

VAML_CORE_EXPORT
void UIElement::InvalidateArrange()
{
    // S839 (D1) — mirror of InvalidateMeasure on the arrange axis: local bit +
    // context enlist (WPF UIElement.cs:284-306). No parent walk. A fresh
    // (never-arranged) element is not enqueued (the host arranges it).
    // S840 (D1, WPF UIElement.cs:283-299): mirror the measure-axis guard —
    // coalesce AND suppress while an Arrange pass is in progress on this
    // element (`if (!ArrangeDirty && !ArrangeInProgress)`).
    if (arrange_dirty_ || arrange_in_progress_)
    {
        return;
    }
    arrange_dirty_ = true;
    if (!never_arranged_)
    {
        vaml::detail::LayoutContext::current().add_arrange(this);
    }
}

VAML_CORE_EXPORT
bool UIElement::is_measure_dirty() const noexcept
{
    // S840 NEEDS-PASS BRIDGE (RETIRED BY S842): the S128 panel/context gates
    // consume this as "does this element need a Measure pass". A fresh element
    // has a CLEAR raw MeasureDirty bit (WPF CoreFlags default) but its
    // NeverMeasured bit is SET, so it DOES need a pass -- fold the never bit in
    // so every existing gate stays correct until S842 measures unconditionally.
    // (The raw `measure_dirty_` bit is still what Measure's clean early-out and
    // the arrange backfill read -- WPF-faithful IsMeasureValid semantics.)
    return measure_dirty_ || never_measured_;
}

VAML_CORE_EXPORT
bool UIElement::is_arrange_dirty() const noexcept
{
    // S840 NEEDS-PASS BRIDGE (RETIRED BY S842) -- arrange mirror of the above.
    return arrange_dirty_ || never_arranged_;
}

VAML_CORE_EXPORT
bool UIElement::is_layout_dirty() const noexcept
{
    return is_measure_dirty() || is_arrange_dirty();
}

// --- S842a pass scaffolding (WPF UIElement/FrameworkElement) ----------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::previous_constraint() const noexcept
{
    // WPF PreviousConstraint == _previousAvailableSize (the same store as
    // previous_available()); exposed under the WPF name for the ArrangeCore
    // backfill (S842b).
    return previous_available_;
}

VAML_CORE_EXPORT
bool UIElement::is_renderable() const
{
    // WPF UIElement.IsRenderable (UIElement.cs:1080-1099): a fresh element has no
    // accumulated state to draw; a Collapsed element reports RenderSize (0,0);
    // otherwise require BOTH axes valid.
    if (never_measured_ || never_arranged_)
    {
        return false;
    }
    if (visibility() == Visibility::Collapsed)
    {
        return false;
    }
    return is_measure_valid() && is_arrange_valid();
}

VAML_CORE_EXPORT
bool UIElement::rendering_invalidated() const noexcept
{
    return rendering_invalidated_;
}

VAML_CORE_EXPORT
void UIElement::invalidate_render()
{
    // WPF RenderingInvalidated set (the "repaint requested" half of InvalidateVisual).
    rendering_invalidated_ = true;
}

VAML_CORE_EXPORT
void UIElement::on_child_desired_size_changed(UIElement& child)
{
    // WPF OnChildDesiredSizeChanged base body is empty; a FrameworkElement may
    // override to invalidate its measure. Base no-op (the S842b waterfall calls it).
    (void)child;
}

VAML_CORE_EXPORT
bool UIElement::use_layout_rounding_flag() const noexcept
{
    return use_layout_rounding_;
}

VAML_CORE_EXPORT
void UIElement::set_use_layout_rounding_flag(bool value) noexcept
{
    use_layout_rounding_ = value;
}

VAML_CORE_EXPORT
bool UIElement::are_transforms_clean() const noexcept
{
    return are_transforms_clean_;
}

VAML_CORE_EXPORT
void UIElement::set_are_transforms_clean(bool value) noexcept
{
    are_transforms_clean_ = value;
}

VAML_CORE_EXPORT
bool UIElement::measure_during_arrange() const noexcept
{
    return measure_during_arrange_;
}

VAML_CORE_EXPORT
bool UIElement::has_width_ever_changed() const noexcept
{
    return has_width_ever_changed_;
}

VAML_CORE_EXPORT
bool UIElement::has_height_ever_changed() const noexcept
{
    return has_height_ever_changed_;
}

// --- S842a arrange-state publish seam --------------------------------------

VAML_CORE_EXPORT
void UIElement::publish_arrange_result(const vaml::Rect& finalRect,
                                       vaml::Point layoutOffset,
                                       vaml::Size renderSize,
                                       bool needsClip,
                                       const vaml::Rect& clipRect)
{
    // The WPF ArrangeCore state writes (RenderSize / SetLayoutOffset /
    // NeedsClipBounds / GetLayoutClip rect) applied atomically through one seam.
    final_rect_    = finalRect;
    layout_offset_ = layoutOffset;
    render_size_   = renderSize;
    needs_clip_    = needsClip;
    clip_rect_     = clipRect;
}

VAML_CORE_EXPORT
bool UIElement::mark_for_size_changed_if_needed(vaml::Size oldSize, vaml::Size newSize)
{
    // WPF UIElement.markForSizeChangedIfNeeded (UIElement.cs:1478-1516):
    // widthChanged/heightChanged via DoubleUtil.AreClose. Sets the
    // HasWidth/HeightEverChanged bits (the OnRenderSizeChanged side-effect that
    // drives ActualWidth/ActualHeight) but does NOT raise SizeChanged here.
    const bool widthChanged  = !sizes_close(oldSize.width, newSize.width);
    const bool heightChanged = !sizes_close(oldSize.height, newSize.height);
    if (widthChanged)
    {
        has_width_ever_changed_ = true;
    }
    if (heightChanged)
    {
        has_height_ever_changed_ = true;
    }
    return widthChanged || heightChanged;
}

// --- S840 distinct WPF bit accessors (UIElement.cs:108-109 / :225-240) ------
//
// The raw dirty bits and the never-* bits are DISTINCT states (WPF keeps them
// in separate CoreFlags arms). `is_measure_valid()`/`is_arrange_valid()` mirror
// WPF IsMeasureValid/IsArrangeValid exactly (`!MeasureDirty` / `!ArrangeDirty`):
// a fresh element is "valid" yet never-measured -- the two are not the same.

VAML_CORE_EXPORT
bool UIElement::is_measure_valid() const noexcept
{
    return !measure_dirty_;
}

VAML_CORE_EXPORT
bool UIElement::is_arrange_valid() const noexcept
{
    return !arrange_dirty_;
}

VAML_CORE_EXPORT
bool UIElement::never_measured() const noexcept
{
    return never_measured_;
}

VAML_CORE_EXPORT
bool UIElement::never_arranged() const noexcept
{
    return never_arranged_;
}

// --- S839 layout-context accessors -----------------------------------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::previous_available() const noexcept
{
    return previous_available_;
}

VAML_CORE_EXPORT
bool UIElement::is_measure_in_progress() const noexcept
{
    return measure_in_progress_;
}

VAML_CORE_EXPORT
bool UIElement::is_arrange_in_progress() const noexcept
{
    return arrange_in_progress_;
}

VAML_CORE_EXPORT
int UIElement::tree_level() const noexcept
{
    return tree_level_;
}

// --- S818 render invalidation (InvalidateVisual) ----------------------------

VAML_CORE_EXPORT
void UIElement::InvalidateVisual()
{
    // WPF UIElement.InvalidateVisual: mark THIS element for re-render. It does
    // NOT walk the ancestor chain — a child's repaint is this element's own
    // concern; ancestors re-render via their OWN invalidation (the Affects* /
    // layout-seam). The render-dirty flag is consumed by the S208/S210
    // render-dirty seam so a repaint of this element is scheduled.
    render_dirty_ = true;
}

VAML_CORE_EXPORT
bool UIElement::is_render_dirty() const noexcept
{
    return render_dirty_;
}

// --- S818 OnPropertyChanged (the FE.cs:2198-2251 Affects* engine) -----------

VAML_CORE_EXPORT
void UIElement::OnPropertyChanged(
    PropertyId id,
    const Value& oldValue,
    const Value& newValue,
    PropertyFlags flags)
{
    // Run the WPF `FrameworkElement.OnPropertyChanged` Affects* invalidation
    // (FE.cs:2198-2251), EXACTLY — in the same order WPF checks the arms.

    // 0) S830 (WPF OnVisibilityChanged → switchVisibilityIfNeeded,
    //    UIElement.cs:2938-2954 / 2976-2994 / 2984-3034): a
    //    Collapsed <-> non-Collapsed transition signals a DESIRED-SIZE change to
    //    the parent, which is what makes a reveal re-measure the ancestor chain.
    //      * Visible→Collapsed : ensureInvisible(true) — sets IsCollapsed, then
    //        signalDesiredSizeChange().
    //      * Collapsed→Visible : ensureVisible() — clears IsCollapsed, then
    //        signalDesiredSizeChange().
    //      * Hidden↔Visible and Collapsed↔Hidden do NOT flip IsCollapsed, so
    //        they signal nothing (WPF guards BOTH edges on the flag, not on the
    //        enum value).
    //    Visibility carries NO AffectsMeasure in WPF (the DP is a plain
    //    PropertyMetadata + ValidateValueCallback) and none here, so this change
    //    callback is the ONLY measure signal a Visibility write produces. It
    //    runs for the Collapsed SKIP path in Measure (which keeps the dirty bit
    //    but produces no DesiredSize change) — hence a separate call site from
    //    the measure-side waterfall.
    if (id == PropertyId::of("Visibility", "UIElement"))
    {
        const bool was_collapsed =
            oldValue.get<std::int32_t>() == static_cast<std::int32_t>(Visibility::Collapsed);
        const bool now_collapsed =
            newValue.get<std::int32_t>() == static_cast<std::int32_t>(Visibility::Collapsed);
        if (was_collapsed != now_collapsed)
        {
            // S830: the WPF-side render invalidation in ensureVisible
            // (`InvalidateVisual()` — re-render properties that changed while
            // Collapsed) has no VAML analogue: this engine has no opacity
            // suppression / IsRenderable gate. Recorded, not silently dropped.
            signal_desired_size_change();
        }
    }

    // 1) AffectsParentMeasure / AffectsParentArrange: walk to the NEAREST
    //    UIElement layout parent and invalidate its measure/arrange
    //    (FE.cs:2219 / 2234). WPF walks the VisualTreeHelper.GetParent chain up
    //    to the nearest UIElement that is a layout parent and invalidates THAT
    //    one (and stops — the nearest only).
    if (has_flag(flags, PropertyFlags::AffectsParentMeasure) ||
        has_flag(flags, PropertyFlags::AffectsParentArrange))
    {
        const bool parentMeasure = has_flag(flags, PropertyFlags::AffectsParentMeasure);
        const bool parentArrange = has_flag(flags, PropertyFlags::AffectsParentArrange);
        // FE.cs:2219/2234 — walk the parent chain to the NEAREST UIElement that
        // is a layout parent and invalidate THAT one, then stop. A non-UIElement
        // ancestor cannot own a layout pass, so it is SKIPPED (the walk
        // continues past it) — only the first UIElement ancestor (the nearest
        // layout parent) is invalidated, and the walk stops there.
        for (DependencyObject* p = parent(); p != nullptr; p = p->parent())
        {
            auto* ue = dynamic_cast<UIElement*>(p);
            if (ue == nullptr)
            {
                // Not a layout participant — keep walking toward the nearest
                // UIElement ancestor (the nearest layout parent).
                continue;
            }
            if (parentMeasure)
            {
                ue->InvalidateMeasure();
            }
            if (parentArrange)
            {
                ue->InvalidateArrange();
            }
            // Nearest layout parent only: we have found and invalidated the
            // nearest one — do not walk further up.
            break;
        }
    }

    // 2) AffectsMeasure -> invalidate own measure (FE.cs:2241).
    if (has_flag(flags, PropertyFlags::AffectsMeasure))
    {
        InvalidateMeasure();
    }

    // 3) AffectsArrange -> invalidate own arrange (FE.cs:2243).
    if (has_flag(flags, PropertyFlags::AffectsArrange))
    {
        InvalidateArrange();
    }

    // 4) AffectsRender -> InvalidateVisual (FE.cs:2251). WPF's exact gate is
    //    `if (e.IsAValueChange || !SubPropertiesDoNotAffectRender)`. In VAML a
    //    Set() that reaches OnPropertyChanged is ALWAYS a value change
    //    (NO_CHANGE does not fire the call-out), so `e.IsAValueChange` is
    //    always TRUE here and the visual ALWAYS invalidates on an AffectsRender
    //    property. The `SubPropertiesDoNotAffectRender` arm only suppresses
    //    NON-value (sub-property) changes — which never arrive through Set()'s
    //    real-change call-out — so a future sub-property call site would gate on
    //    it explicitly there. Carried + readable here for the S819 record.
    if (has_flag(flags, PropertyFlags::AffectsRender))
    {
        InvalidateVisual();
    }

    // 5) S842a (WPF FrameworkElement invalidation callbacks, FE.cs:3423-3429 /
    //    UIElement.cs:1064-1083): the LayoutTransform + Width/Height/Min*/Max*
    //    change callbacks (OnTransformDirty / OnLayoutTransformChanged) and the
    //    RenderTransformOrigin callback all set AreTransformsClean = false (the
    //    transform/geometry must be recomputed at the next arrange). The
    //    UseLayoutRounding change handler (OnUseLayoutRoundingChanged) mirrors
    //    the value into the VisualFlags.UseLayoutRounding bit. Recorded state;
    //    the flags' AffectsMeasure/AffectsArrange arms above already drive the
    //    invalidation, so this is the WPF-faithful ADDITIONAL bookkeeping.
    if (id == PropertyId::of("LayoutTransform", "FrameworkElement") ||
        id == PropertyId::of("Width",   "FrameworkElement") ||
        id == PropertyId::of("Height",  "FrameworkElement") ||
        id == PropertyId::of("MinWidth",  "FrameworkElement") ||
        id == PropertyId::of("MaxWidth",  "FrameworkElement") ||
        id == PropertyId::of("MinHeight", "FrameworkElement") ||
        id == PropertyId::of("MaxHeight", "FrameworkElement") ||
        id == PropertyId::of("RenderTransformOrigin", "UIElement"))
    {
        are_transforms_clean_ = false;
    }
    if (id == PropertyId::of("UseLayoutRounding", "FrameworkElement"))
    {
        use_layout_rounding_ = newValue.get<bool>();
    }
}

// --- S121 visual children ---------------------------------------------------

// Internal attach: owns `child`, wires the S039/S041 parent link + inheritance-
// scope edge, appends as the LAST visual child. Reparent discipline: a child
// that ALREADY has a parent cannot be attached (single-parent ownership
// invariant) — detach first via take_child/remove_child.
VAML_CORE_EXPORT
void UIElement::attach_child(std::unique_ptr<UIElement> child)
{
    if (child == nullptr)
    {
        throw std::invalid_argument("UIElement::attach_child: null child");
    }
    if (child->parent() != nullptr)
    {
        throw std::logic_error(
            "UIElement::add_child: child already has a parent; detach it first (take_child/remove_child)");
    }
    UIElement* raw = child.get();
    // set_parent(this) installs the parent link so a truly-INHERITED property
    // (e.g. DataContext) cascades to the visual child through the S039 ancestor
    // pull walk, BEFORE owning. Order: install the parent link BEFORE owning, so
    // any ancestor-walking read inside an observer sees a coherent tree. Note
    // Opacity is NOT one of those: it is PropertyFlags::None (NOT inherited) —
    // the visual edge merely participates in the S208 encoder's ancestor-chain
    // opacity PRODUCT; the DP itself never cascades.
    raw->set_parent(this);
    children_.push_back(std::move(child));
    // S839: assign the child subtree's TreeLevel (WPF Visual.TreeLevel at
    // connect) so the layout drain can select the TOPMOST dirty element.
    raw->set_subtree_tree_level(tree_level_ + 1);
    // S829 (WPF AffectsMeasure/Arrange): adding a visual child changes this
    // element's DesiredSize + layout, so invalidate. Required for the S829
    // clean-element early-out to be correct — without it, a re-measure at an
    // unchanged constraint would return a STALE cached DesiredSize (WPF's
    // Panel.Children.Add invalidates measure the same way).
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
void UIElement::set_subtree_tree_level(int level) noexcept
{
    tree_level_ = level;
    for (auto& c : children_)
    {
        c->set_subtree_tree_level(level + 1);
    }
}

VAML_CORE_EXPORT
void UIElement::add_child(std::unique_ptr<UIElement> child)
{
    attach_child(std::move(child));
}

VAML_CORE_EXPORT
std::size_t UIElement::remove_child(std::size_t index)
{
    if (index >= children_.size())
    {
        throw std::out_of_range("UIElement::remove_child: index out of range");
    }
    auto it = children_.begin() + static_cast<std::ptrdiff_t>(index);
    UIElement* child = it->get();
    // Unwind the S041 inheritance-scope edge + parent link BEFORE destroying so
    // no dangling scope edge survives the removal (NO-LAZY).
    child->set_parent(nullptr);
    children_.erase(it); // destroys the owned child
    // S829 (WPF AffectsMeasure/Arrange): removing a child changes this element's
    // DesiredSize + layout — invalidate so the next Measure recomputes.
    InvalidateMeasure();
    InvalidateArrange();
    return children_.size();
}

VAML_CORE_EXPORT
std::unique_ptr<UIElement> UIElement::take_child(std::size_t index)
{
    if (index >= children_.size())
    {
        throw std::out_of_range("UIElement::take_child: index out of range");
    }
    auto it = children_.begin() + static_cast<std::ptrdiff_t>(index);
    UIElement* child = it->get();
    // Unwind the parent link + inheritance-scope edge BEFORE transferring so the
    // detached child is fully cut from this parent (its OWN children keep their
    // sub-tree intact).
    child->set_parent(nullptr);
    std::unique_ptr<UIElement> detached = std::move(*it);
    children_.erase(it);
    // S829 (WPF AffectsMeasure/Arrange): a detached child no longer contributes
    // to this element's DesiredSize — invalidate so the next Measure recomputes.
    InvalidateMeasure();
    InvalidateArrange();
    return detached;
}

VAML_CORE_EXPORT
std::size_t UIElement::child_count() const noexcept
{
    return children_.size();
}

VAML_CORE_EXPORT
UIElement* UIElement::child_at(std::size_t index) const noexcept
{
    if (index >= children_.size())
    {
        return nullptr;
    }
    return children_[index].get();
}

VAML_CORE_EXPORT
void UIElement::children(std::vector<UIElement*>& out) const noexcept
{
    // S144: out-param accessor (no return-by-value copy). The caller supplies
    // the sink; this writes the non-owning child pointers in index order.
    out.clear();
    out.reserve(children_.size());
    for (const auto& c : children_)
    {
        out.push_back(c.get());
    }
}

VAML_CORE_EXPORT
void UIElement::walk(const std::function<void(UIElement&)>& visitor) const
{
    // Depth-first PRE-ORDER walk: this element first (if it has a non-null
    // visitor), then each child in index order, recursively. The visitor may
    // freely read owner-free (non-owning) state; the tree is never mutated here.
    if (visitor)
    {
        visitor(const_cast<UIElement&>(*this));
    }
    for (const auto& c : children_)
    {
        c->walk(visitor);
    }
}

// --- S321 routed-event engine (bubbling + tunneling + handled) --------------

VAML_CORE_EXPORT
UIElement::RoutedHandlerToken UIElement::add_routed_handler(
    const RoutedEvent& event, RoutedHandler handler)
{
    // Lazily allocate the store (empty by default — zero cost for an element
    // with no routed handlers). The token is additive + reentrancy-safe.
    if (!routedHandlers_)
    {
        routedHandlers_ = std::make_unique<RoutedHandlerStore>();
    }
    auto& slot = routedHandlers_->byEvent[event.name()];
    if (!slot)
    {
        slot = std::make_unique<Signal<void(RoutedEventArgs&)>>();
    }
    return slot->add(std::move(handler));
}

VAML_CORE_EXPORT
bool UIElement::remove_routed_handler(const RoutedEvent& event,
                                      UIElement::RoutedHandlerToken token)
{
    if (!routedHandlers_)
    {
        return false;
    }
    const auto it = routedHandlers_->byEvent.find(event.name());
    if (it == routedHandlers_->byEvent.end() || !it->second)
    {
        return false;
    }
    return it->second->remove(token);
}

VAML_CORE_EXPORT
void UIElement::RaiseEvent(const RoutedEvent& event, RoutedEventArgs& args)
{
    // Establish the original source once (the element the raise started from);
    // `args.source` is set per hop as the route visits each element.
    if (args.originalSource == nullptr)
    {
        args.originalSource = args.source;
    }

    if (event.direction() == RouteEventDirection::Bubble)
    {
        // BUBBLING: source -> ancestors -> root.
        UIElement* cur = static_cast<UIElement*>(args.source);
        while (cur != nullptr)
        {
            _raise_at(cur, event, args);
            if (args.handled)
            {
                break; // a handler stopped the bubble (later ancestors stopped)
            }
            DependencyObject* p = cur->parent();
            cur = (p != nullptr) ? dynamic_cast<UIElement*>(p) : nullptr;
        }
    }
    else
    {
        // TUNNELING (preview): root -> ... -> source. A handler setting
        // `handled` during the tunnel stops it (the source/bubble never sees).
        // Collect the path source -> ... -> root, then walk it reversed.
        std::vector<UIElement*> path;
        for (UIElement* c = static_cast<UIElement*>(args.source); c != nullptr;)
        {
            path.push_back(c);
            DependencyObject* p = c->parent();
            c = (p != nullptr) ? dynamic_cast<UIElement*>(p) : nullptr;
        }
        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            _raise_at(*it, event, args);
            if (args.handled)
            {
                break;
            }
        }
    }
}

VAML_CORE_EXPORT
void UIElement::_raise_at(UIElement* element, const RoutedEvent& event,
                          RoutedEventArgs& args)
{
    if (element == nullptr)
    {
        return;
    }
    args.source = element;
    if (!element->routedHandlers_)
    {
        return;
    }
    const auto it = element->routedHandlers_->byEvent.find(event.name());
    if (it == element->routedHandlers_->byEvent.end() || !it->second)
    {
        return;
    }
    // Fire this element's handlers in registration order, stopping when a
    // handler sets args.handled = true (a handled event stops the whole route,
    // including the later handlers of THIS element — WPF-faithful).
    it->second->fire_until(args, [&](RoutedEventArgs& a) { return a.handled; });
}

// --- S323 hit-testing -------------------------------------------------------

namespace {

// True iff (x, y) lies within `r` (half-open: the right/bottom edges are
// EXCLUSIVE, so a point at exactly right()/bottom() is NOT inside — matching
// the WPF hit-test window).
[[nodiscard]] bool contains_point(const vaml::Rect& r, float x, float y) noexcept
{
    return x >= r.left() && x < r.right() && y >= r.top() && y < r.bottom();
}

} // namespace

VAML_CORE_EXPORT
UIElement* UIElement::HitTestAt(float x, float y)
{
    return const_cast<UIElement*>(std::as_const(*this).HitTestAt(x, y));
}

VAML_CORE_EXPORT
const UIElement* UIElement::HitTestAt(float x, float y) const
{
    // A non-hit-testable element (Collapsed/Hidden) tests negative, even if the
    // point is inside its rect.
    const Visibility vis = visibility();
    if (vis == Visibility::Collapsed || vis == Visibility::Hidden)
    {
        return nullptr;
    }
    // S841: the point must lie within THIS element's INK box (the drawable
    // render box = RenderSize at the layout offset; UIElement.cpp hit box /
    // UIElement.cs:1049 reads the ink). == final_rect() by default. Content
    // overflowing the ink is clipped and not hit-testable.
    if (!contains_point(ink_rect(), x, y))
    {
        return nullptr;
    }

    // Test children TOPMOST-first (REVERSE index order — the LAST child is on
    // top/z). The deepest hit child (recursive) wins. This descent runs
    // REGARDLESS of THIS element's IsHitTestVisible — the property is NOT
    // inherited (the S595a contract), so a non-hit-test-visible parent still lets
    // its children evaluate their OWN flag (WPF-faithful).
    for (std::size_t i = children_.size(); i-- > 0;)
    {
        const UIElement* hit = children_[i]->HitTestAt(x, y);
        if (hit != nullptr)
        {
            return hit;
        }
    }

    // S595a: DECOUPLED input hit-test transparency. An element with
    // IsHitTestVisible == false still renders but does NOT intercept input (the
    // same skip axis WPF uses for Visibility, independent of it). It is NOT
    // inherited: only gates whether THIS element is returned (it does NOT block
    // the children descent above). The pure tree-walk read; the non-const
    // delegating overload is untouched. ZERO locks.
    if (!is_hit_test_visible())
    {
        return nullptr;
    }
    // No child was hit + this element is hit-test-visible -> this element is the
    // hit (it contains the point and is hit-testable).
    return this;
}

// --- S843 UpdateLayout — the PUBLIC driver entry (UIElement.cs:1630-1633) ----
// Resolve the TOPMOST UIElement root of the tree this element belongs to (the
// parent-chain walk skips non-UIElement ancestors, mirroring LayoutContext's
// ui_parent). LayoutContext::update_layout filters queued work to `in_subtree`,
// so driving from the topmost root reaches every queued dirty element in the
// tree the public call is scoped to.
static UIElement* topmost_ui_root(UIElement* e) noexcept
{
    UIElement* root = e;
    for (DependencyObject* p = e != nullptr ? e->parent() : nullptr; p != nullptr;
         p = p->parent())
    {
        if (auto* ue = dynamic_cast<UIElement*>(p))
        {
            root = ue;
        }
    }
    return root;
}

void UIElement::UpdateLayout()
{
    // No-arg: measure the topmost root at an UNBOUNDED offer + arrange it into
    // its own measured DesiredSize at origin 0 (the parentless-root rule). The
    // internal driver is move-only RAII over a non-owning root; it is a true
    // no-op when the tree is clean. Synchronous single-thread (S843 scope).
    UIElement* root = topmost_ui_root(this);
    if (root == nullptr)
    {
        return;
    }
    vaml::detail::LayoutManager driver(root);
    driver.UpdateLayout();
}

void UIElement::UpdateLayout(const vaml::Size& availableSize, const vaml::Rect& rootSlot)
{
    // HOST-frame drive: measure the topmost root at `availableSize`, arrange it
    // into `rootSlot` (the HwndSource host role). Synchronous, no-op-when-clean.
    UIElement* root = topmost_ui_root(this);
    if (root == nullptr)
    {
        return;
    }
    vaml::detail::LayoutManager driver(root);
    driver.UpdateLayout(availableSize, rootSlot);
}

} // namespace vaml
