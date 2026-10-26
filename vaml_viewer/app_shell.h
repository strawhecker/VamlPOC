// vaml_viewer — the S440 APP-SHELL seam (Sprint 11, dep:S408).
//
// THE APP-SHELL — the in-repo v1 deliverable that makes EVERY SPRINT-10 CONTROL
// FUNCTIONAL with declared children in a RUNNING app (the user's sprint-11
// directive: "make all aspects of sprint 10 functional with children defined;
// sprint 11 exits with a fully functional APP").
//
// This file IS the S440 app-shell harness, on the ADR-021 public-surface-only
// consumer path (compiled by BOTH apps + the test target). It
//   1. LOADS the authored MainView.vaml through a RETAINED VamlReader,
//   2. calls `finalize_visual_tree()` — the S440 core hand-off that TRANSFERS
//      every declared child into its parent's SI21 visual-children list, so the
//      Window root RENDERS its declared Grid/StackPanel/Text/TextBox/Button/
//      Slider/CheckBox subtree (not just the root — this closes the S120/S121
//      gap Sprint 10 left unclosed),
//   3. re-bases the app-shell onto the S440 REAL `Window` container window-type
//      (resolves the S087 MINIMAL REAL seed at its own sprint — NO-GAP),
//   4. returns the LIVE tree + the retained reader so the S443 {Binding}/
//      {Command} wiring (the app-shell's RelayCommands + two-way TextBox +
//      Slider road) can be driven end-to-end (S445).
//
// SPLIT (the standing S221/S222_1 precedent — same as viewer_binding.h):
//   * PURE DECISION SEAM — `app_shell_plan(elevated)` — the deterministic
//     "from a (mode, load) outcome, what should the app-shell do?" table
//     (RenderShell / Report). HEADLESS.
//   * THE REAL PLAN RUNNER — `run_app_shell_plan()` — Loads MainView.vaml
//     through the RETAINED reader, calls finalize_visual_tree(), and returns
//     { ok, root, reader, count, detail }. The EXACT code path the live `--app`
//     viewer launch uses (see main.cpp/app.cpp); HEADLESS (no window/runtime).
//
// S440 accepts the open-ended VIEWER-GROWTH marker: the app-shell is Sprint 11's
// concrete growth token (what Sprint 10 was to the viewer, Sprint 11's app-shell
// is to the v1 app). ADR-018: ZERO new locks (single-threaded on the viewer's UI
// thread). NO MINIMAL REAL introduced — the AppShell harness + finalize
// hand-off are REAL permanent code, kept-and-extended by S441–S449.

#pragma once

#include "vaml/core/DependencyObject.h"   // root ownership
#include "vaml/core/VamlReader.h"         // VamlReader (Load + finalize_visual_tree)
#include "vaml/core/UIElement.h"          // UIElement (the visual-tree child host)

#include <memory>
#include <string>

