// vaml_viewer — the S441/S442 app-shell VM + service implementations
// (dep:S440/S441). Compiled by BOTH apps AND the test target (the ADR-021
// consumer path, single source of truth — see app.cpp / the S441 test). The
// command bodies + service operations + the static composition seam live here.

#include "app_shell_vm.h"

#include "vaml/core/Value.h"   // Value (the ICommand parameter domain)

#include <string>

namespace vaml::viewer {

// ===========================================================================
// AppShellDemoService — the S442 app-constructed demo domain service (plain
// ctor wiring, NO DI — the S250/S256 pattern). The VM's commands target it.
// ===========================================================================

std::int32_t AppShellDemoService::increment()
{
    ++counter_;
    return counter_;
}

std::string AppShellDemoService::echo(const std::string& text)
{
    echoed_ = text;
    return echoed_;
}

// ===========================================================================
// AppShellViewModel — the S441 MainViewModel (a REAL ObservableObject +
// PROPERTY_BINDABLE + RelayCommand over the S240/S241/S246 core).
// ===========================================================================

AppShellViewModel::AppShellViewModel(AppShellDemoService service)
    : service_(std::move(service))
{
    // Seed the observable props the markup binds from the injected service
    // (S250 — the VM surfaces the service's state at construction).
    (void)set_Dim(service_.dim());
    (void)set_SliderValue(service_.dim() * 100.0);   // dim percent (0..100)
    (void)set_Checked(false);                        // run gate default: not checked
    (void)set_Text(std::string{});                   // empty input; the TextBox seeds via S443

    // The RelayCommands are wired to the injected service via their lambdas
    // (S246). Held by unique_ptr (RelayCommand is move-only).
    runCommand_ = std::make_unique<RelayCommand>(
        [this](const Value&) { on_run(Value::Null()); },   // execute
        [this](const Value&) { return can_run(); });       // CanExecute predicate
    echoCommand_ = std::make_unique<RelayCommand>(
        [this](const Value&) { on_echo(Value::Null()); }); // execute (always executable)
}

void AppShellViewModel::on_run(const Value&)
{
    // Run the demo: increment the service counter, fold the service dimmer's
    // current state into Dim (the visible opacity the S208 encoder folds) +
    // SliderValue, and refresh Status. All through the INJECTED service (S442).
    const std::int32_t count = service_.increment();
    (void)set_SliderValue(service_.dim() * 100.0);
    (void)set_Dim(service_.dim());
    (void)set_Status(std::string("ran #") + std::to_string(count));
    // The counter may have crossed the cap — the S246 re-query contract: a
    // consumer re-queries CanExecute and re-disables the command.
    refresh_run_can_execute();
}

void AppShellViewModel::on_echo(const Value&)
{
    // Echo the current Text prop through the service into Status (the two-way
    // TextBox round-trip is S443/S244; here the command drives the service).
    std::string clapped = service_.echo(Text());
    if (clapped.size() > 40u)
        clapped = clapped.substr(0, 40u);   // keep the Status text bounded
    (void)set_Status(std::string("echo: ") + clapped);
}

bool AppShellViewModel::can_run() const
{
    // Run is executable when the CheckBox is checked AND the counter is below
    // the demo cap (so CanExecute + CanExecuteChanged flip observably).
    return Checked() && service_.counter() < kRunCap;
}

void AppShellViewModel::refresh_run_can_execute()
{
    runCommand_->NotifyCanExecuteChanged();
}

// ===========================================================================
// AppShellComposition — the S442 STATIC WIRING SEAM (the S250 "engineer wires,
// we bind" headline): constructs the service + the VM through PLAIN
// CONSTRUCTORS ONLY — nothing registered, nothing resolved, NO library
// CompositionRoot/DI. This is the single seam the S443 binding path drives.
// ===========================================================================

AppShellComposition build_app_shell_composition()
{
    AppShellComposition c;
    // The VM is wired to its demo service through a PLAIN constructor only (the
    // S250 "engineer wires, we bind" headline; NO library CompositionRoot/DI,
    // nothing registered, nothing resolved). The VM owns its injected service
    // by value. The compose test asserts the constructor-only path + that the
    // VM holds a service.
    c.vm = std::make_shared<AppShellViewModel>(AppShellDemoService{});
    return c;
}

} // namespace vaml::viewer
