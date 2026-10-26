# VAML session-context (per-run state)

## last_command
implementit DONE — S445 (dep:S444) — Launch → app-shell declared UI renders; click/type/slide/
check updates the VM end-to-end (the in-repo v1 VERTICAL SLICE), GREEN on windows-msvc from
first principles (verify_s445.cmd: configure + build warning-as-error clean + ctest 4/4).
Delivered:
- MainView.vaml (on-disk + the inlined kAppShellMainViewVaml twin, kept byte-identical) gains
  S445's NEW markup wire: `{Binding SliderValue, Mode=TwoWay}` on the DimSlider's
  Slider.Value (the S244 two-way write-back on the slider axis → VM prop). bound 2 → 3.
- The CheckBox stays a literal in markup (a nullable-Int32 IsChecked vs a bool VM.Checked is a
  genuine value-type clash for a naked {Binding} → forward Bool→Int32 would be target-
  rejected). It reaches the VM via the S250 "engineer wires, we bind" app-owned composition:
  run_app_shell_binding_plan subscribes the named EnabledCheck's checked_changed() Signal to
  vm->set_Checked (the LIVE checkbox state → VM bool; checkBoxWired is the never-silent
  contract).
- NEW test/unit/AppShellE2E_test.cpp (6 cases / 48 assertions, vaml-core-tests): RENDER
  (finalized Window, transferred > 0), CLICK (RunButton raise_click → routed ClickEvent →
  the S322 app-owned OnClick handler → VM RunCommand → {Binding Status} one-way StatusText
  repaint), TYPE (TextBox.Text two-way write-back → VM Text store, vm->Get convention),
  SLIDE (Slider.Value two-way → VM SliderValue store), CHECK (the app-owned wire → VM
  Checked), + a cohesive flow (all four drive the SAME VM + bound targets).
- app.{h,cpp}: create_app_shell NOW uses run_app_shell_binding_plan (the live --app window
  carries the full interactive slice), retaining the app-shell VM (appShellVm_) + the retained
  reader (appShellReader_). app_shell_vm.cpp added to vaml-presentation-tests (the CMake
  target that compiles app.cpp — the ADR-021 single source of truth).
