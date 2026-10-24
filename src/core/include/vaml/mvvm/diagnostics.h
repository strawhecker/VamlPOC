#pragma once
// VAML mvvm — the VM-layer plain accessor to the binding diagnostics sink +
// binding policy. S250_1 (dep:S250). Sprint 6.
//
// This is the ViewModel/VM-layer side of the ADR-016 ONE CHANNEL, TWO
// CONSUMERS contract: the binding failure channel is ONE sink, reached by BOTH
// the VM layer (here) and the view/control layer (vaml/core/BindingAccess.h).
// A VM author includes THIS header and reaches the same sink + the same
// BindingPolicy that a control author reaches through BindingAccess.h — two
// consumers, one channel.
//
// Accessor naming: `vm_binding_diagnostics()` / `set_vm_binding_diagnostics()`
// mirror the view layer's `view_binding_diagnostics()` /
// `set_view_binding_diagnostics()` (BindingAccess.h) — the S250_1 "a PLAIN
// ACCESSOR on EACH layer" symmetry, one channel, two consumers. The `vm_`/`view_`
// prefixes also deliberately avoid an ADL overload collision with the
// identically-shaped core vaml::binding_diagnostics() /
// vaml::set_binding_diagnostics() (same signature, same argument types).
//
// It is NOT a library CompositionRoot and NOT a DI slot — it holds no
// composition state, stores nothing, injects nothing. It is a paper-thin
// named accessor forwarding to the SAME core channel (the atomic current sink
// in BindingDiagnostics.cpp). Installing a sink through this accessor is
// observable from the view layer and vice-versa (proved by the S250_1 tests).
//
// Header-only, compiled into vaml-core like property_bindable.h. ADR-018:
// ZERO new locks — pure inline forward to the existing core atomic.

#include "vaml/core/BindingAccess.h"      // the one channel, both layers reach it
#include "vaml/core/BindingDiagnostics.h" // IBindingDiagnostics, BindingDirection
#include "vaml/core/BindingExpression.h"  // BindingPolicy

namespace vaml::mvvm {

// The VM layer's current diagnostics sink — the SAME core channel a control
// author reaches via vaml::view_binding_diagnostics() (one channel, two
// consumers; ADR-016). Named `vm_*` to mirror the view layer's `view_*`
// accessor (S250_1's "a plain accessor on EACH layer") and to avoid an ADL
// collision with the identically-shaped vaml::binding_diagnostics().
[[nodiscard]] inline IBindingDiagnostics& vm_binding_diagnostics() noexcept
{
    return vaml::view_binding_diagnostics();
}

// The VM layer's opt-in: installs `sink` as the current channel, returns the
// PREVIOUS sink (restore-able). Intended to be called ONCE at startup before
// bindings are created. Installing here is observable from the view layer too.
[[nodiscard]] inline IBindingDiagnostics& set_vm_binding_diagnostics(IBindingDiagnostics& sink) noexcept
{
    return vaml::set_view_binding_diagnostics(sink);
}

// The VM layer's default binding policy — the SAME default the engine uses
// everywhere (BindingPolicy::Fallback), reached as a named per-layer constant.
[[nodiscard]] inline BindingPolicy binding_policy_default() noexcept
{
    return vaml::view_binding_policy_default();
}

} // namespace vaml::mvvm
