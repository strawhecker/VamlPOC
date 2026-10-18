# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **S146 FULLY COMPLETE — BOTH CI LANES GREEN + ASan NO-LEAK** (REFACTOR GATE(S3.5),
                    dep:S145; dead pre-retrofit surface removed; public API string_view/intern-only;
                    steady-state heap-churn counter feeding S628). (1) owned-name gotcha — the last
                    retained unordered_map node, `DependencyObject::subs_` (std::unordered_map<string_view,
                    handlers>), is re-keyed so BOTH `add_property_changed` and the queue drain call
                    `vaml::intern(id.name())`: the subscriber key is now a POOL-STABLE retain-forever
                    view, never a transient caller-owned view (subscriptions thread-affine per
                    S037/ADR-018 — ZERO new locks). (2) ASan-gate teardown — NEW public
                    `vaml::intern_master_destroy_all()` (mirrors object_pool_master_destroy_all) +
                    Asan_teardown_test.cpp installs an exit-time destructor hook that frees both
                    retain-forever pools AFTER all tests; the S141/S145 thread_local destructors also
                    free at thread exit, so the ASan lane is deterministic (zero-leak). (3) steady-state
                    heap-churn counter (feeds S628) — NEW vaml::detail::HeapStats (relaxed atomic) +
                    public `vaml::heap_allocation_count()`/`heap_allocation_bytes()`, funneled ONLY on
                    the VAML-owned monotonic growth paths (InternStore arena/entry growth + ObjectPool
                    block spill), NOT global new/delete overrides (which would trap doctest/download noise
                    + collide with the host allocator). (4) audit — the five title dead-surface categories
                    crossed OFF: ALL remaining pimpls COLLAPSED (ChangeQueue/ChangeRecorder/
                    ChangeProcessor/DependencyProperty/PropertyMetadata/ElementFactoryRegistry/
                    PropertyRegistry/ValueConverter/LayoutManager + Grid::Impl) — no Impl heap node or
                    impl_-> trampoline survives; NameTable.h PROMOTED to vaml/core/detail/NameTable.h to
                    legalize the registry collapses; the ChangeQueue std::mutex inlined as a non-moved
                    member (sole kept approved lock, ADR-018); ObjectPool slot RAISED to 1216 because
                    the collapse grew sizeof(Grid) to 1168 (msvc-debug checked iterators);
                    by-value children() already out-param (S144, re-verified); parser/scanner-owned
                    std::string members (XmlScanner::stack_/nsStack_ + ParseFrame::contentScratch)
                    documented-not-converted as INTERNAL booking buffers (not public semantic owned
                    strings); ParsedElement::content already string_view (S140); VamlReader
                    ::event_names_/root_class_ are S092 MINIMAL REAL reader-owned seeds. (5) public-API
                    audit — every register*/find/try_*/Get/named/names/Load entry point takes string_view
                    + intern() views, asserted in HeapChurn_test.cpp. VERIFIED BOTH LANES + ASan:
                    windows-msvc build/verify_s146.cmd (msvc-debug, warning-clean, full suite PASS) +
                    ubuntu-clang clang-debug from /mnt/hgfs/VamlPOC (ctest 1/1 PASS) + ASan zero-leak
                    both compilers (build/verify_s146_asan.cmd). windows-clang-debug NOT a lane. No commits.
- plan_repo_state : **S146 COMPLETE + GREEN (BOTH LANES) + ASan ZERO-LEAK** — no dead pre-retrofit surface
                    survives: the only unordered_map node (subs_) is now intern-keyed (pool-stable), every
                    hot content-probe is NameTable-intermed (S143/S144), by-value children() is out-param
                    (S144), and the parser/scanner owned-string members are documented-internal catering
                    buffers. NEW surface: public vaml::intern_master_destroy_all() + vaml::detail::
                    HeapStats + public heap_allocation_count()/bytes() (the S628-feed steady-state
                    heap-churn counter). ADR-018: ZERO new locks (subs_ stays lockless thread-local;
                    HeapStats uses a relaxed atomic; the sole kept, approved lock remains the S143-relocated
                    ChangeQueue mutex, now an inlined non-moved member). DependencyProperty's redundant
                    `type_` member was removed during the collapse — `metadata_.boundType()` is the sole
                    live bound-type source (also fixes a clang -Wunused-private-field -Werror). Next = S147
                    (SURVIVOR + checkpoint; re-target S160's dep to S147; record the allocator-policy ADR
                    (feeds S722) + thread-local pool note (feeds S624/S626) + reaffirm LOCK GOVERNANCE
                    ADR-018).
