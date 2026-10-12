# VAML — SESSION-CONTEXT (per-run state)

- last_command    : commitit S031 (session-context.md updated to completion state BEFORE commit+push)
- plan_repo_state : S031 implementit complete — build green MSVC + Linux Clang, ctest green both.
                    commit+push staged via commitit (see below). Post-commit: NO further file changes.
- plan_digest     : S031 dep:S030 — Value round-trip + type-mismatch-throws tests.
                    - New `test/unit/Value_roundtrip_test.cpp` (doctest, NO IMPLEMENT_WITH_MAIN —
                      shares smoke_test's main). Scope: (A) ROUND-TRIP — every non-Null tag's payload
                      round-trips through get<T>() EXACTLY + re-factory Value::X(v.get<P>())==v;
                      String by VALUE (copy, mutation doesn't affect stored); BrushId/UntypedPtr
                      included. (B) TYPE-MISMATCH-THROWS — Null has no payload so EVERY get<T> throws;
                      each non-Null tag throws ValueTypeMismatch for every non-valid payload type
                      (exhaustive, incl. no cross-width/no-promotion); BrushId reads via int32_t ONLY
                      but tag stays BrushId (≠Int32, BrushId(1)!=Int32(1)); String mismatches every
                      scalar; after a throw the SAME value re-reads valid payload + equals original
                      (throw path is a read-only no-op on the union).
                    - test/unit/CMakeLists.txt (MOD): + Value_roundtrip_test.cpp to vaml-core-tests.
                    - build/verify_s031.cmd (NEW): clone of verify_s007 pattern (clean PATH +
                      vcvars64 → msvc-debug). No product/source code changed; pure test step.
                    - No MINIMAL REAL introduced/resolved; NO-GAP unchanged.
                    - Verification: msvc-debug (VS 2026) exe 33/33 cases 237/237 asserts PASS (was
                      25/91 at S030); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green,
                      ctest 1/1 PASS. Only Clang warnings = pre-existing upstream <ciso646> doctest
                      deprecation (not this TU).
- files_touched   :
    ADD      test/unit/Value_roundtrip_test.cpp   (S031 round-trip + mismatch-throws suite)
    MODIFIED test/unit/CMakeLists.txt             (+ Value_roundtrip_test.cpp)
    ADD      build/verify_s031.cmd                (verify_s007-pattern msvc-debug lane)
    MODIFIED docs/roadmap.md                      (S031 [DONE])
    MODIFIED progress.md                          (S031 SURVIVOR + checkpoint → S032)
    MODIFIED session-context.md                   (this file — completion status)
- build_status    : GREEN — msvc-debug (VS 2026) exe 33/33 cases 237/237 asserts PASS; linux
                    clang-debug (Ubuntu VM) build ok, ctest 1/1 PASS. (Last known good prior: S030.)
- next_step       : S032 (PropertyKey + PropertyRegistry, name→key, dedupe detection)
- notes           :
    - S031 is a PURE TEST step: no product symbol introduced/resolved, no NO-GAP change. Roadmap's
      S031 line + header's S030 comment explicitly deferred the mismatch/round-trip suite here, so
      this is scope-complete, not a silent backfill.
    - GOTCHA: get<T>() is [[nodiscard]] (S030) — CHECK_THROWS_AS(a.get<T>(), ...) discards the return
      → MSVC C4834 (warnings-as-errors). Fix: wrap (void)a.get<T>() inside CHECK_THROWS_AS.
    - No NEW md files produced beyond plan deliverables (operational session-context.md +
      existing progress.md/roadmap.md updated; directive honored). No commits made (per command).
    - Next step S032 continues Sprint 1 (PropertyKey + PropertyRegistry).
    - COMMITIT STATUS: session-context.md set to completion state pre-commit. Commit + push of all
      S031 files is outstanding, awaiting your authorization (commitit incidental "no post-commit
      changes" honored). After commit+push: repo is the committed good state; next step S032.
