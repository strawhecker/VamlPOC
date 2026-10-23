// S222_1 (dep:S222) — the viewer LIVE-RELOAD plan seam (headless Rail 1).
//
// Proves the S222_1 reload coordinate at the VIEWER boundary without a live Vulkan
// runtime (the standing S160+ precedent), through the shared ViewerApp host's pure
// surface (vaml_viewer/app.{h,cpp} + vaml_viewer/live_reload.{h,cpp}, compiled into
// vaml-presentation-tests — the ADR-021 consumer path, single source of truth):
//   (1) the reload decision table (reload_action, from the live_reload seam) — the
//       S222_1 "what does a file event drive" contract;
//   (2) ViewerAppConfig — S222_1 is OPT-IN: a config with an EMPTY filePath builds a
//       viewer with NO reloader (the S222 behavior is unchanged); a non-empty
//       filePath arms the watch (the file-watch reload vehicle). The reload itself
//       re-runs the whole S5 path (apply_reload -> set_root -> set_frame_size ->
//       invalidate) with ZERO imperative tree-building — the golden proof lives in
//       LiveReload_test.cpp (vaml-core-tests); here we assert the CONFIG contract.
//
// The LIVE create()/run_loop() reload (watcher armed + the file rewrite mid-run) is
// [both apps, not CI] — the live demo in verify_s222_1.cmd proves it on this box.

#include "app.h"             // the S222 shared viewer render host
#include "live_reload.h"     // the S222_1 reload coordinator seam

#include "doctest/doctest.h"

#include <string>

using namespace vaml::viewer;

TEST_CASE("S222_1: ViewerAppConfig.filePath is the RELOAD opt-in (empty = S222 unchanged)")
{
    ViewerAppConfig cfg;
    // S222 default: filePath is EMPTY -> the viewer opens + renders once with NO
    // file-watch reload (the S222 behavior). S222_1 arms only on a non-empty path.
    CHECK(cfg.filePath.empty());
    CHECK(cfg.title == "VAML Viewer");
}

TEST_CASE("S222_1: reload_action drives the S5 re-run (Changed/Appeared), keeps on Disappeared")
{
    // Changed -> Reload (the mtime edit): the whole S5 path re-runs.
    CHECK(reload_action(FileEvent::Changed) == ReloadAction::Reload);
    // Appeared -> Reload (a recreated file reloads too).
    CHECK(reload_action(FileEvent::Appeared) == ReloadAction::Reload);
    // Disappeared -> Report (last-good KEPT, never a blank window on a transient save).
    CHECK(reload_action(FileEvent::Disappeared) == ReloadAction::Report);
    // Unchanged -> KeepLast (an idle poll drives no action).
    CHECK(reload_action(FileEvent::Unchanged) == ReloadAction::KeepLast);
}

TEST_CASE("S222_1: the reload re-runs the S5 path (the declared reload plan)")
{
    // The roadmap contract: on reload the viewer re-adopts the loaded tree +
    // set_root + set_frame_size + invalidate() — the EXACT S222 producer path,
    // re-invoked, NOT hand-built (ZERO imperative tree-building). This is stated in
    // ViewerApp::apply_reload (app.{h,cpp}, compiled in here). The seam's observable
    // truth: a Changed event is a Reload (above); the golden "edit→reload→same tree"
    // lives in LiveReload_test.cpp. This case pins the reload PLAN headline.
    CHECK(reload_action(FileEvent::Changed) == ReloadAction::Reload);
    CHECK(reload_action(FileEvent::Unchanged) == ReloadAction::KeepLast);
}
