#pragma once
// VAML LayoutManager — the non-owning driver that runs the measure + arrange
// passes over an element tree's dirty subgraph. S126 (dep:S125).
//
// WPF-faithful model: layout is driven by a manager that asks the ROOT of a
// tree to Measure then Arrange, each pass gated on the root's S126 dirty bit.
// The manager is NON-OWNING — the root's tree owns its nodes (S121); the
// manager holds a pointer and never creates/frees nodes.
//
// Dirty-subgraph realization: InvalidateMeasure/InvalidateArrange (S126) mark
// the target + its ancestor chain dirty; a panel's measure/arrange re-runs only
// its DIRTY children (the real StackPanel/Grid adopt this at S129/S131); the
// manager drives the top-level passes and then clears every flag so ONE
// UpdateLayout leaves a fully clean tree — a subsequent clean UpdateLayout() is
// a true no-op (no re-measure, no re-arrange).
//
// Top-level slot: the root is arranged into its own MEASURED DesiredSize at
// origin 0 — a top-level element has no external slot. A real window viewport
// (the top-level arrange slot) arrives at the window sprints, recorded NO-GAP,
// not silent.

#include "vaml/core/UIElement.h"

namespace vaml {

class VAML_CORE_EXPORT LayoutManager
{
public:
    // A manager over no tree: UpdateLayout() is a no-op until set_root().
    LayoutManager();

    // A manager over `root` (the root's tree owns its nodes; the manager never
    // takes ownership). `root` may be null (equivalent to the default ctor).
    explicit LayoutManager(UIElement* root);

    // Move-only (the pimpl owns the non-owning root pointer; copies are
    // meaningless for a driver).
    LayoutManager(const LayoutManager&) = delete;
    LayoutManager& operator=(const LayoutManager&) = delete;
    LayoutManager(LayoutManager&&) noexcept;
    LayoutManager& operator=(LayoutManager&&) noexcept;

    ~LayoutManager();

    // (Re)binds the manager to a new tree root. Passing null unbinds (a
    // subsequent UpdateLayout is a no-op). The manager does NOT own `root`.
    void set_root(UIElement* root) noexcept;

    // The currently bound root (may be null). Non-owning.
    [[nodiscard]] UIElement* root() const noexcept;

    // The layout driver. Runs a MEASURE pass then an ARRANGE pass over the
    // bound root, each gated on the root's S126 dirty bit:
    //   * if the root is measure-dirty  -> root->Measure(Size::Infinite());
    //   * if the root is arrange-dirty  -> root->Arrange(its measured DesiredSize
    //                                     at origin 0).
    // After the passes, EVERY flag in the tree is cleared (a panel clears its
    // dirty children during the passes; the manager clears the root — and any
    // flag a panel left — so the tree is wholly clean). A clean root (no dirty
    // bit on either axis) makes UpdateLayout a true no-op: no re-measure, no
    // re-arrange, nothing re-runs. With no bound root, this is a no-op.
    void UpdateLayout();

private:
    // S146: pimpl collapsed — the non-owning root pointer is now a plain
    // by-value member (no Impl heap node, no impl_-> trampoline). Move-only
    // (a driver has nothing meaningful to copy).
    UIElement* root_ = nullptr;
};

} // namespace vaml
