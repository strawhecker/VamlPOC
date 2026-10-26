# VAML session-context (per-run state)

## last_command
implementit DONE — S444 (vamlcc compile step for the app-shell MainView.vaml→.vbaml, the
RELEASE path), GREEN on BOTH CI lanes from first principles. Delivered the S444 "common
folder" directive + the S443-dep release compile step:
- The reusable `vaml_compile_vaml()` function MOVED out of src/tools/CMakeLists.txt into the
  COMMON `cmake/vaml-compile.cmake` (ONE definition, NO duplication), INCLUDED by BOTH the
  top-level library build (src/tools/CMakeLists.txt, ~/src vamlcc_app_shell target) AND the
  standalone vaml_viewer-linux/CMakeLists.txt (which previously could NOT call it — the whole
  point of the common-folder directive) — each wires MainView.vaml → a .vbaml SIDE-CAR + an
  `--embed` C++ twin (S287 fn, ADR-021).
- vaml_viewer.vcxproj: the native MSBuild CustomBuild analogue on MainView.vaml (prebuilt
  msvc-debug vamlcc.exe → sidecar + embed) + the VAML_APPSHELL_SIDECAR macro.
- NEW shared seam vaml_viewer/app_shell_compiled.h (compiled by BOTH apps + the test):
  load_compiled_app_shell_file (build sidecar via the S293 load_compiled_file seam — the
  file/`--compiled --app` path), register_compiled_app_shell + load_compiled_app_shell_embedded
  (the S289 ResourcePack + InitializeComponentFrom compiled-only release entry — the
  single-exe path), + compiled_app_shell_sidecar_path; all headless + pure.
- `--compiled --app` RELEASE launch wired into BOTH main.cpp + ViewerApp::
  create_compiled_app_shell (app.{h,cpp}): the compiled-release launch opens the build sidecar,
  while plain `--app` keeps the S440/S443 TEXT path.
- S7 VBamlReader::converterNameFor GAINED the missing `ValueType::Double` arm (a real S288
  parity gap closed — the compiled path now converts Double literals exactly as S2 does;
  MainView's FontSize/Slider.* are Double).
- compiled_open.{h} load_compiled_bytes/load_compiled_file gained an OPTIONAL events param
  (S444 passes {"Click"} — the S288/S443 reader-level event contract; S293 callers unchanged).
- AppShellCompile_test.cpp (6 cases / 33 assertions, vaml-tools-tests): .vbaml↔text parity
  (S288), on-disk MainView.vaml compiles + decodes to the shell, embed == sidecar (S287),
  sidecar loads into the live Window (S285/S440), embedded InitializeComponentFrom (S289),
  malformed → structured never-silent failure. Compiles the ACTUAL generated embed source.
- NO-GAP (recorded, not silent): the S443 {Binding}/{Command} wiring on the COMPILED path is
  deferred to S445 (the S286 compiled reader defers {Binding} — exactly as S293/S443 recorded).
Verified on windows-msvc (verify_s444.cmd) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug).
Sprint 11 fifth token closed.

## plan_digest
Sprint 11 (S444) — Compile MainView.vaml → .vbaml via vamlcc in CMake custom command
(dep:S443, the RELEASE path):
- NEW cmake/vaml-compile.cmake — the SHARED vaml_compile_vaml() (the S444 "common folder"
  directive): moved out of src/tools/CMakeLists.txt, included by the top-level library build
  AND the standalone vaml_viewer-linux project (which previously could not call it). Wires a
  per-.vaml add_custom_command → .vbaml sidecar (+ optional --embed C++ twin); VAMLCC_EXE
  default = $<TARGET_FILE:vamlcc>, overridable for the standalone prebuilt vamlcc.
- src/tools/CMakeLists.txt: include() the shared module + the S444 vamlcc_app_shell target
  compiling vaml_viewer/MainView.vaml → sidecar (src/tools binary dir) + embed twin; exports
  VAML_S444_* globals to the test target.
- vaml_viewer-linux/CMakeLists.txt: include() the shared module + vamlcc_app_shell_linux on
  the prebuilt clang-debug vamlcc + VAML_APPSHELL_SIDECAR.
- vaml_viewer.vcxproj: CustomBuild on MainView.vaml (prebuilt vamlcc.exe → sidecar + embed) +
  VAML_APPSHELL_SIDECAR (both configs) + app_shell_compiled.h ClInclude.
- NEW vaml_viewer/app_shell_compiled.h (shared seam, compiled by both apps + the test):
  load_compiled_app_shell_file / register_compiled_app_shell / load_compiled_app_shell_embedded
  / compiled_app_shell_sidecar_path + AppShellCompiledPlanResult (ok/root/retained reader/
  transferred/detail).
- app.{h,cpp}: NEW ViewerApp::create_compiled_app_shell (the --compiled --app RELEASE launch;
  adopts the compiled root + retains the S7 reader).
