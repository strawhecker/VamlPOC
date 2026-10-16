// S087 — built-in Element MINIMAL REAL seeds + startup registration.
//
// Element subclasses that are still seeds are deliberately a THIN identity
// bearer (empty ctor) deriving DependencyObject. NO members / logic here — the
// real content of each control is authored at its OWN resolution sprint (see
// Element.h sentinels). The FIRST control to reach its sprint is StackPanel
// (S129), which RESOLVES its seed here: re-based onto UIElement and delivered
// as a REAL panel (orientation DP + MeasureOverride + ArrangeOverride + last-
// child-stretch). The remaining seeds stay thin until their own sprints.
//
// register_builtin_elements() is the single startup registration call that
// populates ElementFactoryRegistry::global() with every roadmap element name the
// S091 runtime reader and S093 sample need. It is idempotent-guarded: a second
// call (e.g. from a test that also calls the user-facing register) is a no-op,
// so startup population can be re-invoked safely.

#include "vaml/core/Element.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"

#include <algorithm>
#include <utility>

namespace vaml {

namespace {

// Registers the StackPanel Orientation DP into the global table exactly once
// per process (Meyers-static guard; a second call is a no-op — mirrors
// UIElement's self-registration). Orientation is GLOBALLY registered so it
// resolves on any DependencyObject via S043 attached-aware resolution, and
// defaults to Vertical (0), NOT inherited (a panel's orientation is its own).
void register_stackpanel_properties()
{
    (void)DependencyProperty::Register(
        "Orientation", ValueType::Int32, "StackPanel",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

// Runs register_stackpanel_properties() at most once (first StackPanel ctor).
// Function-local static guard => no static-init-order hazard.
void ensure_stackpanel_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_stackpanel_properties();
    }
}

} // namespace

// Element constructors — MINIMAL REAL: an empty ctor is all a seed needs to be
// a linkable concrete type the factory registry can produce. Real construction
// (layout slots, named parts, templates) is each control's own sprint. The
// StackPanel ctor is NOT a seed anymore (S129): it derives UIElement and self-
// registers its Orientation DP.
VAML_CORE_EXPORT
Button::Button() = default;

VAML_CORE_EXPORT
StackPanel::StackPanel()
{
    ensure_stackpanel_properties_registered();
}

VAML_CORE_EXPORT
Grid::Grid() = default;

VAML_CORE_EXPORT
Text::Text() = default;

VAML_CORE_EXPORT
TextBox::TextBox() = default;

VAML_CORE_EXPORT
CheckBox::CheckBox() = default;

VAML_CORE_EXPORT
Slider::Slider() = default;

VAML_CORE_EXPORT
ScrollViewer::ScrollViewer() = default;

VAML_CORE_EXPORT
Window::Window() = default;

// --- S129 StackPanel: Orientation DP + real panel Measure/Arrange ----------

VAML_CORE_EXPORT
SetResult StackPanel::set_orientation(Orientation o)
{
    // Rides the Int32 value path (the enum stores as Int32, same convention as
    // Visibility / alignment enums).
    return Set(PropertyId::of("Orientation"),
               Value::Int32(static_cast<std::int32_t>(o)));
}

VAML_CORE_EXPORT
Orientation StackPanel::orientation() const
{
    return static_cast<Orientation>(Get(PropertyId::of("Orientation")).get<std::int32_t>());
}

// The panel's own content is its children. Orientation-aware measure:
//   * Vertical   — offer each child {available.width, ∞height} (a vertical
//     stack lets each child wrap to content vertically), then the panel's
//     natural extent = {max child width, Σ child heights}.
//   * Horizontal — offer each child {∞width, available.height}, natural =
//     {Σ child widths, max child height}.
// Dirty-respect (S128): re-measure ONLY is_measure_dirty() children; a clean
// child reuses its stored desired_size() (the S128 dirty-only-subpass pattern
// realized inside the real panel). Measure() (base) then finalizes this
// natural extent via the S123 infinity-aware stretch-vs-wrap rule.
VAML_CORE_EXPORT
Size StackPanel::MeasureOverride(const Size& available) const
{
    float width  = 0.0f;
    float height = 0.0f;

    if (orientation() == Orientation::Horizontal)
    {
        const Size rowAvail{ Size::Infinite().width, available.height };
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_measure_dirty())
            {
                child->Measure(rowAvail);
            }
            width += child->desired_size().width;
            height = (child->desired_size().height > height)
                         ? child->desired_size().height
                         : height;
        }
        return { width, height };
    }

    // Vertical (default).
    const Size columnAvail{ available.width, Size::Infinite().height };
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        if (child->is_measure_dirty())
        {
            child->Measure(columnAvail);
        }
        width = (child->desired_size().width > width)
                    ? child->desired_size().width
                    : width;
        height += child->desired_size().height;
    }
    return { width, height };
}