namespace vaml::viewer {

// ---------------------------------------------------------------------------
// What the app-shell should do from a (mode, load) outcome (headless decision):
//   RenderShell — the app-shell file loaded + finalized cleanly -> the app
//                 builds/re-renders the shell's live tree.
//   Report      — the app-shell failed to load/finalize -> report + keep the
//                 last-good shell (never tear down on a transient failure).
// ---------------------------------------------------------------------------
enum class AppShellAction
{
    RenderShell = 0,
    Report,
};

// The PURE decision: given whether the app-shell's MainView.vaml loaded + the
// visual tree finalized, what should the `--app` launch do? HEADLESS.
inline AppShellAction app_shell_plan(bool loadOk) noexcept
{
    return loadOk ? AppShellAction::RenderShell : AppShellAction::Report;
}

// The outcome of an app-shell plan run.
//
// The reader is owned on the HEAP and returned so the S443 live wire STAYS
// ALIVE after the plan returns — a live reader keeps the S252 binding/event
// subscriptions armed + the finalized S121 visual tree reachable, so the caller
// (the app or a test) can drive `click`/`type`/`slide`/`check` end-to-end.
// `root` is the LIVE UIElement tree (owned), `reader` is retained.
struct AppShellPlanResult
{
    bool ok = false;                            // loaded + finalized cleanly
    std::unique_ptr<vaml::UIElement> root;      // the live shell tree (UIElement root)
    std::unique_ptr<vaml::VamlReader> reader;   // RETAINED reader — keeps the tree + bindings armed
    std::size_t transferred = 0;                // the S440 finalized-into-visual-tree child count
    std::string detail;                         // structured failure reason (never silent)
};

// ---------------------------------------------------------------------------
// The AUTHORED app-shell markup (MainView.vaml), inlined here as the single
// source of truth the S440 harness loads. The file `vaml_viewer/MainView.vaml`
// is the on-disk twin (authored + committed for the S444 vamlcc compile step);
// this string keeps the HEADLESS harness (tests + the non-compiled launch)
// self-contained so S440 can prove the app-shell PARSE + RENDER without a
// build-of-a-source-file dependency. S444 compiles the ON-DISK MainView.vaml
// through vamlcc; both must stay byte-identical (a parity test asserts it).
// ---------------------------------------------------------------------------
inline constexpr const char* kAppShellMainViewVaml =
R"VAML(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="root">
  <Grid x:Name="RootGrid">
    <StackPanel x:Name="HeaderPanel" Orientation="Horizontal">
      <Text x:Name="TitleText" Text="VAML App Shell" FontSize="18"/>
      <Button x:Name="RunButton" Click="OnClick" Command="{Command RunCommand}"/>
      <CheckBox x:Name="EnabledCheck" CheckBox.IsChecked="0"/>
    </StackPanel>
    <StackPanel x:Name="BodyPanel" Orientation="Vertical">
      <Text x:Name="PromptText" Text="Enter text:" FontSize="14"/>
      <TextBox x:Name="InputBox" TextBox.Text="{Binding Text, Mode=TwoWay}"/>
      <Text x:Name="StatusText" Text="{Binding Status}" FontSize="12"/>
    </StackPanel>
    <StackPanel x:Name="FooterPanel" Orientation="Vertical">
      <Text x:Name="SliderLabel" Text="Dim:" FontSize="12"/>
      <Slider x:Name="DimSlider" Slider.Minimum="0" Slider.Maximum="100" Slider.Value="{Binding SliderValue, Mode=TwoWay}"/>
    </StackPanel>
  </Grid>
</Window>
)VAML";

// THE REAL PLAN RUNNER (headless — the exact code the live `--app` launch uses):
// Loads the authored MainView.vaml through a RETAINED VamlReader, then calls
// `finalize_visual_tree()` — the S440 hand-off that puts every declared child
// into its parent's S121 visual-children list (so the shell RENDERS children),
// and reports. Returns ok=false on ANY error with the structured detail (never
// silent). The reader + root are returned owned so the S443 binding wiring can
// be driven by the caller.
[[nodiscard]] inline AppShellPlanResult run_app_shell_plan()
{
    AppShellPlanResult r;
    try
    {
        // The reader is created once and RETAINED (heap) so finalize_visual_tree()
        // + the S443 bindings (reader-owned, per-Load) survive the plan's return.
        r.reader = std::make_unique<vaml::VamlReader>();
        // S443: MainView.vaml's RunButton declares `Click="OnClick"` — register the
        // Click EVENT so the S440 load path routes it to a deferred event handler
        // (never an unknown-property throw). The full S443 binding plan additionally
        // binds the OnClick class-owner handler + BindEvents; the S440 harness only
        // needs the load to parse (the deferred record is harmless).
        r.reader->register_event("Click");
        std::unique_ptr<vaml::DependencyObject> loaded = r.reader->Load(kAppShellMainViewVaml);
        if (loaded == nullptr)
        {
            r.ok = false;
            r.detail = "app-shell: MainView.vaml did not produce a root element";
            return r;
        }
        // The root is a UIElement (the Window root of the app-shell — the S440
        // REAL Window). The S2 reader builds the root as a UIElement subclass; a
        // non-UIElement root cannot be the app-shell's renderable root, so fail
        // loudly (never silent).
        vaml::UIElement* uiRoot = dynamic_cast<vaml::UIElement*>(loaded.get());
        if (uiRoot == nullptr)
        {
            r.ok = false;
            r.detail = "app-shell: MainView.vaml root is not a UIElement (cannot render)";
            return r;
        }
        r.root = std::unique_ptr<vaml::UIElement>(static_cast<vaml::UIElement*>(loaded.release()));
        // S440 — the S120/S121 hand-off: render the DECLARED children, not just
        // the root. `transferred` is the never-silent contract (0 == nothing
        // reached the visual tree).
        r.transferred = r.reader->finalize_visual_tree();
        r.ok = (r.transferred > 0);
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
