# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S091 (no commits; no NEW md files)
- plan_repo_state : S091 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    windows-msvc (VS 2026) via build/verify_s091.cmd configure+build warning-clean +
                    ctest 1/1 PASS; ubuntu-clang (Ubuntu VM, clang 21, shared folder
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. No commit+push (per command).
                    S091 delivers the RUNTIME reader entry point: VamlReader::Load(text/ParsedElement ->
                    live dependency-object tree), building children + setting props. PRODUCT surface:
                    VamlReader (Load(std::string_view) = parse_vaml then Load(root); Load(const
                    ParsedElement&) builds a live tree — the headline; retained_objects() exposes the
                    non-root built nodes owned by the reader), VamlReaderException + VamlReaderError::
                    {None,UnknownProperty} (the S091 unknown-property baseline; S094 owns the full
                    reader error taxonomy). BUILD ALGORITHM (private recursive build): resolve_element
                    (S088; unknown type -> ElementResolutionException naming type+line) -> set props
                    (x-directives x:Name/x:Class->S092, x:Key->S361, x:Null->S084 and markup-extensions
                    isExtension->S092 SKIPPED — recorded NO-GAP not silent; plain literals resolved via
                    DependencyProperty::TryGet (nullptr -> VamlReaderException) + converted via
                    ValueConverterRegistry::global() (String identity; Int32/Bool/Color/Thickness by
                    boundType; malformed/unknown-type -> ValueConversionException) + Set on the S043
                    attached-aware path (dotted keys pass through by name)) -> build children
                    (recurse, child->set_parent, move into retained_; no owned children list — S121) ->
                    return the root (owned exactly once; non-root owned by the reader per Decision D1).
                    Startup lazily runs register_builtin_elements() + register_builtin_converters()
                    once (idempotent guards).
- plan_digest     : S091 (dep:S090). ADDED src/core/include/vaml/core/VamlReader.h (VamlReader +
                    VamlReaderException/VamlReaderError{None,UnknownProperty}; ownership model D1;
                    build-algorithm doc). ADDED src/core/src/VamlReader.cpp (VamlReaderException ctor;
                    VamlReader ctor/dtor; converterNameFor boundType->"Int32|Bool|Color|Thickness";
                    ensure_startup_registrations(); convertLiteral (String identity else global registry
                    convert); build (resolve -> set props with directive/isExtension skip + TryGet->
                    throw + Set -> recurse children with set_parent -> retain); Load(root) clears
                    retained_ then build; Load(text) = Load(parse_vaml)). MOD src/core/CMakeLists.txt
                    (+src/VamlReader.cpp). ADDED test/unit/VamlReader_test.cpp (9 TEST_CASEs: root
                    build; children built+retained+parent-linked; plain literals converted+set incl.
                    attached S091Grid.Row; isExtension+x-directive skipped; unknown property throws;
                    ShouldThrow form; malformed literal throws; unknown element throws; Load(ParsedElement)
                    parity with Load(text); two-level nesting parent chain). MOD test/unit/CMakeLists.txt
                    (+VamlReader_test.cpp). ADDED build/verify_s091.cmd. MOD docs/roadmap.md (S091
                    [DONE] entry), session-context.md (this).
- files_touched   :
    ADDED    src/core/include/vaml/core/VamlReader.h           (VamlReader + exception + reader taxonomy)
    ADDED    src/core/src/VamlReader.cpp                       (Load/build/convertLiteral impl)
    MODIFIED src/core/CMakeLists.txt                           (+ src/VamlReader.cpp)
    ADDED    test/unit/VamlReader_test.cpp                     (9 S091 TEST_CASEs)
    MODIFIED test/unit/CMakeLists.txt                          (+ VamlReader_test.cpp)
    ADDED    build/verify_s091.cmd
    MODIFIED docs/roadmap.md                                   (S091 [DONE])
    MODIFIED session-context.md (this — S091 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s091.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (all cases incl. new S091 green).
                    ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation from third_party (not this TU; non-error, already
                    downgraded via -Wno-error=#warnings in the test CMakeLists).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S092. S092 = VamlReader defers markup-extension values (S2 tag) + event hooks (S8);
                    records x:Name registry. S092 is the documented resolution point for the deferral
                    S091 made explicit: markup-extension attributes (isExtension=true, currently
                    SKIPPED by the reader) get resolved (markup {Binding} etc.), and x:Name/x:Class
                    directives (also currently skipped) get recorded/applied. S091 leaves these as
                    recorded NO-GAP deferrals, NOT silent.
- notes           :
    - S091 is the documented upstream consumer of S090's try_infer_type (the recognizer). This step
      converts plain literals via the boundType->converter-name mapping + global convert() rather than
      the recognizer; the recognizer's "is this literal a known value at all?" question is more naturally
      the S092/S093/S094 markup/error-taxonomy concern (where UnknownLiteral becomes reachable via the
      reader). Recorded, not a silent omission.
    - D1 OWNERSHIP MODEL (accepted): the reader retains non-root built objects in retained_objects()
      and returns the root; a plain DependencyObject carries no visual-children list (that is S121,
      recorded no overlap/no gap); ownership of non-root nodes stays in the reader for the live Load
      call (no dangling — set_parent is non-owning) and transfers into the UIElement visual tree at
      S120/S121. retained_.clear() at each Load so reusing one reader never leaks children.
    - Assertion detail in test 4: xmlns:* declarations are NOT ParsedAttributes (excluded at the
      scanner, S082/S085), so the deferral proof uses a real kXamlLanguageNamespace xmlns:x + an
      isExtension {Binding ...} + x:Name/x:Key directives and asserts the object still builds with the
      property at its default.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, S080..S086,
      S087, S088, S089, S090, and now S091 is outstanding, awaiting your authorization. After commit+push:
      repo is the committed good state; next step S092 (Sprint 2). Laptop-added files uncommitted as
      well — one commit covers all.

