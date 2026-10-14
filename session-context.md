# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S083 (no commits; no NEW md files)
- plan_repo_state : S083 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    msvc-debug (VS 2026) via build/verify_s083.cmd configure+build warning-clean +
                    ctest 1/1 PASS (238 cases incl. 12 new S083 cases, 45 new assertions); ubuntu-clang
                    (Ubuntu VM, clang 21, shared folder /mnt/hgfs/VamlPOC) configure+build + ctest 1/1
                    PASS (only pre-existing upstream <ciso646> doctest deprecation from third_party, not
                    this TU — S083 TUs add no warning). No commit+push (per command). S083 made the XML
                    scanner classify the four XAML x-language directives (Name/Class/Key/Null) as a
                    distinct, structured token category, layering on S082's namespace model. NEW public
                    surface in XmlScanner.h: inline constexpr std::string_view kXamlLanguageNamespace =
                    "http://schemas.microsoft.com/winfx/2006/xaml"; enum class XmlDirectiveType { None,
                    Name, Class, Key, Null }; XmlAttribute::directive (default None). Classification is
                    keyed on the RESOLVED namespace_uri == kXamlLanguageNamespace, NOT the literal "x"
                    prefix: any prefix mapped to the canonical xaml URI is the x-language (foo:Name IS a
                    Name directive), while S082's fake xmlns:x="urn:vm:ns:x" attrs stay PLAIN attributes
                    (backward compat — all S080/S081/S082 tests unchanged & green). Matching case-sensitive;
                    unknown local in the xaml ns -> None. Tagged ON the attribute (value stays in `value`),
                    keeping the single ordered attributes list intact. XmlScanner.cpp: static/file-local
                    classifyXDirective helper + wired into the S082 attribute-resolution loop
                    (attr.directive = uri==xaml ? classify(local) : None); reset via per-token attribute
                    rebuild (XmlAttribute default None). XmlScanError taxonomy UNCHANGED (19 categories).
                    12 new S083 TEST_CASEs in a dedicated test/unit/XmlDirective_test.cpp (S083-unique via
                    the real kXamlLanguageNamespace), 45 assertions. TOKENIZATION ONLY — application
                    recorded NO-GAP (not silent): x:Name name-registry -> S092; x:Class root-owner binding
                    -> S092/S089; x:Key resource-key -> S361; x:Null null-set ({x:Null} -> S084) and
                    element-in-language-ns -> parsed by S085.
- plan_digest     : S083 (dep:S082). MOD src/core/include/vaml/core/XmlScanner.h — S083 header rationale
                    block + inline constexpr std::string_view kXamlLanguageNamespace (canonical XAML URI)
                    + enum class XmlDirectiveType {None,Name,Class,Key,Null} + XmlAttribute::directive
                    (default None). MOD src/core/src/XmlScanner.cpp — added anonymous-namespace
                    static classifyXDirective(local) helper (Name/Class/Key/Null case-sensitive) + wired
                    into the S082 attribute-resolution loop (attr.directive = namespace_uri==xaml ?
                    classify(local) : None); XmlAttribute reconstructed per token so directive resets
                    (None) inherently. MOD test/unit/CMakeLists.txt (add XmlDirective_test.cpp). ADDED
                    test/unit/XmlDirective_test.cpp (12 cases) + build/verify_s083.cmd (verify_s082 clone).
                    MOD docs/roadmap.md (S083 [DONE]). session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/XmlScanner.h      (kXamlLanguageNamespace, XmlDirectiveType, XmlAttribute::directive, S083 doc block)
    MODIFIED src/core/src/XmlScanner.cpp                  (classifyXDirective helper + directive wiring in attribute-resolution loop)
    MODIFIED test/unit/CMakeLists.txt                     (add XmlDirective_test.cpp)
    ADDED    test/unit/XmlDirective_test.cpp              (12 S083 directive cases)
    ADDED    build/verify_s083.cmd
    MODIFIED docs/roadmap.md                              (S083 [DONE])
    MODIFIED session-context.md (this — S083 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s083.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (250 test cases, ~1179 assertions all
                    green; 12 new S083 cases + 45 assertions). ubuntu-clang: clang-debug (Ubuntu VM
                    192.168.198.129, clang 21, /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. Only
                    Clang warnings = pre-existing upstream <ciso646> doctest deprecation from third_party
                    (not this TU; non-error). (windows-clang-debug is NOT a lane — not built/tested/flagged
                    per standing rule.)
- next_step       : S084. S084 = attribute value grammar — quoted literal, markup-extension {Binding...}
                    tagged, numeric detection — layers on S083 (which already exposed the directive
                    classification + kXamlLanguageNamespace so {x:Null} and language-ns handling have a
                    URI-keyed base). Scanner + ns + directive plumbing ready.
- notes           :
    - S083 is pure feature progression on XmlScanner (a FULL feature, no MINIMAL REAL introduced/resolved).
      The XmlScanError taxonomy is UNCHANGED (still 19 categories) — S083 adds no malformed path, so the
      S081 1:1 kind<->throw-site contract is untouched. The directive is classification-only.
    - KEY DESIGN (user-confirmed at finalizeit): classification is keyed on the resolved namespace URI
      (kXamlLanguageNamespace), not the literal "x" prefix. This is XAML-correct (the URI, not the letter,
      is authoritative) and PRESERVES S082 backward compat by construction — S082's fake
      xmlns:x="urn:vm:ns:x" attrs resolve to a non-language URI and stay None, so every S080/S081/S082
      test remains unchanged & green.
    - NO-GAP recorded (application deferred, each with a named resolution token — not silent backfill):
      x:Name name-registry -> S092; x:Class root-owner binding -> S092/S089; x:Key resource-key -> S361;
      x:Null null-set -> {x:Null} markup-extension tagged S084; a bare element already in the language
      namespace surfaces via S082's namespace_uri and is parsed/interpreted by S085.
    - Added the directive to XmlAttribute only (attributes carry values; an element-level directive carve
      is intentionally NOT made — element-in-language-ns is S085's parse concern, recorded).
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, and S080, S081,
      S082, and now S083 is outstanding, awaiting your authorization. After commit+push: repo is the
      committed good state; next step S084 (Sprint 2). Laptop-added files uncommitted as well — one commit
      covers all.
