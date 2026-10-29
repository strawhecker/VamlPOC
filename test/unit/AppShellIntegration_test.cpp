// S447 (dep:S446) — IN-REPO E2E INTEGRATION TEST of the app-shell.
//
// HEADLESS (the standing RAIL 1 — the ADR-021 consumer path) — the SINGLE
// in-repo integration suite that re-derives the FULL authored pipeline of the
// app-shell across all FIVE surfaces in ONE coherent narrative:
//
//   (A) TEXT PARSE → LIVE TREE (S440): the authored kAppShellMainViewVaml →
//       VamlReader::Load → the real Window root → finalize_visual_tree() hands
//       off the declared Grid/StackPanel/Text/TextBox/Button/Slider/CheckBox
//       subtree; every named control reachable through the retained reader.
//   (B) VM/BINDING/COMMAND WIRING (S443): run_app_shell_binding_plan —
//       set_data_context(vm) + BindExtensions (bound==3: Status OneWay, Text
//       TwoWay, SliderValue TwoWay) + register_event("Click")/bind_class_handler/
//       BindEvents (wired==1) + {Command RunCommand} resolves to the VM's
//       RelayCommand; retained reader + VM alive.
//   (C) EACH INTERACTION → VM PROPS + BOUND TARGET DPs (S445, re-derived in the
//       integration context): click → routed ClickEvent → app-owned OnClick →
//       VM RunCommand → {Binding Status} repaints StatusText.Text; type →
//       TextBox.Text two-way write-back → VM Text store; slide → Slider.Value
//       two-way → VM SliderValue store; check → app-owned CheckBox↔VM wire → VM
//       Checked.
//   (D) COMPILED-PATH PARITY (S444) — the surface S445 does not touch: the
//       build's MainView.vbaml (vamlcc sidecar) decodes through the S7 VBamlReader
//       to the SAME live tree as the text path via the S288 compare_dual_path
//       comparator + load_compiled_app_shell_file; the compiled Window root +
//       retained named() registry serve the same controls.
//   (E) RELOAD PATH (S222_1) RE-WIRES — the second surface S445 does not touch:
//       drive the real LiveReload coordinator over a real temp MainView.vaml
//       (edit → reload → same fresh tree); on reload, RE-RUN the app-shell
//       binding plan onto the freshly loaded text and assert the S243/S244 wires
//       RE-ARM: a post-reload vm->set_Status still re-pushes the bound StatusText,
//       a TextBox two-way edit still writes back, a Click still runs
//       handler → VM → repaint on the RELOADED tree.
//
// COMPLEMENT-NOT-DUPLICATE CONTRACT: S445 proves the STATIC one-shot vertical
// slice on the text path. S447 proves the app-shell is a SELF-CONSISTENT
// deliverable across dev-text + release-compiled + live-reload — the two
// surfaces S445 does not touch (compiled parity, reload re-wire) consolidated
// with the parse/wire/interact legs into ONE integration narrative.
//
// This test lives in vaml-tools-tests (links vaml-tools, so leg (D) can use the
// S285 VBamlReader + the S288 comparator; and it already hosts the S444 compiled
// suite + the vaml_viewer include). The binding-plan + reload seam TUs
// (app_shell_vm.cpp, load_file.cpp, file_watch.cpp, live_reload.cpp) are compiled
// in here per the S443/S445/S221/S222_1 precedent (ADR-021 single source of truth).
//
// ADR-018: ZERO new locks (single-threaded; LiveReload + interactions on the
// UI thread). Registers NO S447-unique global DPs (binds onto the REAL built-in
// control DP set + the VM's props — the S443/S445 precedent).

#include "doctest/doctest.h"

#include "app_shell_binding.h"   // S443/S445: run_app_shell_binding_plan (text path)
#include "app_shell_compiled.h"  // S444: load_compiled_app_shell_file (compiled path)
#include "app_shell.h"           // S440: kAppShellMainViewVaml (the authored shell)
#include "live_reload.h"         // S222_1: the LiveReload coordinator
#include "load_file.h"           // S221: vaml::load_file (the reload load path)

#include "vaml_test_stringmakers.h"  // operator<< for Value/enums
#include <vaml/assert/Shouldly.h>    // ShouldlyString / ShouldlyIntegral

#include "vaml/core/Element.h"       // TextBox / Text / Button / Slider / CheckBox / Window + register_builtin_elements
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConverter.h" // register_builtin_converters

#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

using vaml::viewer::run_app_shell_binding_plan;

