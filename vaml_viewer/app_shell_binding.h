// vaml_viewer — the S443 APP-SHELL-MVVM-WIRING seam (dep:S442).
//
// THE "APP-SHELL ↔ MainViewModel WIRED" SEAM: one retained reader drives the
// FULL authored MainView.vaml through the S252/S253/S322 reader path —
//   set_data_context(vm)                          (S252: the app-owned VM is the
//                                                  data context for {Binding})
//   BindExtensions()                              (S252: {Binding Status} OneWay
//                                                  + {Binding Text, Mode=TwoWay}
//                                                  — the S244 two-way write-back)
//   register_event("Click") + bind_class_handler("OnClick", ...) (S322: the
//                                                  class-owner handler)
//   BindEvents()                                  (S322: wires the markup
//                                                  Click="OnClick")
//   register_command("RunCommand"/"EchoCommand") (S253: {Command RunCommand}
//                                                  resolves to the VM's command)
//   finalize_visual_tree()                       (S440: the declared children
//                                                  reach the visual tree so the
//                                                  shell RENDERS)
//
// It REUSES + EXTENDS the `run_viewer_binding_plan` (S256) / `run_viewer_interactive_plan`
// (S329_1) patterns into `AppShellBindingPlan`, keeping the retained reader +
// VM ALIVE (heap-owned, returned) so the S243/S244 live wires stay armed after
// the plan returns — a later `vm->set_Status(...)` re-pushes the bound
// StatusText, a TextBox two-way edit writes back to `vm->set_Text(...)`, and a
// Click on the RunButton (BindEvents) fires the app-owned OnClick handler →
// invokes the VM RunCommand → the {Binding Status} one-way wire repaints.
//
// WIRING MODEL (the standing S250/S250/S256 precedent — NO DI, NO container):
//   - the S442 `AppShellComposition` (build_app_shell_composition) constructs
//     the `AppShellDemoService` + `AppShellViewModel` through PLAIN CONSTRUCTORS
//     (nothing registered/resolved);
//   - the plan REGISTERS the VM's RelayCommands (S253), the Click event + the
//     OnClick class-owner handler (S322) on the RETAINED reader — the exact
//     "engineer wires, we bind" path at the app boundary;
//   - the app-owned OnClick handler (constructed by the plan) runs the VM's
//     RunCommand (ICommand&), so a Button Click → handler → VM → binding repaint
//     is ONE headless-testable path.
//
// This header is compiled by BOTH apps AND the test target (the ADR-021 consumer
// path — the vaml_viewer include dir is a test source of truth; see the
// S256/S329_1 precedents). NO MINIMAL REAL introduced — the S443 wiring plan is
// REAL permanent code, kept-and-extended by the S445 E2E vertical slice.
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"   // the plan result owns a unique_ptr<DependencyObject>
#include "vaml/core/Element.h"            // UIElement (the renderable + event surface)
#include "vaml/core/ObservableObject.h"   // ObservableObject (the VM base)
#include "vaml/core/RoutedEvent.h"        // RoutedEventHandler (the S322 handler)
#include "vaml/core/UIElement.h"          // UIElement (visual-tree child host)
#include "vaml/core/VamlReader.h"         // VamlReader (set_data_context/Bind/register/BindEvents)
#include "vaml/mvvm/property_bindable.h"  // PROPERTY_BINDABLE (the S247_1 layer)
#include "app_shell.h"                    // kAppShellMainViewVaml (the authored shell markup)
#include "app_shell_vm.h"                 // AppShellViewModel (the S441 VM)
#include "demo_service.h"                 // AppShellDemoService + AppShellComposition (S442)

#include <memory>
#include <string>
#include <string_view>

