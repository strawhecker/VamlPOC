# Session Context — Sprint 11d-DEBUG (viewer scan/aggregate-errors)

last_command: implementit (COMPLETE) — viewer scan/aggregate-errors 11d-DEBUG DONE
plan_digest: 11d-DEBUG — the viewer parses ONE document in COLLECT mode (reader lenient,
  opt-in) and reports ALL of its reader-OWNED unknown-property errors in ONE pass
  (the "find all errors" goal — fixes seeing only `unknown property 'AllowsTransparency'`
  then whack-a-mole). `--fail_on_first_error` (DEFAULT off) restores the strict
  fail-fast single-first-error behavior. An errored document is NEVER rendered
  (root=null, exit 3, no window) — the "stop writing on first error" safety
  invariant (viewer's write = render surface; never publish a partial doc).
  BOTH CI LANES green.
files_touched: src/core/include/vaml/core/VamlReader.h + src/core/src/VamlReader.cpp
  (ScanIssue struct + set_collect_issues(bool)/issues()/report_unknown_property —
  strict default unchanged, collect opt-in, per-Load cleared issues_;
  build() attribute + property-element unknown-property genre routes through
  report_unknown_property: strict=throw, collect=record+continue),
  vaml_viewer/load_file.h/.cpp (LoadResult.issues + load_scan_text/load_scan_file —
  collect mode entry, aggregated ParseError, root=null on any issue),
  vaml_viewer/cli.h (cli_wants_fail_first + --fail_on_first_error treated as a
  FLAG not a positional in cli_action/cli_file_path + usage),
  vaml_viewer/main.cpp (text path: default load_scan_file, fail-first->load_file;
  prints the FULL aggregated list on ParseError; exit 3),
  test/unit/VamlReader_test.cpp (collect-mode reader tests),
  test/unit/ViewerLoadFile_test.cpp (load_scan_text/scan_file tests),
  test/unit/ViewerCli_test.cpp (cli_wants_fail_first + flag-not-positional + usage),
  test/unit/LiveReload_test.cpp (LoadResult 5-field aggregate fix)
build_status: 11d-DEBUG DONE — BOTH CI LANES green (windows-msvc msvc-debug warning-as-error
  CLEAN + ctest 4/4 100%; ubuntu-clang clang-debug ctest 4/4 100%). No warnings.
next_step: Sprint 12 — S490+ (forward carry: MenuItem element S491/11e; ContentControl/
  control-material abstraction; register Window's real DPs — AllowsTransparency,
  WindowStartupLocation, WindowStyle, Topmost, ResizeMode — so those resolve instead
  of surfacing as unknown; scanner report to show every attribute incl. Window;
  ProgressBar indeterminate/segmented chrome; Ellipse curved outline + Image real
  atlas quad + expand chevron + Border/GroupBox content-child authoring polish -> S649)
notes: 11d-DEBUG complete (Option A only — ONE document, all its errors in one pass;
  NO multi-file directory walk per explicit scope). Scope boundary recorded in
  ScanIssue's comment: collect mode covers the reader-OWNED attribute + property-
  element unknown-property genre only; the DEEPER authored-resource genres (Style
  Setter/Trigger, Brush, ResourceDictionary capture, GradientStop) + lower-layer
  failures (unknown element, malformed value) stay FAIL-FAST in both modes
  (deliberate, recorded, not silent). New public symbols: ScanIssue,
  set_collect_issues, issues(), report_unknown_property, load_scan_text,
  load_scan_file, cli_wants_fail_first. ZERO MINIMAL REAL introduced. No commits
  (standing directive — commitit outstanding, driver's seat is yours).
