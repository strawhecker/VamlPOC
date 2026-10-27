# VAML session-context (per-run state)

## last_command
implementit DONE — S449 (dep:S448, DRIVER-REVERSED to run BEFORE S448) — REFACTOR GATE(S11) + SURVIVOR,
complete + GREEN on BOTH CI LANES + ALL this-box pre-✅ gates re-run + the viewer made FULLY OPERATIONAL
this session. The DRIVER redefined the acceptance at implementit: *"I want a fully operational
vaml_viewer that shows .vaml files. no exceptions."* — AND reversed the S448/S449 order (*"it makes no
sense to me to repair the guide before we have a fully working viewer ... so i reversed the order"*). So
S449 closed FIRST; S448 (the guide) is now the next step, written from the verified viewer.
- (A) TWO-LANE CI GATE — NEW build/verify_s449.cmd — GREEN: windows-msvc (msvc-debug warning-as-error
  clean + ctest 4/4) + ubuntu-clang (clang-debug on the VM /mnt/hgfs/VamlPOC: configure + warning-clean
  build + ctest 4/4). The ViewerCli_test target compiles cli.h (221 steps both lanes), so the NEW
  cli_file_path() is warning-clean on both.
- (B) FULLY OPERATIONAL VIEWER (the redefined core acceptance; this box, real NVIDIA RTX A5000) — FOUR
  launch modes, each real window + Vulkan stack + VamlRenderer::create_device + retained render + clean
  teardown, ALL exit 0: `vaml_viewer <file.vaml>` (S221/S222), `--app` (S440/S443/S445), `--compiled
  <file.vaml>` (S293), `--compiled --app` (S444). THIS REQUIRED 4 REAL CODE FIXES in the STANDALONE
  vaml_viewer (which was NOT operational):
  (1) vaml_viewer.vcxproj `VAML_APPSHELL_SIDECAR="$(IntDir)MainView.vbaml"` -> unescaped-backslash C4129
  (warning-as-error) BLOCKED the viewer build (left the shipped exe stale/pre-Sprint-11, so `--app` failed);
  root fix in the vcxproj: CustomBuild outputs + the macro use `$(OutDir)` with the
  `$(OutDir.Replace('\','/'))` forward-slash property function. (2) NEW pure cli_file_path() in cli.h
  (skips known flags) fixes main.cpp's `--compiled <file>` argv[1] bug. (3) `--compiled --app` got
  `unknown element type 'Window'` — the compiled-only .vbaml direct-load path runs VBamlReader::Load
  WITHOUT priming the element factory; main.cpp now calls register_builtin_elements(). (4) then `no value
  converter registered for type 'Int32'` — same root for the attribute table; fixed via
  register_builtin_converters(). All four modes now render .vaml, exit 0.
- (C) GREP-AUDIT PASS: no STUB/TODO/FIXME/backfill/scaffold in the app-shell set; `[MINIMAL REAL] Element
  Window` greps to ZERO across src/core Element.h + Element.cpp (S440 resolution confirmed); zero
  `[MINIMAL REAL]` remain in src/ (all Sprint-11 guard-rail seeds already resolved).
- (D) DEAD-SURFACE AUDIT PASS: run_app_shell_plan 8, run_app_shell_binding_plan 32, cli_wants_app_shell
  12, load_compiled_app_shell_file 11, load_compiled_app_shell_embedded 2, compiled_app_shell_sidecar_path
  1, ViewerApp::create_app_shell 4, create_compiled_app_shell 2 — nothing orphaned.
- (E) ASan/LSan GREEN: verify_s446_asan.cmd from first principles — msvc-asan zero ASan errors (incl.
  AppShellChurn 10k, ctest 4/4) + clang-asan/LSan on the VM detect_leaks=1:exitcode=86 ZERO VAML-OWNED
  leaks (only the scoped-suppressed GLFW-X11 singleton, ctest 4/4).
