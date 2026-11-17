# session-context.md

last_command: implementit S821 (Sprint 11ab) — VISIBILITY FIDELITY: the WPF `ValidateVisibility` boundary gate on the S120 `Visibility` DP (UIElement owner, Int32, default Visible). `Set`/authoring of a raw value outside Visible(0)/Hidden(1)/Collapsed(2) (e.g. 3 or -1) is REJECTED at the boundary (store unchanged) and THROWS `InvalidPropertyValueException` (naming `UIElement.Visibility` + the offending value) via the S810_1 false-ValidateFunc disposition — matching WPF `ValidateVisibility` (UIElement.cs:2960) + `ArgumentException(InvalidPropertyValue)` (DependencyObject.cs:653). 0/1/2 all resolve. The Int32-rides-enum convention + S600 `convert_int32_for_dp` passthrough + S648/CONVERTER are UNCHANGED. ZERO new locks (ADR-018); NO MINIMAL REAL. BOTH CI LANES GREEN (build/verify_s821.cmd -> "S821 GREEN (both CI lanes)" + UBUNTU-GREEN). Working tree extended (NOT committed; commitit outstanding across S816..S821 — standing directive).

plan_digest: S821 (dep:S820) — the S120 dep:S097 UIElement-base drift closure opener (Sprint 11ab). (1) `UIElement.cpp` Visibility registration gains `.with_validate(x == 0 || x == 1 || x == 2)` mirroring WPF ValidateVisibility (UIElement.cs:2960); a false ValidateFunc THROWS InvalidPropertyValueException (S810_1, WPF ArgumentException(InvalidPropertyValue) DO.cs:653) at the Set boundary (raw gate + post-coerce re-check, both live); store unchanged. (2) `UIElement.h` set_visibility doc + S821 boundary note. (3) `UIElement_test.cpp`: NEW S821 boundary cases — 0/1/2 resolve + 3/-1 throw (store unchanged) + the exception-names-property-and-value pin (UIElement.Visibility=3). (4) new `build/verify_s821.cmd` (two-lane gate + grep-audit: with_validate + ValidateVisibility + throw disposition + S600/S648 mapping unchanged) BOTH lanes green. The Int32-rides-enum convention is PRESERVED (only the validate callback is added). Roadmap S821 -> [DONE]* + 11ab PART 1 SURVIVOR in progress.md. NO MINIMAL REAL; ZERO new locks (ADR-018).

plan_digest: S820 (dep:S819, Sprint 11aa PART 3) — the REFACTOR GATE + SURVIVOR. (1) NEW `build/verify_s820.cmd` (the two-lane gate + grep-audit): full msvc-debug + ubuntu-clang build + ctest from first principles; grep-audit VERIFIED LIVE symbols — all eleven framework arms in PropertyFlags.h; the FE.cs:2198-2251 honor sites (OnPropertyChanged engine + InvalidateVisual + render_dirty_) in UIElement; `BindsTwoWayByDefault` observable runtime effect (create() mode defaulting + modeExplicit + the VamlReader.cpp markup wire); `OverridesInheritanceBehavior` READ + CONSUMED; `NotDataBindable` refusal gate; zero STUB; no invented flag survives. (2) roadmap S820 -> [DONE]* + 11aa PART 3 SURVIVOR block in progress.md + the ACTIVE-CURRENT checkpoint WITHIN progress.md regenerated to S820. (3) session-context updated. ZERO new locks (ADR-018); NO MINIMAL REAL. Next = S821 (Sprint 11ab — the S120 dep:S097 UIElement-base drift closure: Visibility/Opacity/RenderTransform/layout-slot-FrameworkElement; 11ab inserts between 11aa and 11h; 11h follows after).

