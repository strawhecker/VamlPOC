# session-context.md

last_command: implementit S811 + S812 + S813 (Sprint 11t — D2/D3 pipeline correction + token-strike + GATE). S811 finalized from an unfinalized token (code already green) + S812 docs-correction implemented + S813 the REFACTOR GATE(11t) PASSED. BOTH CI LANES GREEN (build/verify_s813.cmd -> "S813 GREEN (both CI lanes)"). Working tree extended (NOT committed; commitit outstanding — standing directive).

plan_digest: 11t close-out — (1) FINALIZE S811 [DONE]* (the prior conversation left the code green but the roadmap token unmarked; recorded + documented); (2) S812 [DONE]* ROADMAP/RECORD — the S037/S041/S038/S042/S143 mutex-protected queue / notification-pipeline tokens STRUCK + corrected (NO pre-approved DP-value lock; NO "notification ChangeQueue" lock; the ONLY cross-thread change-log is 11n-4's CollectionView opt-in fill->drain deque + swap mutex; S143 relocation rewritten to "moved OUT of the DP layer entirely"; S1724 reconciled — re-verifies, not re-authorizes; ADR-018 rewritten: ZERO new locks); (3) S813 [DONE]* REFACTOR GATE(11t) — both lanes green from first principles + the grep-audit (SetResult::QUEUED / add_property_changed / remove_property_changed / ProcessNotificationQueue / pending_notification_count / fire_self_subscribers / ChangeQueue / ChangeProcessor / ChangeRecorder ALL absent; BindingExpression subscribes via with_changed/notifyChanged only; Set order == DO.cs:653/1692 + sync callback == DO.cs:2039). 11t SURVIVOR + checkpoint in progress.md. ZERO MINIMAL REAL; ZERO new locks (ADR-018).

plan_digest: S810_1 (dep:S810) changes DependencyObject::Set so a false ValidateFunc (raw gate DO.cs:653 OR post-coerce re-check DO.cs:1692) THROWS vaml::InvalidPropertyValueException : std::invalid_argument (naming the property + the offending value), matching WPF ArgumentException(InvalidPropertyValue). SetResult::REJECTED is RETIRED as the out-of-range disposition on the Set pipeline (the enum member stays defined; removal = S811). NO-GAP: BindingExpression CATCHES the core throw and maps it to TransferResult::TargetRejected at the binding boundary (ADR-016 observation, never control-flow in the core); the throw is converted at the binding boundary, not suppressed in the core. Tests: S045/S046 REJECTED asserts -> ShouldThrow<InvalidPropertyValueException> + dependent sweep of every DP-backed REJECTED assert + a new explicit BindingExpression NO-GAP TargetRejected test. BOTH CI lanes green from first principles.
plan_digest: Sprint 11s — the composite (name, owner) DP identity lands end-to-end (S804 coexistence live-proof
+ S805 un-revert re-additions merged into one green unit; S806 re-verify/wide-sweep; S807 refactor gate; S808
survivor+checkpoint). Re-adds the DPs the name-only contract forced away, each verified against E:\GitRepos\wpf
ground truth. The name-only DP hosing is ABOLISHED; ZERO MINIMAL REAL introduced (grep-audit PASS).

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

## Sprint 11t S810 — D1 FIX: DP VALUE PIPELINE ORDER (this session)
- **Drift fixed:** Set() pipeline was S035 `coerce -> validate(coerced)`. WPF truth (DependencyObject.cs:653)
  validates the RAW input BEFORE coercion, then (DO.cs:1692) coerces and re-validates the coerced result.
- **Change (src/core/src/DependencyObject.cpp):** `Set()` now runs validate(input) -> coerce -> validate(coerced);
  a raw value invalid at the boundary is REJECTED with the store unchanged (coerce is NOT allowed to normalize it).
- **Contract text corrected (NOT merely implemented):** DependencyObject.h (Set() doc + PIPELINE comment),
  the S035-pipeline comments inside Set() re-tagged D1/WPF-order, and the S046/S036 test-file headers.
- **Tests:** new S046 RawBoundary TEST_CASE proves raw invalid-but-coerce-normalizable is REJECTED at the
  boundary with store unchanged (in DependencyCallbacks_test.cpp). Re-baselined the S036 test that asserted the
  OLD drift order (coerce-before-validate -> APPLIED) to assert WPF boundary-reject (raw -3 -> REJECTED, store 0);
  it was named "coerce runs BEFORE validate" — precisely the drift — corrected per Rule 2 (WPF source audit).
- **No new product symbol:** a pure reorder + disposition-preserving reject (REJECTED stays); no MINIMAL REAL,
  no behavior change to the QUEUED/affinity path (that's S811's D2/D3 removal, out of scope here).
- **Lanes:** windows-msvc (msvc-debug, build/verify_s810.cmd) + ubuntu-clang (VM, /mnt/hgfs/VamlPOC,
  clang-debug) both GREEN — "S810 GREEN". windows-clang-debug NOT built (not a CI lane).
- **Remaining NOTE:** an extra `coerce -> validate(coerced)`-style stale-CONTEXT sentence may linger in the S046
  test header's first block; verified functionally neutral, left as-is to avoid churn (see S046 header lines 9-13).

## Sprint 11t S810_1 — VALIDATION DISPOSITION (this session)
- **Disposition changed:** a false ValidateFunc (raw gate DO.cs:653 OR post-coerce re-check DO.cs:1692) now THROWS
  vaml::InvalidPropertyValueException : std::invalid_argument (naming the property + the offending value) — WPF
  ArgumentException(InvalidPropertyValue). SetResult::REJECTED is RETIRED as the out-of-range disposition on the
  Set pipeline (the enum member stays defined; removal is S811, which this step deliberately defers so the binding's
  exhaustive switch compiles).
- **Core (src/core/src/DependencyObject.cpp):** both validate gates throw; the wrong-thread path throws too (never
  QUEUED for a validate refusal). New local render_to_diagnostic helper formats the refused value into the message.
- **Header (DependencyObject.h):** new InvalidPropertyValueException : std::invalid_argument; Set()/SetResult docs +
  contract corrected (REJECTED retired as out-of-range disposition).
- **Binding boundary (BindingExpression.cpp):** apply_to_target CATCHES the core InvalidPropertyValueException and
  maps it to TransferResult::TargetRejected (ADR-016 — the conversion happens at the binding boundary, never by
  suppressing the throw in the core). REJECTED switch arm retained as a compile-time-known-unreachable (S811 removes).
- **Tests:** S810_1 NO-GAP explicit (BindingExpression_test.cpp — validating target [0,10] + out-of-range push ->
  TargetRejected, no core throw escapes, target unchanged, no SetTargetFailed emission). Sweep: ReadOnlyAndValidation
  (S045 REJECTED->throws), DependencyCallbacks (S046 post-coerce + raw-boundary -> throws), DependencyObject
  (S036 D1 + validate-refuses -> throws), DependencyObject_marshalling (S037 wrong-thread validate refusal -> throws,
  captured via exception_ptr), UIElement (S120 Opacity x3 -> throws), Text (S400 FontSize negative -> throws),
  IconGlyphPlan (S400_1 Width/Height -> throws), Transform_render (S208 non-finite -> CHECK_THROWS_AS),
  AutomationProperties_s11n1 (11n-1 IsOffscreenBehavior -> throws).
- **Lanes:** windows-msvc (msvc-debug, build/verify_s810_1.cmd) + ubuntu-clang (VM, /mnt/hgfs/VamlPOC, clang-debug)
  both GREEN — "S810_1 GREEN".
- **Out of scope (recorded):** Effect/Viewport3D direct-field setters are NOT DP-backed (untouched);
  PropertyMetadata_semantics_test.cpp's own MiniStore Outcome::REJECTED enum is unrelated; windows-clang-debug NOT a CI lane.

## Sprint 11t S810_2 — REGISTRATION-TIME DEFAULT VALIDATION (this session)
- **Gate added:** WPF `ValidateDefaultValueCommon` (DependencyProperty.cs:376-434) validates the DEFAULT at
  registration against type AND the `ValidateValueCallback`. VAML's Register/register_property validated NO default.
- **(a) type-mismatch arm:** already present — both register sites enforce key type == metadata.boundType()
  (== defaultValue().type()) and throw ValueTypeMismatch (the DefaultValuePropertyTypeMismatch analogue). No change.
- **(b) validate-on-default arm (NEW):** `if (!metadata.validate(metadata.defaultValue()))` throws
  InvalidPropertyValueException (S810_1, naming owner.name + the rendered default), mirroring DefaultValueInvalid.
  - **DependencyProperty.cpp Register:** inserted BEFORE the registry + GlobalTable side-effect -> atomic.
    New local render_default_for_diagnostic helper. Include DependencyObject.h for InvalidPropertyValueException.
  - **DependencyObject.cpp register_property:** inserted AFTER the duplicate-pair check, BEFORE the
    owned-metadata/stableKey heap nodes + store_.emplace -> atomic (nothing installed/allocated).
- **Tests:** PropertyMetadata_test.cpp (S034) + DependencyProperty_test.cpp (S039) each gained the TWO rejection
  arms (validate-false-default -> throws + TryGet nullptr / PropertyNotFound; wrong-typed-default -> ValueTypeMismatch
  + nothing stored) + a happy-path control (valid default satisfying its ValidateFunc registers + the gate runs).
  6 new S810_2 cases / 13 assertions, all green.
- **PRE-EXISTING TEST RE-BASE (correct WPF behavior surfaced):** S045 ROPipeline_S045 (ReadOnlyAndValidation_test.cpp)
  registered a read-only property with `with_validate([](const Value&) { return false; })` to prove the read-only
  throw precedes coerce/validate on Set. Under the new gate its default now fails registration (WPF-true). Re-based
  the validate to reject negatives (default 0 passes; the Set(-3) read-only-throw-before-pipeline intent preserved).
- **Lanes:** windows-msvc (build/verify_s810_2.cmd, msvc-debug warning-as-error clean + ctest 4/4 100%;
  vaml-core-tests 1341/1341 incl. 6 new S810_2 cases / 13 assertions, Status SUCCESS) + ubuntu-clang (VM,
  /mnt/hgfs/VamlPOC, clang-debug UBUNTU-GREEN) — "S810_2 GREEN".
- **Fidelity note:** WPF ValidateDefaultValueCommon throws ArgumentException for both arms; VAML reuses
  ValueTypeMismatch (type-refusal) + InvalidPropertyValueException (S810_1, ArgumentException lineage). No new type.
- **NO-GAP:** no MINIMAL REAL introduced; ADR-018 ZERO new locks (registration single-threaded).

## build_status (as of this session — S811/S812/S813: Sprint 11t CLOSED; both lanes GREEN)
- **windows-msvc** (msvc-debug): S811 code (already landed by the prior conversation) + this session's docs-only S812 + the S813 GATE — build/verify_s813.cmd ran to "S813 GREEN (both CI lanes)" with configure clean + build warning-as-error clean + ctest 4/4 100% pass (vaml-core / vaml-vulkan / vaml-tools / vaml-presentation suites all passed).
- **ubuntu-clang** (VM, clang 21, shared folder /mnt/hgfs/VamlPOC): configure + build + ctest — UBUNTU-GREEN via verify_s813.cmd.

## next_step
- Sprint 11t CLOSED (S810..S813): the DP value-pipeline order + the validation-disposition throw + registration-time default validation + the D2/D3 wrong-thread-throw + the invented S041 notification-pipeline removal + the mutex-protected-queue/notification-pipeline token-strike records + the REFACTOR GATE(11t). BOTH CI LANES GREEN at every gate.
- **Next = Sprint 11h (S520 — the 3rd-party IMPORT/INJECT seam)** per the surrounding continuation order.
- Remote-build (GH CI) check-in remains OPEN (S810_1/S810_2/S811/S812/S813 + prior uncommitted work; commits fire only on explicit commitit).

## notes
- Sprint 11t S811/S812/S813 DONE, NOT committed. build/verify_s811.cmd (S811 code, prior conversation) + build/verify_s813.cmd (the 11t GATE, this session) both GREEN (both CI lanes). The S811 token was finalized here (previously left unmarked).
- Sprint 11t S810_2 DONE, NOT committed. build/verify_s810_2.cmd GREEN (both CI lanes: windows-msvc + ubuntu-clang).
- Sprint 11t S810_1 DONE, NOT committed. build/verify_s810_1.cmd GREEN. (Superseded-within-11t by S810_2's
  registration gate — history preserved un-edited; the Set-pipeline disposition is unchanged.)
- Sprint 11t S810 (Part 1, D1) DONE, NOT committed. build/verify_s810.cmd GREEN.
- Sprint 11s DONE, NOT committed. build/verify_s11s.cmd GREEN.
- Prior S803/S11af notes above remain as historical state (superseded by the 11s completion).