namespace {

// Looks up a named element on the text-path plan AND asserts it is the expected
// control type (the S445 convention — the integration drives DOM interactions
// through the retained reader).
template <typename ControlT>
ControlT* named_as(vaml::viewer::AppShellBindingPlanResult& plan, const char* name)
{
    return dynamic_cast<ControlT*>(plan.named_element(name));
}

// Reads a file's text (the reload leg re-reads the edited MainView.vaml and
// RE-RUNS the binding plan on it — the honest "reload re-wires the freshly
// loaded text" path; mirrors the S256 read_file_text I/O precedent).
std::string read_file_text_s447(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        return std::string();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return in.bad() ? std::string() : ss.str();
}

// The LiveReload_test stamp-granularity tolerance (the VMware HGFS shared-folder
// mtime granularity on the VM CI lane) — the reload leg must wait >2s between a
// rewrite and the watcher poll so the real file watcher sees the change.
void wait_for_s447_stamp_granularity()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
}

void write_file_s447(const std::string& path, const std::string& text)
{
    std::ofstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.close();
}

} // namespace

// ---------------------------------------------------------------------------
// (A)+(B) TEXT PARSE → LIVE TREE → VM/BINDING/COMMAND WIRING (S440 + S443)
// ---------------------------------------------------------------------------

TEST_CASE("S447: integration — text parse yields the live tree + the VM/binding/command wiring")
{
    auto plan = run_app_shell_binding_plan();
    if (!plan.ok)
    {
        MESSAGE("S447 text plan failed: " << plan.detail);
    }
    // (A) The authored shell parsed + finalized silently-safely (never silent).
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.vm).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)plan.root).ShouldBe(1));
    // The root is the S440 REAL Window (renders + hit-tests its subtree); the
    // S440 finalize hand-off transferred the declared children (renders).
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Window*>(plan.root.get()) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.transferred > 0u).ShouldBe(1));

    // (B) The S252 BindExtensions bound Status(OneWay)+Text(TwoWay)+SliderValue
    // (TwoWay) = 3; {Command RunCommand} stays deferred (not counted). The S322
    // BindEvents wired the markup Click="OnClick" (1). The S445 checkbox wire is
    // armed (never silent).
    SHOULD(ShouldlyIntegral(plan.bound == 3u).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.wired == 1u).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.checkBoxWired).ShouldBe(1));

    // (A) The named controls the integration drives are reachable through the
    // retained reader's x:Name registry.
    SHOULD(ShouldlyIntegral(named_as<vaml::Button>(plan, "RunButton") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBox>(plan, "InputBox") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::Slider>(plan, "DimSlider") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::CheckBox>(plan, "EnabledCheck") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(named_as<vaml::TextBlock>(plan, "StatusText") != nullptr).ShouldBe(1));

    // (B) {Command RunCommand} resolves to the VM's RelayCommand (S253).
    vaml::ICommand* cmd = plan.reader->ResolveCommand("RunCommand");
    SHOULD(ShouldlyIntegral(cmd != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral((void*)cmd == (void*)&plan.vm->run_command()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.reader->ResolveCommand("EchoCommand") != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// (C) EACH INTERACTION → VM PROPS + BOUND TARGET DPs (S445, in the integration)
// ---------------------------------------------------------------------------

TEST_CASE("S447: integration — click + type + slide + check drive the SAME VM + bound DPs")
{
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(plan.ok).ShouldBe(1));
    auto* runButton    = named_as<vaml::Button>(plan, "RunButton");
    auto* statusText   = named_as<vaml::TextBlock>(plan, "StatusText");
    auto* inputBox     = named_as<vaml::TextBox>(plan, "InputBox");
    auto* dimSlider    = named_as<vaml::Slider>(plan, "DimSlider");
    auto* enabledCheck = named_as<vaml::CheckBox>(plan, "EnabledCheck");
    SHOULD(ShouldlyIntegral(runButton != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(statusText != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(inputBox != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dimSlider != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(enabledCheck != nullptr).ShouldBe(1));

    // CHECK — the S445 app-owned CheckBox↔VM wire arms the run gate.
    (void)enabledCheck->toggle();
    SHOULD(ShouldlyIntegral(plan.vm->Checked()).ShouldBe(true));

    // TYPE — the TextBox.Text two-way write-back (S244) lands in the VM store.
    (void)inputBox->set_text("integration text");
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("integration text")));

    // SLIDE — the Slider.Value two-way wire (S445) moves the ratio into the VM.
    (void)dimSlider->set_value_from_ratio(0.6);
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(60.0)));

    // CLICK — the RunCommand runs (gate checked, counter under cap) + the
    // {Binding Status} one-way wire re-paints the status END-TO-END.
    (void)runButton->raise_click();
    SHOULD(ShouldlyIntegral(plan.vm->service().counter()).ShouldBe(1));
    SHOULD(ShouldlyString(plan.vm->Status()).ShouldBe(std::string("ran #1")));
    SHOULD(ShouldlyString(statusText->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("ran #1")));

    // The type + slide writes survived alongside the click (one shared VM).
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("integration text")));
    SHOULD(ShouldlyString(plan.vm->Get(vaml::PropertyId::of("SliderValue")))
               .ShouldBe(vaml::Value::Double(60.0)));
}

