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

#include "vaml/core/LayoutManager.h"

#include "vaml/core/Geometry.h" // Size, Rect

#include <memory>
#include <utility>

namespace vaml {

struct LayoutManager::Impl
{
    UIElement* root = nullptr;
};

VAML_CORE_EXPORT
LayoutManager::LayoutManager()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
LayoutManager::LayoutManager(UIElement* root)
    : impl_(std::make_unique<Impl>())
{
    impl_->root = root;
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
    impl_->root = root;
}

VAML_CORE_EXPORT
UIElement* LayoutManager::root() const noexcept
{
    return impl_->root;
}

VAML_CORE_EXPORT
void LayoutManager::UpdateLayout()
{
    UIElement* root = impl_->root;
    if (root == nullptr)
    {
        return; // no tree bound -> nothing to drive
    }

    // Measure pass: gated on the root's measure-dirty bit. A clean root skips
    // the pass entirely (no re-measure). The root is offered an UNBOUNDED
    // available (Size::Infinite()) — a top-level element has no external slot;
    // a panel resolves per-child offers + re-measures only its dirty children.
    if (root->is_measure_dirty())
    {
        root->Measure(Size::Infinite());
    }

    // Arrange pass: gated on the root's arrange-dirty bit. The root is
    // arranged into its OWN measured DesiredSize at origin 0 — a top-level
    // element has no external slot (a real window viewport arrives at the
    // window sprints, recorded NO-GAP). A panel re-arranges only its dirty
    // children INTO their slots via root->Arrange.
    if (root->is_arrange_dirty())
    {
        const vaml::Size d = root->desired_size();
        root->Arrange(vaml::Rect{ 0.0f, 0.0f, d.width, d.height });
    }

    // Every flag is already cleared: the root's Measure/Arrange cleared its own
    // bits, and each child visited by a panel's pass cleared its own bits via
    // its own Measure/Arrange. The tree is wholly clean — a subsequent clean
    // UpdateLayout is a true no-op (neither gate fires).
}

} // namespace vaml
