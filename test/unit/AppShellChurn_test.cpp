// S446 (dep:S445) — app-shell HARDENING/VERIFICATION (headless Rail 1): PROVES
// the three S446 title claims against the REAL in-repo app-shell tree through the
// REAL library surfaces, with ZERO product feature code added.
//
// (a) NO LAYOUT THRASH — the S146 steady-state heap-churn contract, elevated to
//     the app-shell's arrange+encode+publish producer steady state: after a
//     warmup pass, an IDENTICAL repeated producer loop performs ZERO VAML-owned
//     heap churn (heap_allocation_count()/heap_allocation_bytes() stay FLAT).
// (b) NO LEAK OVER 10K FRAMES (headless equivalent) — 10,000 repeated producer
//     passes through the SAME app-shell tree; the churn counters stay flat AND
//     the retained reader reachability (the named RunButton/InputBox/DimSlider/
//     EnabledCheck/StatusText) + the retained VM store stay intact (no per-frame
//     object-budget growth = no leak accumulation in the loop).
// (c) RETAINED-STABLE AFTER 10K — the S443/S445 TwoWay wires STILL write back to
//     the VM store after the 10k loop (the loop did not dangle/leak the retained
//     wiring).
//
// Deliverable A of the S446 plan drives the app-shell tree through the REAL S212
// headless CPU host `vaml::presentation::VamlRenderer::create()` — the exact
// producer path the live `--app` window runs (set_root → set_frame_size →
// invalidate = arrange → encode_visual_slice → publish → postbox().consume_latest()).
//
// ADR-018: zero locks (single-threaded on the UI/consumer affinity, same as every
// other headless test). ADR-021: compiles the REAL app_shell_binding.h seam (the
// S443/S445 plan) from the vaml_viewer include (the ADR-021 consumer path). The
// authoritative "no leaks over 10k frames" on BOTH compilers is the ASan/LSan
// lane (build/verify_s446_asan.cmd) running THIS suite under ASan; THIS suite is
// the headless steady-state + retained-stability proof that feeds it.

#include "doctest/doctest.h"

#include "app_shell_binding.h"   // S443/S445: run_app_shell_binding_plan + AppShellBindingPlanResult

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include <vaml/presentation/VamlRenderer.h>  // S212: the headless CPU host (create)

#include "vaml/core/Element.h"       // TextBox / Text / Button / Slider / CheckBox
#include "vaml/core/HeapStats.h"     // S146: the public churn funnel counters
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using vaml::viewer::run_app_shell_binding_plan;
using vaml::heap_allocation_count;
using vaml::heap_allocation_bytes;

namespace {

// Looks up a named element on the plan AND asserts it is the expected control
// type (the retained reader keeps the S443/S445 wires + the named registry armed).
template <typename ControlT>
ControlT* named_as(vaml::viewer::AppShellBindingPlanResult& plan, const char* name)
{
    return dynamic_cast<ControlT*>(plan.named_element(name));
}

// Runs ONE full producer pass on the headless host (the S212 create() path): the
// exact arrange + encode + publish + consume-once-per-frame the render thread
// drains. Returns the drained scene version.
std::uint64_t one_producer_pass(vaml::presentation::VamlRenderer& host,
                                std::vector<vaml::vulkan::ui::DrawItem>& snapshot)
{
    (void)host.invalidate();                       // arrange + encode + publish
    return host.postbox().consume_latest(snapshot); // the render-thread drain
}

// A helper that builds the plan + a headless host ONCE (the retained reader/VM
// stay alive across the whole loop — the exact live `--app` retention).
struct ShellHarness
{
    vaml::viewer::AppShellBindingPlanResult plan = run_app_shell_binding_plan();
    std::unique_ptr<vaml::presentation::VamlRenderer> host;

    ShellHarness()
    {
        REQUIRE(plan.ok);
        REQUIRE(plan.root != nullptr);
        host = vaml::presentation::VamlRenderer::create();
        REQUIRE(host != nullptr);
        REQUIRE(host->set_root(plan.root.get()));
        host->set_frame_size(800.0f, 600.0f);
    }
};

} // namespace

// ---------------------------------------------------------------------------
// (a) NO LAYOUT THRASH — the S146 heap-churn contract on the app-shell producer
// ---------------------------------------------------------------------------

