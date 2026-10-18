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
//
// S142 (dep:S141): the pimpl is COLLAPSED. UIElement's layout data (children,
// desired size, final rect, dirty flags) now lives directly as inline members,
// composing with the base DependencyObject's block-resident Store into ONE
// block-resident unit (pimpl removed, not preserved; ADR-003 revised +
// ADR-019). Behavior is unchanged.

#include "vaml/core/UIElement.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyFlags.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

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

    // Margin: default 0 (S122), NOT inherited (each element's margin is its
    // own outer inset). The payload rides the Thickness value path.
    (void)DependencyProperty::Register(
        "Margin", ValueType::Thickness, "UIElement",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));

    // HorizontalAlignment / VerticalAlignment: default Stretch(0), NOT
    // inherited, stored via the Int32 value path (enum rides Int32, the same
    // convention as Visibility in S120).
    (void)DependencyProperty::Register(
        "HorizontalAlignment", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "VerticalAlignment", ValueType::Int32, "UIElement",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
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
UIElement::~UIElement() = default;

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

    const Size natural = MeasureOverride(inner);
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
const std::vector<UIElement*> UIElement::children() const noexcept
{
    std::vector<UIElement*> raw;
    raw.reserve(children_.size());
    for (const auto& c : children_)
    {
        raw.push_back(c.get());
    }
    return raw;
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

} // namespace vaml
