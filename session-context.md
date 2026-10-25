# VAML session-context (per-run state)

## last_command
implementit in progress — S440 UNBLOCKED + DELIVERED GREEN on BOTH CI lanes. The S440
load-exception blocker (the value-conversion gap) was root-caused + fixed, the app-shell
launch mode wired, and the dedicated Window-rebase test authored. Both lanes pass from
first principles. Sprint 11 first token closed at the test level.

## plan_digest
Sprint 11 (S440) — app-shell authored + Window resolution + render (dep:S408):
- S120/S121 gap CLOSED (the big one): `VamlReader::build()` + `VBamlReader` now call
  `finalize_visual_tree()` (the S440 core hand-off, opt-in) which TRANSFERS every
  declared child into its parent's S121 visual-children list, so a VAML-loaded/compiled
  tree RENDERS its children (not just the root). D1 ownership preserved (retained_
  stays owning; finalize empties it into the visual tree) — all pre-existing tests pass.
- Window RE-BASED onto UIElement (resolves the S087 MINIMAL REAL Window seed — the
  S440 core step): a REAL single-content container window-type (set_content /
  MeasureOverride / ArrangeOverride) ready to be the app-shell root.
- VALUE-CONVERSION GAP FIXED (the actual S440 `plan.detail` blocker — NOT a typo):
  the app-shell is the first LIVE VamlReader::Load document to author real control DPs
  from markup. Two genuine S089-layer gaps + one test bug resolved:
    * `Orientation="Horizontal"` -> Int32 converter now ALSO accepts the common VAML
      enum-name map (WPF-faithful). (ValueConverter.cpp convert_int32_value)
    * `FontSize="18"` / `Slider.Minimum/Maximum/Value` -> added a strict finite Double
      converter + wired ValueType::Double -> "Double" in VamlReader::converterNameFor.
    * AppShellPlan_s440_test test-3 now navigates through the Window root's child (was
      casting the Window to Grid*).
- App-shell authored: `vaml_viewer/app_shell.h` (app_shell_plan decision + run_app_shell_
  plan + MainView.vaml inline) + `vaml_viewer/MainView.vaml` (on-disk twin).
- --app LAUNCH MODE WIRED: `cli.h` gains cli_wants_app_shell + cli_action treats --app as
  Run; `main.cpp` dispatches --app -> ViewerApp::create_app_shell; app.{h,cpp} add
  create_app_shell (adopts the shell root + retains the shell reader). headless CLI tests
  added to ViewerCli_test.cpp.
- NEW dedicated Window_rebase_test.cpp (core): Window IS-A UIElement + factory-resolves
  by name + single-content slot + fills its child (implicit single-child layout).
- MSVC PCH enabled (cmake/vaml_pch.hpp + pch.cmake; the cygwin-cmake cache corruption
  diagnosed at S440 — the build lane now runs VS-native cmake).

## files_touched
- MOD src/core/src/ValueConverter.cpp (Int32 value converter: + common enum-name map;
  NEW Double converter; convert_int32_value/convert_double/enum_name_to_int32)
- MOD src/core/src/VamlReader.cpp (converterNameFor: + ValueType::Double)
- MOD src/core/include/vaml/core/Element.h + src/core/src/Element.cpp (Windows: re-based
  onto UIElement — already committed by the prior S440 session)
- MOD vaml_viewer/cli.h (+ cli_wants_app_shell; cli_action --app -> Run; cli_usage --app)
- MOD vaml_viewer/main.cpp (--app dispatch -> ViewerApp::create_app_shell)
- MOD vaml_viewer/app.h + app.cpp (ViewerApp::create_app_shell + appShellReader_ member)
- NEW test/unit/Window_rebase_test.cpp + MOD test/unit/CMakeLists.txt
- MOD test/unit/ViewerCli_test.cpp (the S440 --app decision tests)
- MOD test/unit/AppShellPlan_s440_test.cpp (test-3 Window-root navigation)
- MOD build/verify_s440.cmd (route VS-native cmake/ctest first; the cygwin-cmake
  corruption fix)
- Pre-existing (prior S440 session, still on disk): finalize_visual_tree on VamlReader
  + VBamlReader, app_shell.h, MainView.vaml, AppShellPlan_s440_test.cpp, cmake PCH.

## build_status
BOTH CI LANES GREEN from first principles at S440:
- windows-msvc (msvc-debug, VS-native cmake, warning-as-error clean): ctest 4/4 PASS —
  vaml-core 1012 (incl. the 4 Window_rebase tests + the 2 new --app CLI tests), vaml-tools
  89, vaml-vulkan 222, vaml-presentation 60 (incl. all 4 AppShellPlan_s440 cases).
  Full clean reconfigure via VS-native cmake (the cygwin cmake on PATH corrupts the cache
  to /cygdrive + CMAKE_CXX_COMPILER=C — that is the build-infra failure surfaced this pass).
- ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, clang 21 on the VM): build warnings==0 +
  ctest 4/4 PASS; S440 core + presentation suites green (1012 / 60).
- windows-clang-debug is NOT a lane (not built/tested/flagged).
- The one "failed as expected" assertion in vaml-core-tests is the demonstration test
  (doctest marks it not-failed), not a regression.

## next_step
S440 remains OPEN only in the [both apps, not CI] sense: the LIVE --app window render
(create_app_shell on a real driver + window) is the DISPLAY proof, verified live on this
box — it is NOT a CI lane (the standing S160+ precedent). The named gate verify_s440.cmd
(re-routed to VS-native cmake) is the deterministic msvc gate; re-run it from first
principles on the next implementit to confirm the lane entry point is clean. The rest of
Sprint 11 (S441–S449: VM, demo service, {Binding}/{Command} wiring, vamlcc compile step,
E2E, hardening, integration test, VAML Guide, refactor gate) is NOT started.

## notes
- S440 root-cause note: the "plan.detail" that kept failing was NEVER an attribute/parse
  typo — it was the S089 value-conversion gap (enum names + Double). Only reading the
  printed plan.detail (a printf the prior session added) revealed it; this session read it
  as `cannot convert 'Horizontal' to Int32 (MalformedValue)` and fixed the layer.
- Build-infra note (recorded NO-GAP, not silent): the msvc-debug cache was re-created by
  VS-NATIVE cmake (not the cygwin cmake on PATH). Any Windows rebuild/CI must run from a
  vcvars64 shell with the VS-native cmake/ninja on PATH (verify_s440.cmd now does this).
- No commits made (per the standing directive unless 'commitit' is commanded).
