# session-context.md

last_command: implementit (S642 — GENERIC CAPABILITY PARITY — dep:S641; the typed capability catalogue
both sides implement IDENTICALLY — the "keep both in sync" mechanism)
plan_digest: S642 authors the PARITY MATRIX (capability → our side status → WPF side status → parity yes/no
→ named gap, never silent) + the typed capability catalogue (typed slots: 10 int / 10 bool / 10 string / 10
double; icons: 10; text: 10 — soft-start counts) + the SYNC TOKEN (a capability may only be introduced by
updating the LIB side AND the WPF-emitter side AND the matrix IN THE SAME STEP). REVISIT S640b/S641
(metadata fine-tune, PROVISIONAL) — iterated the emitter OUTPUT metadata + the tree-scanner placeholder
against the typed-slot/icon/text capabilities; the metadata shape is PROVISIONAL and locks only after
S643's screens. **THIS SESSION: S642 implemented; BOTH CI LANES GREEN: windows-msvc (build/verify_s642.cmd
warning-as-error clean + ctest 4/4 incl. the 2 S642 suites) + ubuntu-clang (VM /mnt/hgfs/VamlPOC clang-debug
UBUNTU-GREEN) + the PARITY MATRIX gen + SYNC-TOKEN check (60 capabilities in sync) + the Windows-authoring
gate (dotnet WPF-emitter build + the catalogue-driven sidecar deposit confirmed).**

## files_touched — S642 (this session)
- tools/capability_catalogue.json (NEW): the SINGLE-SOURCE-OF-TRUTH catalogue (60 capabilities: 10 int / 10
  bool / 10 string / 10 double / 10 icon / 10 text; each row id/family/type/kind/tolerance/our/wpf/parity/gap;
  counts_are_soft_start: true).
- tools/parity_matrix_gen.py (NEW): the PARITY MATRIX generator (docs/PARITY_MATRIX.md, derived never
  hand-edited; fails-loud on blank/unknown status or parity=no-without-gap).
- tools/parity_sync_check.py (NEW): the SYNC-TOKEN enforcer (every capability in sync across our + WPF + matrix).
- docs/PARITY_MATRIX.md (generated): the derived parity matrix (60 rows) + family-summary + soft-start token.
- src/presentation/include/vaml/presentation/scan/CapabilityCatalogue.h + src/presentation/src/scan/
  CapabilityCatalogue.cpp (NEW): the LIB side — parses the embedded catalogue into a CapabilityCatalogue
  (version/counts_are_soft_start/capabilities) + find_capability/catalogue_tolerance_for/family_count/summarize;
  a hand-rolled focused JSON scanner (handles object/array/string/boolean fields) — the S641 EmitterMetadata
  precedent.
- src/presentation/include/vaml/presentation/scan/EmitterMetadata.h + src/presentation/src/scan/
  EmitterMetadata.cpp (MOD): read the per-quad Capability (SYNC-TOKEN id) + Tolerance fields when present
  (forward-compat — S641 keeps working; unknown fields ignored).
- src/presentation/include/vaml/presentation/scan/Scanner.h + src/presentation/src/scan/Scanner.cpp (MOD):
  NEW compliance_compare_catalogued(actual, expected, sidecar) — the catalogue-driven compare; per-quad
  tolerance merge: SCAN-DECLARATION-tolerance WINS > sidecar catalogue-tolerance > built-in kind default.
- src/presentation/CMakeLists.txt (MOD): + src/scan/CapabilityCatalogue.cpp into vaml-presentation.
- test/unit/CapabilityCatalogue_s642_test.cpp (NEW): embedded catalogue loads; soft-start family counts 10x6;
  per-capability kind/tolerance; catalogue-tolerance-resolve; family counts; the SYNC-TOKEN parity assertion.
- test/unit/CapabilityCompare_s642_test.cpp (NEW): the sidecar Capability/Tolerance fields parse
  forward-compatibly; the S641 forward-compat contract; the catalogue-driven compare (sidecar icon capability
  -> loose tolerance; declared-tolerance WINS; a catalogued string capability -> text tolerance); catalogue-tolerance-resolve.
- test/unit/CMakeLists.txt (MOD): + the 2 S642 suites into vaml-presentation-tests.
- wpf_emitter/ImageBuffer.cs (MOD): QuadMetadata + Capability + Tolerance fields (the provisional S642 shape).
- wpf_emitter/ElementTreeScanner.cs (MOD): the catalogue-driven tree-scanner — **RESOLVES the S640b
  tree-scanner MINIMAL REAL marker at its own step** (the placeholder grows into the real scanner; harvests
  text/icon quads tagged with the capability id + kind + tolerance through the catalogue + a deterministic
  fallback; the [MINIMAL REAL] sentinel removed, the provenance record kept in source docs).
