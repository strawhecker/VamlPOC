// vaml_viewer — the S441 APP-SHELL ViewModel (dep:S440).
//
// THE APP-SHELL'S MainViewModel: a REAL `vaml::ObservableObject` (S241) with
// `PROPERTY_BINDABLE` observable props (S247_1) that the authored MainView.vaml
// binds ({Binding}/{Command}, wired at S443) + `RelayCommand`s (S246/ICommand)
// wired to the S442 demo domain service (`AppShellDemoService`) via PLAIN
// CONSTRUCTORS (the S250 "engineer wires, we bind" headline — NO DI, NO library
// CompositionRoot).
//
// The VM:
//   - owns an `AppShellDemoService` BY VALUE (constructed from the caller's
//     argument through a plain ctor — the S256 `ViewerDemoViewModel` precedent;
//     S441 + S442 are delivered together per the user decision),
//   - exposes PROPERTY_BINDABLE props the markup binds:
//       Text        (std::string) — the TextBox text / echo round-trip
//       Checked     (bool)        — the CheckBox state / run-enable gate
//       SliderValue (double)      — the Slider.Value (the dim percent)
//       Dim         (double)      — the visible opacity dim [0,1]
//       Status      (std::string) — the StatusText {Binding Status} target
//     (Status is included because MainView.vaml's only markup binding is
//     `{Binding Status}` — plan-S441 lists the four primary props; Status
//     keeps the S443 binding surface coherent, NO-GAP not silent),
//   - exposes the commands as `ICommand&` (the S253 {Command} resolves + S443
//     Click wiring invokes through the ICommand interface):
//       RunCommand      — runs the demo (increments the service counter, folds
//                         the service dimer into Dim, refreshes Status) gated by
//                         can_run() (Checked == true + counter below cap), with
//                         CanExecuteChanged raised after a state change;
//       EchoCommand     — echoes the Text prop through the service into Status.
//
// RelayCommand is MOVE-ONLY, so the commands are held by unique_ptr and exposed
// as `ICommand&` (never copied). ADR-018: ZERO new locks (single-threaded on the
// viewer's UI thread).
//
// The VM is `final : ObservableObject` per plan-S441. Header/cpp split: the
// command bodies + service wiring live in app_shell_vm.cpp (the .cpp is compiled
// into both apps + the test target, ADR-021). Compiled as REAL permanent code,
// kept-and-extended by S443–S445.

#pragma once

#include "vaml/core/ICommand.h"            // ICommand (the command surface)
#include "vaml/core/ObservableObject.h"   // ObservableObject (the VM base)
#include "vaml/core/RelayCommand.h"       // RelayCommand (the concrete command)
#include "vaml/core/Value.h"              // Value (the ICommand parameter domain)
#include "vaml/mvvm/property_bindable.h"  // PROPERTY_BINDABLE (the S247_1 layer)
#include "demo_service.h"                 // AppShellDemoService (the S442 wiring target)

#include <cstdint>
#include <memory>
#include <string>

namespace vaml::viewer {

// The app-shell's MainViewModel (a REAL ObservableObject + PROPERTY_BINDABLE
// over the S240/S241 core). The markup binds these props; the commands target
// the injected service. Plain-constructor wiring (S250/S256).
class AppShellViewModel final : public ObservableObject
{
public:
    // Plain ctor wiring: the caller passes the service by VALUE (constructed
    // from the same argument), the VM owns a copy (the S256 precedent). The VM
    // seeds its Dim + Status from the injected service.
    explicit AppShellViewModel(AppShellDemoService service);

    // --- PROPERTY_BINDABLE observable props the markup binds (S247_1) ------
    PROPERTY_BINDABLE(std::string, Text)
    PROPERTY_BINDABLE(bool,        Checked)
    PROPERTY_BINDABLE(double,      SliderValue)
    PROPERTY_BINDABLE(double,      Dim)
    PROPERTY_BINDABLE(std::string, Status)

    // --- Commands (exposed as ICommand& — the S253 {Command} resolves these) -
    // RunCommand: runs the demo (increments the service counter + folds the
    // service dimmer into Dim). CanExecute is gated by can_run() (Checked ==
    // true + counter below the cap). Executing raises CanExecuteChanged (the
    // S246 re-query contract — after the counter crosses the cap it flips).
    [[nodiscard]] ICommand& run_command() noexcept { return *runCommand_; }

    // EchoCommand: echoes the current Text prop through the service into Status.
    [[nodiscard]] ICommand& echo_command() noexcept { return *echoCommand_; }

    // The injected service (the S250 "engineer wired it; the VM owns it" — the
    // test asserts the commands target THIS service).
    [[nodiscard]] const AppShellDemoService& service() const noexcept { return service_; }
    [[nodiscard]] AppShellDemoService& service() noexcept { return service_; }

    // Raises CanExecuteChanged on the RunCommand after a state change that may
    // affect can_run() (e.g. the Checked toggle). The S246 re-query contract —
    // a consumer listens, then re-queries CanExecute.
    void refresh_run_can_execute();

private:
    // The command bodies (defined in app_shell_vm.cpp).
    void on_run(const Value& parameter);
    void on_echo(const Value& parameter);
    // The RunCommand CanExecute predicate: runnable iff the check is set AND the
    // service counter is below the demo cap.
    bool can_run() const;

    // The demo cap — after this many runs, RunCommand becomes non-executable
    // (the CanExecute + CanExecuteChanged round-trip is observable).
    static constexpr std::int32_t kRunCap = 3;

    AppShellDemoService service_;                       // the injected service (owned by value)
    std::unique_ptr<RelayCommand> runCommand_;          // Move-only RelayCommand (held by unique_ptr)
    std::unique_ptr<RelayCommand> echoCommand_;
};

} // namespace vaml::viewer
