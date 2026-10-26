// S441 (dep:S440) — MainViewModel for the app-shell (the S441 + S442 pair,
// delivered together per the user decision).
//
// HEADLESS — proves the app-shell's MainViewModel + its S442 demo service
// wiring against the REAL vaml::ObservableObject + PROPERTY_BINDABLE +
// RelayCommand/ICommand core (S241/S246/S247_1 + the S250/S256 plain-constructor
// wiring pattern), compiled from the vaml_viewer include into vaml-core-tests
// (the ADR-021 consumer path):
//   (1) AppShellViewModel is `final : ObservableObject` with PROPERTY_BINDABLE
//       props the markup binds (Text / Checked / SliderValue / Dim + Status);
//   (2) props ROUND-TRIP (getter + the S241 ObservableObject store both update)
//       and follow the REAL-CHANGE-ONLY discipline (same-value set -> false, no
//       store write, no signal);
//   (3) the RelayCommands execute + CanExecute + CanExecuteChanged fire through
//       the REAL ICommand interface (S246);
//   (4) the commands are WIRED TO the injected AppShellDemoService (S442) —
//       RunCommand increments the service counter + folds its dimmer into Dim +
//       Status; EchoCommand echoes the Text prop through the service into Status;
//   (5) the S442 static composition seam (build_app_shell_composition) constructs
//       ONLY through plain constructors — nothing registered/resolved (the
//       "engineer wires, we bind" headline lives at the app boundary).
//
// ADR-018: zero locks. Registers no global DPs (binds onto the VM's own
// ObservableObject props).

#include "doctest/doctest.h"

#include "app_shell_vm.h"      // the S441 VM
#include "demo_service.h"      // the S442 service + composition seam

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/ICommand.h"      // ICommand (the command surface)
#include "vaml/core/ObservableObject.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <vector>

using vaml::ICommand;
using vaml::Value;
using vaml::viewer::AppShellDemoService;
using vaml::viewer::AppShellViewModel;
using vaml::viewer::build_app_shell_composition;

namespace {

// A recording sink that counts + captures PropertyChanged (id, old, new) events.
struct PropRecorder
{
    struct Entry
    {
        vaml::PropertyId id;
        vaml::Value      oldValue;
        vaml::Value      newValue;
    };
    std::vector<Entry> entries;
    void record(vaml::PropertyId id, const vaml::Value& o, const vaml::Value& n)
    {
        entries.push_back(Entry{ id, o, n });
    }
    std::size_t size() const { return entries.size(); }
};

} // namespace

// ---------------------------------------------------------------------------
// 1/2. Props round-trip + real-change-only discipline
// ---------------------------------------------------------------------------

TEST_CASE("S441: VM is a final ObservableObject with the markup-bound props")
{
    AppShellViewModel vm(AppShellDemoService{});
    // The props the S443 markup binds exist + round-trip through the getter.
    SHOULD(ShouldlyString(vm.Text()).ShouldBe(std::string{}));
    SHOULD(ShouldlyString(vm.Checked()).ShouldBe(false));
    SHOULD(ShouldlyString(vm.SliderValue()).ShouldBe(0.5 * 100.0));
    SHOULD(ShouldlyString(vm.Dim()).ShouldBe(0.5));
    SHOULD(ShouldlyString(vm.Status()).ShouldBe(std::string{}));

    // Setting a real value updates the getter AND the S241 ObservableObject store.
    (void)vm.set_Text("hello");
    (void)vm.set_Checked(true);
    (void)vm.set_Dim(0.8);
    (void)vm.set_SliderValue(80.0);
    (void)vm.set_Status("ok");

    SHOULD(ShouldlyString(vm.Text()).ShouldBe(std::string("hello")));
    SHOULD(ShouldlyString(vm.Get(vaml::PropertyId::of("Text"))).ShouldBe(Value::String("hello")));
    SHOULD(ShouldlyString(vm.Checked()).ShouldBe(true));
    SHOULD(ShouldlyString(vm.Dim()).ShouldBe(0.8));
    SHOULD(ShouldlyString(vm.SliderValue()).ShouldBe(80.0));
    SHOULD(ShouldlyString(vm.Status()).ShouldBe(std::string("ok")));
}

