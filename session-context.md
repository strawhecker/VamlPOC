# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S081 (no commits; no NEW md files)
- plan_repo_state : S081 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    msvc-debug (VS 2026) build warning-clean + ctest 1/1 PASS; ubuntu-clang (Ubuntu VM,
                    clang 21, shared folder /mnt/hgfs/VamlPOC) build + ctest 1/1 PASS (only pre-existing
                    upstream <ciso646> doctest deprecation from third_party, not this TU). No commit+push
                    performed (per command). **S081 RESOLVED S080's documented message-only hand-off: the
                    scanner now throws a STRUCTURED, categorized XmlScanException.** XmlScanException gained
                    XmlScanError kind() (18-category taxonomy, each 1:1 with a throw site — NO-GAP, no silent
                    malformed path), line()/col() (1-based position of detection, guaranteed >= 1 on every
                    malformed path), and what() augmented with the position for human diagnostics. Private
                    [[noreturn]] XmlScanner::fail(kind,message) centralizes capture so a throw site can't
                    forget position. base std::runtime_error retained (S080 ShouldThrow<XmlScanException>
                    assertions unchanged → no regression). 25 S080 TEST_CASEs still pass; 18 new S081
                    per-category TEST_CASEs (kind + line/col>=1) + 2 exact line:col tests (multi-line
                    unterminated comment line=2; mismatched </b> detection cursor col=8).
- plan_digest     : S081 (dep:S080). MOD src/core/include/vaml/core/XmlScanner.h — added XmlScanError enum
                    (18: ExpectedName, UnterminatedEntity, InvalidNumericReference, UnterminatedNumericReference,
                    CharacterReferenceOutOfRange, UnknownEntity, UnterminatedAttributeValue, UnterminatedComment,
                    UnterminatedCData, UnterminatedStartTag, MalformedSelfClosing, MalformedStartTag,
                    AttributeMissingEquals, AttributeValueNotQuoted, MalformedEndTag, MismatchedEndTag,
                    UnexpectedEndAfterLessThan, UnrecognizedMarkup), restructured XmlScanException (kind/line/col
                    + position-augmented what()), declared private [[noreturn]] XmlScanner::fail(...). MOD
                    src/core/src/XmlScanner.cpp — all 18 throw sites → fail(kind,...); added XmlScanException
                    ctor + fail() impl. MOD test/unit/XmlScanner_test.cpp — READER/assertStructured helpers
                    (+<optional>), 18 S081: per-category TEST_CASEs + 2 exact-line:col tests; existing S080
                    ShouldThrow<XmlScanException> cases untouched. CMakeLists: no change (no new source/TU).
                    NEW build/verify_s081.cmd (verify_s080.cmd clone). session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/XmlScanner.h      (XmlScanError enum, XmlScanException kind/line/col, fail decl)
    MODIFIED src/core/src/XmlScanner.cpp                  (18 throw sites → fail, ctor + fail impl, header comment)
    MODIFIED test/unit/XmlScanner_test.cpp                (S081 structured-error tests + helpers)
    ADDED    build/verify_s081.cmd
    MODIFIED session-context.md (this — S081 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s081.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (100% tests passed, 0 failed). ubuntu-
                    clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream <ciso646>
                    doctest deprecation from third_party (not this TU; non-error). (windows-clang-debug is
                    NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S082. S082 = XML scanner namespaces (clr-namespace/prefix mapping, full Unicode name
                    handling deferred from S080). Structured-error + line:col plumbing already available to
                    extend. XmlScanner is a full feature (NOT MINIMAL-REAL) — S082 extends it by normal
                    feature progression.
- notes           :
    - S081 RESOLVED S080's explicitly-scheduled hand-off (documented in S080 header/roadmap/progress): the
      message-only XmlScanException is now the structured taxonomy. NO silent backfill — this is the real
      feature's own step (NO-GAP + NO-LAZY-LLM honored).
    - Taxonomy contract: XmlScanError has exactly 18 members, each mapping 1:1 to a distinct throw site in
      XmlScanner.cpp (verified by grep: zero remaining `throw XmlScanException(` in the .cpp; all are
      `fail(...)`). Adding a new malformed path REQUIRES adding a category (or reusing one) — the contract
      that keeps the guarantee honest.
    - Position semantics: line()/col() = the CURRENT scanner cursor at detection (for unterminated constructs
      this is the EOF position; for mismatched </b> it's after consuming the element — col 8, not the tag
      start). Token-START positions remain on XmlToken (unchanged). Documented in tests + header.
    - Constructor signature change on XmlScanException is intentional and recorded: it is new public surface
      (S080) with no external consumers yet; S081 is the declared step that formalized it.
    - Fixed during implementit: (a) `&` followed by `<` is UnknownEntity, not UnterminatedEntity — fixed the
      UNTERMINATED-entity test to use `&` at EOF; (b) mismatched-end exact position is the detection cursor
      (col 8) not the token start (col 4); (c) clang -Werror unused-parameter on assertStructured's `src`
      arg — removed the param + updated all 18 call sites.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, and now S080+S081
      is outstanding, awaiting your authorization. After commit+push: repo is the committed good state;
      next step S082 (Sprint 2). Laptop-added files uncommitted as well — one commit covers all.