- Both main.cpp: the --compiled --app branch (sidecar → create_compiled_app_shell).
- VBamlReader.cpp: converterNameFor += ValueType::Double (the S288 parity gap).
- compiled_open.{h}: load_compiled_bytes/load_compiled_file += optional events param.
- Test AppShellCompile_test.cpp (6 cases / 33 assertions, vaml-tools-tests): (1) .vbaml↔text
  parity (S288 comparator); (2) on-disk vamlcc input compiles + decodes to the shell; (3) embed
  == sidecar (S287); (4) sidecar loads into the live Window tree (S285/S440); (5) embedded
  span InitializeComponentFrom (S289); (6) malformed bytes → typed never-silent failure.
- BOTH CI LANES GREEN: windows-msvc (verify_s444.cmd msvc-debug warning-as-error clean +
  ctest 4/4; S444 6/6 33/33 + prior suites) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug,
  build warnings==0 + ctest 4/4; S444 6/6 33/33). The standalone vaml_viewer-linux build
  produced its S444 sidecar + embed via the SHARED module.
- ADR-018: zero new locks. No MINIMAL REAL introduced. Sprint 11 fifth token closed.

## files_touched
(S444, this token)
- NEW cmake/vaml-compile.cmake (the SHARED vaml_compile_vaml() — the S444 "common folder"
  directive, ONE definition for both build systems)
- MOD src/tools/CMakeLists.txt (include the shared module — removed the inlined
  vaml_compile_vaml() — + the S444 vamlcc_app_shell target: MainView.vaml → sidecar + embed)
- MOD vaml_viewer-linux/CMakeLists.txt (include the shared module + vamlcc_app_shell_linux on
  the prebuilt vamlcc + VAML_APPSHELL_SIDECAR)
- MOD vaml_viewer/vaml_viewer.vcxproj (CustomBuild on MainView.vaml → sidecar + embed +
  VAML_APPSHELL_SIDECAR both configs + app_shell_compiled.h ClInclude)
- NEW vaml_viewer/app_shell_compiled.h (the S444 compiled app-shell seam)
- MOD vaml_viewer/app.{h,cpp} (ViewerApp::create_compiled_app_shell — the --compiled --app
  RELEASE launch)
- MOD vaml_viewer/main.cpp + vaml_viewer-linux/src/main.cpp (the --compiled --app branch)
- MOD src/tools/src/VBamlReader.cpp (converterNameFor += Double arm — the S288 parity gap)
- MOD vaml_viewer/compiled_open.h (load_compiled_bytes/load_compiled_file += optional events)
- NEW test/unit/AppShellCompile_test.cpp (6 cases / 33 assertions; vaml-tools-tests)
- MOD test/unit/CMakeLists.txt (vaml-tools-tests + AppShellCompile_test.cpp + the S444 embed)
- NEW build/verify_s444.cmd
- MOD docs/roadmap.md (S444 [DONE]) + progress.md (checkpoint regenerated to S444)
- MOD session-context.md (this file)

## build_status
BOTH CI LANES GREEN from first principles at S444:
- windows-msvc (msvc-debug, VS-native cmake, warning-as-error clean): ctest 4/4 PASS —
  vaml-tools-tests incl. the 6 S444 cases (33 assertions) green; vaml-core, vaml-vulkan,
  vaml-presentation green (incl. S443 7/7 + S440 4/4 — the MainView change still loads/finalizes).
  verify_s444.cmd routes VS-native cmake/ctest first.
- ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, clang 21): configure clean + build
  warnings==0 + ctest 4/4 PASS incl. the S444 suite (6/6, 33/33).
- The standalone vaml_viewer-linux build configured + produced its S444 sidecar/embed via the
  SHARED module (the pre-existing GCC-15 Element.h:602 `-Werror=comment` header warning — an
  unrelated pre-existing issue, NOT S444, and NOT a CI lane).
- windows-clang-debug is NOT a lane (not built/tested/flagged).

## next_step
S444 delivered + green. The rest of Sprint 11 (S445: launch → app-shell UI renders +
click/type/slide → VM-driven; S446: hardening; S447: in-repo E2E; S448: VAML Guide; S449:
refactor gate) is NOT started. Next implementit = S445. NO-GAP: the {Binding}/{Command} wiring
on the COMPILED path (S286 reader defers {Binding}) is S445's explicit slice.

## notes
- S444 root-cause note (a REAL bug found + fixed, NOT silent): the S7 VBamlReader lacked a
  `ValueType::Double` arm in converterNameFor, so the COMPILED path threw
  "no value converter registered for the property's bound type" on MainView's Double literal
  properties (FontSize/Slider.*) even though the S2 text path handled them. The S288 parity
  contract requires the compiled path to convert literals EXACTLY as the text path does —
  added the Double arm (S089 built-in converters already register a "Double" converter).
- IMPORTANT work-flow guard (learned this session): the Linux verify lane runs from the SHARED
  folder /mnt/hgfs/VamlPOC. NEVER run `git checkout -- .` (or any git checkout that touches
  tracked files) inside the VM against that shared folder — it operates on the SAME Windows
  E:\GitRepos\VamlPOC and REVERTS uncommitted tracked edits. New UNTRACKED files survive, but
  tracked-file edits are wiped. Two CI verify lanes do NOT need a git checkout on the shared
  folder.
- No commits made (per the standing directive unless 'commitit' is commanded).