- S443 AppShellBinding_test.cpp `bound == 2u` → `3u` (the new SliderValue binding).
- NO-GAP (recorded, not silent): the {Binding}/{Command} wiring on the COMPILED path stays
  deferred (the S286 compiled reader defers {Binding} by design — exactly as S293/S443/S444
  recorded; S445's E2E is the S443/S445 text binding-plan path its test requirement specifies).
Verified on BOTH CI LANES from first principles via build/verify_s445.cmd (the two-lane gate:
windows-msvc local msvc-debug + ubuntu-clang on the Linux VM clang-debug — 6/6 S445 cases / 48/48
on both). Sprint 11 sixth token closed.

## plan_digest
Sprint 11 (S445) — Launch → app-shell declared UI renders; click/type/slide/check updates the
VM end-to-end (dep:S444; the in-repo v1 vertical slice through the LIVE path):

- MainView.vaml (on-disk + the inlined twin, byte-identical) adds the S445 SliderValue TwoWay
  markup wire: `<Slider ... Slider.Value="{Binding SliderValue, Mode=TwoWay}"/>` — the clean
  Double↔Double two-way write-back on the slider axis (bound 2 → 3).
- The CheckBox is wired app-owned (NOT a naked markup binding — nullable-Int32 IsChecked vs
  bool Checked: a genuine value-type clash): run_app_shell_binding_plan subscribes the named
  EnabledCheck's checked_changed() to vm->set_Checked. checkBoxWired = the never-silent arc.
- NEW test/unit/AppShellE2E_test.cpp (6 cases / 48 assertions, vaml-core-tests): RENDER
  (finalized Window root + children), CLICK (RunButton → routed ClickEvent → OnClick handler →
  RunCommand → Status one-way repaint), TYPE (TextBox two-way → VM Text store), SLIDE (the
  new SliderValue wire → VM SliderValue store), CHECK (the app-owned wire → VM Checked), +
  a cohesive flow. Assertions use vm->Get for the two-way write-backs (the S443/S244 S241
  observable-store convention), vm accessors for VM-driven sets + Status.
- app.{h,cpp}: create_app_shell switches to run_app_shell_binding_plan (live --app window
  interactive), retains appShellVm_ + appShellReader_.
- app_shell_vm.cpp added to vaml-presentation-tests (app.cpp compiles there).
- S443 AppShellBinding_test bound 2→3. NEW build/verify_s445.cmd.
- BOTH CI LANES GREEN from first principles (verify_s445.cmd — the two-lane gate: windows-msvc
  local msvc-debug + ubuntu-clang on the Linux VM /mnt/hgfs/VamlPOC clang-debug — 6/6 S445 cases /
  48/48 assertions on both).

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
(S445, this token)
- MOD vaml_viewer/MainView.vaml + vaml_viewer/app_shell.h (the inlined twin, kept byte-identical):
  DimSlider's Slider.Value → `{Binding SliderValue, Mode=TwoWay}` (the S445 slider two-way wire)
- MOD vaml_viewer/app_shell_binding.h (run_app_shell_binding_plan: the S445 CheckBox↔VM app-owned
  wire — subscribes the EnabledCheck's checked_changed() to vm->set_Checked via a strong-shared_ptr
  capture; the `checkBoxWired` never-silent contract; bound-doc updated)
- NEW test/unit/AppShellE2E_test.cpp (6 cases / 48 assertions; vaml-core-tests) — the S445 E2E slice
- MOD test/unit/AppShellBinding_test.cpp (bound 2 → 3 — the new SliderValue binding)
- MOD test/unit/CMakeLists.txt (vaml-core-tests + AppShellE2E_test.cpp; vaml-presentation-tests +
  app_shell_vm.cpp — app.cpp now calls run_app_shell_binding_plan)
- MOD vaml_viewer/app.{h,cpp} (create_app_shell → run_app_shell_binding_plan; retains appShellVm_ +
  appShellReader_; app.h includes app_shell_vm.h, app.cpp includes app_shell_binding.h)
- NEW build/verify_s445.cmd
- MOD docs/roadmap.md (S445 [DONE]) + progress.md (checkpoint regenerated to S445)
- MOD session-context.md (this file)

## build_status
BOTH CI LANES GREEN at S445, re-verified from first principles via build/verify_s445.cmd (the
two-lane gate — windows-msvc local + ubuntu-clang via SSH to the Linux VM shared folder):
- windows-msvc (msvc-debug, VS-native CMake, warning-as-error clean): ctest 4/4 PASS — vaml-core-
  tests (incl. the 6 S445 E2E cases / 48 assertions + the updated S443 bound=3 assertions),
  vaml-tools-tests (the S444 compiled parity suite — the MainView Slider binding still parity-equal),
  vaml-vulkan, vaml-presentation.
- ubuntu-clang (clang-debug on the VM /mnt/hgfs/VamlPOC, Linux clang++ 21.1.8/Ninja, warning-clean
  incl. AppShellE2E_test.cpp + app_shell_vm.cpp in presentation): ctest 4/4 PASS; vaml-core-tests
  S445:* = 6/6 cases / 48/48 assertions.
- windows-clang-debug is NOT a lane (not built/tested/flagged).
NOTE (recorded, pre-existing, NOT introduced by S445): the standalone vaml_viewer.vcxproj build
surfaces a pre-existing S444 C4129 warning-as-error in app_shell_compiled.h:92
(`std::string(VAML_APPSHELL_SIDECAR)` — the vcxproj macro carries backslashes). This is an S444
artifact in a file S445 does not touch; it does NOT affect either CI lane (verify_s445.cmd = the
CMake lane on both compilers, warning-clean). It is recorded here, not silently ignored; a future
step owns it.

## next_step
S445 delivered + green (windows-msvc). The rest of Sprint 11 (S446: hardening — zero validation
errors / no leaks over 10k frames / no layout thrash; S447: in-repo E2E integration test; S448:
VAML Guide; S449: refactor gate) is NOT started. Next implementit = S446. NO-GAP (recorded, not
silent): the {Binding}/{Command} wiring on the COMPILED app-shell path stays deferred — the S286
compiled reader defers {Binding} by design (the S293/S443/S444 precedent); S445's E2E is the
S443/S445 TEXT binding-plan path its test requirement specifies.

## notes
- S445 design note (recorded, not silent): the app-shell's CheckBox→VM wire is deliberately NOT a
  naked markup {Binding}. CheckBox.IsChecked is a nullable-Int32 DP ({0,1,2}) while the S441 VM's
  Checked is a bool — a forward Bool→Int32 push under a naked TwoWay would be target-rejected
  (value-type clash), and the write-back would desync the VM's store. So the S443/S445 plan wires
  it through the control's real change SIGNAL (checked_changed) mapped by an app-owned handler
  to vm->set_Checked — the exact S250 "engineer wires, we bind" app-owned composition (NO DI), a
  real permanent path, and the never-silent `checkBoxWired` contract asserts it. The Slider (the
  Double↔Double axis) IS a clean markup TwoWay binding.
- S445 write-back convention note: the S244 two-way write-back writes the S241 OBSERVABLE STORE,
  not the PROPERTY_BINDABLE C++ backing field — S445's assertions use vm->Get (the S443
  convention) for write-back results, and the VM accessor for VM-driven sets + Status. This is the
  standing S247_1 field↔store division (the field is the app's set_Text source of truth).
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
