# session-context.md

last_command: implementit (S643 — TOOL SMOKE TEST — dep:S642; ONE .xaml using ONE of each capability type +
two no-text geometry sentinels, proving the run -> scan -> compare tooling end-to-end + the REVISIT
S640b/S641 metadata-fine-tune closure)
plan_digest: S643 proves the run -> scan -> compare tooling end-to-end over ONE authored `.xaml`
(vaml_ui_tester/sample.smoke.xaml) using ONE of each capability type (1 int / 1 bool / 1 string / 1 icon
/ 1 text) PLUS two no-text geometry sentinels (a Rectangle box01 + a Border border01) as the ONLY EXACT
fails-loud compare path — so a parity failure at S644+ is ATTRIBUTABLE (capability broken vs tooling
broken — separated). OWNER DECISION: bound int/bool/string slots render as TEXT GLYPHS (fuzzy-pool), so the
smoke adds the two no-text geometry sentinels; the fuzzy text/icon quads compare on a DECLARED presence
floor with 0.08 as the SMOKE-ONLY soft start (NEVER a catalogue default/limit — S644/S656/S657 must tighten
it, NO-GAP). The in-XAML SCAN DECLARATION is the PRIMARY oracle; declared tolerance/kind always win the
merge (the locked S642 order). **THIS SESSION: S643 fully implemented; BOTH CI LANES + the Windows-authoring
gate GREEN (build/verify_s643.cmd ran to "S643 GREEN") + the REVISIT S640b/S641 metadata-fine-tune CLOSED
(the WPF scanner is now declaration-driven) + the driver-directed catalogue-kind amendment
(int/bool/double Solid->Text) applied + roadmap/progress/session-context updated.**

## files_touched — S643 (this session, cumulative incl. the planning amendment)
- vaml_ui_tester/sample.smoke.xaml (NEW): the S643 smoke — 7 quads (int01/bool01/str01/text01/icon01 FUZZY
  floor + box01 Rectangle + border01 Border EXACT sentinels) with the in-XAML SCAN DECLARATION (the primary
  oracle; declared kind + capability + tolerance per quad; 0.08 declared as the smoke-only text start). NOTE:
  the XAML comments avoid `--` (XML comments may not contain two consecutive dashes — the WPF XamlReader
  rejects it; validated by parsing the .xaml as XML).
- test/unit/ToolSmoke_s643_test.cpp (NEW): the headless both-lane half of the run->scan->compare proof —
  READS the committed `vaml_ui_tester/sample.smoke.xaml` (via VAML_UI_TESTER_DIR), parses its SCAN
  DECLARATION, drives the compare over a synthetic grid mirroring BOTH regimes, asserts the loop PASSES
  fully-rendered AND FAILS LOUDLY on a blank geometry/text quad + the REVISIT declared-capability round-trip
  + the DRIVER input-hash forward-compat case (the C++ sidecar reader tolerates the new `InputHash` field).
