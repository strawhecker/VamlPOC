// vaml_viewer — the S442 APP-SHELL DEMO DOMAIN SERVICE seam (dep:S441).
//
// THE IN-REPO DEMO INFRASTRUCTURE SERVICE the app-shell ViewModel's
// RelayCommands are wired to via PLAIN CONSTRUCTORS (S441 delivers
// AppShellViewModel; S442 delivers the service it targets + the app-shell's
// own STATIC wiring seam). It mirrors the S256 `ViewerDemoService` precedent
// (the viewer's app-constructed plain-constructor domain service) and adds the
// counter / dimmer / echo trio the app-shell binds.
//
// NO DI, NO library CompositionRoot (S250 — the "engineer wires, we bind"
// headline): the app owns the service; it is a PLAIN class with a plain
// constructor, nothing registered, nothing resolved. The ViewModel takes it in
// BY VALUE through its constructor (the S250/S256 plain-constructor pattern).
//
// SPLIT (the standing S221/S222_1/S256/S440 precedent):
//   * THE SERVICE — `AppShellDemoService`: counter / dimmer / echo state +
//     operations (this is the <src/*/include> consumer surface the VM's
//     commands call).
//   * THE STATIC WIRING SEAM — `AppShellComposition` +
//     `build_app_shell_composition()`: the app-shell's OWN S250-style static
//     composition point (constructor-only — nothing registered/resolved).
//
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread). Header
// compiled by BOTH apps AND the test target (the ADR-021 consumer path — the
// vaml_viewer include dir is a test source of truth; see the S223/S256/S440
// precedent). NO MINIMAL REAL introduced — the service + composition seam are
// REAL permanent code, kept-and-extended by S441/S443–S445.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vaml::viewer {

// The S442 app-constructed demo domain service (plain ctor wiring, NO DI —
// the S250/S256 pattern). Holds the counter / dimmer / echo state the app-shell
// VM's RelayCommands drive.
class AppShellDemoService
{
public:
    AppShellDemoService() = default;
    explicit AppShellDemoService(double dim) noexcept : dim_(dim) {}

    // --- counter -----------------------------------------------------
    // Increments the internal counter and returns the new value (the RunCommand
    // wiring target). The service owns the count; the caller reads it back.
    [[nodiscard]] std::int32_t increment();
    [[nodiscard]] std::int32_t counter() const noexcept { return counter_; }

    // --- dimmer ------------------------------------------------------
    // Sets the "room dim" (double, [0,1]) the VM surfaces as Dim + SliderValue.
    void set_dim(double d) noexcept { dim_ = d; }
    [[nodiscard]] double dim() const noexcept { return dim_; }

    // --- echo --------------------------------------------------------
    // Stores + returns the echoed text (the EchoCommand wiring target). The
    // service is the text round-trip's owned state.
    [[nodiscard]] std::string echo(const std::string& text);
    [[nodiscard]] const std::string& echoed() const noexcept { return echoed_; }

private:
    std::int32_t counter_ = 0;
    double       dim_     = 0.5;
    std::string  echoed_;
};

// The app-shell VM (S441, defined in app_shell_vm.h/.cpp). Forward-declared so
// the composition seam can expose it through shared_ptr without an include cycle
// (app_shell_vm.h includes demo_service.h).
class AppShellViewModel;

// The S442 static-wiring result — the app-shell composition, constructed ONLY
// through plain constructors (the S250 "engineer wires, we bind" headline; NO
// library CompositionRoot, NO container, NOTHING registered or resolved). Owns
// the VM, which holds its injected copy of the service by value (the S256
// pattern).
struct AppShellComposition
{
    std::shared_ptr<AppShellViewModel> vm;   // the VM wired to its service (plain ctor)
};

// Builds the app-shell composition (service + VM) through PLAIN CONSTRUCTORS
// only — nothing registered, nothing resolved. THE single static wiring seam
// the S443 binding path and the S442 composition test exercise. Headless.
[[nodiscard]] AppShellComposition build_app_shell_composition();

} // namespace vaml::viewer
