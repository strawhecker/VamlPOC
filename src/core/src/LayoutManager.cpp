// S126 — LayoutManager out-of-line definitions.
//
// The non-owning layout driver: measures + arranges the bound root over its
// dirty subgraph (gated on the root's S126 dirty bits), then clears every flag
// so one UpdateLayout leaves a fully clean tree.
//
// Flag-clearing model: a completed UIElement::Measure clears ITS measure bit;
// a completed UIElement::Arrange clears ITS arrange bit. A panel (S125
// archetype / S129/S131 real controls) re-measures/re-arranges its DIRTY
// children, so those children clear their own bits. Thus one UpdateLayout pass
// over the dirty subgraph leaves every visited node clean — including the
// root, whose Measure/Arrange clear its own bits. A subsequent clean
// UpdateLayout is a true no-op.
//
// S146 (refactor gate) — the pimpl is COLLAPSED: the non-owning root pointer is
// now a plain by-value member (root_) of LayoutManager (see LayoutManager.h).

#include "vaml/core/LayoutManager.h"

#include "vaml/core/Geometry.h"     // Size, Rect
#include "vaml/core/LayoutContext.h" // S839 — the per-thread layout need-queue

namespace vaml {
namespace detail {

VAML_CORE_EXPORT
LayoutManager::LayoutManager()
    : root_(nullptr)
{
}

VAML_CORE_EXPORT
LayoutManager::LayoutManager(UIElement* root)
    : root_(root)
{
}

VAML_CORE_EXPORT
LayoutManager::~LayoutManager() = default;

VAML_CORE_EXPORT
LayoutManager::LayoutManager(LayoutManager&&) noexcept = default;

VAML_CORE_EXPORT
LayoutManager& LayoutManager::operator=(LayoutManager&&) noexcept = default;

VAML_CORE_EXPORT
void LayoutManager::set_root(UIElement* root) noexcept
{
    root_ = root;
}

VAML_CORE_EXPORT
UIElement* LayoutManager::root() const noexcept
{
    return root_;
}

VAML_CORE_EXPORT
void LayoutManager::UpdateLayout()
{
    UIElement* root = root_;
    if (root == nullptr)
    {
        return; // no tree bound -> nothing to drive
    }

    // S839: drive through the faithful per-thread layout context. The root is
    // measured at an UNBOUNDED offer (a top-level element has no external slot)
    // and arranged into its own measured DesiredSize at origin 0. The context
    // drains the measure queue (topmost-first, at PreviousConstraint) then the
    // arrange queue, so only DIRTY subgraphs re-run: a child whose DesiredSize
    // did NOT change leaves its ancestors clean (no re-measure); one that DID
    // change dirties the ancestor through the measure waterfall. A clean tree is
    // a true no-op.
    vaml::detail::LayoutContext::current().update_layout(
        root, Size::Infinite(), vaml::Rect{ 0.0f, 0.0f, 0.0f, 0.0f },
        /*root_slot_from_desired=*/true);
}

VAML_CORE_EXPORT
void LayoutManager::UpdateLayout(const Size& rootConstraint, const Rect& rootSlot)
{
    UIElement* root = root_;
    if (root == nullptr)
    {
        return; // no tree bound -> nothing to drive
    }

    // S843: the HOST-frame drive — the presentation layer is the "host" that
    // hands its top-level root an explicit frame (measure at `rootConstraint`,
    // arrange into `rootSlot`), exactly as WPF's HwndSource measures/arranges the
    // root visual at the client rect before the ContextLayoutManager drains. The
    // LayoutContext still drains the queue topmost-first, so this is a no-op when
    // the tree is clean and reaches a leaf-only dirty subgraph the root's clean
    // early-out would otherwise starve.
    vaml::detail::LayoutContext::current().update_layout(
        root, rootConstraint, rootSlot, /*root_slot_from_desired=*/false);
}

} // namespace detail
} // namespace vaml
