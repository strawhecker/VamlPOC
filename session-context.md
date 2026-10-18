# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **S144 FULLY COMPLETE — BOTH CI LANES GREEN** (migrate the three lookup registries to
                    the token-keyed store + public accessor shape migration; dep:S143; rewrites
                    S087/S089/S091/S092/S121 in place). PART A (registry re-key): ElementFactoryRegistry +
                    ValueConverterRegistry internal indexes re-keyed OFF std::unordered_map<std::string,...>
                    + owned-name order vectors ONTO vaml::detail::NameTable (the S141-pattern CONTENT-keyed
                    probe store — self-owned stable keys, content match, NO transient std::string on find) —
                    mirroring PropertyRegistry / DependencyProperty::GlobalTable (already at S143, verified
                    not re-done). registered_names() on BOTH returns std::vector<std::string_view> (no
                    by-value vector<std::string> copy). NameTable gains a names() insertion-order listing.
                    PART B (VamlReader accessor shape): VamlReader::names() keys → INTERNED string_views
                    (S140 discipline; content-equal; no owned std::string per entry); name_registered()/
                    named() compare by content. PART C (out-param accessors): UIElement::children() →
                    void children(std::vector<UIElement*>& out); collect_descendant_effectives → out-param.
                    PART D: reserve() the parser/reader vectors (parse_vaml attributes + VamlReader
                    retained_/deferred via count_non_root_descendants). MINIMAL REAL contracts untouched.
                    VERIFIED BOTH LANES: windows-msvc build/verify_s144.cmd msvc-debug warning-clean
                    (628 test cases / 4994 passing assertions / 0 failed; the single in-suite "failed"
                    assertion is the intentional S036_1 should_fail demonstration) + ubuntu-clang clang-debug
                    from /mnt/hgfs/VamlPOC (ctest 1/1 PASS; only pre-existing upstream <ciso646> doctest
                    deprecation). windows-clang-debug NOT a lane (not built/tested/flagged). No commits.
                    (Prior — complete/superseded runs for S142/S143 recorded in progress.md CHECKPOINT +
                    STEP LOG; see those.)
- plan_repo_state : **S144 COMPLETE + GREEN (BOTH LANES)** — the three lookup registries now all live on
                    the content-keyed store. DependencyProperty::GlobalTable + PropertyRegistry: already on
                    NameTable (S143, verified). ElementFactoryRegistry + ValueConverterRegistry: re-keyed
                    onto NameTable at S144 (unordered_map + owned-string order vectors REMOVED; content
                    probe on string_view, no transient std::string on find; registered_names() →
                    vector<string_view>). VamlReader::names() keys → interned string_view (no owned-name
                    gotcha). children()/collect_descendant_effectives → out-param (no return-by-value).
                    Parser/reader vectors reserve()'d. ADR-018: ZERO new locks. Next = S145 (block pool
                    free-list, thread-local, retain-forever, LOCKLESS).
- plan_digest     : S144 = MOD src/core/src/NameTable.h (names() insertion-order listing) + MOD
                    src/core/src/ElementFactoryRegistry.cpp + src/core/src/ValueConverter.cpp (re-key onto
                    NameTable; registered_names() → vector<string_view>; try_infer_type/convert content-
                    probe) + MOD ambient headers (registered_names() return type) + MOD src/core/src/
                    VamlReader.cpp + include/VamlReader.h (interned string_view names key; reserve() via
                    count_non_root_descendants) + MOD src/core/src/UIElement.cpp + include/UIElement.h
                    (children(out)) + MOD src/core/src/DependencyObject.cpp + include/DependencyObject.h
                    (collect_descendant_effectives out-param; both call sites) + MOD src/core/src/
                    ParsedElement.cpp (attributes reserve) + MOD tests (S087/S089/S090/S092/S093/S095/S121
                    shapes) + ADDED build/verify_s144.cmd + MOD docs/roadmap.md (S144 [DONE]) + MOD
                    progress.md (step log + checkpoint last=S144/next=S145) + MOD session-context.md (this).
