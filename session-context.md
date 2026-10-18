# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **S145 FULLY COMPLETE — BOTH CI LANES GREEN** (block pool free-list: thread-local,
                    retain-forever, LOCKLESS; dep:S144; the object-side half of the allocation fix).
                    PREFERRED ROUTE — the S120/S121 unique_ptr ownership is PRESERVED (single-parent,
                    virtual dtor, detach-without-destroy all unchanged); the allocation/deallocation path
                    BENEATH it is rewritten via class-scoped `operator new`/`operator delete` on
                    DependencyObject. NEW vaml::detail::ObjectPool (src/core/src/ObjectPool.{h,cpp}, plain
                    class — NOT thread_local, so tests instantiate isolated pools; the thread_local binding
                    g_objectPool in the .cpp mirrors InternStore/Intern.cpp): allocate()→free-list pop else
                    monotonic bump else spill to a new block (never reset/reorder); deallocate()→push the
                    slot onto an intrusive LIFO (never frees the block / never deletes the object bytes),
                    with a contains() membership check forwarding foreign (oversized process-heap) pointers
                    to ::operator delete; master_destroy_all() frees every block + resets (opt-in teardown,
                    mirrors InternStore). kSlotSize=960=align_up(sizeof(Grid),64): sizeof(Grid)==856
                    (release MSVC) / 848 (release Linux clang) but the msvc-debug lane's checked iterators
                    (/MTd) inflate it to 952 — the slot MUST cover that worst case across BOTH CI lanes;
                    960%64==0 (cache line) + 960%16==0 (Linux max_align_t). The S142 inline Store
                    (slots_[kInline]) is a DependencyObject member at a fixed offset in the object's
                    allocation, so a slot carries the object AND its inline SIMD store with no per-object
                    heap hop; only store/vector overflow spills (monotonic). RECYCLE == SLAB REUSE NOT
                    STATE CARRYOVER (the destructor tears down internal state — Store overflow,
                    children_/subs_/inheritance_scope_ vectors, owned PropertyMetadata/PropertyKey nodes,
                    Grid::Impl — and only the object's slab bytes are retained+reused). VERIFIED BOTH
                    LANES: windows-msvc build/verify_s145.cmd msvc-debug warning-clean (638 test cases /
                    5321 passing assertions / 0 failed; the single in-suite "failed" assertion is the
                    intentional S036_1 should_fail demonstration; 10 new S145 cases / 326 assertions in
                    ObjectPool_test.cpp) + ubuntu-clang clang-debug from /mnt/hgfs/VamlPOC (ctest 1/1
                    PASS, 638 cases SUCCESS; only pre-existing upstream <ciso646> doctest deprecation).
                    windows-clang-debug NOT a lane (not built/tested/flagged). No commits.
- plan_repo_state : **S145 COMPLETE + GREEN (BOTH LANES)** — every DependencyObject/derived element now
                    allocates/deallocates through the thread-local ObjectPool (operator new/delete),
                    recycling slabs on the free-list and never deleting during the thread's life (opt-in
                    object_pool_master_destroy_all). ADR-018: ZERO new locks (the pool is a straight
                    thread-local structure, lockless by construction — same posture as InternStore/
                    NameTable; the only lock in the project remains the S037/S041 ChangeQueue mutex,
                    untouched). SCOPE DECISION (recorded): delivered exactly as titled (the object block
                    pool + recycling); children_ stays std::vector (amortized O(1), non-shrinking, its
                    element payload now pools). The S142 NO-GAP "children slab" RE-SCHEDULED (NOT silently
                    dropped — NO-GAP honored) to S624 (the roadmap's "element pool; no heap churn" home) —
                    a distinct low-level data-structure change, NOT bolted onto S145. Next = S146 (REFACTOR
                    GATE(S3.5)).
- plan_digest     : S145 = ADDED src/core/src/ObjectPool.h + src/core/src/ObjectPool.cpp (vaml::detail::
                    ObjectPool + the thread_local binding + object_pool_allocate/deallocate/
                    master_destroy_all/size/block_count) + MOD src/core/include/vaml/core/
                    DependencyObject.h + src/core/src/DependencyObject.cpp (class-scoped operator new/
                    operator delete routing through the pool) + MOD src/core/CMakeLists.txt (+ObjectPool.cpp)
                    + ADDED test/unit/ObjectPool_test.cpp (10 cases / 326 assertions: dole/recycle reuse,
                    LIFO, monotonic bump + spill, never-delete, alignment, contains(), thread-local
                    isolation, master_destroy_all, make_unique<Button> churn integration,
                    create_child<Canvas>/remove_child tree churn; element-size static_asserts guard
                    kSlotSize) + MOD test/unit/CMakeLists.txt (+ObjectPool_test.cpp) + ADDED
                    build/verify_s145.cmd + MOD docs/roadmap.md (S145 [DONE]; S624 [children-slab
                    re-schedule note]) + MOD progress.md (checkpoint last=S145/next=S146 + S145 survivor
                    log) + MOD session-context.md (this).
- files_touched   :
    ADDED src/core/src/ObjectPool.h                            (S145 pool primitive: blocks, intrusive LIFO, bump)
    ADDED src/core/src/ObjectPool.cpp                          (implementation + thread_local binding + funnel)
    MODIFIED src/core/include/vaml/core/DependencyObject.h     (operator new/delete + S145 comment)
    MODIFIED src/core/src/DependencyObject.cpp                 (operator new/delete impl; include ObjectPool.h)
    MODIFIED src/core/CMakeLists.txt                           (+ src/ObjectPool.cpp)
    ADDED test/unit/ObjectPool_test.cpp                        (S145 pool + integration tests)
    MODIFIED test/unit/CMakeLists.txt                          (+ ObjectPool_test.cpp)
    ADDED build/verify_s145.cmd                                (windows-msvc lane driver)
    MODIFIED docs/roadmap.md                                   (S145 [DONE]; S624 children-slab re-schedule note)
    MODIFIED progress.md                                       (step log + checkpoint last=S145/next=S146)
    MODIFIED session-context.md (this)
    NOTE: src/core/include/vaml/core/export.h + src/tools + src/vulkan (clang __attribute__ deprecated
          adaptation) were ALREADY modified in the working tree from a prior sprint — NOT touched by S145.
- build_status    : **GREEN — BOTH CI LANES (S145, verified from first principles).** windows-msvc:
                    build/verify_s145.cmd (msvc-debug, VS 2026) — warning-as-error clean; test exe
                    638 test cases / 5321 passing assertions / 0 failed (the single in-suite "failed"
                    assertion is the intentional S036_1 should_fail demonstration; ObjectPool_test.cpp 10
                    cases / 326 assertions ALL pass). ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug (Ubuntu
                    VM) — configure+build clean + ctest 1/1 PASS (100%), 638 cases SUCCESS. Only Clang
                    warnings = pre-existing upstream <ciso646> doctest deprecation (non-error).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged.) No commits.
