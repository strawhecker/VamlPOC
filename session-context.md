# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S092 (no commits; no NEW md files)
- plan_repo_state : S092 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    windows-msvc (VS 2026) via build/verify_s092.cmd configure+build warning-clean +
                    ctest 1/1 PASS; ubuntu-clang (Ubuntu VM, clang 21, shared folder
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. No commit+push (per command).
                    S092 delivers THREE strands on VamlReader (dep:S091), all tested:
                    (A) x:Name / x:Class REGISTRY (REAL, permanent): build() now CONSUMES the S083
                    Name/Class directives (previously skipped). New reader API: named(name) -> built
                    object under the x:Name (nullptr if absent; root OR child), name_count(), names()
                    (registration/document order; owned std::string keys, never the PropertyId view),
                    root_class() (the authored x:Class on the root; binding it to an owner object/type
                    deliberately deferred to Sprint 8 / S286 / InitializeComponent-analogue + S243 —
                    recorded NO-GAP, not silent). A DUPLICATE x:Name within one Load throws
                    VamlReaderException with the ONE genuinely-new reader error S092 introduces:
                    VamlReaderError::DuplicateName (WPF-faithful fail-loud; never silent last-wins).
                    x:Key -> S361 and x:Null -> S084 remain deferred unchanged. All registries cleared
                    at the start of each Load (no cross-Load leakage).
                    (B) DEFERRED MARKUP EXTENSIONS (the S2 tag): an S084-classified isExtension
                    attribute ({Binding Title}) is NO LONGER skipped silently — build() records a
                    DeferredExtension{target, key, source} per extension (document order; the ORIGINAL
                    source text retained verbatim as the S2 tag so S252/S253 can re-parse+bind it). The
                    reader does NOT resolve the extension (that is S252's contract); retained + findable
                    via deferred_extensions(), never converted/set. S2 tag recorded in type docs:
                    resolved by S252. Cleared per Load.
                    (C) EVENT HOOKS (Sprint 8) — MINIMAL REAL seed: tagged with the required sentinel
                    `// [MINIMAL REAL] VamlReader event hooks — RESOLVED BY S322 (Sprint 8). Do NOT
                    extend silently; do NOT backfill.` register_event(name) declares an attribute key
                    is an EVENT, not a property; build() routes a matching key to a
                    DeferredEventHandler{target, eventName, handlerName} (never a property set /
                    unknown-property throw), whether or not the event name has any DP. An UNREGISTERED
                    key stays a property (unknown -> VamlReaderException(UnknownProperty)). The actual
                    routed-event engine (RoutedEvent/firing) is deliberately NOT fabricated here —
                    S320/S322 deliver it; S092 only defers event wiring deterministically. Cleared per
                    Load. Registered on roadmap as an active MINIMAL REAL mark RESOLVED BY S322.
- plan_digest     : S092 (dep:S091). MOD src/core/include/vaml/core/VamlReader.h (S092 header comment
                    describing the three strands; VamlReaderError gains DuplicateName; VamlReaderException
                    doc updated for DuplicateName (property holds the duplicate NAME); NEW structs
                    DeferredExtension{target,key,source} + DeferredEventHandler{target,eventName,
                    handlerName}; VamlReader gains named()/name_count()/names()/root_class()/
                    deferred_extensions()/register_event()/deferred_event_handlers() + private members
                    names_/root_class_/deferred_extensions_/event_names_/deferred_event_handlers_).
                    MOD src/core/src/VamlReader.cpp (file-local name_registered() helper; build() now
                    consumes Name directive -> register (DuplicateName throw on repeat), Class -> set
                    root_class_, remaining directives (Key/Null) still skipped; event routing via
                    event_names_ -> DeferredEventHandler; isExtension -> DeferredExtension record; then
                    ordinary literal property path unchanged; Load(root) also clears names_/root_class_/
                    deferred_extensions_/deferred_event_handlers_; new out-of-line accessors). ADDED
                    test/unit/VamlReader_defer_test.cpp (14 TEST_CASEs: root+child x:Name resolution in
                    doc order; named() nullptr unknown; nested-child lookup; root_class(); DuplicateName
                    throw (+ property()); per-Load clearing; DeferredExtension (target/key/source
                    verbatim, NOT set as a property), doc order, empty-when-none, per-Load clearing;
                    register_event routing (target/eventName/handlerName), unregistered key ->
                    UnknownProperty throw, per-Load clearing). MOD test/unit/CMakeLists.txt
                    (+VamlReader_defer_test.cpp). ADDED build/verify_s092.cmd. MOD docs/roadmap.md
                    (S092 [DONE] + active MINIMAL REAL marks += VamlReader event hooks -> RESOLVED BY
                    S322), session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/VamlReader.h           (S092 strands + DuplicateName + structs + accessors)
    MODIFIED src/core/src/VamlReader.cpp                       (Name/Class consumes + deferral retention + event routing)
    ADDED    test/unit/VamlReader_defer_test.cpp               (14 S092 TEST_CASEs)
    MODIFIED test/unit/CMakeLists.txt                          (+ VamlReader_defer_test.cpp)
    ADDED    build/verify_s092.cmd
    MODIFIED docs/roadmap.md                                   (S092 [DONE] + active MINIMAL REAL mark)
    MODIFIED session-context.md (this — S092 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s092.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (all cases incl. new S092 green).
                    ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation from third_party (not this TU; non-error, already
                    downgraded via -Wno-error=#warnings in the test CMakeLists).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S093. S093 = Load a StackPanel+Button with a property set from pure text — tree
                    correct, no hooks. S093 is the first CONSUMER of S092's x:Name registry / named
                    lookups on a realistic StackPanel+Button doc; it exercises a plain-text property
                    set and confirms the tree is correct with no event hooks wired. The deferred
                    markup-extension records (S2 tag) feed S252/S253; the event-hook MINIMAL REAL seed
                    is resolved by S322 (Sprint 8).
- notes           :
    - S091 converted plain literals via the boundType->converter-name mapping + global convert(); the
      S090 recognizer's "is this literal a known value at all?" question remains the S092/S093/S094
      markup/error-taxonomy concern (UnknownLiteral reachable via the reader there). Recorded.
    - D1 OWNERSHIP MODEL (unchanged from S091): the reader retains non-root built objects in
      retained_objects() and returns the root; a plain DependencyObject carries no visual-children
      list (that is S121 — recorded no overlap/no gap); set_parent is non-owning. All S092 registries
      (names_/root_class_/deferred_extensions_/deferred_event_handlers_) clear at each Load so reusing
      one reader never leaks across reads.
    - DECISIONS APPLIED (recommendations accepted): ① duplicate x:Name -> VamlReaderException
      (DuplicateName), fail-loud not silent-last-wins; ② event identification via reader-level
      register_event(name) (no control/event metadata before Sprint 8; explicit declared set is the
      smallest honest mechanism, giving S322 a deterministic record); ③ the event surface is MINIMAL
      REAL, tagged with the required sentinel, RESOLVED BY S322 (Sprint 8) — satisfies NO-GAP.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, S080..S086,
      S087, S088, S089, S090, S091, and now S092 is outstanding, awaiting your authorization. After
      commit+push: repo is the committed good state; next step S093 (Sprint 2). Laptop-added files
      uncommitted as well — one commit covers all.

