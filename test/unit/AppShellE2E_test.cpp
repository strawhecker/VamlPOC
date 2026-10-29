// S445 (dep:S444) — LAUNCH → declared UI renders; click/type/slide/check updates
// the VM end-to-end (the in-repo v1 VERTICAL SLICE through the LIVE path).
//
// HEADLESS (the standing RAIL 1 — the ADR-021 consumer path) — proves the
// S443/S445 binding plan drives the FULL authored MainView.vaml end-to-end,
// WITH the VM + the target DPs observed after each DOM-level interaction, all
// against the retained reader/VM (the live wires stay armed):
//
//   (1) RENDER — the plan loads + finalizes the shell; the Window root renders
//       its declared Grid/StackPanel/controls (transferred > 0). The VM is the
//       S441 MainViewModel; the reader is retained. This is the "declared UI
//       renders" slice.
//   (2) CLICK → routed ClickEvent → handler → VM → one-way repaint — a Click on
//       the RunButton (the S324/S401 raise_click) fires the app-owned OnClick
//       handler → the VM RunCommand → {Binding Status} re-pushes the StatusText
//       ("ran #1") — the one-way repaint end-to-end.
//   (3) TYPE → TextBox two-way write-back → VM prop — the S244 target edit:
//       typing into the InputBox (set_text → Text DP real change) writes back to
//       the VM's Text (S443 already proves the markup two-way on the TextBox).
//   (4) SLIDE → Slider ratio → Slider.Value → VM prop — S445's NEW markup wire
//       {Binding SliderValue, Mode=TwoWay}: a drag (set_value_from_ratio) changes
//       Slider.Value (a real DP change) → the TwoWay write-back pushes the ratio
//       onto the VM's SliderValue — the S244 two-way on the Slider axis.
//   (5) CHECK → CheckBox toggle → VM prop — S445's app-owned CheckBox↔VM wire
//       (checked_changed Signal → vm->set_Checked): a toggle (S404 on-toggle
//       cycle) flips the VM's Checked bool.
//   (6) COHESIVE FLOW — arm the gate (Check), type, run (Status "ran #1"), +
//       the slider + check slices all update the SAME VM + bound targets.
//
// ADR-018: zero locks. Registers NO S445-unique global DPs (binds onto the REAL
// built-in control DP set + the VM's props). ADR-021: compiles the REAL
// app_shell_binding.h seam (the S443/S445 plan) from the vaml_viewer include.

#include "doctest/doctest.h"

#include "app_shell_binding.h"   // S443/S445: run_app_shell_binding_plan + AppShellBindingPlanResult

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include "vaml/core/Element.h"       // TextBox / Text / Button / Slider / CheckBox / Window
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <string>

using vaml::viewer::run_app_shell_binding_plan;

namespace {

// Looks up a named element on the plan AND asserts it is the expected control
// type (the S445 E2E drives these DOM-level interactions through the reader).
template <typename ControlT>
ControlT* named_as(vaml::viewer::AppShellBindingPlanResult& plan, const char* name)
{
    return dynamic_cast<ControlT*>(plan.named_element(name));
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Render — the shell tree renders its declared controls
// ---------------------------------------------------------------------------

TEST_CASE("S445: the app-shell plan renders the declared UI (Window root + finalized children)")
{
    auto plan = run_app_shell_binding_plan();
    if (!plan.ok)
    {
        MESSAGE("S445 plan failed: " << plan.detail);
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.vm).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));

    // The root is the S440 REAL Window (renders + hit-tests its subtree).
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Window*>(plan.root.get()) != nullptr).ShouldBe(1));
    // The S440 finalize hand-off transferred the declared children (renders).
    SHOULD(ShouldlyIntegral(plan.transferred > 0u).ShouldBe(1));

    // Every control the vertical slice drives is reachable through the retained
    // reader's x:Name registry.
    SHOULD(ShouldlyIntegral(named_as<vaml::Button>(plan, "RunButton") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBox>(plan, "InputBox") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Slider>(plan, "DimSlider") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::CheckBox>(plan, "EnabledCheck") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBlock>(plan, "StatusText") != nullptr).ShouldBe(1));
    // The S445 app-owned CheckBox↔VM wire is armed (never silently missing).
    SHOULD(ShouldlyIntegral(plan.checkBoxWired).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 2. Click → routed ClickEvent → handler → VM → one-way repaint
// ---------------------------------------------------------------------------

TEST_CASE("S445: a click on the RunButton updates the VM + re-paints the bound StatusText")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* runButton = named_as<vaml::Button>(plan, "RunButton");
    auto* statusText = named_as<vaml::TextBlock>(plan, "StatusText");
    SHOULD(ShouldlyIntegral(runButton != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));

    // Arm the VM's run gate (Checked==true — the S443/S441 RunCommand gate; the
    // shell CheckBox is a SEPARATE object, and its app-owned wire is exercised
    // in the S445 check test below).
    (void)plan.vm->set_Checked(true);

    // A Click on the RunButton -> the S322 wired OnClick handler runs -> invokes
    // the VM RunCommand -> the counter increments + Status set -> the {Binding
    // Status} live one-way wire re-paints the StatusText.
    (void)runButton->raise_click();
    SHOULD(ShouldlyIntegral(plan.vm->service().counter()).ShouldBe(1));
    SHOULD(ShouldlyString(plan.vm->Status()).ShouldBe(std::string("ran #1")));
    // The bound target DP repainted end-to-end.
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("ran #1")));
}

