# session-context.md

last_command: implementit (Sprint 11p — S600..S605, the lib value/type correctness stream A; this CLOSES Sprint 11p)
plan_digest: Sprint 11p (dep:S596) — GAP-CLOSE the ~5% GENUINE vaml-core *correctness* defects the WPF-corpus audit
surfaced: **S600** VALUE/TYPE-AWARE CONVERSION root A1 (per-DP EnumDomainRegistry replacing the S440 cross-cut
enum_name_to_int32; WPF-true ordinal re-pointing + SYMBOLIC DP defaults); **S601** GEOMETRIC/UNIT root A2 (GridLength
first-class converter + Grid track units); **S602** COLOR FIDELITY root A3 (#RGB/#ARGB shorthand); **S603** BRUSH
property-element AUTHORING ORDER root A4 (GeometryDrawing .Pen/.Brush; bare <GradientStop> ContentProperty);
**S604** STRUCTURED-ERROR taxonomy tightening A5; **S605** REFACTOR GATE + SURVIVOR. Both CI lanes green per step
from first principles (build/verify_s600..s605.cmd msvc-debug + ubuntu-clang clang-debug on /mnt/hgfs/VamlPOC —
all 4/4). SURVIVOR block added to progress.md. Roadmap S600..S605 marked [DONE]. ZERO new locks (ADR-018).
**CHECKPOINT ADVANCED: last = Sprint 11p, next = Sprint 11q (S620 — viewer seed-binding injection, stream B).**

## files_touched — Sprint 11p (S600..S605, this incarnation)
- src/core/include/vaml/core/EnumDomainRegistry.h (NEW) + src/core/src/EnumDomainRegistry.cpp (NEW): the per-DP
  enum-domain table (DP canonical name -> WPF-true ordinal members, WPF Enum.Parse semantics) + register_builtin_enum_domains()
  + the shared convert_int32_for_dp (both readers call it; S288 parity). S600 A1.
- src/core/src/VamlReader.cpp (MOD): convertLiteral routes Int32 -> convert_int32_for_dp; converterNameFor gains
  ValueType::GridLength; ensure_startup_registrations registers the enum domains; S603 authoring fixes
  (LinearGradientBrush bare <GradientStop> + .GradientStops; GeometryDrawing .Geometry/.Pen/.Brush).
- src/core/src/ValueConverter.cpp (MOD): Int32 registry converter now NUMERIC-ONLY (the S440 enum_name_to_int32
  cross-cut map REMOVED — no behavior-guess survives); GridLength converter registered.
- src/core/include/vaml/core/LayoutSlot.h (MOD): re-pointed HorizontalAlignment/VerticalAlignment to WPF-true.
- src/core/include/vaml/core/Element.h (MOD): re-pointed Orientation to WPF-true.
- src/core/src/UIElement.cpp (MOD): Horizontal/VerticalAlignment defaults now SYMBOLIC (::Stretch=3).
- src/core/src/Element.cpp (MOD): Orientation default symbolic (::Vertical=1); WrapDirection default symbolic (::Horizontal=0).
- src/core/src/Color.cpp (MOD): parse_color accepts #RGB (3-digit) + #ARGB (4-digit, ALPHA-FIRST) shorthand.
- src/core/include/vaml/core/Color.h (MOD): doc updated for the shorthand forms.
- src/tools/src/VBamlReader.cpp (MOD): convertLiterals routes Int32 -> convert_int32_for_dp; converterNameFor gains
  ValueType::GridLength (S288 parity).
- src/tools/src/Vamlc.cpp (MOD): register_builtin_enum_domains() in compile().
- src/core/CMakeLists.txt (MOD): + EnumDomainRegistry.cpp.
- test/unit/CMakeLists.txt (MOD): + ValueTypeAware_s600_test.cpp, GeometricValue_s601_test.cpp,
  ColorFidelity_s602_test.cpp, BrushAuthoring_s603_test.cpp, StructuredError_s604_test.cpp.
- test/unit/: the five new S600..S604 test TUs (above).
- test/unit/LayoutSlot_test.cpp / StackPanel_test.cpp / WrapPanel_test.cpp / ValueConverterRegistry_test.cpp (MOD):
  expectation updates for the WPF-true re-pointing + the S602 #RGB shorthand (the S090 "3-digit not supported"
  expectation resolved).
- build/: verify_s600.cmd / verify_s601.cmd / verify_s602.cmd / verify_s603.cmd / verify_s604.cmd / verify_s605.cmd (NEW).
- docs/roadmap.md (MOD): S600..S605 marked [DONE] with their digests.
- progress.md (MOD): the new 11p ACTIVE-CURRENT checkpoint line + the `SPRINT 11p — SURVIVOR` block.
- session-context.md (MOD): this entry.

## build_status (S605 / Sprint 11p — re-verified from first principles)
- windows-msvc: build/verify_s605.cmd -> BUILD OK (warning-as-error clean) + ctest 100% pass (4/4) + all grep-audits
  PASS (ZERO STUB / ZERO [MINIMAL REAL] in the new EnumDomainRegistry surface; enum_name_to_int32 GONE; enum domains
  registered for both readers).
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug -> build clean (warnings==0) + ctest 100% pass (4/4).
- Sprint 11p (S600..S605): ALL GREEN on BOTH CI lanes, each verified from first principles at its own step
  (verify_s600/s601/s602/s603/s604/s605.cmd on windows; clang-debug on the Linux VM).

## next_step — Sprint 11p CLOSED / next sprint
- **Sprint 11p CLOSED (S600..S605 all DONE; the lib correctness stream A delivered).**
  Next per the roadmap = **Sprint 11q (S620 — vaml_view VIEWER seed-binding injection, stream B)**, then Sprint 11h
  (S520 IMPORT/INJECT seam) / S540+ (Sprint 11z).

## notes
- Sprint 11p authored MINIMAL REAL-free, real permanent surface: the EnumDomainRegistry + convert_int32_for_dp (shared
  by both readers) delivers the A1 type-aware conversion; S601/S602/S603/S604 close the geometric/unit, color-fidelity,
  brush-authoring, and structured-error correctness gaps. NO behavior-guess conversion survives (the S440 cross-cut map
  is gone; every enum domain rides the DP's OWN registered bound type). FontWeight stays OUT of the enum-domain (a
  struct with its own WPF converter) — recorded forward, NOT silent.
- The WPF-fidelity decision record (Option B: WPF-true ordinals + symbolic DP defaults) was HUMAN-ACCEPTED mid-sprint;
  the earlier hedged Option A (name-map-only) was rejected in favor of full WPF alignment after the WPF DP-init source
  was verified. Grounded in E:\GitRepos\wpf throughout.
- ZERO new locks (ADR-018); ADR-011 held (all new surface is core DATA / core value conversion). NO silent backfill
  (NO-GAP): the S090 "3-digit NOT supported" expectation update is a documented consequence of the S602 resolution.
- No commit made (standing directive — commitit outstanding; remote build green with the Sprint 11p change set
  uncommitted, the driver's seat is yours).