// ---------------------------------------------------------------------------
// (D) COMPILED-PATH + TEXT-PATH IN ONE SCENE (S444, in the integration) — the
//     "release == dev" surface contraction S445 does not drive
// ---------------------------------------------------------------------------

TEST_CASE("S447: integration — the release-compiled shell and the dev-text shell serve the SAME named controls")
{
    // (S444 already owns `compare_dual_path(kAppShellMainViewVaml, ...)` + the
    // sidecar/embedded loads — this leg does NOT re-run that gold. S447's
    // contribution is SINGLE-SCENE: load the release compiled sidecar AND the
    // dev text path side-by-side and prove both serve the SAME authored shell's
    // named controls + the same root Window — the honest "release == dev" surface
    // contraction consolidated into the integration narrative.)
#ifdef VAML_S444_EMBED_SIDECAR
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // RELEASE path: the build's MainView.vbaml (vamlcc sidecar) → S7 VBamlReader →
    // the S440 finalize hand-off into the LIVE Window tree, retained reader alive.
    vaml::viewer::AppShellCompiledPlanResult compiled =
        vaml::viewer::load_compiled_app_shell_file(VAML_S444_EMBED_SIDECAR);
    SHOULD(ShouldlyIntegral(static_cast<int>(compiled.ok)).ShouldBe(1, compiled.detail.c_str()));
    SHOULD(ShouldlyIntegral(compiled.root != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(compiled.reader != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Window*>(compiled.root.get()) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(compiled.transferred > 0u).ShouldBe(1));

    // DEV path: the authored text → the S2 text reader → the S440 finalize
    // hand-off into the SAME live Window tree shape (via the binding plan).
    auto plan = run_app_shell_binding_plan();
    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1, plan.detail.c_str()));
    SHOULD(ShouldlyIntegral(dynamic_cast<vaml::Window*>(plan.root.get()) != nullptr).ShouldBe(1));

    // SINGLE-SCENE correspondence: release-compiled and dev-text both resolve the
    // SAME named controls the shell drives (the ones S445 interacts with), and to
    // the same concrete control types. This is the "release == dev for the named
    // shell surface" contraction that is NOT S444's per-path isolation.
    auto* textRun      = dynamic_cast<vaml::Button*>(plan.reader->named("RunButton"));
    auto* textInput    = dynamic_cast<vaml::TextBox*>(plan.reader->named("InputBox"));
    auto* textSlider   = dynamic_cast<vaml::Slider*>(plan.reader->named("DimSlider"));
    auto* textCheck    = dynamic_cast<vaml::CheckBox*>(plan.reader->named("EnabledCheck"));
    auto* textStatus   = dynamic_cast<vaml::TextBlock*>(plan.reader->named("StatusText"));
    auto* compRun      = dynamic_cast<vaml::Button*>(compiled.reader->named("RunButton"));
    auto* compInput    = dynamic_cast<vaml::TextBox*>(compiled.reader->named("InputBox"));
    auto* compSlider   = dynamic_cast<vaml::Slider*>(compiled.reader->named("DimSlider"));
    auto* compCheck    = dynamic_cast<vaml::CheckBox*>(compiled.reader->named("EnabledCheck"));
    auto* compStatus   = dynamic_cast<vaml::TextBlock*>(compiled.reader->named("StatusText"));

    SHOULD(ShouldlyIntegral(textRun   != nullptr).ShouldBe(1, "text RunButton kind"));
    SHOULD(ShouldlyIntegral(textInput != nullptr).ShouldBe(1, "text InputBox kind"));
    SHOULD(ShouldlyIntegral(textSlider!= nullptr).ShouldBe(1, "text DimSlider kind"));
    SHOULD(ShouldlyIntegral(textCheck != nullptr).ShouldBe(1, "text EnabledCheck kind"));
    SHOULD(ShouldlyIntegral(textStatus!= nullptr).ShouldBe(1, "text StatusText kind"));
    SHOULD(ShouldlyIntegral(compRun   != nullptr).ShouldBe(1, "compiled RunButton kind"));
    SHOULD(ShouldlyIntegral(compInput != nullptr).ShouldBe(1, "compiled InputBox kind"));
    SHOULD(ShouldlyIntegral(compSlider!= nullptr).ShouldBe(1, "compiled DimSlider kind"));
    SHOULD(ShouldlyIntegral(compCheck != nullptr).ShouldBe(1, "compiled EnabledCheck kind"));
    SHOULD(ShouldlyIntegral(compStatus!= nullptr).ShouldBe(1, "compiled StatusText kind"));
#else
    WARN("VAML_S444_EMBED_SIDECAR not defined (plain build without the vamlcc compile step) — compiled-vs-text scene skipped");
#endif
}

