# VAML session-context (per-run state)

## last_command
implementit DONE — S447 (dep:S446) — IN-REPO E2E INTEGRATION TEST of the app-shell (the SINGLE
in-repo integration suite that re-derives the FULL authored pipeline of the app-shell across all
FIVE surfaces in ONE coherent narrative), complete + GREEN on BOTH CI LANES this session
(build/verify_s447.cmd = the two-lane gate). An INTEGRATION/VERIFICATION ("INV") sprint — NO
product feature code, NO MINIMAL REAL. Delivered TWO linked parts (Deliverables of the accepted
plan):
- (A) NEW test/unit/AppShellIntegration_test.cpp (4 TEST_CASEs / 64 assertions, vaml-tools-tests
  — it links vaml-tools so leg (D) can load the release-compiled sidecar, and it already hosts
  the S444 suite + the vaml_viewer include). KEY ARCHITECTURAL DECISION (accepted at plan review):
  the integration test lives in vaml-tools-tests, NOT vaml-core-tests — because leg (D)
  (compiled-path parity) REQUIRES vaml-tools (the S285 VBamlReader), which vaml-core-tests
  neither links nor has on its include path. One coherent narrative across the five legs:
  (A) TEXT PARSE → LIVE TREE (S440): kAppShellMainViewVaml → VamlReader::Load → real Window root
  → finalize_visual_tree hands off the declared subtree; every named control reachable. (B)
  VM/BINDING/COMMAND WIRING (S443): run_app_shell_binding_plan — set_data_context + BindExtensions
  (bound==3: Status OneWay, Text TwoWay, SliderValue TwoWay) + register_event("Click")/
  bind_class_handler/BindEvents (wired==1) + {Command RunCommand} resolves to the VM RelayCommand;
  retained reader + VM alive. (C) EACH INTERACTION → VM PROPS + BOUND TARGET DPs (S445): click →
  routed Click → app OnClick → VM RunCommand → {Binding Status} repaints StatusText; type → TextBox
  two-way → VM Text; slide → Slider two-way → VM SliderValue; check → app CheckBox↔VM wire → VM
  Checked. (D) COMPILED-PATH + TEXT-PATH IN ONE SCENE (S444 single-scene contraction — NOT S444's
  per-path gold): the release-compiled sidecar (VAML_S444_EMBED_SIDECAR) AND the dev-text path both
  serve the SAME named shell controls, to the same concrete types — the "release == dev" surface
  contraction S445 does not drive. (E) RELOAD PATH (S222_1) RE-WIRES: a real LiveReload over a real
  temp reload fixture (edit → reload → fresh tree) → RE-RUN run_app_shell_binding_plan onto the
  freshly loaded text → the S243/S244 wires RE-ARM (post-reload set_Status re-pushes the bound
  StatusText; a TextBox two-way edit writes back to the VM store) — the S243/S244 re-wire on a
  genuinely reloaded tree.
- (B) NEW build/verify_s447.cmd — the two-lane gate: windows-msvc (msvc-debug warning-as-error
  clean + ctest 4/4 incl. the S447 suite 4/4/64) + ubuntu-clang (clang-debug on the VM shared
  folder /mnt/hgfs/VamlPOC: configure + warning-clean build + ctest 4/4 incl. the S447 suite).
  The validated VS-native PATH recipe (reset PATH → prepend VS 2026 CMake+Ninja → vcvars64) is
  baked in. windows-clang-debug is NOT a lane.
