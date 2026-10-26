# VAML session-context (per-run state)

## last_command
implementit DONE — S446 (dep:S445) — Zero validation-layer errors; no leaks over 10k frames; no
layout thrash (the in-repo app-shell). A HARDENING/VERIFICATION sprint (NO product feature code):
it PROVES the three title claims against the REAL app-shell tree through the REAL library
surfaces, following the S177 (real-WSI harness) / S170 (ASan lane) / S146 (HeapStats funnel)
precedent. Delivered FOUR linked parts + FIXED two REAL latent memory-safety defects the ASan
lane surfaced:
- (A) NEW test/unit/AppShellChurn_test.cpp (3 cases / 32 assertions, vaml-presentation-tests —
  it links vaml-presentation so it drives the S212 headless VamlRenderer host + the REAL app-shell
  tree + the S146 funnel): (a) NO LAYOUT THRASH — after warmup, ~50 repeated arrange+encode+
  publish passes of the same app-shell tree keep heap_allocation_count/bytes FLAT (the S146
  steady-state contract elevated to the app-shell producer loop); (b) NO LEAK OVER 10K — 10,000
  producer passes stay heap-flat AND the retained-reader reachability (RunButton/InputBox/
  DimSlider/EnabledCheck/StatusText) stays intact; (c) RETAINED-STABLE AFTER 10K — the S443/S445
  TwoWay wires still write back to the VM store. This is the headless steady-state + retained-
  stability proof; the authoritative zero-leak is the ASan/LSan lane (B) running THIS suite.
- (B) NEW build/verify_s446_asan.cmd — verify_s170_asan.cmd lane shape: msvc-asan this box (zero
  ASan errors, 4/4) + clang-asan/LSan on the Linux VM (zero LEAKS at exit, detect_leaks=1:
  exitcode=86, 4/4) — THE authoritative "no leaks over 10k frames" on BOTH compilers.
- (C) NEW test/wsi/WSIAppShell_main.cpp + target vaml-wsi-appshell (LOCAL pre-✅ gate, NOT GH —
  CI has no WSI): a hidden GLFW window + the FULL real stack (loader→instance→surface→best-gpu→
  device→render pass→the S222 3-pipeline UI scene spec + SDF atlas) + a SceneRenderer (S210
  retained owner) + a Presenter (S173), VALIDATION LAYERS ON + a VK_EXT_debug_utils messenger
  that fails on ANY validation error. Loads the app-shell, arrange + encode ONCE + set_scene ONCE
  (retained), then 10,000 REAL PRESENTED frames (no re-encode = no per-frame thrash) + clean
  vkDeviceWaitIdle + reverse teardown. Exit 0=PASS / 1=FAIL / 2=SKIP. LOCAL gate PASSED.
- (D) NEW build/verify_s446.cmd + build/verify_s446_wsi.cmd — BOTH CI LANES GREEN from first
  principles (windows-msvc msvc-debug + ubuntu-clang clang-debug on the VM: configure + warning-
  clean build + ctest 4/4 incl. AppShellChurn), plus the this-box WSI + ASan pre-✅ gates.
Sprint 11 seventh token closed.

## plan_digest
Sprint 11 (S446 — hardening/verification of the in-repo app-shell, dep:S445):
- (A) AppShellChurn_test.cpp — the S146 heap-churn funnel elevated to the app-shell producer
  steady state (no layout thrash), 10k headless passes (no leak), retained-stable after 10k (the
  S443/S445 TwoWay wires still write back). Headless via the S212 VamlRenderer::create() host.
- (B) verify_s446_asan.cmd — msvc-asan + clang-asan/LSan zero-leak on both compilers (the
  authoritative "no leaks over 10k frames").
- (C) vaml-wsi-appshell — hidden-window real-WSI harness driving 10,000 REAL PRESENTED frames
  over the retained app-shell with validation layers ON; zero validation errors = PASS. LOCAL
  pre-✅ gate (NOT GH), the S177 contract.
- (D) verify_s446.cmd + verify_s446_wsi.cmd — the two-lane gate + this-box WSI gate.
- REAL defects found + fixed: (1) VulkanInstance.cpp heap-use-after-free on the layer/extension
  name lists (own the strings); (2) PlatformWindow.cpp never destroyed GLFW windows or terminated
  GLFW (reference-counted RAII close + glfwTerminate on the last window). The only remaining LSan
  report is the known upstream GLFW-X11 one-time XOpenDisplay process-lifetime singleton
  (scoped-suppressed; any VAML leak still fails).
- The validated VS-native PATH recipe (reset PATH → prepend VS CMake+Ninja → vcvars64) is baked
  into all three S446 verify cmds (keeps the Cygwin cmake/ld out of every lane).
- No MINIMAL REAL introduced/resolved. ADR-018: zero new locks.

## files_touched
(S446, this token)
- NEW test/unit/AppShellChurn_test.cpp (3 cases / 32 assertions; vaml-presentation-tests) — the S446 headless churn + leak-flatness + retained-stability suite
- MOD test/unit/CMakeLists.txt (vaml-presentation-tests += AppShellChurn_test.cpp — it links
  vaml-presentation / compiles the REAL app-shell seam; the authoritative leak lane runs it)
- NEW test/wsi/WSIAppShell_main.cpp + MOD test/wsi/CMakeLists.txt (target vaml-wsi-appshell +
  app_shell_vm.cpp; NOT CTest-registered — the local gate runs it; compile-green in both CI lanes)