TEST_CASE("S441: props are real-change-only (same-value set -> no store write, no signal)")
{
    AppShellViewModel vm(AppShellDemoService{});
    PropRecorder rec;
    (void)vm.add_property_changed([&rec](vaml::PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    // A real first change fires + stores.
    const bool first = vm.set_Text("A");
    SHOULD(ShouldlyIntegral(first).ShouldBe(1));
    // The SAME value is a no-change: false, no store overwrite, no signal.
    const bool second = vm.set_Text("A");
    SHOULD(ShouldlyIntegral(second).ShouldBe(0));
    SHOULD(ShouldlyString(vm.Text()).ShouldBe(std::string("A")));
    // Only the first (real) change fired across the bound props exercised.
    // set_Text fired exactly one signal.
    std::size_t textSignals = 0;
    for (auto& e : rec.entries)
        if (std::string(e.id.name()) == "Text") ++textSignals;
    SHOULD(ShouldlyIntegral(textSignals).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// 3. RelayCommand execute + CanExecute + CanExecuteChanged (through ICommand)
// ---------------------------------------------------------------------------

TEST_CASE("S441: RunCommand executes + CanExecute + CanExecuteChanged fire (ICommand)")
{
    AppShellViewModel vm(AppShellDemoService{});
    ICommand& run = vm.run_command();

    // The run gate: not-checked -> not runnable; checked -> runnable (under the cap).
    (void)vm.set_Checked(false);
    SHOULD(ShouldlyString(run.CanExecute(Value::Null())).ShouldBe(false));

    (void)vm.set_Checked(true);
    SHOULD(ShouldlyString(run.CanExecute(Value::Null())).ShouldBe(true));

    // CanExecuteChanged fires when the state that gates executability flips:
    // clearing the check raises + a consumer re-queries.
    std::vector<int> raised;
    (void)run.add_can_execute_changed([&raised] { raised.push_back(1); });
    (void)vm.set_Checked(false);
    vm.refresh_run_can_execute();
    (void)vm.set_Checked(true);
    vm.refresh_run_can_execute();
    SHOULD(ShouldlyIntegral(raised.size()).ShouldBe(2u));

    // Execute (through the interface) runs the injected service + updates props.
    (void)vm.set_Checked(true);
    run.Execute(Value::Null());
    // The service counter incremented via the command path.
    SHOULD(ShouldlyIntegral(vm.service().counter()).ShouldBe(1));
    // Status was refreshed by the command (never silent).
    SHOULD(ShouldlyString(vm.Status()).ShouldBe(std::string("ran #1")));
}

TEST_CASE("S441: the run cap flips CanExecute via CanExecuteChanged (re-query)")
{
    AppShellViewModel vm(AppShellDemoService{});
    ICommand& run = vm.run_command();
    (void)vm.set_Checked(true);

    std::vector<int> raised;
    (void)run.add_can_execute_changed([&raised] { raised.push_back(1); });

    // Run past the cap (kRunCap = 3): each run raises CanExecuteChanged, and
    // once the counter reaches the cap the command becomes non-executable.
    for (int i = 0; i < 3; ++i)
        run.Execute(Value::Null());
    SHOULD(ShouldlyIntegral(vm.service().counter()).ShouldBe(3));
    // After the cap, CanExecute is false (the raise lets a consumer re-query).
    SHOULD(ShouldlyString(run.CanExecute(Value::Null())).ShouldBe(false));
    // 3 runs -> 3 raises.
    SHOULD(ShouldlyIntegral(raised.size()).ShouldBe(3u));
}

// ---------------------------------------------------------------------------
// 4. Commands wired to the injected AppShellDemoService (S442)
// ---------------------------------------------------------------------------

TEST_CASE("S441: EchoCommand echoes Text through the injected service")
{
    AppShellViewModel vm(AppShellDemoService{});
    ICommand& echo = vm.echo_command();
    (void)vm.set_Text("VAML echo!");

    // The echo command is always executable (no predicate).
    SHOULD(ShouldlyString(echo.CanExecute(Value::Null())).ShouldBe(true));
    echo.Execute(Value::Null());

    // The service's echoed state reflects the wiring target; the VM's Status was
    // set through it.
    SHOULD(ShouldlyString(vm.service().echoed()).ShouldBe(std::string("VAML echo!")));
    SHOULD(ShouldlyString(vm.Status()).ShouldBe(std::string("echo: VAML echo!")));
}

TEST_CASE("S441: RunCommand folds the service dimmer into Dim/SliderValue")
{
    AppShellViewModel vm(AppShellDemoService{ /* dim 0.5 */ });
    ICommand& run = vm.run_command();
    (void)vm.set_Checked(true);

    // Mutate the injected service's dimmer directly (the S442 wiring target) to
    // a known value, then run: the command folds the LIVE service dim into the
    // VM's bound Dim + SliderValue.
    vm.service().set_dim(0.25);
    run.Execute(Value::Null());

    SHOULD(ShouldlyString(vm.Dim()).ShouldBe(0.25));
    SHOULD(ShouldlyString(vm.SliderValue()).ShouldBe(25.0));
}

// ---------------------------------------------------------------------------
// 5. The S442 static composition seam: plain constructors only
// ---------------------------------------------------------------------------

TEST_CASE("S442: build_app_shell_composition wires the VM via plain constructors")
{
    auto c = build_app_shell_composition();
    SHOULD(ShouldlyIntegral((bool)c.vm).ShouldBe(1));
    // The composed VM holds a real injected service (constructed, not resolved).
    SHOULD(ShouldlyIntegral(c.vm->service().dim() >= 0.0).ShouldBe(1));
    // The command surface is live on the composed VM.
    SHOULD(ShouldlyString(c.vm->run_command().CanExecute(Value::Null())).ShouldBe(false)); // not checked
    // Nothing registered/resolved — this is the "engineer wires, we bind" path:
    // exercise the command end-to-end through the composed VM.
    (void)c.vm->set_Checked(true);
    c.vm->run_command().Execute(Value::Null());
    SHOULD(ShouldlyIntegral(c.vm->service().counter()).ShouldBe(1));
}