namespace vaml::viewer {

// The S443 app-shell-MVVM-wiring plan outcome, HEADLESS (the exact code the
// live `--app` launch uses to drive the authored MainView.vaml + VM).
//
// The reader/vm (lost_list objects) are owned on the HEAP and RETAINED so the
// S243/S244 LIVE WIRES + the S322 wired Click handler + the S440 finalized
// visual tree stay armed after the plan returns: a later `set_Status` re-pushes
// the bound StatusText, a TextBox two-way edit writes back to the VM, and a
// Click on the RunButton fires the handler → VM → binding repaint. The reader +
// VM are move-only; holding them by pointer keeps the raw S2-tag targets (into
// `root`) valid.
struct AppShellBindingPlanResult
{
    bool ok = false;                              // loaded + bound + wired + finalized cleanly
    std::unique_ptr<vaml::UIElement> root;        // the wired live tree (UIElement root)
    std::shared_ptr<AppShellViewModel> vm;        // the app-owned VM (S441, from the S442 composition)
    std::unique_ptr<vaml::VamlReader> reader;     // RETAINED — keeps the live bindings + wired Click armed
    std::size_t bound = 0;                        // the S252 BindExtensions count ({Binding} + {Command}-deferred)
    std::size_t wired = 0;                        // the S322 BindEvents count (Click handlers wired)
    std::size_t transferred = 0;                  // the S440 finalize_visual_tree child count (renders)
    bool checkBoxWired = false;                   // S445: the app-owned CheckBox↔VM wire armed (never silent)
    std::string detail;                           // structured failure reason (never silent)

