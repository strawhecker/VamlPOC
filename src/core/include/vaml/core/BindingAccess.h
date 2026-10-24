#pragma once
// VAML BindingAccess — the VIEW/CONTROL-layer plain accessor to the binding
// diagnostics sink + binding policy. S250_1 (dep:S250). Sprint 6.
//
// ADR-016: the binding failure channel is ONE channel with TWO consumers — the
// VM layer and the view/control layer. This header is the VIEW/control-layer
// consumer's PLAIN ACCESSOR: free functions that forward to the SAME core
// channel (binding_diagnostics / set_binding_diagnostics / BindingPolicy) that
// the vaml/mvvm diagnostics accessor forwards to. Two consumers, one channel.
//
// It is NOT a library CompositionRoot and NOT a DI slot — it never holds
// composition state, never stores a consumer, and never injects anything. It is
// a paper-thin named accessor, reached by a control/view author who includes
// THIS header. It lives OUTSIDE DependencyObject.h so a control author reaches
// the sink without forcing an include cycle (BindingDiagnostics -> PropertyPath);
// a control's DepedencyObject base stays untouched (S250_1's "plain accessor on
// each layer, not a DI slot").
//
// Identity/forwarding contract: vaml::view_binding_diagnostics() and
// vaml::binding_diagnostics() (core) resolve to the SAME underlying sink — they
// forward to the identical atomic pointer (BindingDiagnostics.cpp's current_sink).
// Installing a sink through either accessor is observable through both, exactly
// the "one channel, two consumers" ADR-016 contract the S250_1 tests prove.
//
// ADR-018: ZERO new locks — this is a pure inline forward to the existing
// core atomic; no new shared state is introduced here.

#include "vaml/core/BindingDiagnostics.h" // IBindingDiagnostics, binding_diagnostics, set_binding_diagnostics
#include "vaml/core/BindingExpression.h"  // BindingPolicy (the policy slot these accessors reach)

namespace vaml {

// The view/control layer's current diagnostics sink — forwards to the SAME
// core channel the VM layer reads (one channel, two consumers; ADR-016).
[[nodiscard]] inline IBindingDiagnostics& view_binding_diagnostics() noexcept
{
    return binding_diagnostics();
}

// The view/control layer's opt-in: installs `sink` as the current channel and
// returns the PREVIOUS sink (restore-able). Intended to be called ONCE at
// startup before bindings are created, exactly like set_binding_diagnostics.
// Installing here is observable from the VM layer too (one channel).
[[nodiscard]] inline IBindingDiagnostics& set_view_binding_diagnostics(IBindingDiagnostics& sink) noexcept
{
    return set_binding_diagnostics(sink);
}

// The view/control layer's default binding policy — same default the binding
// engine uses everywhere (BindingPolicy::Fallback; Class A -> emit + nullptr,
// never a throw), reached as a named per-layer constant (S250_1).
[[nodiscard]] inline BindingPolicy view_binding_policy_default() noexcept
{
    return BindingPolicy::Fallback;
}

} // namespace vaml
