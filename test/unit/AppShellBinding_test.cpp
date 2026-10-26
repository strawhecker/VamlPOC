// S443 (dep:S442) — APP-SHELL ↔ MAINVIEWMODEL WIRED via {Binding} + {Command}.
//
// HEADLESS — proves the authored MainView.vaml is wired to the S441 VM through
// the S252/S253/S322 reader path, via the S443 seam (vaml_viewer/app_shell_binding.h,
// the ADR-021 consumer path — compiled from the vaml_viewer include into
// vaml-core-tests, the standing S256/S329_1 precedent), keeping the retained
// reader/VM alive (S243/S244 wires stay armed):
//   (1) one-way FIRST-TRANSFER: {Binding Status} lands the VM's Status onto the
//       StatusText.Text target DP (the S242 first-push);
//   (2) VM SET RE-PUSHES the target DP: a later `set_Status(...)` re-pushes the
//       bound StatusText (the S243 live one-way wire, armed via the retained reader);
//   (3) TextBox.Text TWO-WAY WRITE-BACK (S244): {Binding Text, Mode=TwoWay} — a
//       target edit (a TextBox.Text DP change) writes back to the VM's Text;
//   (4) {Command} resolves to the VM RelayCommand + Click via BindEvents runs
//       handler → VM → binding repaint: the RunButton's Click fires the app-owned
//       OnClick handler → runs the VM RunCommand → {Binding Status} repaints
//       (Status becomes "ran #1");
//   (5) malformed binding → structured never-silent error (VamlReaderException /
//       the plan's detail).
//
// The AUTHORED MainView.vaml is loaded through the S440 finalize_visual_tree
// hand-off as well, so the shell RENDERS its declared children (the S120/S121
// gap-closure is inherited from the S440 harness). ADR-018: zero locks.
// Registers S443-unique global DPs? — NO (binds onto the REAL TextBox.Text /
// Text.Text DP set already registered by the control ctors).

#include "doctest/doctest.h"

#include "app_shell_binding.h"   // the S443 seam (run_app_shell_binding_plan)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/Element.h"       // UIElement controls (TextBox / Text / Button)
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <string>

using vaml::viewer::run_app_shell_binding_plan;

namespace {

// Looks up a named element on the plan and asserts IT IS the expected control
// type (TextBox.Text / Text.Text / Button). Helper for the S443 tests.
template <typename ControlT>
ControlT* named_as(vaml::viewer::AppShellBindingPlanResult& plan, const char* name)
{
    return dynamic_cast<ControlT*>(plan.named_element(name));
}

} // namespace

// ---------------------------------------------------------------------------
// 1. One-way FIRST-TRANSFER — {Binding Status} lands on the StatusText target
// ---------------------------------------------------------------------------

TEST_CASE("S443: the plan wires + loads + binds + wires + finalizes the authored shell")
{
    auto plan = run_app_shell_binding_plan();
    if (!plan.ok)
    {
        MESSAGE("S443 plan failed: " << plan.detail);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.vm).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));
    // The S252 BindExtensions bound the {Binding Status} (OneWay) + {Binding Text,
    // Mode=TwoWay} + S445's {Binding SliderValue, Mode=TwoWay} rows (3 bound).
    // {Command RunCommand} is left deferred by S252 (not counted).
    SHOULD(ShouldlyIntegral(plan.bound == 3u).ShouldBe(1));
    // The S322 BindEvents wired the markup Click="OnClick" (1 wired record).
    SHOULD(ShouldlyIntegral(plan.wired == 1u).ShouldBe(1));
    // The S440 hand-off transferred the declared children (renders).
    SHOULD(ShouldlyIntegral(plan.transferred > 0u).ShouldBe(1));

    // The named elements the markup declares are reachable through the retained
    // reader's x:Name registry (the S443 convenience accessors).
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBox>(plan, "InputBox") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Text>(plan, "StatusText") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Button>(plan, "RunButton") != nullptr).ShouldBe(1));
}

TEST_CASE("S443: {Binding Status} — one-way first-transfer lands the VM status on StatusText.Text")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* statusText = named_as<vaml::Text>(plan, "StatusText");
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));

    // The VM's initial Status is empty; the S242 first-transfer landed it onto
    // the bound StatusText.Text target DP.
    SHOULD(ShouldlyString(plan.vm->Status()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("")));
}

// ---------------------------------------------------------------------------
// 2. VM set re-pushes the target DP (the S243 LIVE one-way wire)
// ---------------------------------------------------------------------------

TEST_CASE("S443: a later VM set_Status re-pushes the bound StatusText (live one-way)")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* statusText = named_as<vaml::Text>(plan, "StatusText");
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));

    // The retained reader keeps the {Binding Status} live binding armed; a real
    // VM change re-pushes through the S243 one-way wire to the target DP.
    (void)plan.vm->set_Status("S443 live");
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("S443 live")));

    (void)plan.vm->set_Status("S443 live 2");
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("S443 live 2")));
}

