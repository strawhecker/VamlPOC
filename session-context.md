# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S094 (no commits; no NEW md files)
- plan_repo_state : S094 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    windows-msvc (VS 2026) via build/verify_s094.cmd configure+build warning-clean +
                    ctest 1/1 PASS; ubuntu-clang (Ubuntu VM, clang 21, shared folder
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. No commit+push (per
                    command). S094 (dep:S093) is the reader error-taxonomy CONSOLIDATION GATE: all
                    four title error cases are already structured + reachable through one realistic
                    reader path, and S094 proves them on that path + resolves the S092-recorded
                    x:Class-on-non-root hand-off with a NEW reader error. PROVED on one reader path:
                    A. unknown attr -> VamlReaderException(VamlReaderError::UnknownProperty) naming
                    the key; B. unknown element -> ElementResolutionException(UnknownType) naming
                    type + line; C. bad/empty type name -> ElementResolutionException(UnknownType)
                    (via a hand-built ParsedElement — the S088-documented corner; the scanner bounds
                    authored names); D. malformed value -> ValueConversionException(MalformedValue)
                    naming the literal; E. (S094-new) VamlReaderError::ClassOnNonRoot — an x:Class
                    on a NON-root throws VamlReaderException(ClassOnNonRoot) naming the class value
                    (root accepted, root_class() recorded); F. taxonomy distinctness — reader/
                    resolver/converter families stay structured + distinct by kind(); G. control — a
                    doc with all four happy paths present loads clean. 8 TEST_CASEs (Shouldly),
                    S094-unique global DPs S094Width(Int32)/S094Text(String).
                    PRODUCT (authored at this step, NO-GAP): VamlReader::build(element, depth)+Load
                    thread a depth parameter gating ClassOnNonRoot — closes the S092-recorded
                    silent-last-wins hole (a child x:Class silently overwrote root_class_).
- plan_digest     : S094 (dep:S093). ADDED test/unit/VamlReader_s094_test.cpp (8 TEST_CASEs).
                    MOD src/core/include/vaml/core/VamlReader.h (new VamlReaderError::ClassOnNonRoot
                    arm [appended, enum-stable] + build(element, depth) signature + docs).
                    MOD src/core/src/VamlReader.cpp (build depth param; ClassOnNonRoot guard; what()
                    "x:Class on a non-root element '<class>'"; recursion+Load thread depth).
                    MOD test/unit/CMakeLists.txt (+VamlReader_s094_test.cpp).
                    ADDED build/verify_s094.cmd (mirror verify_s093.cmd).
                    MOD docs/roadmap.md (S094 [DONE] with full detail); session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/VamlReader.h         (ClassOnNonRoot + build(element, depth))
    MODIFIED src/core/src/VamlReader.cpp                     (build depth; ClassOnNonRoot guard)
    ADDED    test/unit/VamlReader_s094_test.cpp              (8 S094 TEST_CASEs)
    MODIFIED test/unit/CMakeLists.txt                        (+ VamlReader_s094_test.cpp)
    ADDED    build/verify_s094.cmd
    MODIFIED docs/roadmap.md                                 (S094 [DONE])
    MODIFIED session-context.md (this — S094 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s094.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (all cases incl. new S094 green).
                    ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation from third_party (not this TU; non-error, already
                    downgraded via -Wno-error=#warnings in the test CMakeLists).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S095. S095 (dep:S094) = Parser + reader full test suite (golden VAML docs). S094 has
                    now consolidated the reader error taxonomy (unknown attr, unknown element, bad
                    type name, malformed value) + resolved the x:Class-on-non-root hand-off with
                    ClassOnNonRoot; S095 is the golden-doc suite over the now-complete S091 reader +
                    error gates.
- notes           :
    - S094's unique value: the four title cases were ALREADY structured + reachable (S091 proved each
      individually), so inventing new error enums for them would be dead surface (S048 discipline).
      S094's contribution is the FORMAL consolidated taxonomy gate on ONE realistic reader path + the
      genuine hand-off closure (ClassOnNonRoot) that S092 explicitly deferred to this very gate.
    - ClassOnNonRoot is appended to VamlReaderError (not inserted), keeping the enum stable vs S091's
      UnknownProperty/S092's DuplicateName. It is reader-detectable + WPF-faithful (x:Class is
      root-only by definition); leaving it silent was a real last-wins hole, now closed (NO-LAZY-LLM /
      NO-GAP).
    - The bad-type-name case reaches ElementResolutionException(UnknownType) with an EMPTY name via a
      hand-built ParsedElement because the scanner bounds authored type names — the empty resolution
      key is the S088-documented reachable corner, and S094 proves it at the reader boundary.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, S080..S086,
      S087, S088, S089, S090, S091, S092, S093, and now S094 is outstanding, awaiting your
      authorization. After commit+push: repo is the committed good state; next step S095 (Sprint 2).
      Laptop-added files uncommitted as well — one commit covers all.
