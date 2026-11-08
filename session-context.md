# session-context.md

last_command: implementit (S640b — WPF Reference Emitter — Windows-authoring-only; PRECEDES S641)
plan_digest: A small C#/WPF app renders an authored .xaml OFFSCREEN via RenderTargetBitmap, blits to the
SAME ImageBuffer BGRA shape (the convergence contract), and DEPOSITS the diffable reference material on disk
(w*h*4 .bgra + .json sidecar with per-quad text/icon/solid metadata) for S641's compliance_compare. C#/WPF
ONLY — no compare, no SCAN DECLARATION parsing, no capability matrix here (those are S641/S642). The emitter
is a standalone dotnet project EXCLUDED from the CMake/slnx build and BOTH CI lanes (Ubuntu does not author
WPF). Windows-authoring-only gate = build/verify_s640b.cmd (NOT GH CI). Also includes the TREE-SCANNER
PLACEHOLDER (MINIMAL REAL, RESOLVED BY S642) that harvests text/icon quads into the sidecar; and the roadmap
token changes (S640b scope-cut, S641 becomes the seam author + METADATA READ, S642 auth PARITY MATRIX +
REVISIT, S643 REVISIT).
**THIS SESSION: S640b DONE (implementit; the standalone WPF emitter builds + renders + deposits on Windows;
WINDOWS-AUTHORING GATE GREEN; windows-msvc LANE GREEN — 4/4 ctest pass — no new C++ surface).**

## files_touched — S640b (this session)
- wpf_emitter/WpfEmitter.csproj (NEW): net8.0-windows, UseWPF, OutputType Exe, RootNamespace Vaml.WpfEmitter.
  Excludes samples\**\*.xaml from WPF Page compilation (they are LOOSE docs loaded by XamlReader);
  samples\**\* shipped as Content. Standalone — excluded from CMake/slnx + both CI lanes.
- wpf_emitter/Program.cs (NEW): CLI WpfEmitter.exe <file.xaml> <out.bgra> [out.json] — XamlReader.Load →
  true-WPF tree → Measure/Arrange/UpdateLayout → RenderTargetBitmap (Pbgra32 = BGRA) → blit into the C#
  ImageBuffer → write tight w*h*4 .bgra + .json sidecar (JsonStringEnumConverter so QuadKind writes as
  "Icon"/"Text"/"Solid"). Tree-scanner harvest runs AFTER layout (so quads carry real frame-space positions).
- wpf_emitter/ImageBuffer.cs (NEW): the C# twin of vaml::presentation::Impl::ImageBuffer { width, height,
  vector<uint8_t> bgra } — the S640b CONVERGENCE CONTRACT (both sides deposit byte-identical BGRA). Plus
  QuadKind (Solid/Text/Icon), QuadMetadata, EmitterMetadata sidecar types.
- wpf_emitter/ElementTreeScanner.cs (NEW): the [MINIMAL REAL] WPF metadata tree-scanner — RESOLVED BY S642.
  Walks the emitter's OWN rendered element tree, harvests text-bearing (TextBlock) and icon-bearing
  (Image/Shape) nodes into per-quad metadata with a declared kind. The ONE sanctioned [MINIMAL REAL] marker.
- wpf_emitter/samples/WpfEmitterSample.xaml (NEW): a StackPanel element tree with an Image (icon quad, 64x64
  PNG that WPF natively rasterizes) + a TextBlock (text quad, "hello vaml"). Plain XAML only (no SCAN
  DECLARATION comment — S641 adds metadata consumption). Has both xmlns + xmlns:x (WPF presentation ns is
  REQUIRED for loose-XamlReader load).
- wpf_emitter/samples/assets/sample_icon.png (NEW): a small generated PNG (blue + white ellipse) so the icon
  quad rasterizes in WPF (WPF ships NO built-in SVG rasterizer — the sample initially used
  face-blowing-a-kiss-64.svg which rendered 0x0; switched to PNG).
- wpf_emitter/README.md (NEW): build/run recipe, CLI, convergence contract, scanner-metadata explanation,
  samples, not-in-CI note.
- build/verify_s640b.cmd (NEW): the WINDOWS-AUTHORING gate — file presence + MINIMAL REAL grep-audit (asserts
  the ONE sanctioned RESOLVED-BY-S642 marker + zero unsanctioned) + dotnet build + run + deposit assert.
  Adds C:\Program Files\dotnet to PATH (other verify scripts patch cmake/ninja the same way).
- build/assert_s640b_deposit.ps1 (NEW): parses the emitted .json and asserts dims + >=1 Text quad + >=1 Icon
  quad with non-zero size.
