// VAML AdornerDecorator — the WPF-faithful adorner host (11n-6).
//
// AdornerDecorator : Decorator (the S550 single-content base) owns a
// synthesized AdornerLayer and lays it over its single content child; the
// static GetAdornerLayer walks UP to the nearest AdornerDecorator. ADR-018:
// ZERO new locks (single affinity thread).

#include "vaml/core/AdornerDecorator.h"

namespace vaml {

AdornerDecorator::AdornerDecorator()
    : layer_(std::make_unique<AdornerLayer>())
{
}

VAML_CORE_EXPORT
AdornerLayer* AdornerDecorator::GetAdornerLayer(const UIElement* visual) noexcept
{
    // Walk UP the visual-parent chain to the NEAREST AdornerDecorator and
    // return its owned layer (the WPF ancestor walk). A visual that IS an
    // AdornerDecorator returns its own layer.
    const UIElement* cur = visual;
    while (cur)
    {
        if (const auto* deco = dynamic_cast<const AdornerDecorator*>(cur))
            return deco->Layer();
        cur = static_cast<const UIElement*>(cur->parent());
    }
    return nullptr; // no AdornerDecorator ancestor (WPF returns null)
}

VAML_CORE_EXPORT
vaml::Size AdornerDecorator::MeasureOverride(const vaml::Size& available) const
{
    // The S550 Decorator policy: measure the single content child + report its
    // extent. The layer is the decorator's overlay — its box tracks the
    // content, so the decorator measures to the content's size (a layer with
    // no adorners contributes nothing to the measure).
    if (content())
    {
        content()->Measure(available);
        return content()->desired_size();
    }
    return vaml::Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void AdornerDecorator::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // Arrange the content child to fill the decorator's box (the S550 policy),
    // THEN update the owned layer over the content's arranged box (the WPF
    // "layer sits above the content" contract — the layer re-syncs its
    // adorners to the content's final rect).
    if (content())
        content()->Arrange(finalSlot);
    (void)layer_->Update(); // the synced count is informational here
}

} // namespace vaml
