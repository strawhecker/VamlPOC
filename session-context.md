# session-context.md

last_command: implementit (all pre-existing core failures RESOLVED — TextBox:159/172 dangling PropertyId, VamlReader_binding:187 Direct no-path; S443/S256 re-based onto a genuinely-malformed leading-comma form; verify_s803 GREEN both lanes)
plan_digest: S803 makes the reader's dotted-key authoring native `(name, owner)` composite lookup (drop the
flatten), removes the S095/S09x name-unique global-DP discipline, and retires owner-less bridges. WPF ground
truth (audited DependencyProperty.cs:959-979): the ONLY locate is FromName(name, ownerType), owner REQUIRED,
no name-only locate. Every site must carry its real owner.

## COMPOSITE RE-KEY (this session — COMPILE + most RUNTIME done)
- FULL COMPILE green across ALL 4 targets (vaml-core, core-tests, tools-tests, presentation-tests).
- ObservedPropertyAll: no owner-less `PropertyId::of(name)` left in src/core/src.
- S2 text-reader + binding-engine suites (S242/S243/S244/S245/S452...) green on uniform source owner
  `"ObservableObject"` (WPF-true: PropertyChangedEventArgs is name-only).
- TOOLS runtime: 40 failed -> **7 failed** (vaml-tools-tests 151 cases) via per-fixture owner re-key of the
  S7-compiled/parity real-DP TUs (S285/S288/S287/S289/S461/S452/S366 and the binding-validation fixtures
  S280_1/S283/S284/S286). Registration owner + test-read owner unified to a real chain-listed base
  (`"FrameworkElement"`), matching the LIB's collect_owner_chain (universal base owners Control/
  FrameworkElement/UIElement are always emitted, so a DP under one resolves on any element).
- Dotted-attached synthetic props (S285Grid.Row / S285Dock.Dock / S288Grid.Row) converted to BARE names
  (S285Row / S285Dock / S288Row) under `"FrameworkElement"` — the resolver splits dotted keys into
  TryGet(tail, owner), so a single dotted-name registration no longer matches. Parity tool DualPathParity.cpp
  owner literal aligned to `"FrameworkElement"`.

## REMAINING (honest — one genuine failure class left)
- Classes 1-3 RESOLVED this session:
  1. **Vamlc_s280_test.cpp** — golden was authoring `Orientation` (a StackPanel DP) on a `Grid`, which the
     composite contract now correctly rejects. Fixed the golden: `Orientation` moved onto the real `StackPanel`
     owner, preserving element/type counts. GREEN.
  2. **Vamlc_s280_1_test.cpp:183** — stale test expecting `Toggle..IsChecked` to throw MalformedPath. PropertyPath.h
     documents that consecutive dots are deliberately WPF-tolerated (DrillIn.Never / `..` legal), NOT an error.
     Updated the malformed-path case to a genuine TrailingDot (`Toggle/`). GREEN.
  3. **ViewerRestyle S369/S455** — test read `Get(of("Background","UIElement"))`, but `Background` is
     guard-registered under owner `"Control"` (ElementDpFidelity.cpp:158). Fixed the test reads to `"Control"`.
     GREEN.
- **Class 4 — RESOLVED this session** (AppShellIntegration_test.cpp all 5 failing CHECKs now PASS, tools 151/151).
  ROOT CAUSE: `VamlReader::BindExtensions()` set `spec.targetProp = std::string(ext.key)` — the RAW dotted
  authored key (`"TextBox.Text"` / `"Slider.Value"`). The S244 TwoWay write-back subscription keys on
  `PropertyId::of(targetProp_, targetOwner_)` = `("TextBox.Text","TextBox")` — an impossible composite that
  NEVER fires, so a TextBox/Slider target edit dropped the two-way write-back (VM store stale, `subs=3→0`).
  FIX (src/core/src/VamlReader.cpp): when the DP resolves via `lookup_dp_for_attr`, set
  `spec.targetProp` to the DP's CANONICAL `property_id().name()` (not the raw dotted key) so the forward push
  + S244 write-back subscribe on the real `(name, owner)`.

