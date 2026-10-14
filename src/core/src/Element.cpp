// S087 — built-in Element MINIMAL REAL seeds + startup registration.
//
// Each Element subclass is deliberately a THIN identity bearer (empty ctor)
// deriving DependencyObject. NO members / logic here — the real content of each
// control is authored at its OWN resolution sprint (see Element.h sentinels).
//
// register_builtin_elements() is the single startup registration call that
// populates ElementFactoryRegistry::global() with every roadmap element name the
// S091 runtime reader and S093 sample need. It is idempotent-guarded: a second
// call (e.g. from a test that also calls the user-facing register) is a no-op,
// so startup population can be re-invoked safely.

#include "vaml/core/Element.h"

#include "vaml/core/ElementFactoryRegistry.h"

#include <utility>

namespace vaml {

// Element constructors — MINIMAL REAL: an empty ctor is all a seed needs to be
// a linkable concrete type the factory registry can produce. Real construction
// (layout slots, named parts, templates) is each control's own sprint.
VAML_CORE_EXPORT
Button::Button() = default;

VAML_CORE_EXPORT
StackPanel::StackPanel() = default;

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