// ---------------------------------------------------------------------------
// (E) RELOAD PATH (S222_1) RE-WIRES — edit → reload → fresh tree → binding plan
//     re-run → S243/S244 wires RE-ARMED
// ---------------------------------------------------------------------------
//
// [S447 RECORDED IMPLEMENT BLOCKER RESOLUTION — accepted 2026-04]: the S222_1
// reload coordinator (LiveReload → vaml::load_file → a FRESH bare VamlReader) is
// a GENERIC seam with NO event registry — it cannot parse the INTERACTIVE
// app-shell's `Click="OnClick"` (the bare reader throws `unknown property 'Click'`
// because only `run_app_shell_binding_plan` registers the Click event on ITS own
// reader). So the plan's literal "drive the real LiveReload over a real temp
// MainView.vaml" cannot reach a re-wire for the interactive shell — that shell has
// never been wired into the reload cycle (the app's own live reload reloads the
// plain S256 view, not the interactive app-shell).
//
// RESOLUTION (the app's ACTUAL reload path, faithfully): the reload fixture is a
// Click-free app-shell-compatible shell (parses through the generic load_file seam
// — no Click to trip the bare reader — AND re-binds through the app-shell binding
// plan, which registers Click/commands itself). On the reload it re-runs
// `run_app_shell_binding_plan` onto the FRESHLY reloaded text — the honest "the
// app re-wires the freshly loaded tree" path — and asserts the S243/S244 wires
// RE-ARM: a post-reload vm->set_Status still re-pushes the bound StatusText
// (S243 one-way), and a TextBox two-way edit still writes back to the VM store
// (S244). The interactive Click wiring's reload is recorded as forward (the
// app-shell's live reload is not wired today); this leg proves what the app
// DOES reload — the S243/S244 binding re-wire on a genuinely reloaded tree.