TEST_CASE("S446: the app-shell arrange+encode+publish steady state is heap-FLAT after warmup")
{
    ShellHarness h;   // plan (retained reader/VM) + headless host, once
    std::vector<vaml::vulkan::ui::DrawItem> snapshot;

    // WARMUP: several full producer passes so the S146 funnels (InternStore +
    // object pools) content-dedup + relax before the flatness snapshot.
    for (int i = 0; i < 5; ++i)
        (void)one_producer_pass(*h.host, snapshot);

    const std::size_t countAfterWarmup = heap_allocation_count();
    const std::size_t bytesAfterWarmup = heap_allocation_bytes();

    // IDENTICAL repeated producer passes over the SAME app-shell tree: the S146
    // contract elevated to the app-shell steady state — ZERO VAML-owned heap churn
    // (no layout thrash). ~50 passes proves the after-warmup flatness headlessly.
    for (int i = 0; i < 50; ++i)
        (void)one_producer_pass(*h.host, snapshot);

    SHOULD(ShouldlyIntegral(heap_allocation_count()).ShouldBe(countAfterWarmup));
    SHOULD(ShouldlyIntegral(heap_allocation_bytes()).ShouldBe(bytesAfterWarmup));

    // The retained reader still resolves every named control (the loop never
    // dangled/leaked the retained wiring).
    SHOULD(ShouldlyIntegral(named_as<vaml::Button>(h.plan, "RunButton") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBox>(h.plan, "InputBox") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Slider>(h.plan, "DimSlider") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::CheckBox>(h.plan, "EnabledCheck") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBlock>(h.plan, "StatusText") != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (b) NO LEAK OVER 10K FRAMES (headless equivalent) — the loop stays flat + armed
// ---------------------------------------------------------------------------

TEST_CASE("S446: 10k producer passes stay heap-flat and the retained wiring stays intact")
{
    ShellHarness h;
    std::vector<vaml::vulkan::ui::DrawItem> snapshot;

    for (int i = 0; i < 5; ++i)
        (void)one_producer_pass(*h.host, snapshot);

    const std::size_t countAfterWarmup = heap_allocation_count();
    const std::size_t bytesAfterWarmup = heap_allocation_bytes();

    // The 10k-frame headless equivalent: 10,000 repeated producer passes over the
    // SAME app-shell tree. No per-frame object-budget growth => the churn funnel
    // stays FLAT across all 10k (no leak accumulation in the loop).
    for (int i = 0; i < 10000; ++i)
        (void)one_producer_pass(*h.host, snapshot);

    SHOULD(ShouldlyIntegral(heap_allocation_count()).ShouldBe(countAfterWarmup));
    SHOULD(ShouldlyIntegral(heap_allocation_bytes()).ShouldBe(bytesAfterWarmup));

    // Retained-reader reachability survived all 10k passes (never dangled).
    SHOULD(ShouldlyIntegral(named_as<vaml::Button>(h.plan, "RunButton") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBox>(h.plan, "InputBox") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Slider>(h.plan, "DimSlider") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::CheckBox>(h.plan, "EnabledCheck") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBlock>(h.plan, "StatusText") != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (c) RETAINED-STABLE AFTER 10K — the S443/S445 TwoWay wires still write back
// ---------------------------------------------------------------------------

TEST_CASE("S446: the TwoWay wires still write back to the VM store after the 10k loop")
{
    ShellHarness h;
    std::vector<vaml::vulkan::ui::DrawItem> snapshot;

    for (int i = 0; i < 5; ++i)
        (void)one_producer_pass(*h.host, snapshot);
    for (int i = 0; i < 10000; ++i)
        (void)one_producer_pass(*h.host, snapshot);

    // The retained VM store + the TextBox.Text TwoWay wire survived the loop: a
    // TARGET EDIT (a real TextBox.Text DP change, the S244 write-back) STILL lands
    // in the VM's observable store.
    auto* inputBox = named_as<vaml::TextBox>(h.plan, "InputBox");
    SHOULD(ShouldlyIntegral(inputBox != nullptr).ShouldBe(1));
    (void)inputBox->set_text("retained across 10k");
    SHOULD(ShouldlyString(h.plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("retained across 10k")));

    // The S445 SliderValue TwoWay wire survived too: a drag STILL pushes the ratio
    // onto the VM's SliderValue store.
    auto* dimSlider = named_as<vaml::Slider>(h.plan, "DimSlider");
    SHOULD(ShouldlyIntegral(dimSlider != nullptr).ShouldBe(1));
    (void)dimSlider->set_value_from_ratio(0.8);
    SHOULD(ShouldlyString(h.plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(80.0)));

    // The retained reader (the wiring owner) is still the live object (never
    // released/dangling).
    SHOULD(ShouldlyIntegral((bool)h.plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)h.plan.vm).ShouldBe(1));
}
