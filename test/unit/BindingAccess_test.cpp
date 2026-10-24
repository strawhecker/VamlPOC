// S250_1 — ONE CHANNEL, TWO CONSUMERS (dep:S250). Sprint 6.
//
// Proves the ADR-016 contract at the ACCESSOR level: the binding diagnostics
// channel is ONE sink, reached by a PLAIN ACCESSOR on BOTH the VM layer
// (vaml/mvvm/diagnostics.h) and the view/control layer
// (vaml/core/BindingAccess.h). No library CompositionRoot, no DI slot — just a
// named per-layer free-function accessor forwarding to the same core channel.
//
//   (A) IDENTITY: vaml::mvvm::vm_binding_diagnostics() (VM) and
//       vaml::view_binding_diagnostics() (view) resolve to the SAME underlying
//       sink — installing through one is observable through the other.
//   (B) EMISSION: a sink installed through the VM accessor receives an emission
//       the view accessor forwards; the reverse install is equally observable.
//   (C) POLICY: the VM and view policy defaults agree (BindingPolicy::Fallback).
//   (D) NOT-A-DI-SLOT: the accessors are pure forwards — no state is created,
//       nothing is injected; the binding engine still emits to the same channel
//       the S242/S243/S244 pipeline already proves (the SMART sink installs the
//       same way the engine reads it).
//
// Pure value-domain: registers NO property names. ADR-018: ZERO new locks
// (both accessors forward to the existing core atomic).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral (SHOULD seam)

#include "vaml/core/BindingAccess.h"      // view/control-layer accessor
#include "vaml/core/BindingDiagnostics.h" // BindingDiagnostic, IBindingDiagnostics
#include "vaml/core/BindingExpression.h"  // BindingPolicy, BindingExpressionSpec
#include "vaml/mvvm/diagnostics.h"        // VM-layer accessor

#include <cstdint>
#include <string>
#include <vector>

using vaml::BindingDiagnostic;
using vaml::BindingPolicy;
using vaml::IBindingDiagnostics;
using vaml::set_view_binding_diagnostics;
using vaml::view_binding_diagnostics;
using vaml::view_binding_policy_default;
using vaml::mvvm::binding_policy_default;
using vaml::mvvm::set_vm_binding_diagnostics;
using vaml::mvvm::vm_binding_diagnostics;

namespace {

int pol_int(BindingPolicy p) { return static_cast<int>(p); }

// A recording sink proving the emit path end-to-end through EITHER accessor.
struct RecordingSink final : IBindingDiagnostics
{
    std::vector<BindingDiagnostic> seen;
    void emit(const BindingDiagnostic& d) override { seen.push_back(d); }
};

// RAII guard: installs through the VM accessor, restores the previous sink
// (captured through the VM accessor) at scope exit. The channel is global, so
// tests must restore — and may run in any order.
struct SinkGuard
{
    IBindingDiagnostics& previous;
    explicit SinkGuard(IBindingDiagnostics& sink)
        : previous(set_vm_binding_diagnostics(sink)) {}
    ~SinkGuard() { (void)set_vm_binding_diagnostics(previous); }
};

} // namespace

// ===========================================================================
// A - IDENTITY: VM-layer and view-layer accessors resolve to the SAME sink
// ===========================================================================

TEST_CASE("S250_1 identity: the VM accessor and the view accessor resolve to the SAME sink")
{
    // Both accessors must hand back the same underlying channel right now
    // (before any install) — the defaults are one sink, not two.
    IBindingDiagnostics& vmChan   = vm_binding_diagnostics();          // VM layer
    IBindingDiagnostics& viewChan = view_binding_diagnostics();        // view/control layer
    SHOULD(ShouldlyIntegral(&vmChan == &viewChan).ShouldBe(true));
}

TEST_CASE("S250_1 identity: a sink installed through the VM accessor is the sink the view accessor reads")
{
    RecordingSink recorder;
    // Capture the pre-install sink, install via the VM accessor.
    IBindingDiagnostics& original = set_vm_binding_diagnostics(recorder);
    // The view accessor reads the POST-install sink — the same recorder.
    SHOULD(ShouldlyIntegral(&view_binding_diagnostics() == &recorder).ShouldBe(true));
    // The VM accessor reads the same recorder.
    SHOULD(ShouldlyIntegral(&vm_binding_diagnostics() == &recorder).ShouldBe(true));
    // Restore.
    (void)set_vm_binding_diagnostics(original);
}

// ===========================================================================
// B - EMISSION: both consumers observe emissions on the one channel
// ===========================================================================