- (F) real-WSI GREEN: verify_s446_wsi.cmd on the real NVIDIA RTX A5000 — hidden-window real Vulkan
  app-shell (transferred=12), retained encode + change-driven 10,000 PRESENTED frames, ZERO validation
  errors, clean teardown (exit 0).
Sprint 11 closing token done. Full digest in the S449 SURVIVOR block in progress.md + the roadmap S449 [DONE].

## plan_digest
Sprint 11 (S449 — REFACTOR GATE (S11) + SURVIVOR, dep:S448 but run FIRST per the driver's reversal):
- The driver REVERSED the S448/S449 order (guide follows a verified viewer) and REDEFINED S449's core
  acceptance: a FULLY OPERATIONAL vaml_viewer that SHOWS .vaml files — NO EXCEPTIONS.
- Two-lane CI gate (verify_s449.cmd); fully-operational viewer re-verification (4 launch modes, real GPU);
  grep-audit (no stubs/backfills, Window sentinel removed at S440); dead-surface audit (all S440-S448
  symbols consumed); ASan/LSan zero-leak; real-WSI 10k-frame; SURVIVOR + checkpoint advance (last = S449,
  next = S448 guide, then S480 architecture).
- NO-EXCEPTIONS enforcement: the plan's keep-out-of-scope deferral of the pre-existing vcxproj C4129
  artifact is OVERRIDDEN by the explicit mandate — the standalone viewer WAS NOT operational (stale exe +
  C4129 build blocker + compiled-path registration gaps + an argv bug), so 4 real code fixes were required
  and delivered (vcxproj OutDir/forward-slash macro + cli_file_path() + register_builtin_elements() +
  register_builtin_converters() in main.cpp).
- NO MINIMAL REAL introduced/resolved. ADR-018 ZERO new locks. No commits (per the standing directive).

## files_touched
(S450_1 — the `.xaml` acceptance gate + the drop-seam completion)
- NEW build/verify_s449.cmd — the two-lane gate (windows-msvc + ubuntu-clang) [from S449]
- MOD vaml_viewer/vaml_viewer.vcxproj — VAML_APPSHELL_SIDECAR macro + CustomBuild now use $(OutDir) with the
  $(OutDir.Replace('\','/')) forward-slash property function (S449 C4129 root fix) [from S449]
- MOD vaml_viewer/cli.h — NEW pure cli_file_path() (S449) + cli_usage now DECLARES ".vaml and .xaml SOURCE
  files" (S450_1); cli_file_path doc states it serves .xaml positionals
- MOD vaml_viewer/compiled_open.h — NEW detail::is_vaml_or_xaml_source(path) (the DECLARED source-language
  split: .vaml/.xaml → recompile, .vbaml → binary direct) used by load_compiled_file (S450_1)
- MOD vaml_viewer/main.cpp — use cli_file_path(); prime register_builtin_elements/converters (S449) +
  compiled-dispatch comment now names .xaml (S450_1)
- NEW vaml_viewer/sample.xaml — the .xaml demo (a bona-fide XAML-shaped body: Window/StackPanel root by
  local name + x:Class/x:Name + xmlns:x)
- MOD vaml_viewer/drop_reopen.h — is_vaml_drop now ACCEPTS .vaml OR .xaml (case-insensitive): the DROP
  fix — a dropped .xaml reopens through load_file exactly like a .vaml (the drop seam had not been extended
  when the viewer learned to accept .xaml; this is what made "dropping files does not change the display")
- MOD test/unit/ViewerLoadFile_test.cpp — S450_1: a real-WPF-shaped .xaml golden loads via load_text + a
  real temp .xaml FILE via load_file (text path CI-locked)
- MOD test/unit/ViewerCompiledOpen_s293_test.cpp — S450_1: a .xaml path recompiles → VBamlReader
  (root_class + x:Name) (compiled path CI-locked)
- MOD test/unit/ViewerCli_test.cpp — S450_1: cli_usage declares .xaml + cli_file_path resolves a .xaml past
  --compiled
- MOD test/unit/ViewerDropPlan_test.cpp — S450_1: is_vaml_drop accepts .xaml + drop_choice picks a .xaml
  (the drop gate CI-locked)
- MOD docs/roadmap.md (S450_1 [DONE] entry + WHY-B report)
- MOD progress.md (S450_1 SURVIVOR note) — pending final status line
- MOD session-context.md (this file)

## build_status
ALL S450_1 BUILD + TEST GATES GREEN this session:
- Standalone viewer (the VS app) rebuilt clean → vaml_viewer\x64\Debug\vaml_viewer.exe (contains the S450_1
  .xaml drop fix + the S449 viewer fixes).
- CMake msvc-debug test build clean (native-PATH recipe, the verify_s449.cmd pattern):
  vaml-core-tests 1037/1037 PASS (incl. S450_1 .xaml load_text + load_file tests, S450_1 drop tests,
  S450_1 cli tests) + vaml-tools-tests 100/100 PASS (incl. S450_1 compiled .xaml recompile test). The one
  "ERROR" shown is S036_1 (a DELIBERATE swallow-demonstration test) — reported as not-failed / SUCCESS.
- The S450_1 .xaml goldens are CI-LOCKED on BOTH lanes (they compile into the shared vaml-core-tests +
  vaml-tools-tests targets; they will be verified on ubuntu-clang at the next full two-lane gate run).
- windows-clang-debug is NOT a lane (not built/tested/flagged).
- No commits (per the standing directive).

## next_step
S449 delivered + green (both CI lanes) + the viewer FULLY OPERATIONAL (4 modes). NEXT = S448 (the in-repo
app-shell ACCEPTANCE doc), per the driver's reversal now written from the VERIFIED viewer. Then Sprint 12
S480 (architecture.md FINAL from real v1 code, dep:S449). The compiled app-shell {Binding}/{Command}
wiring stays deferred exactly as S445/S447 recorded (the S286 compiled reader defers {Binding} by design —
NOT a readiness gap: the compiled shell RENDERS; only the interactive binding re-push is deferred on the
compiled path, and the text `--app` shell carries the interactive slice). The interactive app-shell's LIVE
reload stays recorded-forward (the S222_1 seam has no event registry; the app's real reload reloads the
plain S256 view).

## notes
- S449 NO-EXCEPTIONS finding (recorded, not silent): the standalone vaml_viewer was NOT fully operational
  before this token — the shipped exe was STALE (10/25, predates all Sprint-11 --app CLI) because the
  vcxproj C4129 (unescaped backslash in the VAML_APPSHELL_SIDECAR macro) BLOCKED any rebuild. The driver's
  "no exceptions" mandate REQUIRED fixing it (the plan's earlier keep-out-of-scope deferral was overridden).
  4 fixes delivered, all warning-clean on both CI lanes. This is the honest interpretation of "fully
  operational": a viewer whose only shippable build cannot compile is not operational.
- S449 viewer-fix scope (recorded): the compile-time escape corruption could NOT be repaired at runtime (the
  \x64/\v/\D/\M sequences are decoded by the C++ compiler before the program runs), so the vcxproj macro was
  the correct fix point — forward slashes via the MSBuild Replace() property function remove the escapes AND
  fopen accepts '/' on Windows. The sidecar now co-locates with the exe (both in $(OutDir)).
- S449 gate (A) build steps: 221 (vs 220 pre-fix) — the +1 is the ViewerCli_test target now compiling cli.h
  (the S449 CLI seam); warning-clean on both lanes confirms the new cli_file_path compiles portably.
- IMPORTANT work-flow guard (still governing): the Linux verify lane runs from the SHARED folder
  /mnt/hgfs/VamlPOC. NEVER run `git checkout -- .` (or any git checkout touching tracked files) inside the
  VM against that shared folder — it reverts uncommitted tracked edits on the SAME Windows
  E:\GitRepos\VamlPOC. New UNTRACKED files survive, but tracked-file edits are wiped.
- No commits made (per the standing directive unless 'commitit' is commanded).