- plan_digest     : S146 = MOD src/core/src/DependencyObject.cpp (subs_ intern-keying) + MOD
                    src/core/include/vaml/core/Intern.h + src/core/src/Intern.cpp (+ intern_master_destroy_all)
                    + ADDED vaml/core/HeapStats.h + src/core/src/HeapStats.{h,cpp} + MOD InternStore.cpp +
                    ObjectPool.cpp (funnel HeapStats) + PIMPL-COLLAPSE: MOD ChangeQueue/ChangeRecorder/
                    ChangeProcessor/DependencyProperty/PropertyMetadata/ElementFactoryRegistry/
                    PropertyRegistry/ValueConverter/LayoutManager/Element (.h+.cpp; Impl members → by-value);
                    NEW src/core/include/vaml/core/detail/NameTable.h (PROMOTED from src/core/src/NameTable.h,
                    then the old src file DELETED); MOD src/core/src/ObjectPool.h (kSlotSize 960 → 1216) +
                    src/core/CMakeLists.txt (+HeapStats.cpp) + ADDED test/unit/HeapChurn_test.cpp +
                    Asan_teardown_test.cpp + MOD test/unit/CMakeLists.txt + ADDED build/verify_s146.cmd +
                    verify_s146_asan.cmd + MOD docs/roadmap.md + progress.md + session-context.md (this).
- files_touched   :
    MODIFIED src/core/src/DependencyObject.cpp                 (subs_ intern-key: add_property_changed + drain)
    MODIFIED src/core/include/vaml/core/Intern.h               (+ public intern_master_destroy_all)
    MODIFIED src/core/src/Intern.cpp                           (+ intern_master_destroy_all impl)
    ADDED src/core/include/vaml/core/HeapStats.h               (public heap_allocation_count/bytes contract)
    ADDED src/core/src/HeapStats.h                             (vaml::detail::HeapStats singleton)
    ADDED src/core/src/HeapStats.cpp                           (public count/bytes definitions)
    MODIFIED src/core/src/InternStore.cpp                      (funnel HeapStats on arena/entry growth)
    MODIFIED src/core/src/ObjectPool.cpp                       (funnel HeapStats on block spill)
    MODIFIED src/core/src/ObjectPool.h                         (kSlotSize 960 -> 1216: Grid grew to 1168)
    MODIFIED src/core/src/ChangeQueue.{h,cpp}                  (PIMPL COLLAPSED: deques + mutex + affinity by-value)
    MODIFIED src/core/src/ChangeRecorder.{h,cpp}               (PIMPL COLLAPSED: order + byName by-value; Pending public)
    MODIFIED src/core/src/ChangeProcessor.{h,cpp}              (PIMPL COLLAPSED: queue by-value)
    MODIFIED src/core/src/DependencyProperty.{h,cpp}           (PIMPL COLLAPSED: name_/type_/metadata_/ownerTypeId_ by-value; dtor now public)
    MODIFIED src/core/src/PropertyMetadata.{h,cpp}             (PIMPL COLLAPSED: default/flags/callbacks by-value)
    MODIFIED src/core/src/ElementFactoryRegistry.{h,cpp}       (PIMPL COLLAPSED: NameTable by-value)
    MODIFIED src/core/src/PropertyRegistry.{h,cpp}             (PIMPL COLLAPSED: NameTable by-value; name-copy-before-move fix)
    MODIFIED src/core/src/ValueConverter.{h,cpp}               (PIMPL COLLAPSED: NameTable by-value)
    MODIFIED src/core/src/LayoutManager.{h,cpp}                (PIMPL COLLAPSED: root_ by-value)
    MODIFIED src/core/include/vaml/core/Element.h + src/core/src/Element.cpp   (Grid::Impl COLLAPSED: rows_/cols_ by-value)
    ADDED src/core/include/vaml/core/detail/NameTable.h        (PROMOTED from src/core/src/NameTable.h)
    DELETED src/core/src/NameTable.h                           (superseded by the public detail tree PROMOTION)
    MODIFIED src/core/CMakeLists.txt                           (+ src/HeapStats.cpp)
    ADDED test/unit/HeapChurn_test.cpp                         (S146 public-API audit + steady-state churn)
    ADDED test/unit/Asan_teardown_test.cpp                     (S146 ASan exit-time pool teardown)
    MODIFIED test/unit/CMakeLists.txt                          (+ HeapChurn_test.cpp + Asan_teardown_test.cpp)
    ADDED build/verify_s146.cmd                                (windows-msvc lane driver)
    ADDED build/verify_s146_asan.cmd                           (ASan zero-leak lane driver)
    MODIFIED docs/roadmap.md                                   (S146 [DONE]: pimpls COLLAPSED)
    MODIFIED progress.md                                       (step log + checkpoint last=S146/next=S147)
    MODIFIED session-context.md (this)
    NOTE: src/core/include/vaml/core/export.h + src/tools + src/vulkan (clang __attribute__ deprecated
          adaptation) ALREADY modified in the working tree from a prior sprint — NOT touched by S146.
