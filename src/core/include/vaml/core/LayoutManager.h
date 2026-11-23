#pragma once
// VAML LayoutManager — the INTERNAL, non-owning driver that runs the measure +
// arrange passes over an element tree's dirty subgraph. S126 (dep:S125);
// INTERNALIZED by S843 (dep:S842c).
//
// WPF-faithful model (S843): this is the INTERNAL `ContextLayoutManager`
// analogue — WPF declares `internal sealed class ContextLayoutManager`
// (LayoutManager.cs:14); it is NOT part of the framework's public surface. The
// PUBLIC entry is `UIElement::UpdateLayout()` (UIElement.cs:1630-1633), which
// this header's driver backs. It therefore lives in `vaml::detail` (the same
// INTERNAL namespace as the sibling `vaml::detail::LayoutContext`, its
// real queue-draining engine) — NOT in the public `vaml` namespace. Do NOT use
// it from application/presentation code; call `UIElement::UpdateLayout()`.
//
// Driver shape: NON-OWNING — the root's tree owns its nodes (S121); the driver
// holds a pointer and never creates/frees nodes. Move-only RAII (a driver has
// nothing meaningful to copy).
//
// S839 engine: the real dirtiness bookkeeping + queue drain lives in
// `vaml::detail::LayoutContext`. This driver is the thin per-tree front the
// public UpdateLayout() constructs over the resolved tree root.
//
// Top-level slot: the no-arg drive measures the root at an UNBOUNDED offer and
// arranges it into its own MEASURED DesiredSize at origin 0 — a top-level
// element has no external slot (WPF getProperArrangeRect's parentless-root rule).
// The HOST-frame overload drives the root into an explicit frame instead (the
// HwndSource "measure/arrange the root at the client rect" host role).

#include "vaml/core/UIElement.h"

namespace vaml {
namespace detail {

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

    // The layout driver (the top-level "measure to content" variant). Runs a
    // MEASURE pass then an ARRANGE pass over the bound root, each gated on the
    // root's dirtiness (via the LayoutContext queue):
    //   * if the root/tree is measure-dirty -> the dirty subgraph is measured,
    //     the root at Size::Infinite() (a top-level element has no external
    //     slot);
    //   * if the root/tree is arrange-dirty -> it is arranged, the root into its
    //     own measured DesiredSize at origin 0 (WPF getProperArrangeRect's
    //     parentless-root rule).
    // A clean tree makes this a true no-op: no re-measure, no re-arrange.
    // With no bound root, this is a no-op.
    void UpdateLayout();

    // The HOST-frame drive (S843). Same synchronous, no-op-when-clean engine, but
    // the root is measured against `rootConstraint` and arranged into `rootSlot`
    // instead of "measure to content". This is the HwndSource host role — the
    // presentation layer (VamlRenderer / RenderTarget) drives its top-level root
    // into the frame at this size. No-op when clean.
    void UpdateLayout(const Size& rootConstraint, const Rect& rootSlot);

private:
    // S146: pimpl collapsed — the non-owning root pointer is now a plain
    // by-value member (no Impl heap node, no impl_-> trampoline). Move-only
    // (a driver has nothing meaningful to copy).
    UIElement* root_ = nullptr;
};

} // namespace detail
} // namespace vaml