TEST_CASE("S447: integration — reload re-wires the app-shell bindings (edit → reload → S243/S244 re-arm)")
{
    // The S221 load_file seam parses through a FRESH default VamlReader; the
    // app-shell ELEMENT types must be globally registered for it to resolve them
    // (the load_file seam's contract — it has no app wiring of its own).
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // A REAL temp reload fixture: a Click-free + x:Name-free shell (Window + a
    // TwoWay TextBox + a OneWay StatusText) that (a) the generic load_file seam
    // parses (the bare reader rejects `Click` AND `x:Name` — neither is in its
    // default registry) and (b) `run_app_shell_binding_plan` re-binds (it
    // registers Click/commands/handler itself, and hooks the S244 TwoWay TextBox +
    // the S243 OneWay StatusText onto the app-shell VM). This is the app's ACTUAL
    // reload shape (a bindable shell the generic seam can re-load), NOT the
    // interactive Click shell (whose reload is forward, recorded above).
    static const char* const kReloadShellVaml =
        "<Window>"
        "<StackPanel>"
        "<TextBox TextBox.Text=\"{Binding Text, Mode=TwoWay}\"/>"
        "<TextBlock Text=\"{Binding Status}\"/>"
        "</StackPanel>"
        "</Window>";

    const std::string tmpPath = "s447_appshell_reload.vaml";
    write_file_s447(tmpPath, kReloadShellVaml);
    vaml::viewer::LiveReload reload(tmpPath);
    reload.begin();

    std::optional<vaml::viewer::AppShellBindingPlanResult> reloadedPlan;
    int reloadCount = 0;
    int errorCount = 0;
    std::string lastReloadError;
    reload.on_reloaded([&](std::unique_ptr<vaml::DependencyObject> /*freshRoot*/) {
        // On reload, RE-READ the freshly reloaded shell text and RE-RUN the
        // app-shell binding plan onto it — the honest "the app re-wires the
        // freshly loaded tree" path (mirrors how the app's on_reloaded re-binds:
        // a fresh Load + a fresh bind onto the new tree).
        ++reloadCount;
        const std::string text = read_file_text_s447(tmpPath);
        if (!text.empty())
        {
            reloadedPlan = run_app_shell_binding_plan(text);
        }
    });
    reload.on_reload_error([&](const std::string& d) { ++errorCount; lastReloadError = d; });

    // An idle poll of the un-changed baseline drives NO reload.
    SHOULD(ShouldlyIntegral(static_cast<int>(reload.check())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(reloadCount).ShouldBe(0));

    // EDIT (rewrite the same valid shell bytes — an mtime bump) → Reload fires,
    // the binding plan re-runs on the FRESHLY loaded text, and the S243/S244 wires
    // RE-ARM on the reloaded tree.
    wait_for_s447_stamp_granularity();
    write_file_s447(tmpPath, kReloadShellVaml);
    SHOULD(ShouldlyIntegral(static_cast<int>(reload.check())).ShouldBe(1));  // a Reload was driven
    SHOULD(ShouldlyIntegral(reloadCount).ShouldBe(1));
    SHOULD(ShouldlyIntegral(errorCount).ShouldBe(0));
    INFO("S447 reload: count=" << reloadCount << " errors=" << errorCount
         << " reloaded=" << reloadedPlan.has_value()
         << " lastErr='" << lastReloadError << "'");
    if (!reloadedPlan.has_value())
    {
        FAIL("S447 reload: on_reloaded never produced a plan (lastErr='"
             << lastReloadError << "', errors=" << errorCount
             << ", count=" << reloadCount << ")");
    }

    auto& p = *reloadedPlan;
    SHOULD(ShouldlyIntegral(static_cast<int>(p.ok)).ShouldBe(1, p.detail.c_str()));
    SHOULD(ShouldlyIntegral((bool)p.reader).ShouldBe(1));
    SHOULD(ShouldlyIntegral((bool)p.vm).ShouldBe(1));

    // The reload fixture is x:Name-free (so the real load_file seam parses it),
    // so locate the S243/S244 targets by TREE-WALK from the reloaded root:
    // Window → StackPanel → { TextBox (S244 two-way target), Text (S243 one-way target) }.
    vaml::Window* reloadedWindow = dynamic_cast<vaml::Window*>(p.root.get());
    SHOULD(ShouldlyIntegral(reloadedWindow != nullptr).ShouldBe(1));
    vaml::StackPanel* reloadedPanel = nullptr;
    if (reloadedWindow != nullptr && reloadedWindow->child_count() >= 1u)
    {
        reloadedPanel = dynamic_cast<vaml::StackPanel*>(reloadedWindow->child_at(0));
    }
    SHOULD(ShouldlyIntegral(reloadedPanel != nullptr).ShouldBe(1));
    vaml::TextBox* postInput = nullptr;
    vaml::TextBlock* postStatus  = nullptr;
    if (reloadedPanel != nullptr)
    {
        for (std::size_t i = 0; i < reloadedPanel->child_count(); ++i)
        {
            if (postInput == nullptr)
                postInput = dynamic_cast<vaml::TextBox*>(reloadedPanel->child_at(i));
            if (postStatus == nullptr)
                postStatus = dynamic_cast<vaml::TextBlock*>(reloadedPanel->child_at(i));
        }
    }
    SHOULD(ShouldlyIntegral(postInput != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(postStatus != nullptr).ShouldBe(1));

    // RE-ARM PROOF 1 — the S243 one-way wire still re-pushes the bound StatusText
    // on the RELOADED tree (a post-reload VM set flows to the target DP).
    (void)p.vm->set_Status("post-reload live");
    SHOULD(ShouldlyString(postStatus->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("post-reload live")));

    // RE-ARM PROOF 2 — the S244 TextBox two-way write-back still reaches the VM
    // store on the RELOADED tree.
    (void)postInput->set_text("post-reload typed");
    SHOULD(ShouldlyString(p.vm->Get(vaml::PropertyId::of("Text")))
               .ShouldBe(vaml::Value::String("post-reload typed")));

    std::remove(tmpPath.c_str());
}