// The arrange-side virtual (S127). Receives the panel's FULL final render slot
// (final_rect()). Orientation-aware placement:
//   * Vertical   — children top-to-bottom, each into {slot.x, y, slot.width,
//     childHeight}, advancing y by the child's desired height; the LAST child
//     FILLS the remaining vertical space (last-child-stretch:
//     remaining = finalSlot.bottom() - y).
//   * Horizontal — left-to-right mirror; the LAST child fills the remaining
//     width.
// Each child's own alignment (Horizontal/Vertical + Margin) is honored inside
// its slot via child->Arrange (compute_final_rect), so a default-Stretch last
// child takes the leftover track while a non-Stretch last child is pinned
// within it (never over-fills). Dirty-respect (S128): re-arrange ONLY
// is_arrange_dirty() children; clean children keep their final_rect untouched.
VAML_CORE_EXPORT
void StackPanel::ArrangeOverride(const Rect& finalSlot)
{
    if (orientation() == Orientation::Horizontal)
    {
        float x = finalSlot.x;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            const bool isLast = (i + 1 == child_count());
            const float remaining = finalSlot.right() - x;
            const float slotWidth = isLast ? remaining : child->desired_size().width;
            if (child->is_arrange_dirty())
            {
                child->Arrange(Rect{ x, finalSlot.y, slotWidth, finalSlot.height });
            }
            x += (isLast ? remaining : child->desired_size().width);
        }
        return;
    }

    // Vertical (default).
    float y = finalSlot.y;
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        const bool isLast = (i + 1 == child_count());
        const float remaining = finalSlot.bottom() - y;
        const float slotHeight = isLast ? remaining : child->desired_size().height;
        if (child->is_arrange_dirty())
        {
            child->Arrange(Rect{ finalSlot.x, y, finalSlot.width, slotHeight });
        }
        y += (isLast ? remaining : child->desired_size().height);
    }
}

VAML_CORE_EXPORT
void register_builtin_elements()
{
    ElementFactoryRegistry& registry = ElementFactoryRegistry::global();

    // register_builtin_elements() is documented single-threaded startup; the
    // bool guards against a stray second call (which would otherwise throw
    // ElementAlreadyRegistered). NO-LAZY: this is a startup-ordering guard, not
    // dead code — a duplicated population is a real double-registration hazard
    // for a process-wide singleton.
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;

    // Register the roadmap element vocabulary. Factories return a fresh,
    // owned instance of the concrete seed class.
    registry.register_element("Button",       [] { return std::make_unique<Button>(); });
    registry.register_element("StackPanel",   [] { return std::make_unique<StackPanel>(); });
    registry.register_element("Grid",         [] { return std::make_unique<Grid>(); });
    registry.register_element("Text",         [] { return std::make_unique<Text>(); });
    registry.register_element("TextBox",      [] { return std::make_unique<TextBox>(); });
    registry.register_element("CheckBox",     [] { return std::make_unique<CheckBox>(); });
    registry.register_element("Slider",       [] { return std::make_unique<Slider>(); });
    registry.register_element("ScrollViewer", [] { return std::make_unique<ScrollViewer>(); });
    registry.register_element("Window",       [] { return std::make_unique<Window>(); });
}

} // namespace vaml
