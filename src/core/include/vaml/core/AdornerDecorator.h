#pragma once
// VAML AdornerDecorator — the WPF-faithful adorner host (WPF
// Controls\AdornerDecorator.cs). 11n-6.
//
// In WPF, `AdornerDecorator : Decorator` is the scaffolding CONTAINER that
// synthesizes (owns) an AdornerLayer and hosts it ABOVE its single content
// child: the layer is the AdornerDecorator's adorner surface, `GetAdornerLayer`
// on a descendant visual walks UP to the nearest AdornerDecorator and returns
// its layer. It renders its content child, then its layer's adorners on top.
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned to the WPF source + the S550
// Decorator branch):
//   * `AdornerDecorator : Decorator` — the S550 Decorator is the WPF-faithful
//     abstract single-child adornment base (Scaffolding.h, real + permanent).
//     AdornerDecorator derives it, so it inherits the ONE-content-child slot +
//     the fill measure/arrange policy (S550).
//   * The decorator OWNS a synthesized `AdornerLayer` (a UIElement). In WPF the
//     layer is a FIELD of the decorator + a visual child registered AFTER the
//     content (the layer renders topmost). VAML mirrors this: the decorator
//     owns a layer (unique_ptr) and, in Arrange, extends the S124 clip to the
//     layer — the layer is the decorator's overlay surface.
//   * `GetAdornerLayer(UIElement*)` STATIC — walks UP the visual parent chain
//     from the given visual to the nearest AdornerDecorator and returns its
//     owned AdornerLayer, or nullptr when none (WPF returns null when no
//     AdornerDecorator/ScrollContentPresenter is found). The layer the caller
//     then Adorners onto.
//
// SCOPE (recorded, NOT silent): the AdornerDecorator's LAYOUT of its layer
// (the layer covers the content child's arranged box) is a real Arrange step;
// the RENDER (the second-pass adorner emission above the normal tree) is the
// GPU/[both apps, not CI] lane (encode_adorner_tree, vulkan). The static
// GetAdornerLayer walk is CORE + headless-testable. ZERO MINIMAL REAL.

#include "vaml/core/Scaffolding.h"    // Decorator (the S550 single-content base)
#include "vaml/core/AdornerLayer.h"   // AdornerLayer (the owned overlay)
#include "vaml/core/export.h"

#include <memory>

namespace vaml {

// The WPF-faithful adorner host (11n-6): a Decorator that owns a synthesized
// AdornerLayer over its single content child, + the static GetAdornerLayer
// ancestor walk.
class VAML_CORE_EXPORT AdornerDecorator final : public Decorator
{
public:
    AdornerDecorator();

    // The OWNED AdornerLayer (the decorator's overlay surface). Non-owning
    // pointer (the decorator owns it); never null.
    [[nodiscard]] AdornerLayer* Layer() const noexcept { return layer_.get(); }

    // The static ancestor walk (WPF AdornerLayer.GetAdornerLayer): walks UP
    // `visual`'s visual-parent chain to the NEAREST AdornerDecorator and
    // returns its owned layer; nullptr when no AdornerDecorator is an
    // ancestor. Pure + headless-testable.
    [[nodiscard]] static AdornerLayer* GetAdornerLayer(const UIElement* visual) noexcept;

protected:
    // AdornerDecorator layout: MEASURE the Single content child (the S550
    // Decorator policy — the layer's box tracks the content, so the decorator
    // measures to the content's extent) + report the content's desired size.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

    // AdornerDecorator ARRANGE: arrange the content child to fill (the S550
    // policy) AND update the owned layer OVER the content's arranged box (the
    // layer re-syncs its adorners to the content's final rect) — the WPF
    // "layer sits above the content" contract.
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    std::unique_ptr<AdornerLayer> layer_; // the OWNED overlay surface
};

} // namespace vaml
