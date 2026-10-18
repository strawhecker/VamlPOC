# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **S142 FULLY COMPLETE — BOTH CI LANES GREEN** (collapse DependencyObject/UIElement
                    pimpl + per-object unordered_map store into ONE block-resident unit; pimpl removed,
                    not preserved; single PD type, no common/root; store inline-started in the slot,
                    heap-grown on demand). DependencyObject/UIElement pimpl REMOVED — the store + all
                    inline state live directly in the object's allocation (Store{PD slots_[kInline=8],
                    count, append-only overflow} block-resident, no per-object heap hop, no ~100%-
                    of-access `impl_->` trampoline); single PD type (owned + borrowed metadata coexist);
                    custom move (queue_mutex_ is now a DIRECT std::mutex -> non-movable -> manual move
                    ctor/assign); ChangeQueue include-cycle broken (ChangeQueue.h no longer includes
                    DependencyObject.h, which now holds a ChangeQueue by value). UIElement layout data
                    (children_/desired_size_/final_rect_/measure&arrange dirty) embedded inline. NEW
                    test/unit/BlockResidentStore_test.cpp (inline path, overflow growth, stable-key-
                    across-growth, move, single-PD coexist, UIElement collapsed-unit layout) proving the
                    S142 properties on the real S035 pipeline. Proper ownership: release_slot frees owned
                    PropertyMetadata* + stableKey PropertyKey* (borrowed never). ABOVE NOTE/RETRACTED
                    rationale recorded (ADR-003 revised + ADR-019). Verify: build/verify_s142.cmd
                    (windows-msvc msvc-debug: 619 test cases / 4735 assertions pass, 0 failed) +
                    ubuntu-clang clang-debug from /mnt/hgfs/VamlPOC (ctest 1/1 PASS). No commits.
                    (Prior — **ARCHITECTURAL CODIFICATION (pre-S142, docs-only)** — rewrote ADR-003 (removed
                    IP/ABI-hiding framing; static = build-model preference + LTO, this MIT-public lib has
                    no IP), added **ADR-019** (CPU/GPU line of responsibility: CPU owns scene graph/
                    inheritance/cascade decisions/coalescing/pipeline-building; GPU owns per-frame render
                    of known structures; fence = flattened pipeline buffers only; GPU never touches live
                    scene graph; no future step reverses it), rewrote architecture §7 + added §7a, and
                    revised EVERY ABI/IP-hiding comment site to "superseded/build-model plumbing only":
                    artifact-model.cmake (header + option comment + export comment + STATUS message),
                    roadmap (S140 tail, S142 rationale re-grounded on SPEED not ABI, Sprint 3.5 RATIONALE
                    correction note, S921), DependencyObject.h (storage-pimpl comment + register_property
                    unordered_map-reference doc), PropertyRegistry.h + DependencyProperty.h (the two
                    GLOBAL-REGISTER pimpl comments — ABI wording retracted, kept ONLY for include-surface
                    isolation until S143/S144 collapse them; removal is S143/S144 scope, NOT S142),
                    progress.md (header Distribution + S032/S034 pimpl-ABI strings). NO S142 implementation.
                    NO build ran. No commits.
                    (Prior — **S140 FULLY COMPLETE** + GREEN both lanes; the S140_2 split was
                    merged back — tree-text finished within S140) + **GREEN ON BOTH CI LANES**
                    (windows-msvc 613 tests/4661 asserts via build/verify_s140.cmd; ubuntu-clang
                    clang-debug 1/1 suite pass + S140_1/S141/S086/S088/S095 suites all green;
                    no commits)
- plan_repo_state : **S142 COMPLETE + GREEN (BOTH LANES)** — DependencyObject/UIElement pimpl collapsed
                    into ONE block-resident unit (Store{PD slots_[8], count, append-only overflow};
                    no per-object heap hop; single PD type — owned+borrowed coexist; stableKey keeps
                    register_property's returned const ref valid across overflow growth; custom move for
                    the non-movable direct std::mutex; ChangeQueue.h include-cycle broken). The store is
                    inline-started (0..kInline touches zero heap) + heap-grown on demand. UIElement layout
                    data embedded inline. BlockResidentStore_test.cpp proves it on the S035 pipeline.
                    windows-msvc 619 test cases / 4735 asserts pass (verify_s142.cmd); ubuntu-clang
                    clang-debug 1/1 suite pass. Next = S143 (mutex relocation + registry re-keying onto
                    S140 token + S141 store; global-register pimpl removal = S144). (Prior: **S140 (FULL)
                    + S140_1 + S141 COMPLETE + GREEN (BOTH LANES)** (S141 InternStore thread-local
                    lockless SIMD store + S140_1 vaml::intern() public primitive + S140 identity keys:
                    DependencyObject per-object store/subs + ChangeRecorder re-keyed to interned
                    string_view, killing the transient std::string(id.name()) at Get/Set/effective/
                    prop-subscribe/drain; PendingWrite + ChangeQueue kept owned std::string per
                    ADR-018 cross-thread rationale) + S140 TREE-TEXT: scanner token/attribute/clr-ns
                    intern-at-emission, ParsedElement/ParsedAttribute + element content intern at
                    finalize (ParseFrame contentScratch), VamlReader DeferredExtension/EventHandler +
                    convertLiteral string_view, S2 test TUs converted (incl. a fixed dangling-view bug
                    in ElementResolver_test.makeElement). Next = S142. Prior sprints complete
                    (Sprint 3 S120-S139 closed). (Prior full record below.)
                    S121 visual children; S122 layout slot geometry + alignment; S123 Measure contract
                    infinity-aware; S124 Arrange contract + clip; S125 leaf/panel measure archetypes
                    TEST-ONLY gate; S126 InvalidateMeasure/InvalidateArrange + LayoutManager dirty-subgraph
                    driver; S127 MeasureOverride/ArrangeOverride virtuals; S128 dirty-only subpass TEST-ONLY
                    gate; S129 StackPanel real panel done; S130 exhaustive DesiredSize/stretch suite done;
                    S130_1 margin-aware Measure resolution done; S131 Grid real panel done (row/col
                    definitions + Absolute/Auto/Star resolution + attached Grid.Row/Grid.Column + basic
                    no-span cell measure/arrange); S132 Grid ColumnSpan/RowSpan + span-aware two-pass
                    measure/arrange math done; S133 exhaustive star+span test-only suite done; **S134
                    WrapPanel REAL panel done (flow/wrap/aligned rows)**; **S135 WrapPanel dedicated
                    test-only suite done**).
                    S134 (dep:S133): REAL WrapPanel — UIElement-derived; WrapDirection DP (reuses the S129
                    Orientation enum: Vertical=0, Horizontal=1), default HORIZONTAL, not inherited,
                    self-registered (S043 attached-aware); flow/wrap/align algorithm in Element.cpp (measure
                    offers children Size::Infinite() on BOTH axes — natural-extent basis refined from the
                    accepted natural-width basis so rows are content-heighted; Horizontal wraps on max-row-
                    width, Desired={maxRowW,ΣrowH}; Vertical wraps on max-col-height, Desired={ΣcolW,maxColH};
                    arrange reproduces the wraps + gives each row its extent (row height = tallest) and
                    honors each child's OWN alignment/margin inside its slot); registered built-in (9→10,
                    ElementFactoryRegistry_test.cpp updated at THIS step). S135 (dep:S134): TEST-ONLY
                    dedicated WrapPanel suite (14 TEST_CASEs). S136 (dep:S135): REAL Canvas — UIElement-
                    derived absolute-position panel; TWO REAL ATTACHED DPs (Canvas.Left + Canvas.Top,
                    Double path, default NaN = "not set", NOT inherited, S043 attached-aware global path);
                    NaN contract introduced at this step; measure offers children Size::Infinite() on BOTH
                    axes, DesiredSize = bounding extent (eff coord + child extent, NaN axis -> 0, clamped
                    >= 0, never NaN); arrange places each child at its explicit origin (NaN -> 0 = canvas
                    origin), natural size, honoring child alignment + margin (compute_final_rect); positions
                    absolute, no flow; dirty-respect (S128); registered built-in (10→11, registry test
                    updated at THIS step). S137 (dep:S136): TEST-ONLY dedicated Canvas suite (14
                    TEST_CASEs). GREEN on both lanes from first principles.
- plan_digest     : S136 (dep:S135): MOD src/core/include/vaml/core/Element.h (Canvas class + attached
                    Left/Top DPs + comments) + MOD src/core/src/Element.cpp (register_canvas_properties +
                    real absolute-position MeasureOverride/ArrangeOverride + register "Canvas" built-in) +
                    MOD test/unit/ElementFactoryRegistry_test.cpp (10→11 count + Canvas resolve) + MOD
                    docs/roadmap.md (S136 [DONE]) + MOD session-context.md (this) + MOD progress.md (step
                    log). S137 (dep:S136): ADDED test/unit/Canvas_test.cpp (dedicated suite, TEST-ONLY) +
                    MOD test/unit/CMakeLists.txt (+Canvas_test.cpp) + ADDED build/verify_s137.cmd + MOD
                    docs/roadmap.md (S137 [DONE]) + MOD session-context.md (this) + MOD progress.md (step
                    log). S136 = REAL Canvas, NOT a seed (NEW element). DESIGN: attached Left/Top ride the
                    Double value path with default NaN = "not set" (the NaN contract, matching user-directive
                    to mirror WPF — introduced at this step); effX = is_nan(v) ? 0 : v in both measure
                    (DesiredSize bounding extent, clamped >= 0, never NaN) and arrange (NaN -> canvas origin,
                    effective top-left); measure offers children Size::Infinite() on BOTH axes (S123 infinity
                    rule, same basis as WrapPanel S134); arrange places each child at its explicit origin at
                    natural size, honoring child alignment + margin via compute_final_rect (S124/S130_1);
                    positions absolute (no flow); dirty-respect (S128); registered built-in count 10→11 with
                    the registry test updated at THIS step. NO MINIMAL REAL introduced. NO-GAP hand-off:
                    S137 = the dedicated Canvas test suite. S137 = the recorded dedicated Canvas suite (not
                    a backfill): 14 TEST_CASEs via a test-local RecorderLeaf (S125/S130 pattern) — derives
                    UIElement; Left/Top default NaN; typed round-trip; explicit 0 distinct from unset; S043
                    global resolution; natural-extent measure (+inf both axes); empty -> 0; bounding
                    DesiredSize incl. finite offsets + NaN-as-0 + clamp >= 0; explicit origin placement;
                    NaN -> (0,0); absolute multi-child; negative coords; margin interplay (S130_1) +
                    dirty-respect.
                    COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088,
                    S089, S090, S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125,
                    S126, S127, S128, S129, S130, S130_1, S131, S132, S133, S134, S135, S136, and S137 is
                    outstanding, awaiting your authorization.
                    (S130 record, historical context — superseded by the above: S130 was the dedicated
                    exhaustive StackPanel suite (TEST-ONLY) + S130_1 the margin-aware Measure resolution; the
                    detailed S130 block below is retained for the historical record.) **S130_1 (RESOLUTION, user-directed at review):
                    MOD src/core/src/UIElement.cpp (UIElement::Measure is now WPF-faithful margin handling —
                    shrink available by Margin, MeasureOverride(inner), finalize, ADD Margin back into
                    DesiredSize clamped to available — so a margin'd child reports a margin-inflated
                    desired_size that propagates into a parent panel's DesiredSize) + MOD src/core/src/
                    LayoutSlot.cpp (compute_final_rect now STRIPS the margin from the inflated DesiredSize
                    before aligning — the ArrangeCore-faithful innerDesired = DesiredSize - margin, so margin
                    is never counted twice) + MOD test/unit/StackPanel_stretch_test.cpp (D17 REWRITTEN to
                    assert the RESOLVED WPF-faithful behavior: a 50x20 child with Margin(10,5,10,5) measures
                    to desired 70x30 and raises a StackPanel's DesiredSize height to 60).**
                    S130 suite: test-local RecorderLeaf instrument (records exact available offer + final
                    slot; overrides MeasureOverride + ArrangeOverride) so Stretch-vs-Wrap is observable.
                    Four parts: PART A — panel DesiredSize under the S123 infinity-aware rule (finite fills
                    both axes; unbounded wraps to natural; mixed axes; non-Stretch clamps; horizontal
                    mirror). PART B — cross-axis stretch of children IN the stack (Stretch child stretches
                    the finite cross axis + wraps the infinite stack axis; non-Stretch does not blindly
                    stretch; unbounded panel wraps even a Stretch child; horizontal cross-axis mirror).
                    PART C — child arrange/stretch inside the panel (per-child alignment in the row; last-
                    child-stretch fills remaining vs non-Stretch keeps content; single child fills the whole
                    slot; non-Stretch panel produces NO phantom leftover; Margin insets the stretch fill;
                    horizontal last-child-stretch + VerticalAlignment in the column). PART D — DesiredSize
                    edges + dirty scoping (empty panel -> 0; recompute never cumulates after remove_child;
                    D17 now asserts margin ENTERS panel DesiredSize per S130_1; dirty-only re-measure/
                    re-arrange of a Stretch child with a clean sibling untouched).
- files_touched   :
    MODIFIED docs/roadmap.md                                (S138 [DONE] — REFACTOR GATE(S3) + S139 [DONE] — SURVIVOR)
    MODIFIED progress.md                                    (checkpoint regenerated last=S139/next=S140 + SPRINT 3 SURVIVOR block + guard rails MINIMAL REAL list)
    MODIFIED session-context.md (this — S138+S139 completion)
- build_status    : **GREEN — BOTH CI LANES (S142, verified from first principles).** windows-msvc:
                    build/verify_s142.cmd (msvc-debug, VS 2026) — warning-as-error clean; test exe
                    619 test cases / 4735 assertions pass / 0 failed (BlockResidentStore_test.cpp added).
                    ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug (Ubuntu VM) — configure+build clean +
                    ctest 1/1 PASS (100%). Only Clang warnings = pre-existing upstream <ciso646> doctest
                    deprecation (non-error). (windows-clang-debug is NOT a lane — not built/tested/flagged.)
                    No commits.
- next_step       : next roadmap token after S142 — **S143 (dep:S142)**: relocate the per-object pending
                    mutex into the thread-affine ChangeQueue AND re-key ChangeQueue/ChangeRecorder/
                    PropertyRegistry/DependencyProperty::GlobalTable onto the S140 token + S141 store
                    (the two S142-recorded hand-offs; global-register pimpl removal = S144 body). S142 is
                    COMPLETE + GREEN; S143 is the next unimplemented task.
- notes           :
    - **ARCHITECTURAL CODIFICATION PASS (pre-S142, docs-only, ~no build):** per the accepted decisions,
      all ABI/IP-hiding language is now REVISED to "superseded / build-model plumbing only" so S142's
      "[pimpl removed, not preserved]" reads as the deliberate end-state. ADR-003 rewrote as REVISED 2:
      this MIT-public lib has no IP to hide; static = build-model preference (+ determinism + LTO), NOT
      IP hiding; the app may statically link for ITS OWN IP (app-level). NEW ADR-019 codifies the CPU/GPU
      line + the "do NOT use GPU for what CPU must do; no future step reverses it" clause + the
      operational model (normal frame = CPU-nearly-idle + GPU-everything; change-driven cascade is the
      only CPU wake-up). architecture.md §7 rewritten (Distribution/build model) + new §7a (CPU/GPU
      line). Comment sites revised: artifact-model.cmake, DependencyObject.h, PropertyRegistry.h +
      DependencyProperty.h (the two global-register pimpl comments — ABI wording retracted per ADR-003;
      global-register pimpl REMOVAL is explicitly S143/S144 scope, recorded on S142/S143/S144, NOT
      changed here), roadmap (S140 tail + S142 rationale + Sprint 3.5 RATIONALE correction + S921),
      progress.md (header + S032 + S034). NO S142 implementation (that is a separate sprint step), no
      source changes, no build, no commits. S142 itself is UNTOUCHED and remains the next implemented step.
    - S138 = REFACTOR GATE(S3), docs-only (no code change). Both CI lanes verified from first principles
      at this gate (windows-msvc via build/verify_s137.cmd msvc-debug + ubuntu-clang via
      /mnt/hgfs/VamlPOC clang-debug) — both ctest 1/1 PASS, warning-clean. Grep-audit clean: no
      STUB/TODO/FIXME/backfill scaffold markers in src/core (the only "backfill"/"dead code"/"unused"
      hits are sanctioned affirmative "NOT a backfill — NO-GAP" statements in Color/Thickness.cpp + genuine
      code comments). No dead/unused layout surface: desired_size/final_rect/margin/alignment DPs/dirty
      bits/Measure-Arrange seam/LayoutManager all consumed by the four REAL panels or recorded future
      consumers (S208/S405). All MINIMAL REAL sentinels intact + forward-resolvable; StackPanel/Grid/
      WrapPanel/Canvas RESOLVED (sentinels removed at S129/S131/S134/S136). NO-GAP satisfied.
    - S139 = SURVIVOR + checkpoint advance, docs-only (no build). Added the consolidated `## SPRINT 3 —
      SURVIVOR` block (distills S120–S138); regenerated checkpoint to last=S139/next=S140/PENDING none;
      GUARD RAILS MINIMAL REAL list updated (StackPanel/Grid/WrapPanel/Canvas RESOLVED; carried forward:
      vulkan→S161, tools→S287, element seeds, event-hooks→S322, transform-owner→S208). DECISION
      (driver-confirmed at plan time, recorded): the checkpoint advances to S140 (the Sprint 3.5 first
      retrofit step, next unimplemented), NOT the literal "S147" of the original title — matching every
      prior wrap precedent and keeping the step list gapless (NO-GAP).
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128,
      S129, S130, S130_1, S131, S132, S133, S134, S135, S136, S137, S138, and S139 is outstanding,
      awaiting your authorization. After commit+push: repo is the committed good state; SPRINT 3 CLOSED,
      next roadmap token after S139 = S140 (Sprint 3.5 retrofit first step). Laptop-added files
      uncommitted as well — one commit covers all.
    - S136 delivers the REAL Canvas — a NEW element (no S087 seed existed), so the built-in element
      count rose 10→11 and the ElementFactoryRegistry_test.cpp `size().ShouldBe(10u)` assertions (two of
      them: the startup gate + the idempotent gate) were updated to 11 at THIS step, plus a Canvas
      registry-resolve assertion authored HERE (registration belongs to S136, not a silent backfill).
    - S136 NaN contract (user-directed at review — "we shall do the same" as WPF): Canvas.Left/Top
      default to NaN = "not set", the WPF-faithful sentinel, rather than my initially-recommended 0.0.
      The S1 value path had no NaN convention before S136, so the NaN contract is introduced explicitly
      at THIS step (part of the product, not a stub): "NaN on an attached positional axis = not set ->
      effective 0". Both Measure (DesiredSize bounding extent) and Arrange (canvas origin) collapse an
      un-set axis to 0 (effX = is_nan(v) ? 0 : v) BEFORE any arithmetic — so no NaN ever reaches the
      DesiredSize min/max fold (never negative, never NaN). An EXPLICIT `Left="0"` stays 0 and is distinct
      from unset (the S043 value path preserves the authored double; a test proves the round-trip).
    - S136 measures offers children Size::Infinite() on BOTH axes (the S123 infinity rule, same basis as
      WrapPanel S134) so a child measures at its NATURAL content extent regardless of its explicit
      position; DesiredSize = the bounding extent max over children of (eff coord + child extent), clamped
      >= 0. Arrange places each child at its explicit origin {effLeft, effTop} at natural size, honoring
      the child's OWN alignment + margin via compute_final_rect (S124/S130_1) — positions are ABSOLUTE
      (children at distinct origins do not stack/flow). Dirty-respect (S128): re-measure/re-arrange ONLY
      dirty children.
    - S137 is TEST-ONLY (zero new product symbols beyond S136) — the recorded dedicated Canvas suite
      via a test-local RecorderLeaf (S125/S130 pattern). 14 TEST_CASEs.
    - S135 is TEST-ONLY (zero new product symbols beyond S134) — the recorded dedicated WrapPanel suite
      via a test-local RecorderLeaf (S125/S130 pattern). 14 TEST_CASEs.
    - S131 RESOLVES the S087 Grid MINIMAL REAL seed at its own sprint (NO-GAP, not a backfill).
      GridLength.h is a pure header-only value type (like Color/Thickness/Geometry), NOT a MINIMAL REAL
      marker — complete + real on delivery. The attached DPs use the real global names "Grid.Row"/
      "Grid.Column" (the S091/S086 parser already passes dotted keys through by name), so the reader can
      declare a Grid's children in VAML (Grid.Row="0" etc.); row/column definitions are owned vectors
      settable via set_row_definitions/set_column_definitions (the S091 reader can set them once child-
      collection support lands — recorded, not silent). Scope boundary: S131 delivers definitions +
      resolution + BASIC no-span placement (each child occupies exactly one cell); RowSpan/ColumnSpan +
      the extended two-pass math (cells consumed by spans) is S132's NAMED scope; the exhaustive
      star-sizing + span suite is S133. Panel DesiredSize in the basic scope = Σ(Absolute+Auto) with Star
      contributing its MINIMUM (0) — recorded NO-GAP, not silent (min/sun sizing fidelity is S132/S133).
      StackPanel untouched; S120 transform-owner sentinel untouched (S208).
    - One test-meaning fix during bring-up (recorded): the first "totalStar == 0" resolver test mis-labelled
      a case where Star(0)+Star(2) had totalStar==2 (so Star(2) correctly got the whole remaining 400). Split
      into two correct cases — (a) zero-weight star resolves 0 while a positive sibling star takes the full
      share, and (b) TRUE totalStar==0 (all star weights zero) leaves every star track 0 with no div-by-zero/
      NaN.
    - S130 was TEST-ONLY (zero new product symbols) — the dedicated exhaustive gate S129 recorded as
      NO-GAP (not a silent backfill). Instrument = a test-local RecorderLeaf with a content extent that
      RECORDS the exact available offer + final slot it receives on both axes, so Stretch-vs-Wrap at
      both measure and arrange is observable on the real panel.
    - S130_1 (RESOLUTION at S130 review) — the D17 "fidelity gap" note was re-examined and the user
      directed making VAML's measure path margin-aware so child margin ENTERS the panel DesiredSize
      (WPF-faithful), flipping the D17 test. The PREMISE of my earlier D17 framing had the direction
      INVERTED vs real WPF: WPF's FrameworkElement.MeasureCore ADDS margin back into DesiredSize (so WPF
      StackPanel DesiredSize INCLUDES child margins), while the pre-S130_1 VAML measure path EXCLUDED it
      (margin was arrange-only) — so VAML UNDER-measured a margin'd panel vs WPF. S130_1 closed that gap:
      UIElement::Measure now shrinks available by Margin, measures inner, and ADDS Margin into
      desired_size (clamped to available); compute_final_rect (S124 arrange) STRIPS the margin from the
      inflated DesiredSize (innerDesired = DesiredSize - margin, WPF ArrangeCore-faithful) so margin is
      never double-counted. D17 rewritten to prove a 50x20 child with Margin(10,5,10,5) measures to 70x30
      and raises a StackPanel's DesiredSize height to 60. All prior tests stay green unchanged (no-margin
      tests unaffected; the arrange-strip recovers inner desired so the S124/S125/S127 margin arrange
      outcomes are preserved exactly; C13 unchanged — a Stretch child fills the inset slot since inner
      desired is never exceeded). The S130_1 resolution is recorded in the roadmap at its OWN step —
      NOT silent, NOT a backfill, and the S130 D17 note is now "RESOLVED" rather than NO-GAP.
    - One test fix during bring-up (recorded): the first D17 run failed because setting margin does NOT
      auto-invalidate the child's measure, and the panel (S128 dirty-respect) reuses a clean child's
      stored desired_size — so b stayed 50x20. Fixed by explicitly invalidating b's MEASURE
      (b->InvalidateMeasure()) before the panel re-measure, which is the correct dirty-driven flow.
    - NO NEW md files (directive honored). No commits made (per command).
    - S132 RESOLVES the S131 NO-GAP hand-off (the span dimension): attached Grid.RowSpan/Grid.ColumnSpan
      DPs (default 1, clamped ≥ 1 and to grid bounds) + span-aware two-pass measure/arrange. Decisions
      settled at review: D1 = star min/re-award (a Star track with a POSITIVE proportional share grows to
      at least its content minimum; a no-share star — totalStar==0 or zero weight — stays 0, keeping the
      S131 resolve contract). D2 = auto equal-division (a spanning child contributes desired/span-count to
      EACH spanned Auto track; single cell = span 1 = the S131 no-span behavior). Panel DesiredSize =
      Σ(non-star) + Σ(star content-minima). The D1 gate (`resolved > 0`) was added during bring-up so a
      star with no proportional share does NOT grow to content (keeps the S131 A4/A5/A6 star-collapses-to-0
      contract); the star re-award tests use recorder leaves that FILL their track (default-Stretch) so the
      track extent is observable as the slot width.
    - S133 is TEST-ONLY (zero new product symbols) — the recorded exhaustive star+span gate over the real
      S132 Grid. Instrument = a test-local RecordLeaf (S125/S130 pattern) recording measure offer +
      arrange slot on both axes. 17 TEST_CASEs in five parts (star proportional / star+span / auto+span /
      span placement / dirty-respect). A3 uses clean-exact numbers (remaining 300 split 2:1 = 200/100) so
      no float-tolerance is needed.
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128,
      S129, S130, S130_1, S131, S132, S133, S134, S135, S136, and S137 is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; SPRINT 3 continues (next
      roadmap token after S137 = S138 refactor gate). Laptop-added files uncommitted as well — one commit
      covers all.
