# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S036_5 (no commits; no NEW md files)
- plan_repo_state : S036_5 complete — msvc-debug (VS 2026) build green + ctest 1/1 PASS (100% tests
                    passed). No commit+push performed (per command). **The full S036_1..S036_5 plan is
                    now COMPLETE.** S037 is next.
- plan_digest     : S036_1..S036_5 — Adopt the re-formulated Shouldly as the unit-test assertion
                    vocabulary (doctest stays the runner). S036_5 (this step, the capstone) =
                    migrate `DependencyObject_test.cpp` (the S036 proof) to the Shouldly vocabulary +
                    run the final gate. SetResult/Value/tag/thread-affinity assertions now all read via
                    SHOULD(ShouldlyString(...)) / ShouldThrow<Ex>. The capstone confirms the whole
                    feature suite uses the Shouldly-for-doctest vocabulary with doctest only as the
                    runner, and annotates S047 as realized early by S036_1..S036_5.
                    Files (S036_5 only):
                    - test/unit/DependencyObject_test.cpp (MOD → full Shouldly rewrite: all 11 S036
                      cases converted; includes wired: vaml_test_stringmakers.h + <vaml/assert/Shouldly.h>)
                    - docs/roadmap.md (MOD): S036_5 [DONE]; backfilled S036_1..S036_4 [DONE] markers
                    - session-context.md (this)
- files_touched   :
    MODIFIED test/unit/DependencyObject_test.cpp    (full Shouldly rewrite, capstone)
    MODIFIED docs/roadmap.md                         (S036_1..S036_5 all marked DONE)
    MODIFIED session-context.md                      (this — S036_5 completion)
- build_status    : GREEN — msvc-debug (VS 2026) configure+build+ctest 1/1 PASS (100% tests passed).
                    (Last known good prior: S036_4.)
- next_step       : S037 (Set/Get local values; wrong-thread write queued-not-applied tests
                    (marshalling foundation)). S037 CONSUMES S036's thread-affinity marker
                    (thread_affinity_id / is_affine_to_current_thread) as the basis for wrong-thread
                    detection; S036 records affinity only, S037 adds actual wrong-thread write QUEUING.
- notes           :
    - S036_5 GATE MET: zero CHECK*/CHECK_THROWS_AS across all 7 migrated feature files (Value_test,
      Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test,
      PropertyMetadata_test, DependencyObject_test).
    - SANCTIONED non-migrated asserts (all deliberate + documented in their files, and now recorded in
      roadmap S036_5 [DONE]): (1) `PropertyMetadata_semantics_test.cpp` — the S035 reference SPEC
      harness — is intentionally NOT migrated; it IS the spec S036 must satisfy, and rewriting it would
      alter the exact contract under test (its single CHECK_THROWS_AS stays). (2) smoke_test.cpp — the
      doctest runner/main harness — untouched. (3) StringMaker_test.cpp — asserts the raw doctest::String
      output of the S036_2 StringMaker itself (an infrastructure test of the stringifier, not a vaml
      domain assertion). (4) 3 REQUIRE precondition guards (null-deref gates) in PropertyKeyRegistry_test
      + PropertyRegistryExhaustive_test.
    - windows-clang-debug preset remains broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl
      incompatibility, cross-cutting, out of scope, NOT introduced here); maintained verify lane is
      msvc-debug. Full dual-compiler verification of the S036_* suite is deferred to the S041+ gates
      / an explicit later dual-compiler run (recorded, not silent).
    - No NEW md files produced (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit + push of S032..S036 and S036_1..S036_5 is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S037.
