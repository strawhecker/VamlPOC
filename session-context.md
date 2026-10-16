# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S129 (no commits; no NEW md files)
- plan_repo_state : S129 complete — **SPRINT 3** (S120/UIElement base; S121 visual children; S122 layout
                    slot geometry + alignment; S123 Measure contract infinity-aware; S124 Arrange contract +
                    clip; S125 leaf/panel measure archetypes TEST-ONLY gate; S126 InvalidateMeasure/
                    InvalidateArrange + LayoutManager dirty-subgraph driver; S127 MeasureOverride/
                    ArrangeOverride virtuals; S128 dirty-only subpass TEST-ONLY gate; S129 StackPanel real
                    panel done; S130 next — exhaustive DesiredSize/stretch suite).
                    S129 (dep:S128) RESOLVES the S087 StackPanel MINIMAL REAL seed at its own sprint
                    (NO-GAP, not a backfill): S125's ChildMeasuringPanel archetype rule ("real StackPanel
                    algorithm + last-child-stretch -> S129") raised to the REAL control. StackPanel is
                    RE-BASED onto UIElement (was DependencyObject), gains a real Orientation DP
                    (Vertical/Horizontal) + MeasureOverride + ArrangeOverride with last-child-stretch and
                    the S128 dirty-respect. GREEN on both lanes from first principles.
- plan_digest     : S129 (dep:S128): MOD src/core/include/vaml/core/Element.h (StackPanel re-based onto
                    UIElement [was DependencyObject]; new enum class Orientation { Vertical=0,
                    Horizontal=1 } riding the Int32 value path; real Orientation DP default Vertical not
                    inherited; typed set_orientation/orientation; protected MeasureOverride/ArrangeOverride
                    overrides declared; the S087 StackPanel MINIMAL REAL sentinel REMOVED at its own
                    resolution sprint), MOD src/core/src/Element.cpp (StackPanel ctor derives UIElement +
                    self-registers Orientation via Meyer-static ensure_stackpanel_properties_registered;
                    MeasureOverride orientation-aware [vertical: max-w/Σ-h, horizontal: Σ-w/max-h] with
                    S128 dirty-only child re-measure; ArrangeOverride stacks top-to-bottom / left-to-right
                    giving the LAST child the remaining track [last-child-stretch] with S128 dirty-only
                    child re-arrange), ADDED test/unit/StackPanel_test.cpp (14 TEST_CASEs: derivation;
                    orientation default/round-trip/attached-aware-global; vertical + horizontal stacking;
                    last-child-stretch Stretch-fills vs non-Stretch-does-not; DesiredSize derivation;
                    dirty-respect clean-child-not-re-measured/re-arranged), MOD test/unit/CMakeLists.txt
                    (+StackPanel_test.cpp), ADDED build/verify_s129.cmd, MOD docs/roadmap.md — S129 [DONE]
                    with full detail + StackPanel marked RESOLVED in GUARD RAILS.
- files_touched   :
    MODIFIED src/core/include/vaml/core/Element.h           (StackPanel re-based onto UIElement + Orientation DP + sentinel removed)
    MODIFIED src/core/src/Element.cpp                       (StackPanel ctor + Orientation accessors + MeasureOverride/ArrangeOverride)
    ADDED    test/unit/StackPanel_test.cpp                  (S129 real-panel proof, 14 TEST_CASEs)
    MODIFIED test/unit/CMakeLists.txt                       (+ StackPanel_test.cpp)
    ADDED    build/verify_s129.cmd                          (S129 windows-msvc lane)
    MODIFIED docs/roadmap.md                                (S129 [DONE] + StackPanel RESOLVED in GUARD RAILS)
    MODIFIED session-context.md (this — S129 completion)
- build_status    : GREEN — BOTH CI LANES (verified from first principles at S129). windows-msvc:
                    msvc-debug (VS 2026) via build/verify_s129.cmd — configure+build warning-clean +
                    ctest 1/1 PASS (StackPanel_test 14 cases green INSIDE the full suite, 502 cases;
                    all prior S120-S128 tests still green, incl. GoldenDocs which exercises StackPanel
                    through the reader). ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21,
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS (502 cases green, S129:* 14 cases
                    all green). Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation
                    from third_party (not this TU; non-error, already downgraded via -Wno-error=#warnings
                    in the test CMakeLists). (windows-clang-debug is NOT a lane — not built/tested/flagged
                    per standing rule.)
