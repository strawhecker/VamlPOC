# VAML session-context (per-run state)

## last_command
implementit DONE — S441 + S442 delivered together (per the user decision "do both 442 and
441 at the same time"), GREEN on BOTH CI lanes from first principles. The S441 MainViewModel
(AppShellViewModel final : ObservableObject with PROPERTY_BINDABLE Text/Checked/SliderValue/
Dim/Status + RelayCommands wired to the S442 demo service via plain constructors) + the S442
AppShellDemoService (counter/dimmer/echo) + the S442 static composition seam
(build_app_shell_composition) all authored + tested (AppShellViewModel_test.cpp, 7 cases /
32 assertions) + verified on windows-msvc (verify_s441.cmd) and ubuntu-clang (/mnt/hgfs/VamlPOC
clang-debug). Sprint 11 second token closed.

## plan_digest
Sprint 11 (S441+S442, delivered together) — MainViewModel for the app-shell + in-repo demo
infra service + constructor wiring (dep:S440/S441):
- `AppShellViewModel final : ObservableObject` (S241) with PROPERTY_BINDABLE props the
  markup binds — Text (std::string) / Checked (bool) / SliderValue (double) / Dim (double)
  + Status (std::string, for MainView's `{Binding Status}`) — plus RunCommand + EchoCommand
  RelayCommands (S246/ICommand) wired to the S442 service via PLAIN CONSTRUCTORS (S250/S256,
  NO DI).
- RunCommand increments the injected service counter + folds the service dimmer into
  Dim/SliderValue + refreshes Status; CanExecute gated by can_run() (Checked==true AND
  counter<cap) with the S246 re-query contract (NotifyCanExecuteChanged). EchoCommand echoes
  Text through the service into Status (always executable). Commands exposed as ICommand&.
- S442 AppShellDemoService: plain constructor; counter/dimmer/echo (mirrors S256
  ViewerDemoService). S442 static composition seam AppShellComposition +
  build_app_shell_composition() — constructor-only, nothing registered/resolved.
- Test AppShellViewModel_test.cpp (7 cases / 32 assertions): props round-trip +
  real-change-only; commands execute + CanExecute + CanExecuteChanged fire (ICommand);
  wiring targets the injected service; constructor-only composition path.
- Build wiring: vaml-core-tests (+test + app_shell_vm.cpp), vaml_viewer.vcxproj (+cpp + 3
  headers), vaml_viewer-linux/CMakeLists.txt (+app_shell_vm.cpp), new build/verify_s441.cmd.
- BOTH CI LANES GREEN: windows-msvc (verify_s441.cmd msvc-debug warning-as-error clean +
  ctest 4/4) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, build warnings==0 + ctest 4/4).
- ADR-018: zero new locks. No MINIMAL REAL introduced/resolved. Sprint 11 second token closed.

## files_touched
- NEW vaml_viewer/app_shell_vm.h + vaml_viewer/app_shell_vm.cpp (S441: AppShellViewModel final
  ObservableObject + PROPERTY_BINDABLE Text/Checked/SliderValue/Dim/Status + RunCommand/EchoCommand
  RelayCommands wired to the injected service; S442: AppShellDemoService + AppShellComposition +
  build_app_shell_composition in the same TU)
- NEW vaml_viewer/demo_service.h (S442: AppShellDemoService + AppShellComposition seam)
- NEW test/unit/AppShellViewModel_test.cpp (7 cases: props round-trip + real-change-only; commands
  execute + CanExecute + CanExecuteChanged fire (ICommand); wiring targets the injected service;
  constructor-only composition path)
- MOD test/unit/CMakeLists.txt (vaml-core-tests +AppShellViewModel_test.cpp + ../../vaml_viewer/
  app_shell_vm.cpp)
- MOD vaml_viewer/vaml_viewer.vcxproj (+app_shell_vm.cpp ClCompile + app_shell.h/app_shell_vm.h/
  demo_service.h ClInclude)
- MOD vaml_viewer-linux/CMakeLists.txt (+../vaml_viewer/app_shell_vm.cpp)
- NEW build/verify_s441.cmd
- MOD session-context.md (this file)

## build_status
BOTH CI LANES GREEN from first principles at S441+S442:
- windows-msvc (msvc-debug, VS-native cmake, warning-as-error clean): ctest 4/4 PASS —
  vaml-core-tests incl. the 7 S441/S442 cases (32 assertions) green; vaml-tools, vaml-vulkan,
  vaml-presentation all green. verify_s441.cmd routes VS-native cmake/ctest first (the S440
  cygwin-cmake corruption fix).
- ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, clang 21): configure clean + build (211 steps,
  warnings==0) + ctest 4/4 PASS incl. the S441/S442 suite.
- windows-clang-debug is NOT a lane (not built/tested/flagged).

## next_step
S442 is now OPEN only in the [both apps, not CI] sense? No — S441 + S442 are both delivered
together + green. The rest of Sprint 11 (S443: App-shell ↔ MainViewModel wired via {Binding}/
{Command}; S444: vamlcc compile step; S445: launch→render/interactions; S446: hardening;
S447: in-repo E2E; S448: VAML Guide; S449: refactor gate) is NOT started. Next implementit
= S443.

## notes
- S440 root-cause note: the "plan.detail" that kept failing was NEVER an attribute/parse
  typo — it was the S089 value-conversion gap (enum names + Double). Only reading the
  printed plan.detail (a printf the prior session added) revealed it; this session read it
  as `cannot convert 'Horizontal' to Int32 (MalformedValue)` and fixed the layer.
- Build-infra note (recorded NO-GAP, not silent): the msvc-debug cache was re-created by
  VS-NATIVE cmake (not the cygwin cmake on PATH). Any Windows rebuild/CI must run from a
  vcvars64 shell with the VS-native cmake/ninja on PATH (verify_s440.cmd now does this).
- No commits made (per the standing directive unless 'commitit' is commanded).