TEST_CASE("S250_1 emission: a sink installed via the VM accessor receives an emission the VIEW forward reaches")
{
    RecordingSink recorder;
    // Install through the VM accessor (one channel).
    IBindingDiagnostics& original = set_vm_binding_diagnostics(recorder);

    // A view/control author emits through the VIEW accessor -> the ONE channel.
    BindingDiagnostic d;
    d.bindingId = 501;
    d.sourcePath = "S250_1Title";
    d.reasonCode = vaml::BindingReasonCode::MissingProperty;
    view_binding_diagnostics().emit(d);

    SHOULD(ShouldlyIntegral(recorder.seen.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(recorder.seen[0].bindingId).ShouldBe(std::uint64_t{501}));
    SHOULD(ShouldlyIntegral(recorder.seen[0].sourcePath == "S250_1Title").ShouldBe(true));

    // A VM author emits through the VM accessor -> SAME channel, SAME recorder.
    BindingDiagnostic d2;
    d2.bindingId = 502;
    d2.reasonCode = vaml::BindingReasonCode::SetTargetFailed;
    vm_binding_diagnostics().emit(d2);

    SHOULD(ShouldlyIntegral(recorder.seen.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(recorder.seen[1].bindingId).ShouldBe(std::uint64_t{502}));

    (void)set_vm_binding_diagnostics(original);
}

TEST_CASE("S250_1 emission: a sink installed via the VIEW accessor is observable through the VM accessor")
{
    RecordingSink recorder;
    // Install through the VIEW accessor (one channel).
    IBindingDiagnostics& original = set_view_binding_diagnostics(recorder);

    // A VM author emits through the VM accessor -> the recorder (installed via view) sees it.
    BindingDiagnostic d;
    d.bindingId = 503;
    d.reasonCode = vaml::BindingReasonCode::WriteBackRejected;
    vm_binding_diagnostics().emit(d);

    SHOULD(ShouldlyIntegral(recorder.seen.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(recorder.seen[0].bindingId).ShouldBe(std::uint64_t{503}));

    // Restore via the view accessor (both layer accessors forward to the same core set).
    (void)set_view_binding_diagnostics(original);
    SHOULD(ShouldlyIntegral(&view_binding_diagnostics() == &original).ShouldBe(true));
}

TEST_CASE("S250_1 emission: guard restores the previous sink so later tests stay isolated")
{
    RecordingSink recorder;
    {
        SinkGuard guard(recorder);
        SHOULD(ShouldlyIntegral(&view_binding_diagnostics() == &recorder).ShouldBe(true));
        // Restore happens at scope exit through set_vm_binding_diagnostics(previous).
    }
    // After the guard, the channel is back to a non-recorder sink (the default/
    // whatever preceded). The restore is provable by reinstalling + restoring.
    IBindingDiagnostics& now = view_binding_diagnostics();
    SHOULD(ShouldlyIntegral(&now != &recorder).ShouldBe(true));
}

// ===========================================================================
// C - POLICY: VM and view defaults agree (one BindingPolicy default)
// ===========================================================================

TEST_CASE("S250_1 policy: the VM and view layer defaults agree on BindingPolicy::Fallback")
{
    SHOULD(ShouldlyIntegral(pol_int(binding_policy_default())).ShouldBe(pol_int(BindingPolicy::Fallback)));
    SHOULD(ShouldlyIntegral(pol_int(view_binding_policy_default())).ShouldBe(pol_int(BindingPolicy::Fallback)));
    // And they agree with the engine's own default (BindingExpressionSpec).
    vaml::BindingExpressionSpec spec;
    SHOULD(ShouldlyIntegral(pol_int(spec.policy)).ShouldBe(pol_int(BindingPolicy::Fallback)));
}

// ===========================================================================
// D - NOT-A-DI-SLOT: the accessors are pure forwards, no injection
// ===========================================================================

TEST_CASE("S250_1 not-a-DI-slot: the accessors install/read the one channel, never inject")
{
    // The accessors are free functions with NO associated object — a consumer
    // holds no composition state and is never injected into. Proof: installing a
    // recorder through the VM accessor and emitting through the view accessor
    // lands in the same object (already covered). Here we re-confirm the
    // forward-only contract by checking the pre-install default is a stable
    // single identity across repeated reads (no per-call allocation/state).
    IBindingDiagnostics& a = view_binding_diagnostics();
    IBindingDiagnostics& b = view_binding_diagnostics();
    SHOULD(ShouldlyIntegral(&a == &b).ShouldBe(true));
    IBindingDiagnostics& c = vm_binding_diagnostics();
    SHOULD(ShouldlyIntegral(&a == &c).ShouldBe(true));
}