- next_step       : next roadmap token after S145 — **S146 (dep:S145)**: REFACTOR GATE(S3.5) — remove ALL
                    dead pre-retrofit surface (pimpl heap nodes, unordered_map/std::string-key paths,
                    by-value children(), owned-name gotchas, parser/scanner-owned std::string members);
                    assert the public API accepts string_view or intern()-produced views everywhere; full
                    build + ctest green both compilers; ASan no-leak; steady-state heap-churn counter
                    recorded (feeds S628). S145 is COMPLETE + GREEN; S146 is the next unimplemented task.
- notes           :
    - PREFERRED ROUTE rationale: rewriting the S120/S121 unique_ptr ownership MODEL at every call site
      would destabilize layout/reader/marshalling; rewriting the allocation/deallocation PATH beneath it
      (class-scoped operator new/delete) achieves "route frees back to the pool" with the unique_ptr
      owner/lifetime semantics byte-for-byte unchanged — non-destabilizing, and every existing
      make_unique<Button>/create_child<T>/retained_ push / remove_child / take_child / reader teardown
      automatically routes through the pool.
    - kSlotSize=960 sizing was MEASURED, not guessed: release sizeof(Grid)=856 (MSVC) / 848 (Linux
      clang), but the msvc-debug checked-iterator (/MTd) lane inflates it to 952. The slot must cover the
      worst case across BOTH CI lanes, so kSlotSize=align_up(952,64)=960. The kLargestElement constant
      documents the worst case; a future element larger than it must bump kSlotSize (compile-time constant;
      element-size static_asserts in ObjectPool_test.cpp guard the invariant).
    - Alignment: blocks are allocated with std::align_val_t(64) (cache line); 960%64==0 keeps every slot
      cache-line aligned on BOTH lanes; 960%16==0 satisfies Linux max_align_t (16). The intrusive free-list
      writes a `next` pointer into each freed slot (slots are ≥8B, so the overwrite is safe).
    - No new operator new arrays / no placement-new churn: allocation only; objects are constructed by
      the normal make_unique<T>(...) path after operator new returns a pooled slot. Slab reuse happens on
      a later make_unique after deallocate.
    - NO NEW md files (directive honored). No "backfill" — everything resolved at its own sprint. No
      commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128,
      S129, S130, S130_1, S131, S132, S133, S134, S135, S136, S137, S138, S139, S140, S140_1, S141,
      S142, S143, S144, and S145 is outstanding, awaiting your authorization.
