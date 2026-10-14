# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S082 (no commits; no NEW md files)
- plan_repo_state : S082 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    msvc-debug (VS 2026) build warning-clean + ctest 1/1 PASS; ubuntu-clang (Ubuntu VM,
                    clang 21, shared folder /mnt/hgfs/VamlPOC) build + ctest 1/1 PASS (only pre-existing
                    upstream <ciso646> doctest deprecation from third_party, not this TU — S082 TUs add no
                    warning). No commit+push performed (per command). S082 made the XML scanner
                    NAMESPACE-AWARE (XmlScanner is a FULL feature extended by normal progression, not
                    MINIMAL REAL): (1) tag/attribute names split on the FIRST ':' into (prefix, local_name)
                    via XmlScanner::splitName — the '.' attached-owner form ("Grid.Row") is NOT a namespace
                    separator; `name` (and XmlAttribute::name) stay the FULL raw text so S080's assertions
                    hold unchanged; (2) a per-element xmlns SCOPE stack (nsStack_, std::map prefix->uri incl.
                    "" default) is pushed on every ElementOpen (inheriting the enclosing frame + applying its
                    OWN xmlns self-applying) and popped on the matching ElementClose/self-close; (3) an
                    unprefixed ELEMENT resolves to the default namespace via resolvePrefix(""), an unmapped
                    prefix resolves to "" (NOT a scan error — the structured unknown-namespace/type authority
                    is S085/S087, recorded NO-GAP); the default does NOT apply to ATTRIBUTES (XML rule);
                    (4) xmlns/xmlns:p are NAMESPACE DECLARATIONS collected into XmlToken::xmlns, never
                    emitted as ordinary attributes; (5) a WPF "clr-namespace:Ns.Name[;assembly=Asm]" value is
                    TOKENIZED into ClrNamespaceReference{clrNamespace,assembly} by the new public
                    parse_clr_namespace(value[,&isClrNamespace][,line,col]) (parts only; type resolution is
                    S087 — recorded NO-GAP), with a malformed body (empty ns / bad ';' suffix) throwing the
                    new XmlScanError::MalformedClrNamespace category — keeps the S081 1:1 kind<->throw-site
                    taxonomy contract honest. XmlScanError now has 19 categories. 12 new S082 TEST_CASEs in
                    a dedicated test/unit/XmlNamespace_test.cpp (S082-unique urn:vm:ns:* URIs); all existing
                    S080/S081 scanner tests unchanged & green.
- plan_digest     : S082 (dep:S081). MOD src/core/include/vaml/core/XmlScanner.h — added prefix/local_name/
                    namespace_uri to XmlAttribute + XmlToken, added XmlToken::xmlns (vector<pair<prefix,uri>>),
                    new struct ClrNamespaceReference, new free parse_clr_namespace(...) (VAML_CORE_EXPORT),
                    new XmlScanError::MalformedClrNamespace (now 19 categories), private static
                    XmlScanner::splitName + const resolvePrefix, member std::vector<std::map<...>> nsStack_,
                    <map> include, S082 header rationale. MOD src/core/src/XmlScanner.cpp — reworked
                    scanElementOpen (collect xmlns declarations + ordinary attrs together, push+apply the
                    scope for EVERY element incl. self-closing, resolve attrs + own name AFTER all xmlns are
                    known, pop immediately for self-closing / keep for children), added splitName +
                    resolvePrefix + parse_clr_namespace impls (2 new MalformedClrNamespace throw sites),
                    popped nsStack_ in scanElementClose, cleared new fields in scanMarkup/next. MOD
                    test/unit/CMakeLists.txt (add XmlNamespace_test.cpp). ADDED test/unit/
                    XmlNamespace_test.cpp (12 cases) + build/verify_s082.cmd (verify_s081 clone). MOD
                    docs/roadmap.md (S082 [DONE]). session-context.md (this).
- files_touched   :
    MODIFIED src/core/include/vaml/core/XmlScanner.h      (attr/token prefix+local+ns, xmlns list, ClrNamespaceReference, parse_clr_namespace, MalformedClrNamespace, splitName/resolvePrefix/nsStack_)
    MODIFIED src/core/src/XmlScanner.cpp                  (scanElementOpen ns scope + resolution, splitName/resolvePrefix/parse_clr_namespace, scanElementClose pop, new-field clears)
    MODIFIED test/unit/CMakeLists.txt                     (add XmlNamespace_test.cpp)
    ADDED    test/unit/XmlNamespace_test.cpp              (12 S082 namespace cases)
    ADDED    build/verify_s082.cmd
    MODIFIED docs/roadmap.md                              (S082 [DONE])
    MODIFIED session-context.md (this — S082 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s082.cmd —
                    configure+build warning-clean (238 test cases, 1134 assertions all green) + ctest 1/1
                    PASS. ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21,
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS (238 cases). Only Clang warnings =
                    pre-existing upstream <ciso646> doctest deprecation from third_party (not this TU
                    either; non-error). (windows-clang-debug is NOT a lane — not built/tested/flagged per
                    standing rule.)
- next_step       : S083. S083 = x: directives (Name, Class, Key, Null) tokenized — layers on top of S082's
                    prefix/local split + xmlns scope (x: resolves via the xmlns:x declaration). Scanner + ns
                    plumbing ready; XmlScanner stays a full feature extended by normal progression.
- notes           :
    - S082 is pure feature progression on XmlScanner (a FULL feature, no MINIMAL REAL introduced/resolved).
      The structured "unknown namespace"/type-resolution authority is explicitly S085/S087's job (recorded
      NO-GAP, not a silent omission); parse_clr_namespace only TOKENIZES the mapping into parts.
    - Taxonomy contract preserved: XmlScanError grew 18 -> 19 categories EXACTLY ONE new member
      (MalformedClrNamespace) mapping 1:1 to the new clr-namespace malformed path (2 throw sites in the
      parser). All 18 prior categories + their S081 throw sites untouched.
    - Position semantics (S081) unchanged: parse_clr_namespace takes optional line/col (defaults 1,1); the
      scanner passes its real cursor when it calls it (not currently wired into scanElementOpen — a
      clr-namespace attribute VALUE at S082 is still just an ordinary string; parse_clr_namespace is the
      explicit S082 "mapping tokenized" deliverable the S085/S087 parser will invoke with a real position).
    - Design note (self-application order): the element's own xmlns declarations are applied to a fresh
      frame BEFORE its own name/attributes resolve, so <Self xmlns="..."/> (and prefixed attributes) see
      their own declarations regardless of declaration order within the tag — XML semantics.
    - Fixed during implementit: (a) resolvePrefix was not declared in the header (added private member);
      (b) clang -Werror unused-function firstOpen helper — removed it; (c) initial implementation resolved
      attributes inline (before all xmlns were read) — moved resolution AFTER the full tag loop to honor
      order-independent self-application; (d) self-closing elements now push+apply their own xmlns scope
      too (then pop), so <Self xmlns="..."/> resolves its own name.
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, and now S080+
      S081+S082 is outstanding, awaiting your authorization. After commit+push: repo is the committed good
      state; next step S083 (Sprint 2). Laptop-added files uncommitted as well — one commit covers all.
