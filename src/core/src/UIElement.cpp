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

#include "vaml/core/ResourceDictionary.h" // S360 — the complete Resources slot type (dtor + accessors)
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/RoutedEvent.h"   // S321 routed handlers
#include "vaml/core/Signal.h"        // S320 Signal<T>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>
#include <unordered_map>
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

    // Margin: default 0 (S122), NOT inherited (each element's margin is its
    // own outer inset). The payload rides the Thickness value path.
    (void)DependencyProperty::Register(
        "Margin", ValueType::Thickness, "UIElement",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));

    // RenderTransform (S208): default identity, NOT inherited, validated finite.
    // A Matrix2D with any NaN/Inf element is REJECTED at Set (the renderer never
    // sees non-finite geometry). Set triggers InvalidateArrange (see accessor).
    (void)DependencyProperty::Register(
        "RenderTransform", ValueType::Matrix2D, "UIElement",
        std::move(PropertyMetadata::WithDefault(Value::Matrix2D(vaml::Matrix2D::identity()),
                                                PropertyFlags::None))
            .with_validate([](const Value& v) {
                return is_transform_valid(v.get<vaml::Matrix2D>());
            }));

    // HorizontalAlignment / VerticalAlignment: default Stretch(0), NOT
    // inherited, stored via the Int32 value path (enum rides Int32, the same
    // convention as Visibility in S120).
    (void)DependencyProperty::Register(
        "HorizontalAlignment", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "VerticalAlignment", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Width / Height (WPF-style layout DPs, added for the .xaml-acceptance /
    // WPF source import line). WPF-faithful semantics: an EXPLICIT explicit
    // double (NaN = "auto/unset"), NOT inherited, riding the Double value path.
    // 0 or NaN mean auto — the element's natural/content size governs the axis,
    // exactly as VAML's existing panels resolve an unset axis. A positive
    // explicit value is a HARD constraint the measure pipeline applies
    // (clamped to the available offer, never producing negative geometry). See
    // the measure finalizer (reconcile_axis, applied in UIElement::Measure —
    // genuinely consumed, synthesized in this same translation unit):
    (void)DependencyProperty::Register(
        "Width", ValueType::Double, "UIElement",
        PropertyMetadata::WithDefault(Value::Double(std::numeric_limits<double>::quiet_NaN()),
                                      PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Height", ValueType::Double, "UIElement",
        PropertyMetadata::WithDefault(Value::Double(std::numeric_limits<double>::quiet_NaN()),
                                      PropertyFlags::None));

    // Focusable (S325): default true, NOT inherited. Declares whether THIS
    // element can take keyboard focus (WPF-faithful: a focusable leaf is true;
    // a container/panel is set false by its author or default). The Int32
    // value path (Bool rides Int32? — Value uses Int32 as the closest to a
    // bool tag; keep 0/1 semantics).
    (void)DependencyProperty::Register(
        "Focusable", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));

    // Tag (11j-8, GAP CLOSE, dep:S036) — the WPF FrameworkElement.Tag analogue.
    // An OBJECT-SLOT DP on the element base so `Tag="..."` / `<X.Tag>...`
    // resolve everywhere S043-attached-style. WPF Tag is `object`.
    //
    // VAML's DP registration binds ONE value type for literal conversion (the
    // S043 attached-aware read path resolves by bare name on any element). The
    // corpus authors `Tag="..."` as an ATTRIBUTE (literal) — the PRIMARY use —
    // AND `<X.Tag><SomeObject/></X.Tag>` as a property-element (object). A
    // single object-typed holder that accepts BOTH:
    //   * Tag is registered with a STRING bound type (default "" = no tag), so
    //     a literal attribute `Tag="abc"` converts to Value::String ("abc")
    //     through the String converter — never UnknownType.
    //   * The property-element OBJECT branch calls obj->Set(...,
    //     Value::UntypedPtr(raw)) DIRECTLY (no convertLiteral); DependencyObject:
    //     :Set does NOT enforce the Value tag against the declared bound type
    //     (coerce/validate passthrough), so the UntypedPtr object identity IS
    //     stored on the String-typed DP — the "metadata must not reject a type
    //     mismatch" contract the plan records.
    // The typed accessors read the stored `Value` BY TAG (String when a literal
    // tag, UntypedPtr when an object tag), so a caller distinguishes them.
    (void)DependencyProperty::Register(
        "Tag", ValueType::String, "UIElement",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
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
UIElement::~UIElement()
{
    // S360 lifetime: an element that owned a ResourceDictionary releases it on
    // destruction so the side-table holds no dangling key (the S245 detach-on-
    // destroy discipline applied to the resource side-table).
    vaml::detail::remove_element_resources(this);
}

// --- S360 Resources slot (stored OUT-OF-LINE; see ResourceDictionary.cpp) ----

VAML_CORE_EXPORT
void UIElement::set_resources(std::unique_ptr<ResourceDictionary> resources)
{
    // The side-table is populated/owned by ResourceDictionary.cpp (the only TU
    // that knows the complete ResourceDictionary type).
    vaml::detail::set_element_resources(this, std::move(resources));
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
    return Set(PropertyId::of("Visibility"), Value::Int32(static_cast<std::int32_t>(v)));
}

VAML_CORE_EXPORT
Visibility UIElement::visibility() const
{
    return static_cast<Visibility>(Get(PropertyId::of("Visibility")).get<std::int32_t>());
}

VAML_CORE_EXPORT
SetResult UIElement::set_focusable(bool value)
{
    return Set(PropertyId::of("Focusable"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool UIElement::focusable() const
{
    return Get(PropertyId::of("Focusable")).get<std::int32_t>() != 0;
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

// --- S208 RenderTransform (the S120 transform-owner resolution) ---------------

VAML_CORE_EXPORT
SetResult UIElement::set_render_transform(const vaml::Matrix2D& value)
{
    // A non-finite matrix is REJECTED by the DP validate (never stored). Set
    // triggers InvalidateArrange: a render transform is a visual change the
    // encoder re-emits, so the arrange-dirty flag (plus its ancestor chain)
    // marks the element for a fresh encode pass.
    const SetResult r = Set(PropertyId::of("RenderTransform"), Value::Matrix2D(value));
    if (r == SetResult::APPLIED)
    {
        InvalidateArrange();
    }
    return r;
}

VAML_CORE_EXPORT
vaml::Matrix2D UIElement::render_transform() const
{
    return Get(PropertyId::of("RenderTransform")).get<vaml::Matrix2D>();
}

// --- S122 layout slot (RESOLVED BY S122) ------------------------------------

VAML_CORE_EXPORT
const vaml::Size& UIElement::desired_size() const noexcept
{
    return desired_size_;
}

VAML_CORE_EXPORT
const vaml::Rect& UIElement::final_rect() const noexcept
{
    return final_rect_;
}

VAML_CORE_EXPORT
SetResult UIElement::set_margin(const vaml::Thickness& value)
{
    return Set(PropertyId::of("Margin"), Value::Thickness(value));
}

VAML_CORE_EXPORT
vaml::Thickness UIElement::margin() const
{
    return Get(PropertyId::of("Margin")).get<vaml::Thickness>();
}

VAML_CORE_EXPORT
SetResult UIElement::set_horizontal_alignment(vaml::HorizontalAlignment value)
{
    return Set(PropertyId::of("HorizontalAlignment"), Value::Int32(static_cast<std::int32_t>(value)));
}

VAML_CORE_EXPORT
vaml::HorizontalAlignment UIElement::horizontal_alignment() const
{
    return static_cast<vaml::HorizontalAlignment>(
        Get(PropertyId::of("HorizontalAlignment")).get<std::int32_t>());
}

VAML_CORE_EXPORT
SetResult UIElement::set_vertical_alignment(vaml::VerticalAlignment value)
{
    return Set(PropertyId::of("VerticalAlignment"), Value::Int32(static_cast<std::int32_t>(value)));
}

VAML_CORE_EXPORT
vaml::VerticalAlignment UIElement::vertical_alignment() const
{
    return static_cast<vaml::VerticalAlignment>(
        Get(PropertyId::of("VerticalAlignment")).get<std::int32_t>());
}

// --- Width / Height accessors (WPF-style explicit size DPs) -----------------

VAML_CORE_EXPORT
SetResult UIElement::set_width(double value)
{
    return Set(PropertyId::of("Width"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::width() const
{
    return Get(PropertyId::of("Width")).get<double>();
}

VAML_CORE_EXPORT
SetResult UIElement::set_height(double value)
{
    return Set(PropertyId::of("Height"), Value::Double(value));
}

VAML_CORE_EXPORT
double UIElement::height() const
{
    return Get(PropertyId::of("Height")).get<double>();
}

// Applies an EXPLICIT Width/Height to an axis's natural measured extent.
// WPF-faithful (an explicit FrameworkElement.Width is a hard constraint,
// clamped to the offer — never negative): returns the explicit value when it is
// a POSITIVE double, otherwise the natural extent unchanged. NaN or 0 = auto.
constexpr float reconcile_axis(double explicitValue, float natural) noexcept
{
    if (explicitValue > 0.0)
    {
        // Clamp explicit >= 0 (never negative geometry). NaN never slips in:
        // `explicitValue > 0.0` is false for NaN.
        return std::max(0.0f, static_cast<float>(explicitValue));
    }
    return natural;
}

// --- S123 Measure contract ------------------------------------------------

VAML_CORE_EXPORT
Size UIElement::MeasureOverride(const Size& available) const
{
    // Base UIElement has no content; a leaf with content (Text) or a panel
    // with children measures its content in its S129/S131/S400 override. This
    // is the S127 MeasureOverride seam (upgraded from S123's
    // measure_natural_size). `available` is unused by the base (no content),
    // so silence the unused-parameter diagnostic.
    (void)available;
    return Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void UIElement::Measure(const Size& available)
{
    // The S123 contract: Measure(available) -> DesiredSize, WPF-faithful margin
    // handling (S130_1 — resolves the S130-recorded fidelity gap; not silent):
    // the offered `available` is first SHRUNK by this element's Margin (a
    // margin'd child's content wraps within its INNER constraint, mirroring
    // WPF FrameworkElement.MeasureCore), MeasureOverride sees that inner size,
    // the infinity-aware stretch-vs-wrap rule (compute_measured_desired)
    // finalizes against the inner size, and the margin is then ADDED BACK into
    // DesiredSize — so a margin'd element reports a LARGER DesiredSize that
    // legitimately propagates into a parent panel's DesiredSize (WPF StackPanel
    // DesiredSize includes child margins). The margin-inflated extent is
    // clamped to the original `available` (never exceeds the offer, never
    // negative) — the same anti-overflow, anti-negative guarantee S123 upholds.
    const Thickness m = margin();
    const float innerAvailW = std::max(0.0f, available.width - m.left - m.right);
    const float innerAvailH = std::max(0.0f, available.height - m.top - m.bottom);
    const Size inner{ innerAvailW, innerAvailH };

    const Size natural0 = MeasureOverride(inner);
    // WPF-style explicit Width/Height (the .xaml source-import line): an
    // explicit POSITIVE value becomes the axis's natural extent (a hard
    // constraint the finalizer then stretches/wraps/clamps WPF-faithfully);
    // NaN/0 leave the content extent unchanged (auto). Never negative.
    const Size natural = {
        reconcile_axis(width(), natural0.width),
        reconcile_axis(height(), natural0.height),
    };
    Size d = vaml::compute_measured_desired(
        natural, inner, horizontal_alignment(), vertical_alignment());

    d.width += m.left + m.right;
    d.height += m.top + m.bottom;
    desired_size_ = {
        std::max(0.0f, std::min(d.width, available.width)),
        std::max(0.0f, std::min(d.height, available.height)),
    };

    // A completed Measure pass clears this element's measure-dirty flag (S126):
    // it no longer needs measuring. The LayoutManager gates its pass on the
    // root's bit; panels re-measure only dirty children, each of which clears
    // its own bit here, so one pass over a dirty subgraph leaves it clean.
    measure_dirty_ = false;
}

// --- S124 Arrange contract --------------------------------------------------

VAML_CORE_EXPORT
void UIElement::ArrangeOverride(const Rect& finalSlot)
{
    // The S127 arrange-side virtual. Receives the element's FULL final render
    // slot (final_rect()). Base UIElement has no children, so a leaf has
    // nothing to place — default no-op. Panels (S129/S131) override this to
    // lay out their children into their own render slot, completing the
    // arrange-subgraph auto-drive (LayoutManager -> root Arrange -> panel
    // ArrangeOverride -> children arranged). `finalSlot` is unused by the
    // base, so silence the unused-parameter diagnostic.
    (void)finalSlot;
}

VAML_CORE_EXPORT
void UIElement::Arrange(const Rect& arrangeSlot)
{
    // The S124 contract: Arrange(arrangeSlot) -> final_rect(). This is the
    // write-side pair to Measure: consume the S122 alignment algorithm
    // (compute_final_rect) with the element's own measured DesiredSize +
    // Margin + alignments, and write the S122 final_rect() read target.
    //
    // final_rect() is BOTH the render slot (where S208 draws the element) AND
    // the clip bounds (content overflowing it is not rendered). A base
    // UIElement has no content, so its final_rect == clip bounds trivially.
    //
    // S127 auto-drive: after computing final_rect(), Arrange invokes
    // ArrangeOverride(final_rect()) so a panel places its children into its
    // OWN render slot automatically (LayoutManager -> root Arrange -> panel
    // ArrangeOverride -> children arranged). This removes the S125/S126 manual
    // arrange-call pattern; the arrange-subgraph auto-drive completes here.
    const LayoutSlot slot{ arrangeSlot };
    final_rect_ = vaml::compute_final_rect(
        slot, desired_size(), margin(), horizontal_alignment(), vertical_alignment());
    // A completed Arrange pass clears this element's arrange-dirty flag (S126),
    // mirroring Measure's clear of the measure bit.
    arrange_dirty_ = false;
    // S127: hand the element its final render slot so a panel arranges its
    // children into it (the arrange-subgraph auto-drive).
    ArrangeOverride(final_rect_);
}

// --- S126 layout invalidation ------------------------------------------------

VAML_CORE_EXPORT
void UIElement::InvalidateMeasure()
{
    // Short-circuit: an already-measure-dirty element needs no re-walk (the
    // whole chain above it is already dirty from when it was first dirtied).
    if (measure_dirty_)
    {
        return;
    }
    measure_dirty_ = true;
    // Propagate to the root: a parent's DesiredSize derives from its children,
    // so a child invalidation dirties every ancestor's measure pass. Walk the
    // S039/S041 parent link; stop at the root (parent() == nullptr). Each
    // ancestor is marked unconditionally here (the local short-circuit prevents
    // a redundant walk, but the chain must stay dirty for the manager's pass).
    for (DependencyObject* p = parent(); p != nullptr; p = p->parent())
    {
        auto* ue = dynamic_cast<UIElement*>(p);
        if (ue == nullptr)
        {
            // A non-UIElement ancestor cannot own a measure pass (only
            // UIElement subclasses participate in the visual tree / layout);
            // nothing to mark above it — stop.
            break;
        }
        if (ue->measure_dirty_)
        {
            // Already dirty: the chain above this ancestor is already dirty
            // from an earlier invalidation — no need to keep walking.
            break;
        }
        ue->measure_dirty_ = true;
    }
}

VAML_CORE_EXPORT
void UIElement::InvalidateArrange()
{
    // Mirror of InvalidateMeasure on the arrange axis.
    if (arrange_dirty_)
    {
        return;
    }
    arrange_dirty_ = true;
    for (DependencyObject* p = parent(); p != nullptr; p = p->parent())
    {
        auto* ue = dynamic_cast<UIElement*>(p);
        if (ue == nullptr)
        {
            break;
        }
        if (ue->arrange_dirty_)
        {
            break;
        }
        ue->arrange_dirty_ = true;
    }
}

VAML_CORE_EXPORT
bool UIElement::is_measure_dirty() const noexcept
{
    return measure_dirty_;
}

VAML_CORE_EXPORT
bool UIElement::is_arrange_dirty() const noexcept
{
    return arrange_dirty_;
}

VAML_CORE_EXPORT
bool UIElement::is_layout_dirty() const noexcept
{
    return measure_dirty_ || arrange_dirty_;
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
    // set_parent(this) registers `raw` into THIS element's inheritance-scope
    // (S041 inverse edge) so Inherits/InheritOnly (e.g. Opacity) cascade through
    // the visual child. Order: install the parent link BEFORE owning, so the
    // inherited-Opacity read inside any observer sees a coherent tree.
    raw->set_parent(this);
    children_.push_back(std::move(child));
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
    // The point must lie within THIS element's final_rect (the clip bounds;
    // content overflowing final_rect is clipped and not hit-testable).
    if (!contains_point(final_rect_, x, y))
    {
        return nullptr;
    }

    // Test children TOPMOST-first (REVERSE index order — the LAST child is on
    // top/z). The deepest hit child (recursive) wins.
    for (std::size_t i = children_.size(); i-- > 0;)
    {
        const UIElement* hit = children_[i]->HitTestAt(x, y);
        if (hit != nullptr)
        {
            return hit;
        }
    }
    // No child was hit -> this element is the hit (it contains the point and is
    // hit-testable).
    return this;
}

} // namespace vaml
