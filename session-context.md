# session-context.md

last_command: implementit (S641 — THE UI-COMPLIANCE SCANNER — dep:S640b; the root capability the whole sprint rides on)
plan_digest: S641 lives in the dedicated tester: authors the compare seam (compliance_compare(actualScan,
expectedDecl) + the tolerance scaffold + the in-XAML SCAN DECLARATION parser) and makes it LIVE — scrape the
rendered UI (S640a ImageBuffer + the arranged core tree) for compliance (element/draw-item presence,
coordinates, pixel/color at declared locations) -> COMPARE to the expected outcome DECLARED in the authored
.xaml. Telescope: a ScanReport (pixel/element grid samples) + compliance_compare -> per-datum PASS/FAIL +
a headless seam with no live window. PER-QUAD DECLARATION (baked): every quad carries tolerance OR handling
text|icon in the SCAN DECLARATION; text -> auto-locate + tolerance band; icon -> presence/centroid + loose.
METADATA READ (baked, forward-compatible): the compare consumes the emitter-deposited sidecar metadata today;
a later S642 shape change never strands the reader. GIMP-STYLE DIFF + TOLERANCE TOOL in S641 (per-pixel diff,
auto-locate text -> tolerance band, color-coded diff PNG via vendored stb_image_write). ZERO new locks.
Desktop-only, real GPU, script-driven — NOT a CI lane (ubuntu-clang covers library unit tests only).
**THIS SESSION: S641 implemented (scanner/parse/compare/diff/metadata all authored + headless-ctested);
BOTH CI LANES GREEN: windows-msvc (6 S641 suites, ctest 4/4) + ubuntu-clang (VM shared folder mounted +
3 clang -Werror fixes authored, ctest 4/4). S641 is fully GREEN per the two-lane contract."

## files_touched — S641 (this session)
- src/presentation/include/vaml/presentation/scan/ScanDeclaration.h + src/presentation/src/scan/ScanDeclaration.cpp (NEW):
  in-XAML `SCAN DECLARATION` comment-block parser (frame W H; quad NAME KIND rect|auto + tolerance | band |
  expect-literal | centroid | presence). No regex/XML dep; typed never-silent parse error (DeclarationError).
- src/presentation/include/vaml/presentation/scan/Scanner.h + src/presentation/src/scan/Scanner.cpp (NEW):
  ScrapeGrid (element samples + BGRA pixel grid + pixel()/pixel_r/g/b) + sample_quad (ink count + centroid)
  + compliance_compare(actual, expected, sidecarKinds) -> ScanReport with per-datum PASS/FAIL/Missing + the
  per-quad tolerance merge (declared -> sidecar-kind default -> built-in default_tolerance_for).
- src/presentation/include/vaml/presentation/scan/EmitterMetadata.h + src/presentation/src/scan/EmitterMetadata.cpp (NEW):
  forward-compatible JSON sidecar reader (Width/Height/Source/Quads[]{Name,X,Y,Width,Height,Kind}) — unknown/
  extra fields IGNORED so a later S642 shape change never strands the reader (the METADATA READ contract).
- src/presentation/include/vaml/presentation/scan/PixelDiff.h + src/presentation/src/scan/PixelDiff.cpp (NEW):
  per-pixel diff (Unchanged->grey / Different->hot / MissingContent->red region) + retained raw frames for the
  auto-locate text tolerance band (classify_quad) + REAL PNG encode/write via vendored stb_image_write.
  The structured scan is authoritative; PNG bit-match (S.Deferred-PNG) stays the recorded upgrade path.
