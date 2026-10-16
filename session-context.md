# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S126 (no commits; no NEW md files)
- plan_repo_state : S126 complete — **SPRINT 3** (S120/UIElement base; S121 visual children; S122 layout
                    slot geometry + alignment; S123 Measure contract infinity-aware; S124 Arrange contract +
                    clip; S125 leaf/panel measure archetypes TEST-ONLY gate; S126 InvalidateMeasure/
                    InvalidateArrange + LayoutManager dirty-subgraph driver done; S127 next). S126 (dep:S125)
                    delivered the WPF-faithful dirty-flag + layout-driver contract: UIElement dirty bits
                    (initial TRUE) + InvalidateMeasure/InvalidateArrange (short-circuit + upward propagation)
                    + vaml::LayoutManager (non-owning driver; UpdateLayout = measure pass iff root
                    measure-dirty, arrange pass iff root arrange-dirty -> clean UpdateLayout is a TRUE
                    no-op). GREEN on both lanes from first principles.
- plan_digest     : S126 (dep:S125): MOD src/core/include/vaml/core/UIElement.h + MOD src/core/src/UIElement.cpp
                    (dirty bits + InvalidateMeasure/InvalidateArrange + accessors; Measure/Arrange clear own
                    bits on completion), ADDED src/core/include/vaml/core/LayoutManager.h + ADDED
                    src/core/src/LayoutManager.cpp (non-owning driver, UpdateLayout gated on root dirty bits),
                    MOD src/core/CMakeLists.txt (+LayoutManager.cpp), ADDED test/unit/LayoutManager_test.cpp
                    (7 TEST_CASEs + CountingLeaf + DirtyAwarePanel test-local subclasses), MOD
                    test/unit/CMakeLists.txt (+LayoutManager_test.cpp), ADDED build/verify_s126.cmd, MOD
                    docs/roadmap.md — S126 [DONE] with full detail.
- files_touched   :
    MODIFIED src/core/include/vaml/core/UIElement.h        (S126 invalidation surface + accessors)
    MODIFIED src/core/src/UIElement.cpp                    (dirty bits, InvalidateMeasure/Arrange, clear-on-complete)
    ADDED    src/core/include/vaml/core/LayoutManager.h    (S126 non-owning layout driver)
    ADDED    src/core/src/LayoutManager.cpp                (UpdateLayout over dirty subgraph)
    MODIFIED src/core/CMakeLists.txt                       (+ LayoutManager.cpp)
    ADDED    test/unit/LayoutManager_test.cpp              (S126 tests)
    MODIFIED test/unit/CMakeLists.txt                      (+ LayoutManager_test.cpp)
    ADDED    build/verify_s126.cmd                         (S126 windows-msvc lane)
    MODIFIED docs/roadmap.md                               (S126 [DONE])
    MODIFIED session-context.md (this — S126 completion)
- build_status    : GREEN — BOTH CI LANES (verified from first principles at S126). windows-msvc:
                    msvc-debug (VS 2026) via build/verify_s126.cmd — configure+build warning-clean +
                    ctest 1/1 PASS (LayoutManager_test 7 cases green). ubuntu-clang:
                    clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC) configure+build +
                    ctest 1/1 PASS. Only Clang warnings = pre-existing upstream <ciso646> doctest
                    deprecation from third_party (not this TU; non-error, already downgraded via
                    -Wno-error=#warnings in the test CMakeLists). (windows-clang-debug is NOT a lane — not
                    built/tested/flagged per standing rule.)
- next_step       : S127. S127 (dep:S126) = Sprint 3, eighth token — MeasureOverride/ArrangeOverride
                    virtuals for panels.
- notes           :
    - S126 real product surface (no MINIMAL REAL, no STUB): UIElement dirty flags + invalidation +
      accessors; vaml::LayoutManager (pimpl'd, move-only, non-owning driver). Dirty bits are initial TRUE
      (a fresh element is never laid out). InvalidateMeasure/InvalidateArrange: short-circuit when already
      set (no redundant upward walk) + propagate to root via the S039/S041 parent link, stopping at
      parent()==nullptr and at the first already-dirty ancestor.
    - LayoutManager::UpdateLayout: measure pass iff root is measure-dirty (root->Measure(Size::Infinite()),
      top-level has no external slot); arrange pass iff root arrange-dirty (root->Arrange(its measured
      DesiredSize at origin 0); a real window viewport arrives at the window sprints, recorded NO-GAP).
      A clean UpdateLayout is a TRUE NO-OP. Measure/Arrange (S123/S124) clear their own dirty bit on
      completion.
    - Dirty-subgraph realization lives where offers live — in panels. The measure-subgraph is driven
      through the S123 virtual measure_natural_size() seam: a dirty-aware panel re-measures ONLY dirty
      children (child->is_measure_dirty()), reusing clean children's stored desired_size(). Arrange
      dirty-respect is proven with a test-local panel's arrange method (re-arranges only dirty children,
      leaving clean children's final_rect untouched). S127 completes auto-arrange via ArrangeOverride —
      the S123/S124 seam deliberately has NO virtual Arrange (recorded at S124, not silent).
    - Tests (Shouldly vocabulary, 7 TEST_CASEs): fresh root dirty + first UpdateLayout measures+arranges +
      clears both axes; clean UpdateLayout true no-op (leaf NOT re-measured); leaf InvalidateMeasure
      propagates to root + sibling branch stays clean (arrange axis untouched); coalescing (two leaf
      invalidations before ONE UpdateLayout -> exactly ONE panel measure pass + each dirty leaf re-measured
      once); short-circuit (invalidating already-dirty element changes nothing + one pass still cleans);
      panel re-measures only dirty child (clean leaf's measure-count unchanged) + re-arranges only dirty
      child (clean leaf's final_rect untouched); measure/arrange are independent axes.
    - NO-GAP: S127 upgrades the seam to MeasureOverride/ArrangeOverride (arrange subgraph auto-drive
      completes there — the arrange-side of the driver); S128 formal dirty-only-subpass gate; S129/S131
      real StackPanel/Grid adopt dirty-respect. S126 does NOT redesign the measure seam, does NOT rewrite
      the S125 archetype (kept as its own proof), does NOT re-base the seeded controls (their sentinels
      forbid it).
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, and now S126 is
      outstanding, awaiting your authorization. After commit+push: repo is the committed good state;
      SPRINT 3 continues (next is S127). Laptop-added files uncommitted as well — one commit covers all.