plan_digest: S819 (dep:S818) — Sprint 11aa PART 2 — the THREE S818-carried framework flags get their RUNTIME effect. (1) `BindingDiagnostics.{h,cpp}`: NEW `BindingReasonCode::NotDataBindableTarget = 11` (Class B) + `diagnostic_from_not_data_bindable` factory + `binding_class_of` Class B arm. (2) `BindingExpression.cpp` create(): the NotDataBindable refusal gate (resolve target DP on the composite pair; flagged -> emit + return nullptr, never throws under either policy) + the metadata-aware mode defaulting (`modeExplicit` -> `BindsTwoWayByDefault` → TwoWay, else OneWay). (1) all eleven real WPF `FrameworkPropertyMetadataOptions` arms (FPM.cs:18-55) LIVE in PropertyFlags.h (AffectsMeasure..SubPropertiesDoNotAffectRender incl. Inherits/BindsTwoWayByDefault/OverridesInheritanceBehavior/NotDataBindable/Journal); (2) the FE.cs:2198-2251 honor sites LIVE in UIElement (OnPropertyChanged Affects* engine + InvalidateVisual + render_dirty_); (3) `BindsTwoWayByDefault` has OBSERVABLE runtime effect (not carried-only, the S045/S819 NO-GAP closed); (4) OverridesInheritanceBehavior READ + CONSUMED in the S039 walk; (5) zero STUB / zero silent backfill; (6) no invented flag survives (`InheritOnly` only as the sanctioned S814 retirement comment). ZERO new locks (ADR-018); NO MINIMAL REAL introduced/resolved. BOTH CI LANES GREEN (build/verify_s820.cmd -> "S820 GREEN (both CI lanes)" + UBUNTU-GREEN). Working tree extended (NOT committed; commitit outstanding across S816/S817/S818/S819/S820 — standing directive).

plan_digest: S820 (dep:S819) — Sprint 11aa PART 3 — the REFACTOR GATE + SURVIVOR. (1) NEW `build/verify_s820.cmd` (the two-lane gate + grep-audit): full msvc-debug + ubuntu-clang build + ctest from first principles; grep-audit VERIFIED LIVE symbols — all eleven framework arms in PropertyFlags.h; the FE.cs:2198-2251 honor sites (OnPropertyChanged engine + InvalidateVisual + render_dirty_) in UIElement; `BindsTwoWayByDefault` observable runtime effect (create() mode defaulting + modeExplicit + the VamlReader.cpp markup wire); `OverridesInheritanceBehavior` READ + CONSUMED; `NotDataBindable` refusal gate; zero STUB; no invented flag survives. (2) roadmap S820 -> [DONE]* + 11aa PART 3 SURVIVOR block in progress.md + the ACTIVE-CURRENT checkpoint WITHIN progress.md regenerated to S820. (3) session-context updated. ZERO new locks (ADR-018); NO MINIMAL REAL. Next = S821 (Sprint 11ab — the S120 dep:S097 UIElement-base drift closure: Visibility/Opacity/RenderTransform/layout-slot-FrameworkElement; 11ab inserts between 11aa and 11h; 11h follows after).

plan_digest: S819 (dep:S818) — Sprint 11aa PART 2 — the THREE S818-carried framework flags get their RUNTIME effect. (1) `BindingDiagnostics.{h,cpp}`: NEW `BindingReasonCode::NotDataBindableTarget = 11` (Class B) + `diagnostic_from_not_data_bindable` factory + `binding_class_of` Class B arm. (2) `BindingExpression.cpp` create(): the NotDataBindable refusal gate (resolve target DP on the composite pair; flagged -> emit + return nullptr, never throws under either policy) + the metadata-aware mode defaulting (`modeExplicit` -> `BindsTwoWayByDefault` → TwoWay, else OneWay). (3) `BindingExpression.h`: NEW `BindingExpressionSpec::modeExplicit` (default true). (4) `VamlReader.cpp` BindExtensions: `spec.modeExplicit = markup.hasMode`. (5) `DependencyObject.{h,cpp}`: `GetEffectiveValue` reads `OverridesInheritanceBehavior` + threads it into `nearest_ancestor_local` (new param, the walk authority — WPF TreeWalkHelper gate). (6) NEW `FlagsRuntime_s819_test.cpp` (41 assertions: a/b/c + Strict non-throw + explicit-Mode-wins + un-flagged-OneWay + programmatic-verbatim + OIB carry/consume) + `BindingDiagnostics_test.cpp` extended (Class B tagging + factory shape). (7) build/verify_s819.cmd (grep-audit: NotDataBindableTarget live + create() gates + modeExplicit in markup + OIB read/threaded + zero STUB) BOTH lanes green. (8) roadmap S819 -> [DONE] + 11aa PART 2 SURVIVOR in progress.md. NO MINIMAL REAL; ZERO new locks (ADR-018). Next = S820 (11aa gate; then S821 = Sprint 11ab / S120 dep:S097 drift closure).