- build_status    : **GREEN — BOTH CI LANES + ASan NO-LEAK (S146, verified from first principles).**
                    windows-msvc: build/verify_s146.cmd (msvc-debug, VS 2026) — warning-as-error clean,
                    641 test cases / 5352 assertions / 0 failed, full suite PASS incl. new
                    HeapChurn_test.cpp (3 cases / 31 assertions) + Asan_teardown_test.cpp.
                    ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug (Ubuntu VM, clang 21) — configure+build
                    clean (after removing the redundant DependencyProperty::type_ member) + ctest 1/1
                    PASS, 641 cases SUCCESS. ASan NO-LEAK: clang-asan preset on the VM (clang
                    -fsanitize=address + LSan detect_leaks=1) — 641/641 SUCCESS, ZERO LeakSanitizer
                    reports, exit 0 (the exit-time Asan_teardown hook frees the retain-forever pools so
                    LSan sees a clean heap); windows msvc-asan (build/verify_s146_asan.cmd) —
                    100% pass with zero ASan memory errors (MSVC ASan has no LSan). Only Clang warnings
                    = pre-existing upstream <ciso646> doctest deprecation (non-error). (windows-clang-debug
                    is NOT a lane — not built/tested/flagged.) No commits.
- next_step       : next roadmap token after S146 — **S147 (dep:S146)**: SURVIVOR + checkpoint;
                    re-target S160's dep to S147 (already dep:S147 in roadmap); record the allocator-policy
                    ADR (feeds S722) + thread-local pool note (feeds S624/S626) + reaffirm LOCK GOVERNANCE
                    ADR-018 (Sprint 3.5 added zero new locks; the S143 mutex relocation is the sole kept,
                    approved lock). S146 is COMPLETE + GREEN (+ ASan clean); S147 is the next unimplemented
                    task.
- notes           :
    - The S146 title's "pimpl heap nodes" was satisfied in FULL: the S142 DependencyObject/UIElement
      pimpl (already collapsed, no redo) AND every OTHER remaining pimpl (ChangeQueue/ChangeRecorder/
      ChangeProcessor/DependencyProperty/PropertyMetadata/ElementFactoryRegistry/PropertyRegistry/
      ValueConverter/LayoutManager/Grid) was COLLAPSED so its owned runtime state is now plain by-value
      members. The registries' vaml::detail::NameTable probes were legalized by PROMOTING NameTable.h
      into the public detail tree (vaml/core/detail/NameTable.h — stays in vaml::detail, not a promised
      API). The ChangeQueue std::mutex (the S143-relocated sole kept approved lock) became a direct
      member: an unmovable std::mutex stays in place across the move ctor/assign, with only the protected
      deques + affinity moved under lock (standard mutex-owner practice). ObjectPool::kSlotSize was
      raised 960 → 1216 because the collapse grew sizeof(Grid) to 1168 in the msvc-debug lane (the worst
      poolable case; release Linux ~1040) — the inline vectors + the S142 inline Store now ride a
      1216-byte slot. No dead surface left behind; everything resolved at its own sprint (NO-GAP).
    - HeapStats is registered on the MONOTONIC growth paths only (InternStore arena/entry growth,
      ObjectPool block spill), NEVER on recycle/dedup-hit. That is what makes the S146 steady-state
      assertion meaningful: after warmup, an identical workload is FLAT (counter does not grow) because
      steady state is pure recycle + content-dedup. Global new/delete overrides were deliberately NOT used
      (they would trap doctest/download noise + collide with the host allocator).
    - The ASan teardown ordering matters: Asan_teardown_test.cpp is an exit-time DESTRUCTOR (runs after
      doctest::Context::run() returns), NOT a TEST_CASE — a test that freed the pools early would (a)
      invalidate retained views used by the other tests in-process and (b) leave the pools repopulated at
      process exit, defeating the leak check. The S141/S145 thread_local pool destructors also free at
      thread exit, giving double safety (both idempotent — pointers reset to null after free).
    - NO NEW md deliverables (session-context/progress/roadmap are the only md; honored). Everything
      resolved at its own sprint (NO-GAP). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128,
      S129, S130, S130_1, S131, S132, S133, S134, S135, S136, S137, S138, S139, S140, S140_1, S141,
      S142, S143, S144, S145, and S146 is outstanding, awaiting your authorization.
