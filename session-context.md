# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S034 (no commits; no NEW md files)
- plan_repo_state : S034 implementit complete — build green MSVC + Linux Clang, ctest green both.
                    No commit+push performed (per command). Files below are the full S034 change-set.
- plan_digest     : S034 dep:S033 — PropertyMetadata { defaultValue; flags; changed cb; coerce; validate }.
                    Move-only metadata bundle under an Impl pimpl. Real public symbol (NOT MINIMAL REAL).
                    - src/core/include/vaml/core/PropertyMetadata.h (NEW)
                    - src/core/src/PropertyMetadata.cpp (NEW)
                    - test/unit/PropertyMetadata_test.cpp (NEW)
                    - src/core/CMakeLists.txt (MOD): +PropertyMetadata.cpp
                    - test/unit/CMakeLists.txt (MOD): +PropertyMetadata_test.cpp
                    - build/verify_s034.cmd (NEW): verify_s033-pattern msvc-debug lane.
                    Design: field surface mirrors roadmap exactly — defaultValue (Value, also fixes bound
                    type = defaultValue.type()), flags (PropertyFlags validating carrier recomposing S032's
                    set), changed cb (old,new), coerce Value(Value), validate Value->bool. Construction:
                    canonical factory PropertyMetadata::WithDefault(Value, PropertyFlags) (REQUIRED — no
                    default-constructible empty metadata) + && return-by-value builder hooks
                    with_coerce/with_validate/with_changed (each installs one callback, returns a NEW
                    metadata, leaves source untouched). Passthrough when absent: coerce=identity,
                    validate=always-accept, changed=no-op. TYPE-BOUND: coerce/validate/notifyChanged throw
                    vaml::ValueTypeMismatch on a Value whose tag != boundType (NO-LAZY wrong-type guard).
                    Copy deleted (move-only, proven by static_assert). Tests: move-only; WithDefault seeds
                    default+bound type+flags; flags carrier round-trips all 3 bits; coerce/validate/changed
                    present+absent semantics; single builder chain composes all three; wrong-tag throws
                    from all entry points + correct tag post-throw; builder return-by-value.
                    Deferral (recorded, not silent): global static registry instance + DependencyProperty
                    ::Register landing site = S036 (DependencyObject with local-value store needing a
                    registered DP); resolves S032(b)/S033 recording. S034 delivers the metadata TYPE + its
                    own unit semantics; invocation at Set-time is S036/S035.
                    Verification: msvc-debug (VS 2026) ctest 1/1 PASS; REAL LINUX clang-debug (Ubuntu VM
                    clang 21.1.8) build green, ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation.
- files_touched   :
    ADD      src/core/include/vaml/core/PropertyMetadata.h (S034 move-only metadata bundle, pimpl)
    ADD      src/core/src/PropertyMetadata.cpp            (WithDefault factory + builder hooks + behavior)
    ADD      test/unit/PropertyMetadata_test.cpp          (construction, defaults, coerce/validate/cb, move-only)
    MODIFIED src/core/CMakeLists.txt                     (+PropertyMetadata.cpp)
    MODIFIED test/unit/CMakeLists.txt                    (+PropertyMetadata_test.cpp)
    ADD      build/verify_s034.cmd                       (verify_s033-pattern msvc-debug lane)
    MODIFIED docs/roadmap.md                             (S034 [DONE])
    MODIFIED progress.md                                 (S034 SURVIVOR + checkpoint → S035)
    MODIFIED session-context.md                          (this file — completion status)
- build_status    : GREEN — msvc-debug (VS 2026) ctest 1/1 PASS; linux clang-debug (Ubuntu VM) build
                    ok, ctest 1/1 PASS. (Last known good prior: S033.)
- next_step       : S035 (Metadata tests: default applied, cb fires only on real change, coerce applied, validate rejects)
- notes           :
    - S034 delivers the PropertyMetadata TYPE as a move-only, type-bound bundle. The three callables
      (coerce/validate/changed) live under a pimpl so the public header hides the std::function ABI and
      copy is deleted at the boundary — move-only proven by static_assert, not just convention.
    - GOTCHA: the builder hooks are &&-qualified return-by-value; they are meant to be chained on a
      temporary (WithDefault(...).with_coerce(...)) or via std::move on a named lvalue. Calling them on
      an lvalue without std::move is a compile error by design (forces move semantics).
    - DESIGN re-deferral (resolves S032(b)/S033): global static registry instance + DependencyProperty
      ::Register landing site are S036's concern (S036 = DependencyObject with a local-value store needing
      a registered DP). S034 adds NO static registration surface; the metadata TYPE + its unit semantics
      are fully real here, while Set-time coercion/validation/changed invocation is S035/S036.
    - No NEW md files produced beyond plan deliverables (directive honored). No commits made (per command).
    - Next step S035 tests every metadata field (default applied, cb-only-on-real-change, coerce applied,
      validate rejects).
    - COMMITIT STATUS: commit + push of all S032 + S033 + S034 files is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S035.