- next_step       : S130. S130 (dep:S129) = Sprint 3, eleventh token — StackPanel DesiredSize + stretch
                    correctness tests (the dedicated exhaustive suite; recorded NO-GAP at S129, not a
                    silent backfill).
- notes           :
    - S129 is PRODUCT (not TEST-ONLY): it RESOLVES the S087 StackPanel MINIMAL REAL seed at its own
      resolution sprint (NO-GAP, not a backfill) and ships a REAL panel control. This is the FIRST
      control seed to reach its resolution, so it sets the precedent for the remaining seeds (Grid->S131,
      Button->S401, Text->S400, TextBox->S402, CheckBox->S404, Slider->S403, ScrollViewer->S405,
      Window->S440): re-base onto UIElement at the control's own sprint, remove the sentinel, author the
      real behavior there.
    - PRODUCT content: StackPanel derives UIElement (visible-tree + Measure/Arrange participation); new
      enum class Orientation { Vertical=0, Horizontal=1 } on the Int32 value path; real Orientation DP
      (default Vertical, NOT inherited) self-registered via the S043 attached-aware global path
      (Meyers-static ensure_stackpanel_properties_registered, mirroring UIElement) + typed accessors
      also settable via Set/Get(PropertyId("Orientation")). MeasureOverride is orientation-aware
      (vertical: offer {available.w, ∞h}, natural = {max child w, Σ child h}; horizontal: {∞w,
      available.h}, natural = {Σ child w, max child h}) and re-measures ONLY dirty children (clean
      desired_size reused — the S128 dirty-only-subpass pattern realized INSIDE the real panel).
      ArrangeOverride stacks children top-to-bottom / left-to-right and gives the LAST child the
      remaining track (last-child-stretch: remaining = finalSlot.bottom()-y vertically /
      finalSlot.right()-x horizontally), honoring each child's OWN alignment inside its slot via
      child->Arrange (compute_final_rect, so a default-Stretch last child fills the leftover track while
      a non-Stretch last child does NOT over-fill), and re-arranges ONLY dirty children (S128).
    - 14 TEST_CASEs (Shouldly vocabulary, S129-unique "Orientation" global DP): derives UIElement via
      dynamic_cast (still DependencyObject transitively); Orientation defaults Vertical; typed round-trip
      Vertical<->Horizontal; Orientation resolves globally on a plain DependencyObject (S043 attached-
      aware); vertical stacking (children top-to-bottom in order, correct final_rects); vertical per-
      child HorizontalAlignment honored within its full-width row (Left x=0 vs Center x=450 in a
      1000-wide row); horizontal stacking (left-to-right); last-child-stretch (last default-Stretch
      child fills remaining 70 height after a 30 first child; a non-Stretch last child keeps its content
      height 10 instead of over-filling 70; horizontal mirror fills remaining 100 width); panel
      DesiredSize (vertical: max-w/Σ-h = 100/50; horizontal: Σ-w/max-h = 150/30); dirty-respect (a clean
      child is NOT re-measured nor re-arranged; an arrange-invalidated child IS re-arranged but a clean
      one is not).
    - NO-GAP clean: resolves the StackPanel seed; no new MINIMAL REAL introduced. S130 (dep:S129) = the
      dedicated exhaustive DesiredSize + stretch-correctness suite (recorded, NOT silent). S131 (Grid)
      adopts this panel Measure/Arrange + dirty-respect pattern.
    - Two test fixes during bring-up (recorded): panel needed to be default-Stretch (not Left-aligned).
      before per-child-alignment was meaningful, and the override virtuals had to be declared in the
      header. Both resolved; suite green.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128, and
      now S129 is outstanding, awaiting your authorization. After commit+push: repo is the committed good
      state; SPRINT 3 continues (next is S130). Laptop-added files uncommitted as well — one commit
      covers all.