- tools/compare_hash.py (NEW): the S643 DRIVER's source-hash authority — `hash` (write the principle-files
  SHA-256 to stdout) + `stale` (fresh/stale vs the metadata's stamped hash; exit 0/1/2). Principle set (read
  in + appended, deterministic order): the test `.xaml` + tools/capability_catalogue.json + wpf_emitter/*.cs +
  src/presentation/src/scan/*.{cpp,h} + the scan public headers.
- wpf_emitter/HashInput.cs (NEW): the C# twin of the source hash (SHA-256 over the same principle files);
  cross-implementation-consistent with compare_hash.py (verified identical 64-hex digest).
- wpf_emitter/ImageBuffer.cs (MOD): EmitterMetadata gains the `InputHash` field.
- wpf_emitter/Program.cs (MOD): passes the XAML text to ElementTreeScanner.Harvest (so it can read the SCAN
  DECLARATION) + stamps metadata.InputHash via HashInput.Compute.
- build/run_compare.cmd (NEW): the S643 TWO-APP DRIVER (approved) — loops build/compare_tests.manifest; A
  (the WPF composer) runs ONLY on missing/stale (via compare_hash.py), B (vaml_ui_tester --compare) runs
  ALWAYS + ACCUMULATES every failure (a silent fail is banned); one frame at a time; `--rebuild` forces A.
- build/compare_tests.manifest (NEW): the ordered test-name list (one file per test; sample.smoke).
- vaml_ui_tester/vaml_ui_tester.vcxproj (MOD): sample.smoke.xaml None item + `_CRT_SECURE_NO_WARNINGS`
  (the msbuild-as-B build needed it under TreatWarningAsError).
- build/verify_s643.cmd (MOD): the desktop lane now builds the tester (B) via msbuild when absent + runs the
  DRIVER (`call build\run_compare.cmd --frames ... --report ...`) as an ENFORCED desktop two-app lane (not a
  skip); gate exits `S643 GREEN (both CI lanes + authoring gate + desktop two-app driver)`.
- tools/capability_catalogue.json (MOD): typed-slot kinds Solid -> Text for slot_int / slot_bool / slot_double
  (family entries + all 30 capability rows); tolerance rows UNCHANGED (0.02). 33 replacements. Symmetric with
  the embedded bytes.
- src/presentation/src/scan/CapabilityCatalogue.cpp (MOD): the embedded bytes mirror the JSON (same 33
  Solid -> Text edits) — the compile-time single source of truth stays byte-identical to the JSON.
- docs/PARITY_MATRIX.md (REGEN): regenerated via tools/parity_matrix_gen.py (60 rows; slot_int/bool/double
  now kind Text, tolerance 0.02; SYNC-CHECK OK).
- test/unit/CapabilityCatalogue_s642_test.cpp (MOD): the slot_int_01 + slot_double_05 kind assertions updated
  to QuadKind::Text (tolerance still 0.02) — matching the corrected semantics + comments.
- wpf_emitter/ElementTreeScanner.cs (MOD): the REVISIT S640b/S641 CLOSURE — the placeholder scanner was NOT
  declaration-synced (Rectangle->Icon, Border dropped, capability ids collapsed); now DECLARATION-DRIVEN: it
  parses the SAME in-XAML SCAN DECLARATION as the C++ side (the C# mirror of the S641 grammar) and emits ONE
  sidecar quad PER DECLARED quad (Name/Kind/Capability/Tolerance from the DECLARATION; X/Y/W/H from the REAL
  layout position of the named WPF element). ALSO added: FindByName (visual-tree walk) + ParseScanDeclaration
  (the C# declaration mirror). QuadKind::Border NOT needed (a Border falls under the tight `solid` diff kind).
- wpf_emitter/Program.cs (MOD): passes the XAML text to ElementTreeScanner.Harvest (so it can read the
  SCAN DECLARATION).
- docs/roadmap.md (MOD): S643 entry -> [DONE] + the REVISIT-CLOSED record; S642 entry -> the CATALOGUE-KIND
  AMENDMENT record (from planning); PARITY_MATRIX regenerated.
- docs/progress.md (MOD): NEW S643 — SURVIVOR block + NEW ACTIVE-CURRENT checkpoint (11r-S643) at the top
  (the whole doc's checkpoint history preserved, not truncated).

## build_status (S643 — from first principles)
- windows-msvc: build/verify_s643.cmd -> GREEN. cmake --preset msvc-debug configure clean + build
  warning-as-error clean + ctest 4/4 PASS (the vaml-presentation suite 265/265 cases / 1606/1606 assertions,
  incl. the S643 ToolSmoke 6 cases / 80 assertions + the re-verified S642 17 cases / 226 assertions).
- ubuntu-clang (VM /mnt/hgfs/VamlPOC, clang 21): build/verify_s643.cmd -> UBUNTU-GREEN; clang-debug
  configure+build+ctest 4/4. The S643 doctest filter confirmed directly on the VM: 6 cases / 80 assertions /
  0 failed.
- PARITY matrix gen + SYNC-TOKEN check: OK (60 capabilities in sync — our + WPF + matrix agree).
- Windows-authoring gate (local, NOT CI): GREEN — the WPF emitter built 0 warnings/0 errors, deposited
  build/_s643_out.bgra (440x340) + build/_s643_out.json (with the InputHash stamped), and the tightened
  assertion passed: `S643-SIDECAR-OK quads=7 catalogued=5 solidsentinels=2`.
- **Desktop two-app DRIVER (local, NOT CI — ENFORCED, not skipped):** BOTH green. (a) The driver's own clean
  run (`build/run_compare.cmd`) exits 0 (`ALL tests PASSED`; A skipped on a fresh hash), `--rebuild` forces A
  (re-render then B PASSED), and the accumulate path catches every failure across the manifest in one batch
  (a missing-test entry is reported while the other tests still run B). (b) The full gate's desktop lane
  builds the tester (B, `vaml_ui_tester.exe x64\Debug`) via msbuild when absent, then runs the driver, and
  `build/verify_s643.cmd` exits `S643 GREEN (both CI lanes + authoring gate + desktop two-app driver)`. The
  smoke's real compare against the WPF frame passes **14/14** (data=14 pass=14) through the tester (B), after
  the smoke declaration rects + the icon (Image->Ellipse, no external asset) were corrected to real layout.
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).

## next_step — S643 implemented (BOTH CI LANES + the authoring gate + the desktop two-app DRIVER GREEN)
- S643 implemented: the TOOL SMOKE TEST (ONE .xaml, both compare regimes, fails-loud sentinels) proving the
  run -> scan -> compare tooling end-to-end via the APPROVED TWO-APP DRIVER + the REVISIT S640b/S641
  metadata-fine-tune CLOSED (the WPF scanner is declaration-driven) + the driver-directed catalogue-kind
  amendment (int/bool/double Solid->Text).
- Next per the roadmap chain = **S644 (TextBlock — real glyphs via the S400/S409_1 SDF path; the scanner
  confirms authored text is READABLE at the text rect, not a nominal full-atlas quad; the FIRST step that
  tightens the fuzzy-compare quality the S643 smoke left soft — the NO-GAP owner)**, with the
  [11r EXHAUSTIVE: S644 TextBlock x6 xaml instances] token. Each new surface's `.xaml` joins
  build/compare_tests.manifest so B (the vaml compare) runs it — the S643 driver is the standing harness.

## notes
- PRIMARY ORACLE: the in-XAML SCAN DECLARATION is authoritative for kind/capability/tolerance; the
  emitter-sidecar is the SECONDARY oracle (now declaration-derived, so it round-trips by construction). The
  declared tolerance ALWAYS wins the merge (declared > sidecar catalogue > built-in kind default).
- CATALOGUE-KIND SEMANTICS: a bound int/bool/double slot renders as a TEXT GLYPH (a TextBlock showing
  "42"/"True"), NOT a filled solid box — so its DEFAULT compare kind is Text (fuzzy), not Solid. The
  per-capability tolerance stays 0.02 (the tight text floor); 0.08 is the SMOKE-ONLY soft start, NEVER a
  catalogue default or limit (S644/S656/S657 must tighten it).
- NO-GAP: the fuzzy-pool soft start is OWNED by a named forward obligation recorded in roadmap + progress
  (S644 TextBlock / S656 Icon / S657 Label MUST tighten the fuzzy-compare quality); the geometry sentinels
  (box01/border01) prove the scan->compare loop can FAIL LOUDLY. ZERO MINIMAL REAL (grep-audit in
  verify_s643.cmd); ADR-018 ZERO new locks; ZERO invented names (Rectangle/Border are real WPF types).
- SOURCE OF TRUTH: the catalogue (tools/capability_catalogue.json) is read by the LIB side (embedded at
  compile time), the WPF-emitter C# side (runtime load), and the PARITY MATRIX generator/sync-check — a
  capability can never drift one-sided. SYNC-TOKEN audience applies (a capability only introduced by updating
  BOTH sides + the matrix IN THE SAME STEP).
- S643 DRIVER (approved two-app): build/run_compare.cmd loops build/compare_tests.manifest (ordered names,
  one test = one .xaml = one frame+metadata pair). A (WPF composer) runs only on missing/stale (the
  input-hash in the metadata — compare_hash.py compares it); B (vaml_ui_tester --compare) runs ALWAYS +
  ACCUMULATES failures into a marker file (a silent fail is banned); ONE frame at a time (portable; never N
  tests composited into one large frame). `--rebuild` + deleting an artifact are manual overrides, never the
  correctness mechanism. Enforced in verify_s643.cmd's desktop lane (builds the tester via msbuild when
  absent). Desktop acceptance = this driver green + headless CI lanes green.
- INPUT-HASH PRINCIPLE: SHA-256 over the SOURCE files (test .xaml + capability_catalogue.json +
  wpf_emitter/*.cs + src/presentation/src/scan/*.{cpp,h} + scan public headers). NOT the rebuilt exe (WPF
  rebuilds churn binary bytes), NOT a version constant (forgotten bump = silent stale frame). Cross-impl
  consistent (HashInput.cs == compare_hash.py). A source change (xaml/emitter/scanner) IS the invalidation —
  that's when a re-render fires + B re-runs (the regression radar as the vaml lib/viewer changes).
- XML comment rule (recorded): an XML comment may not contain `--`. The smoke .xaml comments use single
  hyphens/em-dashes only; validated by XML-parsing the file.
- No commit made (standing directive — commitit outstanding, driver's seat is yours; the S643 + the S642
  change set are uncommitted).
