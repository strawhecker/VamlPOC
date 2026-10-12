# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S036_3 (no commits; no NEW md files)
- plan_repo_state : S036_3 complete — msvc-debug (VS 2026) build green + ctest 1/1 PASS (100% tests
                    passed, 90 cases / 558 asserts). No commit+push performed (per command). S036_1..3
                    of the S036_1..S036_5 plan complete; S036_4, S036_5 pending.
- plan_digest     : S036_1..S036_5 — Adopt the re-formulated Shouldly as the unit-test assertion
                    vocabulary (doctest stays the runner). S036_3 (this step) = ShouldThrow<Ex>
                    typed-exception assertion category: first-class replacement for doctest's
                    CHECK_THROWS_AS / _WITH / _WITH_AS that reports the ACTUAL thrown type (RTTI typeid)
                    + message when the wrong thing is thrown (CHECK_THROWS_AS cannot). Routes through
                    the same pluggable sink as every predicate, so it composes with the SHOULD(...)
                    doctest seam. Resolves the typed-assertion category at its own step (NO-GAP /
                    NO-LAZY-LLM — NOT lazy-avoidance).
                    Files (S036_3 only):
                    - src/core/include/vaml/assert/Shouldly.h (MOD): +ShouldThrow<Ex> 3 overloads
                      (no-message / std::string / const char* message-matcher),
                      +detail::describe_thrown_std_exception helper, +<typeinfo> include
                    - test/unit/shouldly_test.cpp (MOD): +6 TEST_CASEs covering pass (right type /
                      base-of-derived / message match), fail (nothing-thrown, wrong-type-with-actual-
                      message, wrong-message-with-actual-message); +using vaml::test::ShouldThrow;
                      +namespace-local DomainError/throwDomainError/throwLogicError/throwNothing fixtures
                    - session-context.md (this)
- files_touched   :
    MODIFIED src/core/include/vaml/assert/Shouldly.h
    MODIFIED test/unit/shouldly_test.cpp
    MODIFIED session-context.md
- build_status    : GREEN — msvc-debug (VS 2026) configure+build+ctest 1/1 PASS (100% tests passed,
                    90 cases / 558 asserts). (Last known good prior: S036_2.)
- next_step       : S036_4 (migrate Value / roundtrip / PropertyId / PropertyKeyRegistry /
                    PropertyRegistryExhaustive / PropertyMetadata feature tests from CHECK* to
                    SHOULD(Shouldly...)/ShouldThrow; typed-exception sites -> ShouldThrow<Ex>).
                    Then S036_5 (migrate DependencyObject_test + final gate). After S036_*: S037.
- notes           :
    - ShouldThrow design (source-grounding): try/catch structures the three failure modes.
      Ex==std::exception handled via if constexpr (avoids duplicate-handler C2312: the Ex-catch would
      be identical to catch(std::exception)). Derived Ex uses separate Ex-catch + catch(std::exception)
      + catch(...). Any path that does not return reaches the trailing "nothing was thrown" report.
      typeid(e).name() gives the ACTUAL thrown type (MSVC mangled here, but message + "got <...>" are
      present) — this is the typeinfo the message-matcher path + wrong-type path exercise.
    - ShouldThrow composes with SHOULD(...): SHOULD( ShouldThrow<PropertyNotFound>([&]{...}) ),
      SHOULD( ShouldThrow<PropertyNotFound>([&]{...}, "msg") ).
    - The suite-wide CHECK_THROWS_AS->ShouldThrow migration is S036_4/S036_5, NOT this step (this step
      proves the category compiles + behaves; migration sweeps are the later steps per plan).
    - windows-clang-debug preset remains broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl
      incompatibility, cross-cutting, out of scope, NOT introduced here); maintained verify lane is
      msvc-debug.
    - No NEW md files produced (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit + push of S032..S036 and S036_1..S036_3 is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S036_4.
