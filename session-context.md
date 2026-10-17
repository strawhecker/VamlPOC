# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S134+S135 (no commits; no NEW md files)
- plan_repo_state : S130+**S130_1**+**S131**+**S132**+**S133**+**S134**+**S135** complete — **SPRINT 3** (S120/UIElement base;
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
                    dedicated WrapPanel suite (14 TEST_CASEs). GREEN on both lanes from first principles.
- plan_digest     : S134 (dep:S133): MOD src/core/include/vaml/core/Element.h (WrapPanel class + WrapDirection
                    DP) + MOD src/core/src/Element.cpp (register_wrappanel_properties + real flow/wrap/align
                    MeasureOverride/ArrangeOverride + register "WrapPanel" built-in) + MOD
                    test/unit/ElementFactoryRegistry_test.cpp (9→10 count + WrapPanel resolve) + ADDED
                    build/verify_s134.cmd + MOD docs/roadmap.md (S134 [DONE]) + MOD session-context.md
                    (this) + MOD progress.md (step log). S135 (dep:S134): ADDED test/unit/WrapPanel_test.cpp
                    (dedicated suite, TEST-ONLY) + MOD test/unit/CMakeLists.txt (+WrapPanel_test.cpp) +
                    ADDED build/verify_s135.cmd + MOD docs/roadmap.md (S135 [DONE]) + MOD session-context.md
                    (this) + MOD progress.md (step log). S134 = REAL WrapPanel, NOT a seed (NEW element).
                    DESIGN: WrapDirection reuses the S129 Orientation enum (Vertical=0, Horizontal=1),
                    default HORIZONTAL (WPF-faithful), NOT inherited, self-registered via the S043 attached-
                    aware global path; property NAME "WrapDirection" (owner "WrapPanel") deliberately NOT
                    "Orientation" (StackPanel owns it in the global registry — a same-name pair under a
                    different owner would be ambiguous to the S091 dotted-key reader). MEASURE offers every
                    child Size::Infinite() on BOTH axes (the S123 infinity rule — a Stretch child wraps to
                    content; CROSS axis ALSO unbounded so rows are content-heighted, refining the accepted
                    "natural-width basis" to a "natural-extent basis" recorded in Element.cpp) and re-measures
                    ONLY is_measure_dirty() children (S128); Horizontal flows left-to-right, WRAPS when
                    `rowWidth + childWidth > available.width` (checked ONLY when the row is non-empty, so a
                    child wider than the whole boundary still gets its own row), DesiredSize = {max row
                    width, Σ row heights}; Vertical is the mirror (top-to-bottom columns, wrap on height,
                    DesiredSize = {Σ column widths, max column height}). ARRANGE reproduces the same wraps
                    (boundary = final_rect extent), gives each row its extent (row height = tallest child on
                    Horizontal), places each child into its row slot {cursor, rowY, childDesiredW, rowHeight}
                    via child->Arrange (honoring the child's OWN alignment + margin inside the slot via S124
                    compute_final_rect — a default-Stretch child fills the row's cross extent while keeping
                    its natural flow extent = the "aligned rows" headline), re-arranging ONLY is_arrange_dirty()
                    children (S128). Registered as built-in (count 9→10; ElementFactoryRegistry_test.cpp size
                    assertions + a WrapPanel resolve assertion updated HERE, not silent). NO ItemWidth/
                    ItemHeight (WPF fixed-item mode — not in the S134 title, recorded NO-GAP deferred, not
                    fabricated). NO MINIMAL REAL introduced. S135 = the recorded dedicated WrapPanel test
                    suite (not a backfill): 14 TEST_CASEs via a test-local RecorderLeaf (S125/S130 pattern) —
                    derives UIElement; WrapDirection default Horizontal + round-trip + global S043 resolution;
                    default-Horizontal overflow WRAPS; over-wide child gets its own row; DesiredSize
                    Horizontal = max/Σ; children measured at natural extent (+inf both axes); aligned rows
                    (row height = tallest; Stretch fills row height; per-child Top/Center/Bottom); vertical
                    mirror (WRAPS to new column + DesiredSize Σ/max); empty -> 0; dirty-respect; margin enters
                    DesiredSize (S130_1). NO-GAP clean on both steps.
                    COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088,
                    S089, S090, S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125,
                    S126, S127, S128, S129, S130, S130_1, S131, S132, S133, S134, and S135 is outstanding,
                    awaiting your authorization.
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
    MODIFIED src/core/include/vaml/core/Element.h            (S134: WrapPanel class + WrapDirection DP + comments)
    MODIFIED src/core/src/Element.cpp                        (S134: register_wrappanel_properties + real flow/wrap/align MeasureOverride/ArrangeOverride + register "WrapPanel" built-in)
    MODIFIED test/unit/ElementFactoryRegistry_test.cpp       (S134: 9→10 built-in count + WrapPanel resolve assertion)
    ADDED    test/unit/WrapPanel_test.cpp                    (S135 dedicated WrapPanel suite)
    MODIFIED test/unit/CMakeLists.txt                        (+ WrapPanel_test.cpp)
    ADDED    build/verify_s134.cmd                           (S134 windows-msvc lane)
    ADDED    build/verify_s135.cmd                           (S135 windows-msvc lane)
    MODIFIED docs/roadmap.md                                 (S134 [DONE] + S135 [DONE] + guard-rail WrapPanel resolution)
    MODIFIED session-context.md (this — S134+S135 completion)
    MODIFIED progress.md                                     (checkpoint + guard rails + step log)
- build_status    : GREEN — BOTH CI LANES (verified from first principles at S134+S135). windows-msvc:
                    msvc-debug (VS 2026) via build/verify_s135.cmd (re-runs S134+S135 together) —
                    configure+build warning-clean + ctest 1/1 PASS (all S134/S135 cases green, all prior
                    S133/S132/S131/S130/S130_1/S129/S120-S128 still green). ubuntu-clang: clang-debug
                    (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC) configure+build + ctest 1/1
                    PASS (WrapPanel tests all green). Only Clang warnings = pre-existing upstream <ciso646>
                    doctest deprecation from third_party (not this TU; non-error, already downgraded via
                    -Wno-error=#warnings in the test CMakeLists). (windows-clang-debug is NOT a lane — not
                    built/tested/flagged per standing rule.)
- next_step       : next roadmap token after S135 (Sprint 3 — S134/S135 committed to roadmap as done; the
                    WrapPanel control + its test suite are complete; S135 is the last defined Sprint-3
                    token, so the next token is to be assigned per the roadmap conventions).
- notes           :
    - S134 delivers the REAL WrapPanel — a NEW element (no S087 seed existed), so the built-in element
      count rose 9→10 and the ElementFactoryRegistry_test.cpp `size().ShouldBe(9u)` assertions (two of
      them: the startup gate + the idempotent gate) were updated to 10 at THIS step, plus a WrapPanel
      registry-resolve assertion authored HERE (registration belongs to S134, not a silent backfill).
    - S134 refinement (recorded in Element.cpp): the S134 measure offers children Size::Infinite() on
      BOTH axes — the accepted plan said "natural-width basis" (unbounded flow axis), but the CROSS axis
      is ALSO offered unbounded so a default-Stretch child cannot fill a finite cross offer: rows stay
      CONTENT-HEIGHTED (row height = tallest child), which is what makes the "aligned rows" headline
      (per-child alignment within a content row) real. Refined at implementation, documented in
      Element.cpp + roadmap S134.
    - S134 property NAME is "WrapDirection" (owner "WrapPanel") — deliberately NOT "Orientation"
      (StackPanel owns "Orientation" in the global registry; a same-name pair under a different owner
      would be ambiguous to the S091 dotted-key dotted resolution). It rides the Int32 value path via
      the S129 Orientation enum (Vertical=0, Horizontal=1), default Horizontal (WPF-faithful), NOT
      inherited.
    - NO ItemWidth/ItemHeight (WPF fixed-item mode) — not in the S134 title; recorded NO-GAP (deferred,
      not fabricated). No MINIMAL REAL introduced (− no new sentinel; − no resolution beyond S129/S131
      stays pending for WrapPanel).
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
      S129, S130, S130_1, S131, S132, S133, S134, and S135 is outstanding, awaiting your authorization.
      After commit+push: repo is the committed good state; SPRINT 3 continues (next roadmap token after
      S135). Laptop-added files uncommitted as well — one commit covers all.