    // --- S443 convenience accessors ---------------------------------------
    // The reader's x:Name registry is cleared at the start of each Load; these
    // look up the named elements the test drives (InputBox / StatusText /
    // RunButton) through the retained reader AFTER the plan has run. The names
    // are registered during the Load above, so they resolve here.
    [[nodiscard]] vaml::UIElement* named_element(const char* name) const
    {
        return dynamic_cast<vaml::UIElement*>(reader ? reader->named(name) : nullptr);
    }
};

// THE REAL PLAN RUNNER (headless — the exact code the live `--app` launch uses):
// builds the S442 composition (plain ctors), wires the VM + the Click event +
// the OnClick class-owner handler + the VM's commands onto a RETAINED VamlReader,
// Loads the authored MainView.vaml, then BindExtensions ({Binding Status} OneWay
// + {Binding Text, Mode=TwoWay} — the live S244 write-back) + BindEvents (the
// markup Click="OnClick") + finalize_visual_tree (the S440 render hand-off), and
// reports. The app-owned OnClick handler invokes the VM's RunCommand (ICommand&),
// so a Button Click is a single end-to-end headless path.
//
// ok=false on ANY reader/parse/event/finalize error with the structured detail
// (never silent); a binding/event failure surfaces the exact reader what().
[[nodiscard]] inline AppShellBindingPlanResult
run_app_shell_binding_plan(std::string_view text = kAppShellMainViewVaml)
{
    AppShellBindingPlanResult r;
    try
    {
        // The S442 static composition: plain constructors only (constructs the
        // AppShellDemoService + the AppShellViewModel wired to it). NULL-checked
        // so a composition failure is never a silent no-root.
        AppShellComposition comp = build_app_shell_composition();
        if (!comp.vm)
        {
            r.ok = false;
            r.detail = "app-shell: S442 composition produced no ViewModel";
            return r;
        }
        auto vm = std::move(comp.vm);

        // The reader is created once and RETAINED (heap) so the live bindings +
        // wired Click + finalized tree survive the plan's return. App-owned
        // config: register the Click event, the VM as the data context (S252),
        // the VM's RelayCommands (S253), and the OnClick class-owner handler
        // (S322) BEFORE Load/Bind so the records resolve.
        r.reader = std::make_unique<vaml::VamlReader>();
        r.reader->register_event("Click");                                  // S092/S322
        r.reader->set_data_context(vm.get());                               // S252: the data context
        r.reader->register_command("RunCommand", &vm->run_command());       // S253: {Command RunCommand}
        r.reader->register_command("EchoCommand", &vm->echo_command());     // S253: {Command EchoCommand}
        r.reader->bind_class_handler("OnClick",                             // S322: the app-owned handler —
            [vmWeak = std::weak_ptr<AppShellViewModel>{ vm }](vaml::RoutedEventArgs&) {
                // The app-owned OnClick handler: run the VM's RunCommand (the
                // counter/dimmer/Status-run path). The handler runs on the UI
                // thread; the VM outlives it (retained below). If the VM is gone
                // (a torn-down shell), the click is a no-op (never a dangling call).
                if (auto vm = vmWeak.lock())
                    vm->run_command().Execute(vaml::Value::Null());
            });

        std::unique_ptr<vaml::DependencyObject> loaded = r.reader->Load(text);
        if (loaded == nullptr)
        {
            r.ok = false;
            r.detail = "app-shell: MainView.vaml did not produce a root element";
            return r;
        }
        vaml::UIElement* uiRoot = dynamic_cast<vaml::UIElement*>(loaded.get());
        if (uiRoot == nullptr)
        {
            r.ok = false;
            r.detail = "app-shell: MainView.vaml root is not a UIElement (cannot render)";
            return r;
        }
        r.root = std::unique_ptr<vaml::UIElement>(static_cast<vaml::UIElement*>(loaded.release()));

        // S252 — resolve the {Binding Status} (OneWay) + {Binding Text,
        // Mode=TwoWay} (S244 write-back) + S445's {Binding SliderValue,
        // Mode=TwoWay} into live BindingExpressions against the VM.
        r.bound = r.reader->BindExtensions();
        // S322 — wire the markup Click="OnClick" into the live routed handler.
        r.wired = r.reader->BindEvents();
        // S445 — the CHECKBOX ↔ VM wire (the S250 "engineer wires, we bind"
        // headline, app-owned composition — NOT a naked markup binding): the
        // authored CheckBox's CheckBox.IsChecked is a nullable-Int32 DP while the
        // VM's Checked is a bool — a genuine value-type clash a naked {Binding}
        // TwoWay cannot bridge (a forward Bool→Int32 push would be rejected).
        // So the app-shell wires the check END-TO-END through the control's real
        // change SIGNAL: a checkbox toggle (S404 on-toggle cycle) fires
        // `checked_changed()`, whose app-owned handler maps the checkbox's LIVE
        // nullable state (cb->checked(): 0/1/2) to the VM's bool Checked. This
        // is the exact "engineer wires, we bind" pattern — the app owns the
        // mapping, NO DI, staying on the app-shell's plain-composition theme.
        // `checkedBoxWired` is the never-silent contract: it must be true for
        // the check→VM slice to be driveable end-to-end.
        {
            auto* cb = dynamic_cast<vaml::CheckBox*>(r.reader->named("EnabledCheck"));
            if (cb != nullptr)
            {
                // Capture the VM shared_ptr by value (the composed VM the plan
                // retains below) so the handler keeps the VM alive for the tree's
                // lifetime — never a dangling capture.
                std::shared_ptr<AppShellViewModel> vmStrong = vm;
                // `checked_changed()` returns the Signal (the subscription
                // registry); `.add(...)` returns the removal token (nodiscard —
                // kept in a local so the checkbox→VM wire stays registered for
                // the tree's lifetime; the plan never removes it).
                [[maybe_unused]] const auto chkToken =
                    cb->checked_changed().add([vmStrong, cb] {
                        // Map the checkbox's present nullable state -> the VM bool
                        // run gate (checked(1) = true; unchecked(0)/indeterminate(2)
                        // = false — the WPF-faithful bool? → bool collapse).
                        (void)vmStrong->set_Checked(cb->checked() == 1);
                    });
                r.checkBoxWired = true;
            }
        }
        // S440 — the S120/S121 hand-off: render the DECLARED children, not just
        // the root. `transferred` is the never-silent render contract.
        r.transferred = r.reader->finalize_visual_tree();

        r.vm = std::move(vm);
        r.ok = (r.root != nullptr) && (r.transferred > 0);
        if (!r.ok)
        {
            r.detail = "app-shell: finalize_visual_tree() transferred 0 children "
                       "(declared UI would not render)";
        }
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();   // never silent
    }
    return r;
}

} // namespace vaml::viewer
