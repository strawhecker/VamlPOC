# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S036_1 (no commits; no NEW md files)
- plan_repo_state : S036_1 complete — msvc-debug (VS 2026) build green + ctest 1/1 PASS. No commit+push
                    performed (per command). This run implements ONLY S036_1 of the S036_1..S036_5
                    plan; S036_2..S036_5 are pending.
- plan_digest     : S036_1..S036_5 — Adopt the re-formulated Shouldly as the unit-test assertion
                    vocabulary (doctest stays the runner). S036_1 (this step) = Shouldly routing
                    reform: predicates keep the S006 default-sink THROW contract; the new doctest seam
                    (SHOULD / SHOULD_REQUIRE macros, active only when doctest.h is included) opens a
                    doctest::detail::ResultBuilder in-test, runs the predicate, catches the thrown
                    TestFailureException, routes its message into m_decomp/m_failed, then log()/react()
                    -> lands in doctest counters/summary/-s with explicit severity (SHOULD=DT_CHECK
                    continue, SHOULD_REQUIRE=DT_REQUIRE abort). shouldly_test.cpp rewritten to assert
                    the NEW routing contract in the same step (golden rule: the old throw-contract is
                    invalidated by the reform, so deferring its test rewrite would leave an unbuildable
                    window). feature test files are NOT migrated here (S036_4/S036_5). Gate = full
                    suite green, feature files unchanged.
                    Files (S036_1 only):
                    - src/core/include/vaml/assert/Shouldly.h (MOD): +SHOULD/SHOULD_REQUIRE doctest
                      seam (guarded by #ifdef DOCTEST_LIBRARY_INCLUDED so core stays doctest-free)
                    - test/unit/shouldly_test.cpp (MOD): +SHOULD success-path cases (integrals, string,
                      pointer), +a should_fail() demonstration case proving a failing SHOULD is
                      swallowed+routed (not rethrown as an uncaught exception); default-sink-throws
                      contract kept and now documented as the exact contract SHOULD consumes
                    - session-context.md (this)
- files_touched   :
    MODIFIED src/core/include/vaml/assert/Shouldly.h   (SHOULD/SHOULD_REQUIRE doctest seam, guarded)
    MODIFIED test/unit/shouldly_test.cpp               (routing-contract tests; default-throw kept)
    MODIFIED session-context.md                        (this — S036_1 completion status)
- build_status    : GREEN — msvc-debug (VS 2026) configure+build+ctest 1/1 PASS (100% tests passed).
                    (Last known good prior: S036.)
- next_step       : S036_2 (StringMaker domain exposure: StringMaker<vaml::Value>, <SetResult>,
                    <PropertyId> — framework-native, no macro surgery). Then S036_3 (ShouldThrow<Ex>),
                    S036_4 (migrate feature tests), S036_5 (migrate DependencyObject_test + final gate).
                    After S036_*: S037 (Set/Get local values; wrong-thread queued-not-applied).
- notes           :
    - S036_1 keeps the Shouldly predicate bodies + default sink CONTRACT untouched (they still throw
      TestFailureException). The seam is a macro, so a failing predicate can ALSO be asserted bare via
      CHECK_THROWS_AS(..., vaml::test::TestFailureException) — no sink capture needed. This is why the
      doctest ResultBuilder cannot be invoked from inside the predicate: a fluent call evaluated inside
      an expression has no scoped RB; the macro provides it.
    - Core must stay doctest-free: the SHOULD/SHOULD_REQUIRE macros are defined only under
      #ifdef DOCTEST_LIBRARY_INCLUDED, so any non-test TU that includes <vaml/assert/Shouldly.h>
      without doctest gets the canonical DSL and no doctest dependency.
    - Severity is explicit and correct: SHOULD -> DT_CHECK (continue-on-failure), SHOULD_REQUIRE ->
      DT_REQUIRE (abort case). The suite uses no REQUIRE today, so existing behavior of feature tests
      (continue-on-failure) is preserved when they migrate to SHOULD in S036_4/S036_5.
    - windows-clang-debug preset remains broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl
      incompatibility, cross-cutting, out of scope, NOT introduced here); maintained verify lane is
      msvc-debug.
    - No NEW md files produced (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit + push of S032..S036 and S036_1 is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S036_2.