- files_touched   :
    MODIFIED src/core/src/NameTable.h                             (names() insertion-order listing accessor)
    MODIFIED src/core/src/ElementFactoryRegistry.cpp              (re-key onto NameTable; registered_names → string_view)
    MODIFIED src/core/include/vaml/core/ElementFactoryRegistry.h  (registered_names return type + S144 comment)
    MODIFIED src/core/src/ValueConverter.cpp                      (re-key onto NameTable; try_infer_type/convert content-probe)
    MODIFIED src/core/include/vaml/core/ValueConverter.h          (registered_names return type + S144 comment)
    MODIFIED src/core/src/VamlReader.cpp                          (interned string_view names keys; reserve(); name_registered/named)
    MODIFIED src/core/include/vaml/core/VamlReader.h              (names() string_view keys + member type)
    MODIFIED src/core/src/UIElement.cpp                           (children() → out-param)
    MODIFIED src/core/include/vaml/core/UIElement.h               (children(out) signature + comment)
    MODIFIED src/core/src/DependencyObject.cpp                    (collect_descendant_effectives out-param + 2 call sites)
    MODIFIED src/core/include/vaml/core/DependencyObject.h        (collect_descendant_effectives out-param)
    MODIFIED src/core/src/ParsedElement.cpp                       (per-element attributes reserve)
    MODIFIED test/unit/ElementFactoryRegistry_test.cpp            (registered_names string_view shape)
    MODIFIED test/unit/ValueConverter_test.cpp                    (registered_names string_view shape)
    MODIFIED test/unit/ValueConverterRegistry_test.cpp            (moved/assigned registered_names string_view shape)
    MODIFIED test/unit/VamlReader_defer_test.cpp                  (names()[i].first string_view)
    MODIFIED test/unit/VamlReader_s093_test.cpp                   (names()[i].first string_view)
    MODIFIED test/unit/GoldenDocs_test.cpp                        (names()[i].first string_view)
    MODIFIED test/unit/VisualTree_test.cpp                        (children(out) out-param)
    ADDED build/verify_s144.cmd                                   (windows-msvc lane driver)
    MODIFIED docs/roadmap.md                                      (S144 [DONE])
    MODIFIED progress.md                                          (step log + checkpoint last=S144/next=S145)
    MODIFIED session-context.md (this)
    NOTE: src/core/include/vaml/core/export.h + src/tools + src/vulkan (clang __attribute__ deprecated
          adaptation) were ALREADY modified in the working tree from a prior sprint — NOT touched by S144.
- build_status    : **GREEN — BOTH CI LANES (S144, verified from first principles).** windows-msvc:
                    build/verify_s144.cmd (msvc-debug, VS 2026) — warning-as-error clean; test exe
                    628 test cases / 4994 passing assertions / 0 failed (the single in-suite "failed"
                    assertion is the intentional S036_1 should_fail demonstration). ubuntu-clang:
                    /mnt/hgfs/VamlPOC clang-debug (Ubuntu VM) — configure+build clean + ctest 1/1 PASS
                    (100%), 628 cases SUCCESS. Only Clang warnings = pre-existing upstream <ciso646> doctest
                    deprecation (non-error). (windows-clang-debug is NOT a lane — not built/tested/flagged.)
                    No commits.
- next_step       : next roadmap token after S144 — **S145 (dep:S144)**: block pool free-list — thread-local,
                    retain-forever, LOCKLESS (the object pool stays lockless per ADR-018). S144 is COMPLETE +
                    GREEN; S145 is the next unimplemented task.
- notes           :
    - The S144 title's "three lookup registries (ElementFactoryRegistry, ValueConverterRegistry,
      DependencyProperty global table)" — the DependencyProperty global table was ALREADY token-keyed
      content store (NameTable) at S143 (verified, not re-done). S144's real work = the two remaining
      unordered_map registries + the public accessor shape migration. ADR-018: ZERO new locks (all four
      registries retain single-threaded-registration-then-read; content-keyed stores add no lock).
    - The pre-existing pimpl is KEPT on ElementFactoryRegistry/ValueConverterRegistry for include-surface
      isolation only (NameTable.h lives under src/core/src; same rationale as PropertyRegistry/
      DependencyProperty at S143). The unordered_map + owned-string order vector are GONE.
    - registered_names() now returns std::vector<std::string_view> into the registry's OWNED stable
      storage (valid for the registry's lifetime) — removes the owned-name gotcha; callers that need an
      owned copy (e.g. the S089 try_infer_type outType path) copy explicitly.
    - VamlReader::names() keys are interned string_views (vaml::intern → S140 thread-local pool) — valid
      for this reader's thread; named()/name_registered() compare by content. root_class_ stays owned
      std::string (not hot path, per S140 decision).
    - children() out-param: child_count()/child_at() remain the no-alloc read paths; children(out) is for
      bulk iteration without a return-by-value copy. collect_descendant_effectives is private; both
      internal call sites updated.
    - NO NEW md files (directive honored). No "backfill" — everything resolved at its own sprint
      (S144 rewrites S087/S089/S091/S092/S121 in place). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044, S045, S046..S049, S080..S086, S087, S088, S089, S090,
      S091, S092, S093, S094, S095, S096, S097, S120, S121, S122, S123, S124, S125, S126, S127, S128,
      S129, S130, S130_1, S131, S132, S133, S134, S135, S136, S137, S138, S139, S140, S140_1, S141,
      S142, S143, and S144 is outstanding, awaiting your authorization.