## 11af CRASH — RESOLVED (this session, user directive #fix test crash, option A)
- Root cause (verified via Linux clang-ASan authoritative stack trace): the MSVC "vector subscript out of
  range" crash is the STALE 11af path tests, NOT a dangling sink (SinkGuard restore-to-default hardening was
  still applied as a latent UAF fix across 7 binding TUs). The uncommitted 11af parser makes empty/`.` a valid
  Direct self-binding and `..`/trailing-dot variants legal (DrillIn.Never); stale tests that expected those to be
  Class A errors called `sink.reasons[0]` on an empty vector.
- Re-baselined the stale tests onto the 11af-legal grammar (Sprint 11af S848 work):
  - S245_1 BindingFailure_test: `"a..b"` -> genuinely-malformed `"a[0"`; empty path -> valid Direct (no EmptyPath).
  - S242_1 BindingPolicy_test: two `".."` malformed cases -> `"S2421Text["` (UnterminatedIndexer); empty/whitespace
    Strict -> valid Direct (no Empty throw).
  - S252 BindingMarkup_test: "{Binding} with no path is malformed" -> valid Direct self-binding.
  - S252 VamlReader_binding_test: "{Binding} no-path -> MalformedBindingMarkup" -> valid Direct (no throw).
- RESULT: MSVC "vector subscript out of range" crash GONE. vaml-core-tests runs to completion on BOTH lanes
  (no SIGABRT / no RC=-1073740791). The re-baselined Direct/malformed tests all pass.

## build_status (as of this session)
- Compile: ALL targets warning-as-error CLEAN (both CI lanes).
- vaml-tools-tests: 151/151 PASS.
- vaml-presentation-tests: PASS.
- vaml-core-tests: **1332/1332 PASS — the last 2 pre-existing core failures are RESOLVED** (TextBox TwoWay +
  VamlReader Direct no-path). verify_s803 = GREEN (windows-msvc + ubuntu-clang).

## ControlTemplate/TemplateBinding color-DP fix (this fix, user directive)
- Root cause: the S803 composite re-key split the visual-state engine write target from the test read. enter_state/exit_state resolve the bare `Background` against the element owner-chain -> owner `Control` (Control.Background). The tests read(`Background`, `InkCanvas`) — a DISTINCT DP that stays at its default black, so red reads as black.
- Fix: re-keyed the three color reads in ControlTemplate_test and the enter_state read in TemplateBinding_test to owner `Control`. Self-consistent Set/Get `InkCanvas` pairs in TemplateBinding left alone.
- msvc-debug: ControlTemplate_test + TemplateBinding_test now PASS. Remaining core failures: TextBox:159/172 (TwoWay forward transfer), VamlReader_binding:187 (Direct empty-path boundary) — pre-existing, out of this directive's scope.

## ABORT — TextBox TwoWay debug halted by user
- User aborted the TextBox:159/172 forward-transfer probe debug. Probe (_probe_tb) crashed; the user clicked
  abort. No source/header files were modified this session; the throwaway build/_probe_tb.obj was deleted.
  In-flight state rolled back to last ✅ (the ControlTemplate/TemplateBinding color-DP fix baseline).
- REMAINING core failures (both pre-existing, untouched): TextBox:159/172 TwoWay forward transfer,
  VamlReader_binding:187 Direct empty-path boundary.

## next_step
- ALL S803 core failures RESOLVED; verify_s803 GREEN (windows-msvc + ubuntu-clang).
- Flip docs/roadmap.md S803 row [INCOMPLETE] → [DONE], then continue to S804. No commit made.

## notes
- No commit made (commitit fires only on explicit command).
- The earlier "empty-path Direct change REGRESSED AppShellBinding + ViewerBindingPlan" note is SUPERSEDED:
  those were stale tests encoding the retired pre-11af premise. Re-based S443/S256 onto the genuinely
  malformed `{Binding ,Mode=TwoWay}` (leading comma, no path) form, which still throws MalformedBindingMarkup
  while `{Binding}` stays a valid Direct self-binding. All green.