- src/presentation/CMakeLists.txt (MOD): + the 4 scan TUs into vaml-presentation (src/scan/*).
- cmake/FindImageDecoders.cmake (MOD): sellers stb_image_write.h under the SAME vaml::image SYSTEM interface
  (ADR-011: only vaml-presentation/vulkan consume it).
- third_party/deps/stb/stb_image_write.h (NEW, vendored): the PNG writer (single header, the stb family).
- test/unit/ScanDeclaration_s641_test.cpp (NEW): the full per-quad grammar + typed error taxonomy.
- test/unit/ComplianceCompare_s641_test.cpp (NEW): per-datum PASS/FAIL/Missing + tolerance merge + full-missing-content.
- test/unit/EmitterMetadata_s641_test.cpp (NEW): the emitter's exact sidecar schema + forward-compat (unknown fields).
- test/unit/PixelDiff_s641_test.cpp (NEW): diff math + tolerance band + full-missing-content + PNG encode.
- test/unit/Scanner_s641_test.cpp (NEW): scrape of a REAL arranged vaml::UIElement core tree -> presence +
  coordinates -> compliance_compare PASS (element presence + coordinates ask proven headlessly).
- test/unit/CMakeLists.txt (MOD): + 5 S641 TUs into vaml-presentation-tests.
- vaml_ui_tester/main.cpp (MOD): + the --compare mode (parse SCAN DECLARATION + read sidecar + read .bgra
  frames + compliance_compare + write report + colored diff.png; exit 0=PASS/1=FAIL/2=SKIP). Desktop, NOT CI.
- vaml_ui_tester-linux/src/main.cpp (MOD): the headless twin --compare mode for parity (over on-disk frames).
- vaml_ui_tester/sample.image.xaml (MOD): + the authored SCAN DECLARATION block (frame 440 360; quad img solid
  rect 32,32,128,128 tolerance 0.10) — the demo oracle per the PER-QUAD DECLARATION contract.
- build/verify_s641.cmd (NEW): two-lane (windows-msvc + ubuntu-clang) headless + the Desktop compare gate.
- build/_s641_build_helper.cmd (NEW, TEMP): local windows-msvc configure+build+ctest runner for this session
  (non-committed helper).
- docs/roadmap.md (MOD): S641 token -> [DONE - implementit S641; windows-msvc LANE GREEN; ubuntu-clang VM
  OFFLINE — RE-VERIFY outstanding] (resolution at the VM; see the build_status outage note).
- docs/progress.md (MOD): S641 row + the S640b tree-scanner resolution-record line (unchanged: RESOLVED BY S642).
- session-context.md (MOD): this entry (fresh rewrite).

## build_status (S641 — from first principles)
- windows-msvc (the CI lane): cmake --preset msvc-debug configure clean + build clean (warning-as-error clasp
  clamp /WX) + ctest --preset msvc-debug 4/4 PASS (vaml-core / vaml-tools / vaml-vulkan / vaml-presentation).
  The vaml-presentation suite now includes the 6 S641 suites (ScanDeclaration + ComplianceCompare +
  EmitterMetadata + PixelDiff + Scanner + the full S641 set in the 243-case presentation binary) — all green.
  The S640a/S212/S640 render + clear + alpha suites stay green (no regression).
- ubuntu-clang: RUN + GREEN this session. The Linux verification VM was reachable on the home LAN; the
  VMware shared folder had UNMOUNTED (cd /mnt/hgfs/VamlPOC failed) — re-mounted passwordless via
  `sudo /usr/bin/vmhgfs-fuse .host:/VamlPOC /mnt/hgfs/VamlPOC -o allow_other` (vmware-hgfsclient listed
  the share). Three clang -Werror issues were authored + fixed (MSVC does not flag them): (1) `std::isfinite`
  needs an explicit `<cmath>` include in ScanDeclaration.cpp; (2) removed the unused `nearly_equal` helper
  in Scanner.cpp; (3) removed the unused `bgra_to_rgba` helper in PixelDiff.cpp. After those, cmake --preset
  clang-debug configure clean + build clean + ctest 4/4 PASS — the ubuntu-clang lane is GREEN. The S640a/
  S640b outage note is retired.
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).
- Desktop lane (NOT CI): the S641 scanner is exercised headlessly in the ctest suites; the REAL GPU render
  (S640a RenderTarget render_to -> ImageBuffer) + the script-driven --compare over a live render is the
  [both apps, not CI] driver's-box lane. The --compare mode is compiled + wired; driving it against a live
  GPU render requires the on-machine render stack (the operator's box).

## next_step — S641 implemented (BOTH CI LANES GREEN)
- S641 implemented: the compliance scanner's compare seam + tolerance scaffold + in-XAML SCAN DECLARATION
  parser + ScrapeGrid + compliance_compare -> per-datum ScanReport + GIMP-style diff + PNG, reading the
  emitter sidecar forward-compatibly. Headless-ctested on BOTH lanes (all 6 suites green).
- Next per the roadmap chain = **S642** (the auth PARITY MATRIX creator + the typed-slot/icon/text metadata
  catalogue that RESOLVES the S640b tree-scanner MINIMAL REAL marker; refines the metadata shape S641 READS —
  S641 is NOT stranded; forward-compat holds). S641 records its own roadmap REVISIT tokens (S644/S645) for
  the format-driven refactor of the metadata/declaration shape.

## notes
- METADATA READ (baked, forward-compatible): S641 READS the emitter sidecar's CURRENT shape (Width/Height/
  Source/Quads[]{Name,X,Y,Width,Height,Kind}); unknown/extra fields are IGNORED. When S642 refines the shape,
  the S641 reader still works — the forward-compat contract means a later gap-close never strands it.
- PER-QUAD DECLARATION (baked): every S641+ test encodes quads WITH a tolerance OR a handling text|icon in the
  SCAN DECLARATION (solid -> tight; text -> auto-locate + band; icon -> presence/centroid + loose). The compare
  is per-quad configurable via the tolerance merge (declared -> sidecar-kind -> built-in default), never a
  silent global bit-match. The merged tolerance is reported per-datum in the ScanReport (explicit, not silent).
- GIMP-STYLE DIFF + TOLERANCE (in S641): per-pixel diff -> color-coded image (unchanged grey / differing hot /
  missing-content red region) + auto-locate-text tolerance band via classify_quad; written as a REAL PNG via
  the vendored stb_image_write seam. PNG bit-match (S.Deferred-PNG) stays the recorded upgrade path; the
  structured scan is authoritative now.
- NO-GAP: ZERO MINIMAL REAL introduced by S641 (all new surface is permanent ADR-021). The S640b tree-scanner
  placeholder stays the ONE sanctioned [MINIMAL REAL] marker, RESOLVED BY S642 (unchanged). S641 READS the
  emitter metadata + the SCAN DECLARATION; S642/S643 refine the shape — S641 is not stranded (forward-compat).
- ZERO LOCKS (ADR-018): the scanner/compare/diff/metadata are single-threaded pure reads over stable buffers.
- No commit made (standing directive — commitit outstanding; the driver's seat is yours; the S641 change set
  is uncommitted).