plan_digest: S818 (dep:S817) — (1) `PropertyFlags.h` rewritten as a BYTE-FAITHFUL import of WPF `FrameworkPropertyMetadataOptions` (Option B, user-directed drift-removal renumbering the surviving `Inherits`=0x020/`BindsTwoWayByDefault`=0x100 ordinals to WPF canonical; VAML-only `ReadOnly` in the sole free gap 0x200; the nine new framework arms AffectsMeasure 0x001..SubPropertiesDoNotAffectRender 0x800 all live; no invented `InheritOnly`). (2) `PropertyKey.cpp` describe() names every arm in a stable ordered list. (3) WIRE: the DO.cs:2039 `OnPropertyChanged` virtual call-out fires from `DependencyObject::Set` on a real change only; `UIElement` override runs the FE.cs:2198-2251 Affects* engine (nearest UIElement layout-parent walk for AffectsParentMeasure/Arrange -> InvalidateMeasure/Arrange; own-axis for AffectsMeasure/Arrange; InvalidateVisual + new S208/S210 render_dirty_ for AffectsRender; SubPropertiesDoNotAffectRender gates only non-value changes, never reachable via Set()'s real-change call-out = WPF IsAValueChange||!flag). (4) S039 flags()/metadata() round-trip + S045 consolidated all-flags extended; NEW AffectsInvalidation_test.cpp (7 WPF-faithful invalidation tests / 73 assertions). (5) build/verify_s818.cmd (grep-audit: all nine arms live + legacy bit-shift positions re-derived + zero STUB) BOTH lanes green. (6) roadmap S818 -> [DONE] + 11aa PART 1 SURVIVOR in progress.md. NO MINIMAL REAL; ZERO new locks (ADR-018).

plan_digest: S815 (dep:S814) — (1) untangle the stale comments keyed on "Inherits/InheritOnly" in UIElement.cpp (the attach_child inverse-edge cascade comment + the Opacity INHERITED registration comment), the test narrative headers (AttachedInheritanceNotification_test.cpp S044 "broadens the push walk / contextual captureAttachedInherits flag"; InheritanceCascade_test.cpp "S041 owns the push/ChangeQueue pipeline" + the kInh comment; UIElement_test.cpp "PUSHES a notification ... descendant push"), and confirmed DependencyProperty.{h,cpp} carries ZERO InheritOnly references (no-op). The accurate "RETRACTED by S811" / "retired by S814" history text is preserved un-edited per the corrective convention. (2) re-verify the S041/S042/S043/S044 suites gate on the SINGLE Inherits flag + the WPF-true local-wins contract (local ?? inherited ?? default; origin sync metadata cb; passive-child pull read; local-wins-not-clobbered; wrong-thread throw). (3) build/verify_s815.cmd (NEW) both lanes green + grep-audit (collect_descendant_effectives/notify_descendants/visit_scope/captureAttachedInherits ABSENT from live DependencyObject.cpp; Inherits/InheritOnly absent from UIElement.cpp; InheritOnly absent from DependencyProperty.{h,cpp}). (4) roadmap S815 -> [DONE]* + 11u PART 2 SURVIVOR block in progress.md. No new public symbol; no functional change; NO MINIMAL REAL; ZERO new locks (ADR-018).

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

## build_status (as of this session — S821: Sprint 11ab VISIBILITY FIDELITY; both lanes GREEN)
- **windows-msvc** (msvc-debug): S821 — build/verify_s821.cmd ran to "S821 GREEN (both CI lanes)" with configure clean + build warning-as-error clean + ctest 4/4 100% (vaml-core / vaml-tools / vaml-vulkan / vaml-presentation all Passed).
- **ubuntu-clang** (VM, clang 21, shared folder /mnt/hgfs/VamlPOC): configure + build + ctest — UBUNTU-GREEN via verify_s821.cmd.
- Prior Sprint 11aa S818 (PART 1) + S819 (PART 2) + S820 (PART 3 gate) both-lanes-green results remain historical; Sprint 11t S810..S813 + Sprint 11u S814..S815 results remain historical.

## next_step
- Sprint 11ab S821 (dep:S820) DONE — the WPF `ValidateVisibility` boundary gate is live on the S120 `Visibility` DP (0/1/2 resolve; 3/-1 throw `InvalidPropertyValueException`, store unchanged); the Int32-rides-enum convention + S600/S648 mapping UNCHANGED. Both lanes green from first principles.
- **Next = the rest of Sprint 11ab (the S120 dep:S097 UIElement-base drift closure, continuing from S821): Opacity loses Inherits + validation (compositor model), RenderTransform becomes a WPF-faithful Transform-object DP + RenderTransformOrigin, and a real FrameworkElement : UIElement base carrying the layout slot; 11ab inserts between 11aa and 11h; 11h follows after.**
- Remote-build (GH CI) check-in remains OPEN (S816/S817/S818/S819/S820/S821 + prior uncommitted work; commits fire only on explicit commitit).

## notes
- Sprint 11ab S821 (dep:S820) DONE, NOT committed. build/verify_s821.cmd GREEN (both CI lanes: windows-msvc ctest 4/4 100% + ubuntu-clang UBUNTU-GREEN). Files touched: src/core/src/UIElement.cpp (Visibility registration gains the WPF ValidateVisibility `.with_validate(x==0||x==1||x==2)` boundary gate — the S120 Int32 DP now rejects out-of-domain values at Set with InvalidPropertyValueException, store unchanged, via the S810_1 disposition; default Visible still passes; S600/S648/CONVERTER mapping untouched), src/core/include/vaml/core/UIElement.h (set_visibility doc + S821 boundary note), test/unit/UIElement_test.cpp (NEW S821 boundary cases: 0/1/2 resolve + 3/-1 throw + store-unchanged + the exception-names `UIElement.Visibility=3` pin), build/verify_s821.cmd (new two-lane gate + grep-audit). ZERO new product symbol (the gate rides the existing with_validate + S810_1 throw); NO MINIMAL REAL introduced/resolved; ZERO new locks (ADR-018). Commits outstanding across S816..S821 (standing directive).
- Sprint 11aa S820 (REFACTOR GATE + SURVIVOR) DONE, NOT committed. build/verify_s820.cmd GREEN (both CI lanes: windows-msvc ctest 4/4 100% + ubuntu-clang UBUNTU-GREEN). Files touched: NEW build/verify_s820.cmd (the two-lane gate + the S820 grep-audit). **MD DONE-MARKED:** docs/roadmap.md S820 → **[DONE]*** + the SPRINT 11aa - PART 3 SURVIVOR block written to progress.md + the ACTIVE-CURRENT checkpoint WITHIN progress.md regenerated to S820 (S820 DONE + both lanes green, next = S821 → Sprint 11ab / the S120 dep:S097 drift closure). NO new product symbol (verification-only gate); ZERO new locks (ADR-018); NO MINIMAL REAL introduced/resolved.
- Sprint 11aa S819 DONE, NOT committed. build/verify_s819.cmd GREEN (both CI lanes: windows-msvc 1313/1313 + ubuntu-clang). Files touched: BindingDiagnostics.{h,cpp} (NEW `NotDataBindableTarget` Class B arm + `diagnostic_from_not_data_bindable` factory + `binding_class_of` arm), BindingExpression.{h,cpp} (NEW `spec.modeExplicit` + the create() NotDataBindable refusal gate + the metadata-aware BindsTwoWayByDefault mode defaulting), VamlReader.cpp (markup `modeExplicit = hasMode`), DependencyObject.{h,cpp} (GetEffectiveValue reads OverridesInheritanceBehavior + `nearest_ancestor_local` new param), NEW FlagsRuntime_s819_test.cpp (41 assertions), BindingDiagnostics_test.cpp extended, test/unit/CMakeLists.txt, build/verify_s819.cmd (new). **MD DONE-MARKED:** docs/roadmap.md S819 → **[DONE]** + the SPRINT 11aa - PART 2 SURVIVOR block written to progress.md + the ACTIVE-CURRENT checkpoint advanced (S819 DONE + both lanes green, next = S820 → S821/Sprint 11ab).
- Sprint 11aa S818 DONE, NOT committed. build/verify_s818.cmd GREEN (both CI lanes: windows-msvc + ubuntu-clang). Files touched: PropertyFlags.h (byte-faithful WPF ordinals, Option B renumber), PropertyKey.cpp (describe() full-arm serialization), DependencyObject.{h,cpp} (OnPropertyChanged virtual + Set() real-change call-out), UIElement.{h,cpp} (InvalidateVisual + render_dirty_ + OnPropertyChanged FE.cs override), tests (DependencyProperty_test.cpp S039 round-trip + ReadOnlyAndValidation_test.cpp S045 consolidated), NEW AffectsInvalidation_test.cpp (7 TESTS/73 assertions), test/unit/CMakeLists.txt, build/verify_s818.cmd (new). **MD DONE-MARKED:** docs/roadmap.md S818 → **[DONE]** + the SPRINT 11aa - PART 1 SURVIVOR block written to progress.md (S818 DONE + both lanes green, next = S819).
- Sprint 11u S815 DONE, NOT committed. build/verify_s815.cmd GREEN (both CI lanes: windows-msvc + ubuntu-clang). Files touched: UIElement.cpp (attach_child inverse-edge + Opacity-registration comments untangled to single Inherits + S039 pull), test headers (AttachedInheritanceNotification_test.cpp / InheritanceCascade_test.cpp / UIElement_test.cpp narrative corrected to the retired-descendant-push + single-Inherits + WPF local-wins truth), build/verify_s815.cmd (new). DependencyProperty.{h,cpp} grep-verified ZERO InheritOnly (no-op). No new public symbol; no functional change. **MD DONE-MARKED:** docs/roadmap.md S815 → **[DONE]*** + the 11u PART 2 SURVIVOR block written to progress.md (SPRINT 11u - PART 2 SURVIVOR, S815 DONE + both lanes green).
- Sprint 11u S814 DONE, NOT committed. build/verify_s814.cmd GREEN (both CI lanes: windows-msvc + ubuntu-clang). Files touched: PropertyFlags.h (bit removed), PropertyKey.cpp (serialization arm), DependencyObject.cpp (GetEffectiveValue collapse), DependencyObject.h (S039 contract text), DependencyProperty.h (flags comment), tests: EffectiveValue / DependencyProperty / InheritanceCascade / ReadOnlyAndValidation / AttachedInheritanceNotification (InheritOnly arms deleted or converted to local-wins; round-trip/converted to surviving flags), build/verify_s814.cmd (new). **MD DONE-MARKED:** docs/roadmap.md S814 → **[DONE]*** + the 11u SURVIVOR block written to progress.md (SPRINT 11u - PART 1 SURVIVOR, S814 DONE + both lanes green).
- Sprint 11t S811/S812/S813 DONE, NOT committed. build/verify_s811.cmd (S811 code, prior conversation) + build/verify_s813.cmd (the 11t GATE, this session) both GREEN (both CI lanes). The S811 token was finalized here (previously left unmarked).
- Sprint 11t S810_2 DONE, NOT committed. build/verify_s810_2.cmd GREEN (both CI lanes: windows-msvc + ubuntu-clang).
- Sprint 11t S810_1 DONE, NOT committed. build/verify_s810_1.cmd GREEN. (Superseded-within-11t by S810_2's
  registration gate — history preserved un-edited; the Set-pipeline disposition is unchanged.)
- Sprint 11t S810 (Part 1, D1) DONE, NOT committed. build/verify_s810.cmd GREEN.
- Sprint 11s DONE, NOT committed. build/verify_s11s.cmd GREEN.
- Prior S803/S11af notes above remain as historical state (superseded by the 11s completion).