- .gitignore (MOD): + **/bin/ and **/obj/ (wpf_emitter build output).
- docs/roadmap.md (MOD): S640b token marked [DONE - implementit S640b; WINDOWS-AUTHORING GATE GREEN;
  windows-msvc LANE GREEN] (ubuntu-clang lane NOT run — see the build_status outage note). The S640b scope-cut
  + SCANNER METADATA + TREE-SCANNER + S641 METADATA READ + S642/S643 REVISIT tokens were already applied in
  the plan phase (prior session).
- session-context.md (MOD): this entry (fresh rewrite).

## build_status (S640b — from first principles)
- WINDOWS-AUTHORING GATE (build/verify_s640b.cmd): GREEN — dotnet build clean; WpfEmitter.exe ran against
  samples/WpfEmitterSample.xaml; deposited 440x240 / 422400 bytes / 105600 pxX4 .bgra + .json (2 quads: Text
  109x32 + Icon 64x64); assert_s640b_deposit.ps1 PASSED (dims + Text quad + non-zero Icon quad).
- windows-msvc (the S640a CI lane re-driven to confirm no regression): cmake --preset msvc-debug configure
  clean + build clean ("ninja: no work to do" — the emitter adds ZERO new CMake surface) + ctest --preset
  msvc-debug 4/4 PASS (vaml-core / vaml-tools / vaml-vulkan / vaml-presentation). The emitter is excluded from
  the CMake build so it cannot regress this lane.
- ubuntu-clang: NOT RUN this session — the Linux verification VM (robert@192.168.198.129, home-LAN VMware NAT
  host) is OFFLINE/UNREACHABLE (ssh connect timed out — same standalone environment outage as the S640a
  session). The clang-debug lane could not be executed from this network. The emitter is a standalone dotnet
  project excluded from the CMake build, so it CANNOT affect the clang-debug lane in any way. See notes.
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).

## next_step — S640b complete (Windows-authoring gate green; ubuntu-clang RE-VERIFY outstanding)
- S640b implemented: the standalone C#/WPF WPF Reference Emitter deposits the ImageBuffer-shaped raw BGRA
  frame (+ .json sidecar with per-quad text/icon/solid metadata) from an authored .xaml — the true-WPF oracle
  for S641's compliance_compare. The tree-scanner placeholder harvests text/icon quads (RESOLVED BY S642).
- RE-VERIFY on the ubuntu-clang lane when the VM is on the home LAN (cd /mnt/hgfs/VamlPOC && cmake --preset
  clang-debug && cmake --build --preset clang-debug && ctest --preset clang-debug). The emitter cannot affect
  that lane (it is not in the CMake build), so this is a re-affirmation of the standing repo gate.
- Next per the roadmap chain = **S641 (THE UI-COMPLIANCE SCANNER** — dep:S640b; authors the compare seam
  (compliance_compare + tolerance scaffold + in-XAML SCAN DECLARATION parser) and makes it LIVE over the S640a
  ImageBuffer; consumes the emitter-deposited metadata via the forward-compatible METADATA READ).

## notes
- CONVERGENCE CONTRACT: C# ImageBuffer { Width, Height, byte[] Bgra } = C++ Impl::ImageBuffer { width, height,
  vector<uint8_t> bgra } — tight w*h*4, BGRA, row 0 = TOP. BOTH sides must deposit byte-identical BGRA so the
  S641 compare sees identical frames. Any drift = a NAMED PARITY-MATRIX divergence, never silent.
- RenderTargetBitmap uses PixelFormats.Pbgra32, which IS BGRA byte order (premultiplied BGRA) — matches the
  raw-frame contract row-0-top.
- MINIMAL REAL: the tree-scanner placeholder is the ONE sanctioned [MINIMAL REAL] marker, tagged
  // [MINIMAL REAL] WPF metadata tree-scanner — RESOLVED BY S642. It is a deliberate forward-resolvable seed
  (the S642 typed-slot/icon/text catalogue grows it into the real implementation); S641 READS it; S642/S643
  fine-tune via the roadmap REVISIT tokens. NO-GAP holds (the resolution is explicitly scheduled, authored at
  the SAME step that flagged it).
- The emitter is REAL WPF (XamlReader → genuine WPF tree → RenderTargetBitmap), so there's no "WPF behavior
  asserted from memory" concern for depositing; behavior claims land in the (future) PARITY MATRIX, not here.
- WPF could not rasterize the repo's SVG icon (WPF has no built-in SVG rasterizer) — the authored sample uses
  a small PNG so the icon quad renders. Recorded as a real WPF-source fact, not a silent workaround.
- ZERO invented capability names; ZERO new locks (single-threaded emitter). The emitter is single-threaded
  STA (Main marked [STAThread]); no threading/channel changes.
- No commit made (standing directive — commitit outstanding; the driver's seat is yours; the S640b change set
  is uncommitted).
