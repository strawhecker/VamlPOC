# VAML session-context (per-run state)

## last_command
implementit DONE — S443 (app-shell ↔ MainViewModel wired via {Binding}+{Command}), GREEN on
BOTH CI lanes from first principles. The authored MainView.vaml (S440) is now driven through
the S252/S253/S322 reader path via the new seam vaml_viewer/app_shell_binding.h
(run_app_shell_binding_plan), which reuses/extends the S256/S329_1 plan pattern: S442
composition (plain ctors) → retained VamlReader → set_data_context(vm) + register_event("Click")
+ bind_class_handler("OnClick") + register_command("RunCommand"/"EchoCommand") → Load the
authored MainView → BindExtensions ({Binding Status} OneWay + {Binding Text, Mode=TwoWay}) +
BindEvents (Click) + finalize_visual_tree. Retained reader/VM alive (S243/S244 wires armed).
AppShellBinding_test.cpp (7 cases / 37 assertions): one-way first-transfer; VM set re-pushes
target DP; TextBox.Text two-way write-back (S244); {Command} resolves to VM RelayCommand +
Click via BindEvents runs handler→VM→binding repaint; malformed binding→structured never-silent
error. MainView.vaml + app_shell.h kAppShellMainViewVaml updated (RunButton Click + Command,
TextBox two-way). Verified on windows-msvc (verify_s443.cmd) + ubuntu-clang (/mnt/hgfs/VamlPOC
clang-debug). Sprint 11 fourth token closed.

## plan_digest
Sprint 11 (S443) — app-shell ↔ MainViewModel wired via {Binding} + {Command} (dep:S442):
- NEW vaml_viewer/app_shell_binding.h — run_app_shell_binding_plan: the S252/S253/S322
  reader-path wiring of the authored MainView.vaml to the S441 AppShellViewModel, reusing/
  extending the S256/S329_1 plan pattern. S442 composition (plain ctors) → retained
  VamlReader → set_data_context(vm) + register_event("Click") + bind_class_handler("OnClick")
  + register_command("RunCommand"/"EchoCommand") → Load → BindExtensions ({Binding Status}
  OneWay + {Binding Text, Mode=TwoWay}) + BindEvents (Click) + finalize_visual_tree. Retained
  reader/VM alive (S243/S244 wires stay armed). Plan result carries bound/wired/transferred +
  named_element() accessors.
- MainView.vaml + app_shell.h kAppShellMainViewVaml updated: RunButton gets Click="OnClick" +
  Command="{Command RunCommand}"; InputBox TextBox.Text becomes {Binding Text, Mode=TwoWay}.
  app_shell.h run_app_shell_plan registers "Click" so the S440 load path still parses.
- Test AppShellBinding_test.cpp (7 cases / 37 assertions): (1) plan wires/loads/binds/wires/
  finalizes + x:Name lookup; (2) {Binding Status} one-way first-transfer; (3) set_Status
  re-pushes the target DP (S243 live); (4) TextBox.Text two-way write-back (S244); (5)
  {Command RunCommand} resolves to the VM RelayCommand; (6) Click on RunButton → handler →
  VM RunCommand → Status repaint; (7) malformed {Binding} → structured never-silent error.
- Build wiring: vaml-core-tests (+AppShellBinding_test.cpp), vaml_viewer.vcxproj
  (+app_shell_binding.h), new build/verify_s443.cmd.
- Tests-fixed refs to the honest contracts: the S244 two-way write-back writes the VM's
  observable Value store (base Set), so the test seeds via set_Text then reads Get()/the
  TextBox target; the RunCommand gate is the VM's OWN Checked (the markup CheckBox IsChecked
  is a separate shell object, not linked to the VM) — the test arms set_Checked(true).
- BOTH CI LANES GREEN: windows-msvc (verify_s443.cmd msvc-debug warning-as-error clean +
  ctest 4/4; S443 7/7 + S440 4/4) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, build
  warnings==0 + ctest 4/4; S443 7/7).
- ADR-018: zero new locks. No MINIMAL REAL introduced/resolved. Sprint 11 fourth token closed.

## files_touched
(S443, this token)
- NEW vaml_viewer/app_shell_binding.h (S443: run_app_shell_binding_plan — the S252/S253/S322
  reader-path wiring of MainView.vaml to the S441 VM, reusing the S256/S329_1 plan pattern;
  retained reader/VM alive; AppShellBindingPlanResult + named_element accessors)
- MOD vaml_viewer/MainView.vaml (RunButton Click="OnClick" + Command="{Command RunCommand}";
  InputBox TextBox.Text="{Binding Text, Mode=TwoWay}") + the mirror inline kAppShellMainViewVaml
  in vaml_viewer/app_shell.h (unchanged on-disk twin parity) + app_shell.h run_app_shell_plan
  now register_event("Click") so the S440 load path still parses
- NEW test/unit/AppShellBinding_test.cpp (7 cases / 37 assertions; two-way write-back/S443 wiring
  tests)
- MOD test/unit/CMakeLists.txt (vaml-core-tests +AppShellBinding_test.cpp)
- MOD vaml_viewer/vaml_viewer.vcxproj (+app_shell_binding.h ClInclude)
- NEW build/verify_s443.cmd
- MOD session-context.md (this file)

## build_status
BOTH CI LANES GREEN from first principles at S443:
- windows-msvc (msvc-debug, VS-native cmake, warning-as-error clean): ctest 4/4 PASS —
  vaml-core-tests incl. the 7 S443 cases (37 assertions) green; vaml-tools, vaml-vulkan,
  vaml-presentation green (incl. S440 4/4 — the MainView change still loads/finalizes).
  verify_s443.cmd routes VS-native cmake/ctest first (the S440 cygwin-cmake corruption fix).
- ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, clang 21): configure clean + build (212 steps,
  warnings==0) + ctest 4/4 PASS incl. the S443 suite (7/7).
- windows-clang-debug is NOT a lane (not built/tested/flagged).

## next_step
S443 delivered + green. The rest of Sprint 11 (S444: vamlcc compile step; S445: launch→render/
interactions; S446: hardening; S447: in-repo E2E; S448: VAML Guide; S449: refactor gate) is NOT
started. Next implementit = S444.

## notes
- S440 root-cause note: the "plan.detail" that kept failing was NEVER an attribute/parse
  typo — it was the S089 value-conversion gap (enum names + Double). Only reading the
  printed plan.detail (a printf the prior session added) revealed it; this session read it
  as `cannot convert 'Horizontal' to Int32 (MalformedValue)` and fixed the layer.
- Build-infra note (recorded NO-GAP, not silent): the msvc-debug cache was re-created by
  VS-NATIVE cmake (not the cygwin cmake on PATH). Any Windows rebuild/CI must run from a
  vcvars64 shell with the VS-native cmake/ninja on PATH (verify_s440.cmd now does this).
- No commits made (per the standing directive unless 'commitit' is commanded).