- wpf_emitter/README.md (MOD): the scanner metadata section updated (catalogue-driven; the marker RESOLVED).
- wpf_emitter/samples/WpfEmitterSample.xaml (unchanged): the sample the authoring gate runs.
- build/verify_s642.cmd (NEW): the two-lane gate (new-files + MINIMAL-REAL resolution audit + matrix gen +
  sync-check + windows-msvc + ubuntu-clang + the authoring gate).
- build/verify_s642_authoring.cmd (NEW): the Windows-authoring gate (grep-audit marker-resolved + dotnet
  WPF-emitter build + run against the sample -> the sidecar carries Capability/Tolerance).
- docs/roadmap.md (MOD): S642 token -> [DONE] with the full digest (DELIVERED bullets + COUNT-SCOPE token +
  NO-GAP note + REVISIT record).
- docs/progress.md (MOD): the S642 record (this session digest + build status + checkpoint).
- session-context.md (MOD): this entry (fresh rewrite).

## build_status (S642 — from first principles)
- windows-msvc (the CI lane) via build/verify_s642.cmd: cmake --preset msvc-debug configure clean + build
  warning-as-error clean + ctest 4/4 PASS (vaml-core / vaml-tools / vaml-vulkan / vaml-presentation — the
  vaml-presentation suite now includes the 2 S642 suites: CapabilityCatalogue_s642 + CapabilityCompare_s642,
  all green). The PARITY MATRIX gen + SYNC-TOKEN check run in the gate (60 capabilities in sync).
- ubuntu-clang: RUN + GREEN this session (build/verify_s642.cmd -> UBUNTU-GREEN; /mnt/hgfs/VamlPOC
  clang-debug configure+build+ctest 4/4).
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).
- Windows-authoring gate (local, NOT CI): verify_s642_authoring.cmd -> GREEN (minimal-real audit OK + dotnet
  WPF-emitter build clean + run over the sample -> the deposited sidecar carries Capability: icon_01/text_01 +
  Tolerance — the S642 metadata shape confirmed).

## next_step — S642 implemented (BOTH CI LANES GREEN)
- S642 implemented: the PARITY MATRIX (generated) + the typed capability catalogue (LIB + WPF-emitter sides,
  embedded/runtime-loaded from the SAME JSON) + the SYNC-TOKEN mechanism (parity + the catalogue-driven
  compare) + the metadata fine-tune (REVISIT S640b/S641, PROVISIONAL). The S640b tree-scanner MINIMAL REAL
  marker RESOLVED at its own step. Headless-ctested on BOTH lanes + the authoring gate + the matrix/sync-check.
- Next per the roadmap chain = **S643** (TOOL SMOKE TEST — ONE .xaml using ONE of each type (1 int/1 bool/1
  string/1 icon/1 text) proving the run->scan->compare script end-to-end + the S642/S643 REVISIT metadata
  fine-tune — confirm the placeholder tree-scanner metadata suffices or extend it).

## notes
- SINGLE-SOURCE-OF-TRUTH: the catalogue (tools/capability_catalogue.json) is read by the LIB side (embedded
  at compile time — no runtime file dep, both CI lanes run the identical bytes), the WPF-emitter C# side
  (runtime load with a deterministic fallback), and the PARITY MATRIX generator / sync-check — so a
  capability can never drift one-sided. The C++ headless parity test is the enforceable half; the C# side
  commits to the SAME committed JSON.
- SYNC TOKEN: every S642 capability may only be introduced by updating OUR side AND the WPF-emitter side AND
  the matrix IN THE SAME STEP (never one-sided, never silent). parity=no must carry a named gap.
- COUNT-SCOPE (soft start): 10 per family is an INITIAL EXEMPLAR, NOT a hard limit — the catalogue + matrix
  + sync check GROW IN PLACE (no new step, no backfill) when testing permutations demand more. Documented in
  the matrix + roadmap + progress.
- METADATA FINE-TUNE (REVISIT S640b/S641, PROVISIONAL): the emitter sidecar now deposits the per-quad
  Capability (SYNC-TOKEN id) + Tolerance fields; the LIB reader + the catalogue-driven compare consume them.
  The shape is PROVISIONAL — locked only after S643's screens prove the quads carry the tolerance info the
  compare needs (S643 carries the REVISIT token).
- NO-GAP: ZERO NEW MINIMAL REAL introduced; the S640b tree-scanner marker RESOLVED at its own step (provenance
  record kept); ADR-018 ZERO new locks (all single-threaded pure reads); ADR-011 held (core data + scan; no
  classifier/render change). The S641 metadata reader is NOT stranded (forward-compat).

