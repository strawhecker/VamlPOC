# VAML session-context (per-run state)

## last_command
implementit no commits no NEW md files — S222_1 (dep:S222) FILE-WATCH LIVE RELOAD, completed + verified + LIVE-proven.

## plan_digest
S222_1 complete + verify + docs: (A) vaml_viewer/file_watch.{h,cpp} (mtime-poll + NOTIFY seam, pure table + FileWatcher) + (B) vaml_viewer/live_reload.{h,cpp} (reload_action + LiveReload coordinator) + (C) MOD app.{h,cpp} (filePath opt-in + LiveReload + run_loop poll + apply_reload re-running the whole S5 path) + both main.cpp (CLI path → filePath) + build wiring (vcxproj + vaml_viewer-linux CMake + test CMake) + (D) FileWatch_test + LiveReload_test (vaml-core-tests, incl. the GOLDEN edit→reload→same tree) + ViewerReloadPlan_test (vaml-presentation-tests) + (E) build/verify_s222_1.cmd + GREEN BOTH CI LANES (windows-msvc incl. the LIVE reload demo close=0 + ubuntu-clang) + (F) roadmap S222_1 [DONE] + deferred resolved + progress checkpoint → S223 + session-context.

## files_touched
- ADDED vaml_viewer/file_watch.{h,cpp} (the mtime-poll + notify seam)
- ADDED vaml_viewer/live_reload.{h,cpp} (the "edit → reload → same tree" coordinator)
- MOD vaml_viewer/app.{h,cpp} (ViewerAppConfig.filePath opt-in + LiveReload armed + run_loop poll + apply_reload — the whole S5 path re-ran with ZERO imperative tree-building)
- MOD vaml_viewer/main.cpp + vaml_viewer-linux/src/main.cpp (CLI path → config.filePath)
- MOD vaml_viewer/vaml_viewer.vcxproj (+file_watch.cpp + live_reload.cpp)
- MOD vaml_viewer-linux/CMakeLists.txt (+file_watch.cpp + live_reload.cpp)
- ADDED test/unit/FileWatch_test.cpp (vaml-core-tests: pure table + real-file lifecycle + notify seam)
- ADDED test/unit/LiveReload_test.cpp (vaml-core-tests: THE GOLDEN edit→reload→same tree + reload_action + parse-error never-silent)
- ADDED test/unit/ViewerReloadPlan_test.cpp (vaml-presentation-tests: config opt-in + decision table)
- MOD test/unit/CMakeLists.txt (+the three test files + file_watch.cpp + live_reload.cpp into BOTH vaml-core-tests AND vaml-presentation-tests + load_file.cpp into vaml-presentation-tests)
- ADDED build/verify_s222_1.cmd
- MOD docs/roadmap.md (S222_1 [DONE] write-up + the deferred "live file-reload" item → RESOLVED)
- MOD progress.md (checkpoint → S223; Sprint 5a S222_1 SURVIVOR)
- MOD session-context.md (this file)

## build_status
GREEN both CI lanes (from first principles):
- windows-msvc: build/verify_s222_1.cmd — msvc-debug warning-as-error CLEAN + ctest 3/3 PASS (incl. FileWatch + LiveReload + ViewerReloadPlan) + vaml_viewer Debug/x64 clean + CLI proofs 0/2/1/3 never-silent + LIVE file-watch reload demo close=0 on the NVIDIA RTX A5000 (window opens armed on a temp .vaml, the file REWRITTEN mid-run → the whole S5 path re-runs → re-render, clean auto-close).
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — configure/build warnings==0 + ctest 3/3 PASS + vaml_viewer-linux builds clean under clang 21.1.8.
windows-clang-debug is NOT a lane (pre-existing /Zc:preprocessor failure; not built/tested).

## next_step
S223 (dep:S222_1) — Drag-and-drop entry: PlatformWindow (S160) gains a FILE-DROP callback (GLFW drop path, Win32 + X11); viewer reopens the dropped `.vaml`; drop-dispatch → reopen handler seam is headless-testable.

## notes
- TEST-ROBUSTNESS (recorded at S222_1): the real-file watcher tests sleep >2s for the mtime bump because the VM CI lane runs on the VMware HGFS shared folder (~2s mtime granularity) vs ~100ns on native filesystems — the 2200ms sleep covers BOTH lanes so the golden "edit → reload → same tree" is deterministic everywhere.
- The reload re-runs the whole S5 path with ZERO imperative tree-building: apply_reload → adopt the new tree → set_root → set_frame_size → invalidate() (the exact S222 producer path, re-invoked). The retained renderer records + presents the re-declared tree; the swapchain/presenter/device are NOT recreated.
- A parse-error edit keeps the last-good tree + surfaces on_reload_error to stderr (never silent); a disappeared file is Reported (last-good kept), never a blank window on a transient save.
- No MINIMAL REAL introduced/resolved at S222_1 (the file-watch/live-reload seams are REAL permanent code). ADR-018: ZERO new locks (single-threaded, driven off the viewer's UI thread in run_loop()).
- No commits made (per standing directive unless 'commitit' is explicitly commanded).
- No NEW md files created (only the existing operational files roadmap/progress/session-context were updated).
