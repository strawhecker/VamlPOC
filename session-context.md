# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S036 (no commits; no NEW md files)
- plan_repo_state : S036 implementit complete — msvc-debug (VS 2026) build green + ctest 1/1 PASS.
                    No commit+push performed (per command). Files below are the full S036 change-set.
- plan_digest     : S036 dep:S035 — DependencyObject base: local value store, Get/Set, per-prop mastery,
                    thread-affinity marker. Satisfies the S035 apply pipeline (coerce → validate(on
                    coerced) → fire changed cb only on real change → store; MiniStore harness in
                    PropertyMetadata_semantics_test.cpp is the spec). DESIGN accepted: option A —
                    object-scoped registration. A global static registry + DependencyProperty::Register
                    (S034-recorded "S036's concern") is re-scoped to S039 (recorded, NOT silent), where
                    effective-value resolution is the first real consumer.
                    Files:
                    - src/core/include/vaml/core/DependencyObject.h (NEW)
                    - src/core/src/DependencyObject.cpp           (NEW)
                    - test/unit/DependencyObject_test.cpp         (NEW, 11 S036 cases)
                    - build/verify_s036.cmd                       (NEW, verify_s035-pattern msvc-debug lane)
                    - src/core/CMakeLists.txt                     (MOD): +DependencyObject.cpp
                    - test/unit/CMakeLists.txt                    (MOD): +DependencyObject_test.cpp
                    API: register_property(PropertyKey, PropertyMetadata) → stored-key ref, name-dedupe
                    throws PropertyAlreadyRegistered-unchanged, type-coherence guard → ValueTypeMismatch;
                    Get(PropertyId) → Value by value, PropertyNotFound if unregistered, default-applied
                    fallback; Set(PropertyId, const Value&) → SetResult{APPLIED,REJECTED,NO_CHANGE}
                    reproducing S035 pipeline verbatim (oldEffective = local-else-default; wrong-tag →
                    ValueTypeMismatch); thread-affinity marker thread_affinity_id() +
                    is_affine_to_current_thread() (created-thread recorded; wrong-thread QUEUING → S037).
                    Wire-up: docs/roadmap.md (S036 [DONE]; S037 annotated with S036 thread-affinity
                    consumer; S039 re-scope note), progress.md (S036 SURVIVOR + checkpoint → S037),
                    session-context.md (this).
                    Verification: msvc-debug (VS 2026) ctest 1/1 PASS (100% tests passed).
- files_touched   :
    ADD      src/core/include/vaml/core/DependencyObject.h  (public DependencyObject API, pimpl'd)
    ADD      src/core/src/DependencyObject.cpp              (register/get/set pipeline + affinity)
    ADD      test/unit/DependencyObject_test.cpp            (11 apply/per-prop/dedupe/affinity cases)
    ADD      build/verify_s036.cmd                          (verify_s035-pattern msvc-debug lane)
    MODIFIED src/core/CMakeLists.txt                        (+src/DependencyObject.cpp)
    MODIFIED test/unit/CMakeLists.txt                       (+DependencyObject_test.cpp)
    MODIFIED docs/roadmap.md                                (S036 [DONE]; S037 annot; S039 re-scope note)
    MODIFIED progress.md                                    (S036 SURVIVOR + checkpoint → S037)
    MODIFIED session-context.md                             (this file — completion status)
- build_status    : GREEN — msvc-debug (VS 2026) configure+build+ctest 1/1 PASS (100% tests passed).
                    (Last known good prior: S035.)
- next_step       : S037 (Set/Get local values; wrong-thread write queued-not-applied tests (marshalling
                    foundation)) — consumes S036's thread-affinity marker.
- notes           :
    - S036 introduces a REAL permanent public symbol vaml::DependencyObject (NOT MINIMAL REAL, no
      sentinel — every member is genuine surface with scheduled consumers: S037 marshalling, S039
      effective-value, S041 notification).
    - DESIGN re-scope (recorded, NOT silent): the global static registry + DependencyProperty::Register
      landing site (S034 recorded it as "S036's concern") is authored at S039 instead. S036 delivers
      object-scoped registration because a global static registry in S036 would be speculative surface
      with no consumer until S039's effective-value resolution. NO-GAP satisfied (documented re-scope,
      not silent backfill); roadmap S039 line carries the response note.
    - GOTCHA (recorded in roadmap+progress): the internal store keys on an OWNED std::string (copied
      name), NOT on PropertyId — a PropertyId is a non-owning std::string_view that dangles once the
      registering PropertyKey dies (SSO+heap names both failed until the owned-string key). Get/Set
      re-derive std::string(id.name()) for lookup.
    - GOTCHA: Value has NO public default ctor → PropEntry::local initializes to Value::Null() (not {}).
    - GOTCHA: Set is [[nodiscard]] by design → tests that discard it cast to (void) (MSVC C4834 /WX).
    - windows-clang-debug preset was attempted but fails on EVERY TU (incl. untouched version.cpp) on a
      pre-existing `/Zc:preprocessor`-unused clang-cl incompatibility — cross-cutting preset issue wholly
      out of S036 scope, NOT introduced here; the maintained verify lane for this step is msvc-debug.
    - No NEW md files produced beyond plan deliverables (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit + push of all S032 + S033 + S034 + S035 + S036 files is outstanding,
      awaiting your authorization. After commit+push: repo is the committed good state; next step S037.