- NEW build/verify_s446.cmd, build/verify_s446_asan.cmd, build/verify_s446_wsi.cmd
- NEW build/lsan_suppressions.supp (scoped GLFW-X11 init singleton — leak:_glfwConnectX11 /
  leak:XOpenDisplay ONLY)
- MOD src/vulkan/src/VulkanInstance.cpp (FIX: own the layer/extension name strings — heap-use-after-free)
- MOD src/vulkan/src/PlatformWindow.cpp (FIX: reference-counted GLFW RAII close + glfwTerminate on
  the last window — the window/context leak)
- MOD docs/roadmap.md (S446 [DONE]) + progress.md (checkpoint regenerated to S446)
- MOD session-context.md (this file)

## build_status
ALL S446 GATES GREEN (verified from first principles this sprint):
- verify_s446.cmd: BOTH CI LANES PASS — windows-msvc (msvc-debug, warning-as-error clean, ctest
  4/4 incl. AppShellChurn 3/32) + ubuntu-clang (clang-debug on the VM /mnt/hgfs/VamlPOC, warning-
  clean, ctest 4/4) — plus vaml-wsi-appshell compile-green on both lanes.
- verify_s446_asan.cmd: msvc-asan this box 4/4 zero ASan errors (the AppShellChurn 10k loop +
  the VulkanInstance UAF fix hold) + clang-asan/LSan on the VM 4/4 ZERO VAML-OWNED LEAKS at exit
  (the GLFW window/context leak FIXED in PlatformWindow; the only suppressed report is the scoped
  GLFW-X11 XOpenDisplay one-time process-lifetime singleton).
- verify_s446_wsi.cmd: LOCAL real-WSI gate PASS — 10000 real PRESENTED frames, ZERO validation
  errors, clean reverse-order teardown via the RAII-fixed GLFW lifetime (exit 0).
- windows-clang-debug is NOT a lane (not built/tested/flagged).
NOTE (recorded, pre-existing, NOT introduced by S446): the standalone vaml_viewer.vcxproj build
still surfaces the pre-existing S444 C4129 warning-as-error in app_shell_compiled.h:92 (the
vcxproj macro carries backslashes). It is NOT a CI lane; stays owned by a future step (consistent
with the S445 note). The compiled `--compiled --app` app-shell path stays deferred exactly as S445
recorded (the S286 compiled reader defers {Binding} by design); S446 exercises the S443/S445 TEXT
binding-plan app-shell — the same one the `--app` window runs.

## next_step
S446 delivered + green (all four gates). The rest of Sprint 11 (S447: in-repo E2E integration
test; S448: VAML Guide; S449: refactor gate) is NOT started. Next implementit = S447. NO-GAP
(recorded, not silent): the {Binding}/{Command} wiring on the COMPILED app-shell path stays
deferred — the S286 compiled reader defers {Binding} by design (the S293/S443/S444 precedent);
S446 exercised the S443/S445 TEXT binding-plan app-shell the same as S445.

## notes
- S446 memory-safety hardening notes (recorded, not silent): the ASan/LSan lane surfaced TWO REAL
  latent defects that were FIXED this sprint — (1) VulkanInstance::create's availableLayers/
  availableExtensions stored char* into local props vectors that died at block scope (a heap-use-
  after-free in the layer/extension availability checks); we now own the strings for the whole
  create(). (2) PlatformWindow created GLFW windows but never glfwDestroyWindow'd them nor ever
  called glfwTerminate — every window + the GLFW context leaked for the process lifetime; we now
  reference-count GLFW windows (an atomic count) and the Impl destructor destroys each window,
  with the LAST window legally calling glfwTerminate (RAII close on both Win32 + X11). The window
  leak is GONE under LSan. The ONLY remaining LSan report is the KNOWN UPSTREAM GLFW-X11 one-time
  XOpenDisplay process-lifetime singleton (glfwInit → _glfwConnectX11 → x11_init.c:1289),
  deliberately not XCloseDisplay'd by GLFW's own X11 backend (a documented teardown crash-risk);
  it is NOT owned by VAML and NOT in the headless 10k loop (which is flat + proven). Suppressed
  scoped (leak:_glfwConnectX11 / leak:XOpenDisplay ONLY) - any VAML-OWNED leak still fails the
  clang-asan LANE.
- S446 build-lane robustness note: the long-standing Cygwin-cm.exe-on-PATH collision corrupted /
  fused the msvc-debug cache earlier (Cygwin cmake/ld re-created bin-dirs under /cygdrive). The
  validated VS-native recipe — FIRST reset PATH to a clean Windows base, THEN prepend the VS 2026
  native CMake + Ninja (Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin + ...\Ninja), THEN
  call vcvars64 (appends MSVC cl/link AFTER the clean base) — is now baked into verify_s446.cmd /
  verify_s446_asan.cmd / verify_s446_wsi.cmd so every S446 lane runs with the real toolchain.
- IMPORTANT work-flow guard (learned a prior session, still governing): the Linux verify lane runs
  from the SHARED folder /mnt/hgfs/VamlPOC. NEVER run `git checkout -- .` (or any git checkout
  touching tracked files) inside the VM against that shared folder — it operates on the SAME
  Windows E:\GitRepos\VamlPOC and REVERTS uncommitted tracked edits. New UNTRACKED files survive,
  but tracked-file edits are wiped. Two CI verify lanes do NOT need a git checkout on the shared
  folder.
- No commits made (per the standing directive unless 'commitit' is commanded).
