# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S037 (no commits; no NEW md files)
- plan_repo_state : S037 complete — **BOTH CI LANES GREEN**: msvc-debug (VS 2026) build warning-clean +
                    ctest 1/1 PASS; ubuntu-clang (Ubuntu VM, clang 21, shared folder /mnt/hgfs/VamlPOC)
                    build + ctest 1/1 PASS. No commit+push performed (per command). **Marshalling
                    foundation landed: S036's thread-affinity marker is now the basis for actual
                    wrong-thread write QUEUING (queued-not-applied).** S041+ generalizes the per-object
                    queue into the per-thread ChangeQueue + notification processor.
- plan_digest     : S037 (dep:S036_5) — Set/Get local values; wrong-thread write queued-not-applied
                    tests (marshalling foundation). CONSUMES the S036+ thread-affinity marker. Adds:
                    Set() becomes affinity-aware — a write from a NON-affine thread runs coerce+validate
                    at queue time (instant writer feedback) but POSTS (queued-not-applied), returning
                    new SetResult::QUEUED; ProcessPendingChanges() (affinity-thread-only, throws
                    WrongThreadException otherwise) drains the mutex-protected per-object queue FIFO
                    through the S036 store half (correct (old,new) cb chain [5,7]->cb(0->5),cb(5->7));
                    pending_change_count() exposes queued-not-applied depth. Coerce/validate-at-queue,
                    drain-on-affine + guard = the honest marshalling foundation (S041's own resolution).
                    Files (S037):
                    - src/core/include/vaml/core/DependencyObject.h (MOD): SetResult::QUEUED;
                      WrongThreadException; ProcessPendingChanges(); pending_change_count(); affinity-aware
                      Set contract + cross-thread contract comments.
                    - src/core/src/DependencyObject.cpp (MOD): mutex-protected std::deque<PendingWrite>;
                      affinity branch in Set (queue-time coerce/validate -> QUEUED/REJECTED);
                      ProcessPendingChanges (affinity guard + FIFO replay of the S036 store half);
                      WrongThreadException def; pending_change_count.
                    - test/unit/DependencyObject_marshalling_test.cpp (NEW): S037 proof file (Shouldly
                      vocabulary): QUEUED+store-unchanged+depth 1; FIFO drain with correct (old,new) cbs;
                      equal-effective queued then NO_CHANGE at drain; rejected -> REJECTED nothing queued;
                      wrong-tag -> ValueTypeMismatch nothing queued; wrong-thread drain -> WrongThreadException;
                      affinity-thread empty drain legal; coalescing across two frames; affine Set not queued.
                    - test/unit/vaml_test_stringmakers.h (MOD): SetResult::QUEUED in operator<< + StringMaker.
                    - test/unit/CMakeLists.txt (MOD): added DependencyObject_marshalling_test.cpp to target.
                    - build/verify_s037.cmd (NEW): msvc-debug verify lane.
                    - docs/roadmap.md (MOD): S037 [DONE]. session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/DependencyObject.h
    MODIFIED src/core/src/DependencyObject.cpp
    MODIFIED test/unit/vaml_test_stringmakers.h
    ADDED    test/unit/DependencyObject_marshalling_test.cpp
    MODIFIED test/unit/CMakeLists.txt
    ADDED    build/verify_s037.cmd
    MODIFIED docs/roadmap.md
    MODIFIED session-context.md (this — S037 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) configure+build warning-
                    clean + ctest 1/1 PASS. ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21,
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. (windows-clang-debug is NOT a lane
                    — not built/tested/flagged per standing rule.)
- next_step       : S038+ (dependent-step queue). S041 generalizes the S037 per-object pending queue into
                    the per-thread ChangeQueue + notification processor — S037's explicit resolution point,
                    NO backfill.
- notes           :
    - S037 GATE MET on both lanes. SetResult enum is backward-compatible: APPLIED/REJECTED/NO_CHANGE
      unshifted; QUEUED appended.
    - Cross-thread contract (recorded in DependencyObject.h + roadmap): the only truly cross-thread datum
      is the mutex-protected pending queue; the value store stays affinity-thread-owned by construction
      (only the affinity thread stores, via on-affine Set or drain). Get() is declared safe on the affinity
      thread or when no concurrent affinity-thread mutation is in flight. Full store-race freedom is
      TSan/S620/S723 territory (recorded, not silent). All S037 tests use a join barrier so no concurrent
      drain/read occurs in the demonstration.
    - Coerce/validate run AT QUEUE TIME (wrong-thread writer gets REJECTED/ValueTypeMismatch instantly);
      the drain only replays accepted, coerced values. Drain swaps the queue out under the lock so
      wrong-thread writers during a drain enqueue into a fresh queue (drained next call).
    - No MINIMAL REAL markers introduced or resolved this step (nothing to backfill; queue generalization
      is explicitly S041's own step, per NO-GAP/NO-LAZY).
    - No NEW md files produced beyond the two operational docs (directive honored). No commits made
      (per command).
    - COMMITIT STATUS: commit+push of S032..S037 (incl. S036_1..S036_5) is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S038+.
