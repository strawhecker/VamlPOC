#pragma once
// VAML AdornerLayer — the WPF-faithful adorner layer (WPF
// Documents\AdornerLayer.cs). 11n-6.
//
// An AdornerLayer is a UIElement that hosts the adorners attached to the
// elements it covers. WPF's AdornerLayer holds:
//   * an ElementMap — adorned element -> its adorners (the hosted list), and
//   * a ZOrderMap — adorned element -> per-adorner z-index (WPF's
//     `_zOrderMap`; DefaultZOrder = Int32::MaxValue means "merely attached,
//     render in attach order").
// This is the AUTHORITY for which adorned elements have adorners + the
// topmost z-order an adorner renders above its adorned element.
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned to the WPF source):
//   * `Add(Adorner*)` — REGISTERS the adorner under its AdornedElement (WPF
//     AdornerLayer.Add). The layer stores a NON-OWNING REFERENCE: the CALLER
//     owns the adorner's lifetime (WPF's layer holds references; the app owns
//     the adorner object). This matches the VAML borrowed-observer convention
//     (GetParent/GetChild/AdornedElement all return borrowed pointers) and is
//     the memory-safe contract — the layer never frees a caller-owned adorner.
//     A caller that wants the layer to own it uses Remove before its dtor.
//   * `Add(Adorner*, int zOrder)` — with an explicit WPF z-index (the
//     z-ordered add).
//   * `Remove(Adorner*)` — detaches the reference, NEVER THROWS on an absent
//     adorner (WPF-faithful: "always returns normally"). Returns whether it
//     removed one.
//   * `Update()` / `Update(UIElement*)` — re-evaluates the layer's adorners
//     (the WPF "Update called when the adorned element repaints"). In VAML the
//     headless seam re-measures/re-arranges the layer's adorners over their
//     adorned elements; a real-change discipline on a dirty layer.
//   * `GetAdorners(UIElement*)` — the adorner list for an adorned element
//     (WPF GetAdorners(Visual)), nullptr when it has none.
//   * `GetAdornerLayer(Visual)` STATIC — the nearest AdornerLayer by walking
//     UP the visual tree (WPF walks to the nearest AdornerDecorator /
//     ScrollContentPresenter that hosts a layer). In VAML a layer is
//     discovered by walking UP from a visual to the nearest node that exposes
//     one (an AdornerDecorator, the parent that synthesizes it). Adorned
//     elements must outlive the layer's references (the layer borrows, so a
//     caller removes its adorners before destroying an adorned element).
//
// SCOPE (recorded, NOT silent): the AdornerDecorator HOST (the WPF-scaffolding
// node that OWNS a synthesized AdornerLayer + the S550 Decorator single-content
// slot) is a sibling of this header — see _AdornerDecorator.h. The RENDER (the
// second-pass adorner emission above the normal tree) is the GPU/[both apps,
// not CI] lane (encode_adorner_tree, vulkan). ADR-018: ZERO new locks. ZERO
// MINIMAL REAL.

#include "vaml/core/UIElement.h"      // UIElement (layer + adorned element)
#include "vaml/core/Adorner.h"        // Adorner (the hosted adorner type)
#include "vaml/core/export.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace vaml {

// The WPF-faithful adorner layer (11n-6). Borrows the adorners attached to the
// elements it covers, keyed by the ADORNED element, with WPF's z-order model.
class VAML_CORE_EXPORT AdornerLayer : public UIElement
{
public:
    AdornerLayer();

    // WPF-faithful default z (AdornerLayer.DefaultZOrder): adorner added
    // WITHOUT an explicit z renders in ATTACH ORDER (the relative order of
    // Add calls) at the top of its adorned element's adorner stack.
    static constexpr std::int32_t DefaultZOrder = 0x7FFFFFFF;

    // --- Adorner attach/detach (WPF add/remove) ---------------------------
    // REGISTERS `adorner` (a NON-OWNING borrowed reference — the caller owns
    // its lifetime) under its AdornedElement (WPF AdornerLayer.Add). The
    // adorner must be AdornedElement-bound (see Adorner); an adorner with no
    // adorned element is a no-op (returns false). Adding the same adorner
    // twice is a no-op (single registration — returns false on a repeat).
    [[nodiscard]] bool Add(Adorner* adorner);
    // Same, but with an explicit WPF z-index (Smaller value = LOWER / further
    // behind; the topmost = the MAX z among the adorned element's adorners).
    [[nodiscard]] bool Add(Adorner* adorner, std::int32_t zOrder);

    // Detaches the registered reference to `adorner`. NEVER THROWS on an
    // absent adorner (the WPF "always returns normally" contract — Remove of a
    // non-present adorner is a silent no-op returning false). Returns true iff
    // it removed one.
    [[nodiscard]] bool Remove(Adorner* adorner);

    // The adorner list for `adorned` (WPF GetAdorners(Visual)), nullptr when it
    // has none. The returned vector is a borrowed snapshot (const view of the
    // registered adorner references in z order). Pure + headless-testable.
    [[nodiscard]] const std::vector<Adorner*>* GetAdorners(UIElement* adorned) const noexcept;

    // True iff `adorned` currently has at least one registered adorner.
    [[nodiscard]] bool HasAdorners(UIElement* adorned) const noexcept;

    // The number of Adorners this layer currently registers (across all adorned
    // elements). Pure.
    [[nodiscard]] std::size_t AdornerCount() const noexcept;

    // --- Update (WPF AdornerLayer.Update) ---------------------------------
    // Re-evaluates the layer's adorners OVER their adorned elements (the WPF
    // "repaint the adorners" contract). The VAML headless seam re-measures +
    // re-arranges each registered adorner into its adorned element's final
    // rect (the adorner's Measure... the layer's Arrange path) so an adorned
    // element that moved/resized re-syncs its overlay. Returns the number of
    // adorners re-synced. `Update()` = the whole layer; `Update(adorned)` =
    // only that adorned element's adorners.
    [[nodiscard]] std::size_t Update();
    [[nodiscard]] std::size_t Update(UIElement* adorned);

private:
    // The registered adorner record: the borrowed adorner + its z-order.
    struct Hosted
    {
        Adorner*    adorner;              // NON-OWNING borrowed reference
        std::int32_t zOrder = DefaultZOrder;
    };

    // Adorned element -> its registered, z-ordered adorner list (the WPF
    // ElementMap + ZOrderMap unified).
    std::unordered_map<UIElement*, std::vector<Hosted>> hosted_;
};

} // namespace vaml