// ---------------------------------------------------------------------------
// 3. TextBox.Text two-way write-back (S244) — {Binding Text, Mode=TwoWay}
// ---------------------------------------------------------------------------

TEST_CASE("S443: TextBox.Text two-way — a target edit writes back to the VM Text")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* inputBox = named_as<vaml::TextBox>(plan, "InputBox");
    SHOULD(ShouldlyIntegral(inputBox != nullptr).ShouldBe(1));

    // {Binding Text, Mode=TwoWay} arms the S244 write-back. Seed the VM's Text
    // store (set_Text writes the type field AND the observable Value store the
    // binding reads/writes — the S247_1 bindable-store sync contract). The FIRST
    // transfer then pushes that value onto the TextBox.Text target DP.
    (void)plan.vm->set_Text("Hello, VAML");
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("Hello, VAML")));
    SHOULD(ShouldlyString(inputBox->Get(vaml::PropertyId::of("TextBox.Text")))
               .ShouldBe(vaml::Value::String("Hello, VAML")));

    // A TARGET EDIT: simulate a user typing into the TextBox — the edit lands on
    // the TextBox.Text target DP as a real change (a TextBox set_text is a real
    // DP change). The S244 write-back fires and pushes the new value into the VM's
    // observable Value store (the S240/S241 store the S242 binding writes via the
    // base ObservableObject::Set — the S252-markup two-way contract).
    (void)inputBox->set_text("typed by the user");
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("typed by the user")));
}

// ---------------------------------------------------------------------------
// 4. {Command} resolves to the VM RelayCommand + Click via BindEvents runs
//    handler → VM → binding repaint
// ---------------------------------------------------------------------------

TEST_CASE("S443: {Command RunCommand} resolves to the VM RelayCommand via ResolveCommand")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));

    // The S253 command surface: the reader resolves {Command RunCommand} to the
    // VM's registered RelayCommand (ICommand&). The first {Binding...} rows +
    // the {Command...} row are distinct markup rows.
    vaml::ICommand* cmd = plan.reader->ResolveCommand("RunCommand");
    SHOULD(ShouldlyIntegral(cmd != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral((void*)cmd == (void*)&plan.vm->run_command()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.reader->ResolveCommand("EchoCommand") != nullptr).ShouldBe(1));
}

TEST_CASE("S443: a Click on the RunButton runs handler → VM RunCommand → Status repaint")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* runButton = named_as<vaml::Button>(plan, "RunButton");
    auto* statusText = named_as<vaml::Text>(plan, "StatusText");
    SHOULD(ShouldlyIntegral(runButton != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));

    // The RunCommand executes only when the run gate is set (Checked==true AND
    // counter<cap). The S442 VM's own Checked prop defaults to false (the markup
    // CheckBox IsChecked="1" is the SHELL's CheckBox state, a separate object —
    // they are NOT linked). So the app (or test) arms the VM's run gate before
    // clicking, the same way the S441 VM-run test does.
    (void)plan.vm->set_Checked(true);
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(1));

    // Raise the Click on the RunButton -> the S322 wired OnClick handler runs ->
    // it invokes the VM RunCommand (ICommand&) -> the counter increments + Status
    // set -> the {Binding Status} live wire repaints the StatusText.
    (void)runButton->raise_click();
    SHOULD(ShouldlyIntegral(plan.vm->service().counter()).ShouldBe(1));
    SHOULD(ShouldlyString(plan.vm->Status()).ShouldBe(std::string("ran #1")));
    // The one-way binding repainted the target DP.
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("ran #1")));
}

// ---------------------------------------------------------------------------
// 5. Malformed binding → structured never-silent error
// ---------------------------------------------------------------------------

TEST_CASE("S443: a malformed {Binding} reports a structured, non-silent detail")
{
    // A malformed {Binding} in the shell markup (empty path) is a Class A
    // authoring error surfaced by the plan's structured detail (never silent).
    const char* bad =
        R"VAML(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root">
  <Grid x:Name="RootGrid">
    <Text x:Name="StatusText" Text="{Binding}" FontSize="12"/>
  </Grid>
</Window>
)VAML";
    auto plan = run_app_shell_binding_plan(bad);
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(0));
    // The structured detail is non-empty (never silent) — the exact reader what().
    SHOULD(ShouldlyString(std::string(plan.detail)).ShouldNotBe(std::string("")));
    // The failure is the MalformedBindingMarkup reader error surfaced through the
    // plan's structured detail (a malformed {Binding} must NOT be a silent no-op).
    const bool namesBinding = std::string(plan.detail).find("Binding") != std::string::npos;
    SHOULD(ShouldlyIntegral(namesBinding).ShouldBe(1));
}
