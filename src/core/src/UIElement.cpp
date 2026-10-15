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

// S121 pimpl: the owned, ordered visual children list. A parent OWNS its
// children (unique_ptr — resolving the S091/S120 ownership-transfer note).
// Index order is the visual/z-order (WPF-faithful ordered Children collection).
struct UIElement::Impl
{
    std::vector<std::unique_ptr<UIElement>> children;

    // S122 layout-slot membership (RESOLVED): DesiredSize (S123 writes) +
    // final rect (S124 writes), read via desired_size()/final_rect().
    vaml::Size desiredSize;
    vaml::Rect finalRect;
};

VAML_CORE_EXPORT
UIElement::UIElement()
    : impl_(std::make_unique<Impl>())
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
    return impl_->desiredSize;
}

VAML_CORE_EXPORT
const vaml::Rect& UIElement::final_rect() const noexcept
{
    return impl_->finalRect;
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
    impl_->children.push_back(std::move(child));
}

VAML_CORE_EXPORT
void UIElement::add_child(std::unique_ptr<UIElement> child)
{
    attach_child(std::move(child));
}

VAML_CORE_EXPORT
std::size_t UIElement::remove_child(std::size_t index)
{
    if (index >= impl_->children.size())
    {
        throw std::out_of_range("UIElement::remove_child: index out of range");
    }
    auto it = impl_->children.begin() + static_cast<std::ptrdiff_t>(index);
    UIElement* child = it->get();
    // Unwind the S041 inheritance-scope edge + parent link BEFORE destroying so
    // no dangling scope edge survives the removal (NO-LAZY).
    child->set_parent(nullptr);
    impl_->children.erase(it); // destroys the owned child
    return impl_->children.size();
}

VAML_CORE_EXPORT
std::unique_ptr<UIElement> UIElement::take_child(std::size_t index)
{
    if (index >= impl_->children.size())
    {
        throw std::out_of_range("UIElement::take_child: index out of range");
    }
    auto it = impl_->children.begin() + static_cast<std::ptrdiff_t>(index);
    UIElement* child = it->get();
    // Unwind the parent link + inheritance-scope edge BEFORE transferring so the
    // detached child is fully cut from this parent (its OWN children keep their
    // sub-tree intact).
    child->set_parent(nullptr);
    std::unique_ptr<UIElement> detached = std::move(*it);
    impl_->children.erase(it);
    return detached;
}

VAML_CORE_EXPORT
std::size_t UIElement::child_count() const noexcept
{
    return impl_->children.size();
}

VAML_CORE_EXPORT
UIElement* UIElement::child_at(std::size_t index) const noexcept
{
    if (index >= impl_->children.size())
    {
        return nullptr;
    }
    return impl_->children[index].get();
}

VAML_CORE_EXPORT
const std::vector<UIElement*> UIElement::children() const noexcept
{
    std::vector<UIElement*> raw;
    raw.reserve(impl_->children.size());
    for (const auto& c : impl_->children)
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
    for (const auto& c : impl_->children)
    {
        c->walk(visitor);
    }
}

} // namespace vaml
