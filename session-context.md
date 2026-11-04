# session-context.md

last_command: implementit (Sprint 11o — S596 REFACTOR GATE + SURVIVOR; this CLOSES Sprint 11o)
plan_digest: S596 (dep:S595a) — the REFACTOR GATE(11o) + SURVIVOR: full build + ctest BOTH CI lanes green
from first principles (build/verify_s596.cmd msvc-debug + ubuntu-clang clang-debug on /mnt/hgfs/VamlPOC);
render_surface_check.py -> OK (60 classifier arms / 67 catalog rows) on BOTH lanes; grep-audit PASS —
ZERO STUB + the S590 [MINIMAL REAL] composite-alpha sentinel is GONE (RESOLVED at S591) + the global hard
clear (app.cpp clearR/G/B/A host constant) is REMOVED; SURVIVOR block consolidated in progress.md
(`## SPRINT 11o — SURVIVOR`); roadmap S596 [DONE]. ZERO new locks (ADR-018). **CHECKPOINT ADVANCED:
last = Sprint 11o, next = Sprint 11h (S520 — IMPORT/INJECT seam).**

## files_touched — Sprint 11o (S590..S595a + S596, this incarnation)
- src/vulkan/include/vaml/vulkan/Swapchain.h (MOD): S590 probe seams + compositeAlpha config field + the
  [MINIMAL REAL] sentinel; S591 SwapchainMode + negotiate_composite_alpha + mode field + accessors +
  RESOLVED the S590 sentinel.
- src/vulkan/src/Swapchain.cpp (MOD): the two probe seams + negotiate_composite_alpha + negotiate fills
  mode/compositeAlpha + rebuild consumes compositeAlpha.
- src/vulkan/include/vaml/vulkan/PlatformWindow.h + src/vulkan/src/PlatformWindow.cpp (MOD): S594
  window_transparency_hints + WindowTransparencyHints + Config::transparentFramebuffer + the GATED create
  application + the VAML_VULKAN_EXPORT guard.
- src/core/include/vaml/core/UIElement.h + src/core/src/UIElement.cpp (MOD): S595a IsHitTestVisible Bool DP
  + accessors + the HitTestAt skip; src/core/src/ElementDpFidelity.cpp (MOD): removed the duplicate Int32
  IsHitTestVisible guard.
- src/presentation/include/vaml/presentation/VamlRenderer.h (MOD): the clear::ClearValue + decide_clear
  seam + superseded clear_ comment; src/presentation/src/clear.cpp (NEW); src/presentation/src/VamlRenderer.cpp
  (MOD): record_draw derives the clear + the S593 base-arm single-fold + Label Option A; src/presentation/CMakeLists.txt
  (MOD): added clear.cpp.
- vaml_viewer/app.cpp (MOD): removed the hard clear constant + S595 threads AllowsTransparency -> the OS
  hint; vaml_viewer/viewer_transparency.h (NEW); vaml_viewer/sample.transparent.vaml (NEW);
  vaml_viewer/viewer_growth_s595a.h (NEW); vaml_viewer/sample.s595a.xaml (NEW); vaml_viewer/vaml_viewer.vcxproj (MOD).
- test/unit: Swapchain_test.cpp (S590/S591), PlatformWindow_test.cpp (S594), VamlRenderer_clear_test.cpp
  (NEW), VamlRenderer_alpha_test.cpp (NEW), ViewerTransparency_s595_test.cpp (NEW), ViewerHitTest_s595a_test.cpp
  (NEW core), ViewerHitTest_s595a_pres_test.cpp (NEW presentation), CMakeLists.txt (MOD).
- build/: verify_s590.cmd / verify_s591.cmd / verify_s592.cmd / verify_s593.cmd / verify_s594.cmd /
  verify_s595.cmd / verify_s595a.cmd / verify_s596.cmd (NEW).
- docs/roadmap.md (MOD): S590..S595a + S596 all marked [DONE] with their digests + the S590->S591 RESOLVED
  + NO-GAP + RENDER_SURFACE conformance. docs/RENDER_SURFACE.md (MOD): the S593 alpha rows + the Window
  row + the `Alpha compositing (S593)` note.
- progress.md (MOD): the `SPRINT 11o — SURVIVOR` block + the new ACTIVE-CURRENT (11o) line.
- session-context.md (MOD): this entry.

## build_status (S596 / Sprint 11o)
- windows-msvc: build/verify_s596.cmd -> BUILD OK (warning-as-error clean) + ctest 100% pass (4/4) +
  render_surface_check.py OK (60/67) + grep-audit PASS.
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug -> build clean + ctest 100% pass (4/4) + render_surface_check.py
  (python3) OK (60/67) + grep-audit PASS (no composite-alpha sentinel).
- Sprint 11o (S590..S595a + S596): ALL GREEN on BOTH CI lanes, each verified from first principles at its
  own step.

## next_step — Sprint 11o CLOSED / next sprint
- **Sprint 11o CLOSED (S590..S595a + S596 all DONE; transparency is a first-class render surface).**
  Next per the roadmap dependency chain = **Sprint 11h (S520 — IMPORT/INJECT seam)**, then S540+/Sprint 11z.

## notes
- Sprint 11o delivered the transparency surface end-to-end: composite-alpha probe (S590) -> premultiplied
  swapchain (S591, RESOLVING the S590 [MINIMAL REAL]) -> per-window clear + Background (S592) -> element
  alpha single-fold (S593, RENDER_SURFACE-conformant) -> OS-window transparency (S594) -> live
  AllowsTransparency + viewer growth (S595) -> IsHitTestVisible (S595a) -> the S596 gate + SURVIVOR.
- ZERO new locks (ADR-018); ADR-011 held (transparency is core DATA; compositing/present in the
  vulkan/presentation layers). NO silent backfill (NO-GAP): every S590..S595a symbol is real, permanent,
  forward-bound. The LIVE windows (see-through viewer + transparent sample + overlay) are
  [both apps, not CI] local-gate demonstrations, never CI gates.
- No commit made (standing directive — commitit outstanding; remote build green with the change set
  uncommitted, the driver's seat is yours).