// ---------------------------------------------------------------------------
// 3. Type → TextBox two-way write-back → VM prop (S244)
// ---------------------------------------------------------------------------

TEST_CASE("S445: typing into the InputBox writes the Text back to the VM (two-way)")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* inputBox = named_as<vaml::TextBox>(plan, "InputBox");
    SHOULD(ShouldlyIntegral(inputBox != nullptr).ShouldBe(1));

    // A TARGET EDIT: simulate a user typing into the TextBox — the edit lands on
    // the TextBox.Text target DP as a real change (set_text). The S244 write-back
    // (the {Binding Text, Mode=TwoWay} wire) pushes it into the VM's Text prop —
    // the S241 OBSERVABLE STORE, asserted via vm->Get (the S443 convention: the
    // two-way write-back writes the observable store, not the PROPERTY_BINDABLE
    // C++ backing field; the field is the app's set_Text source of truth).
    (void)inputBox->set_text("typed by the user");
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("typed by the user")));
}

// ---------------------------------------------------------------------------
// 4. Slide → Slider ratio → Slider.Value → VM prop (S445's new TwoWay wire)
// ---------------------------------------------------------------------------

TEST_CASE("S445: dragging the slider writes the ratio back to the VM SliderValue (two-way)")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* dimSlider = named_as<vaml::Slider>(plan, "DimSlider");
    SHOULD(ShouldlyIntegral(dimSlider != nullptr).ShouldBe(1));

    // The S252 FirstTransfer at BindExtensions already pushed the VM's initial
    // SliderValue (50.0, since the service dim is 0.5) onto the Slider.Value DP.
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(50.0)));
    SHOULD(ShouldlyString(dimSlider->value()).ShouldBe(50.0));

    // A DRAG: set_value_from_ratio(0.8) maps the [0,1] thumb ratio to
    // Value = Minimum + 0.8*(Max-Min) = 80. This is a REAL Slider.Value DP change
    // (S244 target-changed signal), so the TwoWay write-back pushes 80.0 onto the
    // VM's SliderValue — the S241 OBSERVABLE STORE (asserted via vm->Get, the
    // S443 write-back convention). The slice: slide → Slider.Value → VM prop.
    const double newVal = dimSlider->set_value_from_ratio(0.8);
    SHOULD(ShouldlyString(newVal).ShouldBe(80.0));
    // The VM prop (its observable store) updated end-to-end via the write-back.
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(80.0)));

    // A second drag — the live write-back stays armed (the retained reader/VM).
    (void)dimSlider->set_value_from_ratio(0.25);
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(25.0)));
}

// ---------------------------------------------------------------------------
// 5. Check → CheckBox toggle → VM prop (S445's app-owned CheckBox↔VM wire)
// ---------------------------------------------------------------------------

TEST_CASE("S445: toggling the checkbox updates the VM Checked (app-owned wire)")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.checkBoxWired).ShouldBe(1));
    auto* enabledCheck = named_as<vaml::CheckBox>(plan, "EnabledCheck");
    SHOULD(ShouldlyIntegral(enabledCheck != nullptr).ShouldBe(1));

    // The shell checkbox starts unchecked (the authored literal 0) + the VM
    // Checked defaults false.
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(false));

    // A TOGGLE (the S404 WPF on-toggle cycle: unchecked(0) -> checked(1)) fires
    // the checkbox's checked_changed Signal -> the app-owned handler maps the
    // LIVE checkbox state (1) to the VM's bool Checked. The check → VM prop slice.
    const std::int32_t newState = enabledCheck->toggle();
    SHOULD(ShouldlyIntegral(newState).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(true));

    // A second toggle (checked(1) -> unchecked(0)) flips the VM back to false.
    (void)enabledCheck->toggle();
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(false));
}

// ---------------------------------------------------------------------------
// 6. Cohesive end-to-end flow — all four interactions drive the SAME VM + targets
// ---------------------------------------------------------------------------

TEST_CASE("S445: click + type + slide + check drive the SAME VM + bound targets end-to-end")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* runButton   = named_as<vaml::Button>(plan, "RunButton");
    auto* statusText  = named_as<vaml::TextBlock>(plan, "StatusText");
    auto* inputBox    = named_as<vaml::TextBox>(plan, "InputBox");
    auto* dimSlider   = named_as<vaml::Slider>(plan, "DimSlider");
    auto* enabledCheck= named_as<vaml::CheckBox>(plan, "EnabledCheck");
    SHOULD(ShouldlyIntegral(runButton != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(inputBox != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dimSlider != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(enabledCheck != nullptr).ShouldBe(1));

    // CHECK — the app-owned wire arms the run gate.
    (void)enabledCheck->toggle();
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(true));

    // TYPE — the two-way write-back lands a value in the VM.
    (void)inputBox->set_text("hello e2e");

    // SLIDE — the two-way write-back moves the slider value into the VM.
    (void)dimSlider->set_value_from_ratio(0.6);
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(60.0)));

    // CLICK — the RunCommand runs (gate checked, counter under cap) + re-paints
    // the bound StatusText end-to-end.
    (void)runButton->raise_click();
    SHOULD(ShouldlyIntegral(plan.vm->service().counter()).ShouldBe(1));
    SHOULD(ShouldlyString(plan.vm->Status()).ShouldBe(std::string("ran #1")));
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("ran #1")));

    // The type + slide writes survived alongside the click (one shared VM). The
    // two-way write-backs land in the S241 observable store (vm->Get convention).
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("hello e2e")));
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(60.0)));
}
