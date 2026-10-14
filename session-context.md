# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S080 (no commits; no NEW md files)
- plan_repo_state : S080 complete — Sprint 2 FIRST FEATURE (XML scanner, dep:S049). **BOTH CI LANES GREEN**
                    (verified from first principles): msvc-debug (VS 2026) build warning-clean + ctest 1/1
                    PASS (100% tests passed); ubuntu-clang (Ubuntu VM, clang 21, shared folder
                    /mnt/hgfs/VamlPOC) build + ctest 1/1 PASS (only pre-existing upstream <ciso646> doctest
                    deprecation from third_party, not this TU). No commit+push performed (per command).
                    **S080 delivered vaml::XmlScanner — a complete, non-MINIMAL-REAL, hand-rolled XML
                    scanner covering all six declared constructs — elements (open/close names incl.
                    dotted Grid.Row + prefixed names tokenized whole), attributes (double + single quote,
                    entities decoded inline in values), self-closing (<a/> is a flag on ElementOpen, NO
                    separate ElementClose), comments (raw content, multi-line, NEVER entity-decoded),
                    CDATA (raw content, ]]&gt; termination, interior ]]] boundary), entities (5 predefined
                    + decimal/hex numeric, decoded inline into Text AND AttributeValue — no separate
                    EntityRef token by design), nested tags (depth stack, interleaved open/close/text,
                    siblings; stray/mismatched end tag -> XmlScanException).** Every token carries 1-based
                    line:col; next() returns false + emits EndOfInput at exhaustion. Structural failures
                    (unterminated comment/CDATA/attr quote/entity, malformed start/end tag) throw
                    message-only XmlScanException; S081 is the explicit structured-error-taxonomy step
                    (NO-GAP hand-off, token line:col already available to consume). NO new product
                    dependency — S096 "no manual XML dep" satisfied by design.
- plan_digest     : S080 (dep:S049). NEW src/core/include/vaml/core/XmlScanner.h — XmlTokenType
                    (ElementOpen/ElementClose/Text/Comment/CData/EndOfInput), XmlAttribute, XmlToken
                    {type,name,value,self_closing,attributes,line,col}, XmlScanner (string_view ctor,
                    next(XmlToken&)->bool), XmlScanException (message-only, std::runtime_error). NEW
                    src/core/src/XmlScanner.cpp — hand-rolled iterative scanner (name rules = letters/
                    digits/_ : . -, comment/CDATA terminator scanning, entity decoder incl. numeric UTF-8,
                    attr parser, depth stack for end-tag matching). NEW test/unit/XmlScanner_test.cpp (25
                    TEST_CASEs via Shouldly vocabulary; drain/typeName/describe/joinSeq helpers). MOD
                    src/core/CMakeLists.txt (+XmlScanner.cpp), MOD test/unit/CMakeLists.txt
                    (+XmlScanner_test.cpp). NEW build/verify_s080.cmd (verify_s046.cmd clone). session-
                    context.md (this).
- files_touched   :
    ADDED    src/core/include/vaml/core/XmlScanner.h
    ADDED    src/core/src/XmlScanner.cpp
    ADDED    test/unit/XmlScanner_test.cpp
    MODIFIED src/core/CMakeLists.txt   (+src/XmlScanner.cpp)
    MODIFIED test/unit/CMakeLists.txt  (+XmlScanner_test.cpp)
    ADDED    build/verify_s080.cmd
    MODIFIED session-context.md (this — S080 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s080.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (100% tests passed, 0 failed). ubuntu-
                    clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation from third_party (not this TU; already non-error).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S081. S081 = XML scanner structured error taxonomy (categories, guaranteed line:col on
                    every malformed path) — the documented consumption point for S080's token line:col
                    and message-only XmlScanException.
- notes           :
    - S080 is NOT a MINIMAL-REAL marker: vaml::XmlScanner is delivered complete for its declared scope
      (all six listed constructs real + tested, 25 TEST_CASEs). Subsequent steps (S081 structured errors,
      S082 namespaces/clr-namespace prefix mapping, S083 x: directives, S084 attribute grammar) extend it
      by normal feature progression — no sentinel, nothing left for silent backfill (NO-GAP intact).
    - Test-authorship note: S080 tests confirm XML semantics precisely — self-closing <a/> is a single
      ElementOpen with self_closing=true (not a separate ElementClose), whitespace between elements IS a
      Text token (not skipped at top level), and Comment/CData content is returned raw (never
      entity-decoded). Mismatched/stray end tag + unknown/unterminated entity throw XmlScanException
      (asserted via ShouldThrow per S036_3).
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push remains outstanding (S032..S044 incl. S036_1..S036_5, S045, S046..S049,
      and now S080) awaiting your authorization. After commit+push: repo is the committed good state;
      next step S081 (Sprint 2).