## CI-FIX (GH CI failed on both lanes for the last committed push 26c4377/S641)
- SYMPTOM: the GH CI matrix (windows-msvc + ubuntu-clang) FAILED at CONFIGURE on the last committed push
  (S641). A fresh deps clone + configure reproduced it locally: `FATAL_ERROR: stb_image_write.h not found`.
- ROOT CAUSE: the S641 commit's `cmake/FindImageDecoders.cmake` requires the vendored single header
  `third_party/deps/stb/stb_image_write.h`, but that file existed ONLY on this local box (gitignored —
  `third_party/deps/` is fully gitignored) and was NEVER pushed to the public `vaml-deps` remote that
  GH CI clones fresh. So CI's deps clone had only `stb_image.h` + `stb_truetype.h`, and S641's source/
  CMake references (PixelDiff.cpp STB_IMAGE_WRITE_IMPLEMENTATION; FindImageDecoders.cmake check) made
  configure fail on BOTH lanes. Local builds passed only because the file happened to exist locally.
- FIX (per the S160/S161 dep-migration discipline — deps live in the public `vaml-deps` remote, NOT tracked
  in-repo): vendored the genuine `stb_image_write.h` v1.16 (the same canonical stb header, verified: 46
  references to stbi_write_png / STB_IMAGE_WRITE_IMPLEMENTATION / the STBIWDEF API guard) into `vaml-deps`
  and PUSHED it (commit c4516c8, 1724 insertions; remote master 9fdf028..c4516c8). No in-repo CMake/source
  change was needed — the fix is purely on the deps remote.
- VERIFIED (from a fresh deps clone, the exact CI checkout path, BOTH lanes):
  * windows-msvc: fresh `git clone vaml-deps` into `third_party/deps`, then `cmake --preset msvc-debug`
    (CONFIG_OK) → `cmake --build --preset msvc-debug` (BUILD_OK) → `ctest --preset msvc-debug` (TEST_OK).
  * ubuntu-clang (VM /mnt/hgfs/VamlPOC, fresh deps clone): clang-debug configure+build+ctest → CI_UNIX_GREEN.
- No commit to the VamlPOC repo was made for this (the fix lives on the vaml-deps remote; standing directive —
  commitit outstanding, driver's seat is yours).

## GAP-CLOSURE RUN (S642 follow-up — the driver asked "is anything incomplete; the carry on")
The driver probed for carry-on (the SILENCE RULE guards against assuming green = complete). An audit against
the FINALIZED plan found THREE plan-named items that were incomplete; all CLOSED this session + re-verified
on BOTH CI lanes + the authoring gate:
1. GAP (S642-B): the in-XAML `SCAN DECLARATION` was NOT extended with the per-quad `capability <id>` token.
   FIXED: `ScanQuad.capability` added + the parser accepts the optional `capability ID` quad option (empty when
   absent = forward-compatible; a bare token with no id -> typed MalformedLine). Tests added in
   ScanDeclaration_s641_test.cpp (3 cases, all pass).
2. GAP (S642-C): `wpf_emitter/Program.cs` did not OWN a catalogue read (only ElementTreeScanner did).
   FIXED: `ElementTreeScanner.UsesCommittedCatalogue` exposes the located-flag; `Program.cs` now FAILS LOUDLY
   (return 2, never silent) if the COMMITTED tools/capability_catalogue.json was not located (a fallback-only
   run is rejected — the WPF side cannot silently drift from the LIB side + matrix). The authoring gate sets
   `VAML_REPO_ROOT` + the catalogue root-discovery was made robust (env var + assembly + cwd-relative).
3. GAP (sync-token fixture half): the plan's "the C# side commits to a committed sample fixture the C++ side
   reads" was not concretely shipped. FIXED: NEW `test/fixtures/s642_emitter_sidecar.json` (the committed
   emitter-sidecar sample) + NEW C++ test case in CapabilityCompare_s642_test.cpp that READS the fixture via
   the new `VAML_TEST_FIXTURES_DIR` compile definition and asserts every fixture capability id resolves to a
   catalogue row with a matching kind + tolerance (the enforceable half). All pass.
VERIFY after the closure: build/verify_s642.cmd -> S642 GREEN (both CI lanes + matrix/sync-check + authoring
gate); the focused doctest run of `*S642*` = 17 cases / 226 assertions / 0 failed.
- ZERO LOCKS (ADR-018): the catalogue parse + the compare are single-threaded pure reads over the embedded JSON.
- No commit made (standing directive — commitit outstanding; the driver's seat is yours; the S642 change set
  is uncommitted).