- plus MOD test/unit/CMakeLists.txt (vaml-tools-tests += AppShellIntegration_test.cpp +
  the binding/reload seam TUs: ../../vaml_viewer/app_shell_vm.cpp + load_file.cpp + file_watch.cpp
  + live_reload.cpp — the ADR-021 single source of truth; handles the S440/S443/S445/S221/S222_1
  shared-seam compile per the other targets' precedent).
Sprint 11 ninth token closed.

## plan_digest
Sprint 11 (S447 — in-repo E2E integration test of the app-shell, dep:S446):
- An INV/verification sprint: the SINGLE in-repo integration suite (vaml-tools-tests) that
  re-derives the FULL authored pipeline of the app-shell in ONE coherent narrative — (A) text
  parse → live Window tree (S440), (B) VM/binding/command wiring (S443), (C) each interaction →
  VM props + bound target DPs (S445), (D) compiled-path + text-path in ONE scene serving the SAME
  named shell (S444 single-scene contraction, NOT S444's per-path gold), (E) the S222_1 reload
  re-wire: a real LiveReload over a real temp fixture → re-run the app-shell binding plan onto the
  freshly loaded text → the S243/S244 wires re-arm.
- [S447 RECORDED IMPLEMENT BLOCKER RESOLUTION — accepted 2026-04]: the S222_1 reload coordinator
  (LiveReload → vaml::load_file → a FRESH bare VamlReader) is a GENERIC seam with NO event
  registry — it CANNOT parse the interactive app-shell's Click="OnClick" (the bare reader throws
  `unknown property 'Click'` because only run_app_shell_binding_plan registers the Click event on
  ITS OWN reader). The app's ACTUAL live reload reloads the plain S256 view (run_viewer_binding_plan
  over Click-free/x:Name-free markup), NOT the interactive app-shell. RESOLUTION (accepted): leg
  (E) drives the real LiveReload over a Click-free + x:Name-free shell (parses through the generic
  load_file seam AND re-binds through run_app_shell_binding_plan), and asserts the S243/S244
  RE-ARM on the freshly reloaded tree — the honest "the app re-wires the freshly loaded tree"
  path (a fresh Load + a fresh bind onto the new tree). The interactive Click shell's reload is
  recorded forward (the app-shell's live reload is not wired today — consistent with the codebase).
- NO-GAP (recorded, not silent): the {Binding}/{Command} wiring on the COMPILED app-shell path
  stays deferred — the S286 compiled reader defers {Binding} by design (the S293/S443/S444
  precedent); S447's leg (D) is S444's release-parse/parity surface and its interactive/reload
  legs are the S443/S445 TEXT binding-plan path. No MINIMAL REAL introduced/resolved. ADR-018:
  zero new locks (single-threaded; LiveReload + interactions on the UI thread).

## files_touched
(S447, this token)
- NEW test/unit/AppShellIntegration_test.cpp (4 cases / 64 assertions; vaml-tools-tests) — the in-repo E2E integration suite (5 legs; compiled+text single scene; reload S243/S244 re-wire)
- MOD test/unit/CMakeLists.txt (vaml-tools-tests += AppShellIntegration_test.cpp + app_shell_vm.cpp + load_file.cpp + file_watch.cpp + live_reload.cpp seam TUs — the ADR-021 shared seams)
- NEW build/verify_s447.cmd — the two-lane gate (windows-msvc + ubuntu-clang)
- MOD docs/roadmap.md (S447 [DONE]) + progress.md (checkpoint regenerated to S447)
- MOD session-context.md (this file)

## build_status
ALL S447 GATES GREEN (verified from first principles this sprint):
- verify_s447.cmd: BOTH CI LANES PASS — windows-msvc (msvc-debug warning-as-error clean + ctest
  4/4 incl. the S447 suite 4/4 / 64/64) + ubuntu-clang (clang-debug on the VM /mnt/hgfs/VamlPOC:
  configure + warning-clean build (221 steps) + ctest 4/4 incl. the S447 suite 4/4 / 64/64).
- The S447 suite standalone (--test-case="*S447*"): 4 passed / 0 failed / 64 assertions, SUCCESS.
- windows-clang-debug is NOT a lane (not built/tested/flagged).
NOTE (recorded, pre-existing, NOT introduced by S447): the standalone vaml_viewer.vcxproj build
still surfaces the pre-existing S444 C4129 warning-as-error in app_shell_compiled.h:92 (NOT a CI
lane; stays owned by a future step, consistent with the S445/S446 note). The compiled
`--compiled --app` app-shell {Binding} wiring stays deferred exactly as S445/S446 recorded (the
S286 compiled reader defers {Binding} by design); S447 exercises the S443/S445 TEXT binding-plan
app-shell — the same one the `--app` window runs.

## next_step
S447 delivered + green (both CI lanes). The rest of Sprint 11 (S448: in-repo app-shell acceptance
doc; S449: refactor gate) is NOT started. Next implementit = S448. NO-GAP (recorded, not silent):
the {Binding}/{Command} wiring on the COMPILED app-shell path stays deferred — the S286 compiled
reader defers {Binding} by design (the S293/S443/S444 precedent); S447's compiled leg was S444's
single-scene parity contraction + its interactive/reload legs were the S443/S445 TEXT binding-plan
path. The interactive app-shell's LIVE reload (a Click-capable shell through the generic S222_1
seam) is recorded forward — the S222_1 load_file seam has no event registry; the app's actual
reload reloads the plain S256 view, exactly what leg (E) faithfully drives.

## notes
- S447 integration notes (recorded, not silent): leg (D) deliberately does NOT re-run S444's
  compare_dual_path(kAppShellMainViewVaml,…) gold (S444 owns it in this same binary) — S447's
  contribution is the SINGLE-SCENE correspondence (release-compiled + dev-text both resolve the
  same named shell controls to the same concrete types), the complement-not-duplicate contract.
- S447 reload-leg reality (recorded): the generic S222_1 load_file seam rejects BOTH `Click` AND
  `x:Name` on a fresh bare VamlReader (`unknown property 'Click'` / `unknown property 'x:Name'`).
  The reload fixture is therefore Click-free + x:Name-free (so the real seam parses it), and the
  S243/S244 targets are located by TREE-WALK (Window → StackPanel → {TextBox, Text}) instead of
  by-name. run_app_shell_binding_plan registers Click/commands/handler itself + hooks the S244
  TwoWay TextBox + the S243 OneWay StatusText onto the app-shell VM — so both the S243 one-way
  (post-reload VM set → target DP) and the S244 two-way (target edit → VM store) re-arm on the
  genuinely reloaded tree.
- Build-lane robustness: the validated VS-native PATH recipe (reset PATH → prepend VS 2026
  CMake+Ninja → call vcvars64) is baked into verify_s447.cmd so the Cygwin cmake/ld stays out of
  the lane (same as S446).
- IMPORTANT work-flow guard (still governing): the Linux verify lane runs from the SHARED folder
  /mnt/hgfs/VamlPOC. NEVER run `git checkout -- .` (or any git checkout touching tracked files)
  inside the VM against that shared folder — it reverts uncommitted tracked edits on the SAME
  Windows E:\GitRepos\VamlPOC. New UNTRACKED files survive, but tracked-file edits are wiped.
- No commits made (per the standing directive unless 'commitit' is commanded).
