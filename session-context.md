# Session Context — Sprint 11k-4 (BRUSHES + VECTOR-DRAWING RESOURCES)

last_command: implementit (IN PROGRESS) — 11k-4 implemented; windows-msvc GREEN;
  ubuntu-clang lane BLOCKED on infrastructure (VM unreachable at gate time)
plan_digest: 11k-4 (dep:11k-3) — the corpus's LARGEST omitted volume (Icons/*.xaml
  + Icons.xaml/NewIcons.xaml): ImageBrush (TagConfigurationUserControl) +
  DrawingBrush/DrawingGroup/DrawingImage + Geometry RESOURCES authored + resolved
  through the S360/S453 resource path + rasterized via the S202 atlas mesh (the
  vulkan vector seam). WPF-fidelity: Geometry/Drawing are ABSTRACT bases (internal
  ctor, NOT authorable); the concrete DO leaves (PathGeometry/GeometryGroup/
  RectangleGeometry/EllipseGeometry; GeometryDrawing/DrawingGroup) ARE. Resolves
  the 11k-3 `<Geometry>` OUT-OF-SCOPE(11k-4) at THIS step (NO-GAP, not silent).
  Test 11k4_vectorResources_test.cpp.

IMPLEMENTATION (delivered):
  core:
  - NEW src/core/include/vaml/core/VectorDrawing.h — the 11k-4 model: abstract
    Geometry (Transform DP, bounds) + abstract Drawing (bounds/representative_
    color) + DO leaves PathGeometry(Figures/FillRule + parse_path_geometry),
    GeometryGroup, RectangleGeometry(Rect), EllipseGeometry(RadiusX/Y),
    GeometryDrawing(Brush/Pen/PenThickness/Geometry), DrawingGroup(children/
    Transform), DrawingBrush(Drawing), DrawingImage(Drawing), ImageBrush
    (ImageSource). PathCommand/PathStep/PathFigure/PathData + the WPF MiniLanguage
    parser (M/L/H/V/C/S/Q/T/A/Z + repeats).
  - NEW src/core/src/VectorDrawing.cpp — the parser + bounds + DP registrations
    (Transform/FillRule/Brush/Pen/PenThickness/ImageSource) + representative_color.
    DP statics hoisted above constructors (ctor forces registration, the S364
    SolidColorBrush precedent).
  - src/core/src/Element.cpp — register the 9 authorable resource element types
    (41 -> 50): PathGeometry/GeometryGroup/RectangleGeometry/EllipseGeometry/
    GeometryDrawing/DrawingGroup/DrawingBrush/DrawingImage/ImageBrush.
  - src/core/CMakeLists.txt — +VectorDrawing.cpp.
  - src/core/include/vaml/core/VamlReader.h — the 9 build_* declarations +
    build_retained_child/build_property_children (private members, own the child
    graphs via reader-retained objects).
  - src/core/src/VamlReader.cpp — the build() dispatch for the 9 types + the
    builder impls (attributes Write, property-elements Drawing.Drawing /
    GeometryDrawing.Geometry / group children wire the shared_ptr graph) +
    parse_int32 helper. Brush/Pen attributes accept only numeric brush ids (the
    literal-color `#RRGGBB` brush-id conversion is a recorded NO-GAP later touch).
  vulkan:
  - NEW src/vulkan/include/vaml/vulkan/vector/VectorRaster.h + src/vulkan/src/
    vector/VectorRaster.cpp — the headless raster seam: raster_drawing_to_tile /
    raster_geometry_to_tile (geometry -> 64x64 RGBA8 S202 tile, ink-box UVs) +
    vector_quad (the S206 textured-quad DrawItem sampling the tile — the "drawn
    via the S202 atlas mesh"; ADR-015 single-quad). Device upload = [both-apps,
    not CI] path (mirrors S400_1).
  - src/vulkan/CMakeLists.txt — +src/vector/VectorRaster.cpp.
  tests:
  - NEW test/unit/11k4_vectorResources_test.cpp (vaml-vulkan-tests target):
    parse_path_geometry matrix (M/L/H/V/C/S/Q/T/A/Z + repeats + H/V axis + multi-
    figure + degenerate) + set_figures fail-loud on empty + authoring round-trip
    capturing Object resources by x:Key (arrowGlyph/rounded/tagBrush/vectorBrush
    incl. the DrawingGroup->GeometryDrawing->PathGeometry graph) + abstract
    Geometry non-authorable + duplicate-x:Key dedupe + the raster seam
    (geometry->tile->textured quad, ink colour + UV) + degenerate drawing ok=false.
  - test/unit/Milestone407_test.cpp + BrushAuthoring_s460_test.cpp +
    ElementFactoryRegistry_test.cpp — element-count gates 41 -> 50 updated AT THIS
    STEP (never a silent backfill).
  - test/unit/CMakeLists.txt — +11k4_vectorResources_test.cpp (vulkan tests).
  infra:
  - NEW build/verify_s11k4.cmd (two-lane gate, verify_s460.cmd shape).
  - session-context.md (this file).

build_status: **BOTH CI LANES GREEN** — build/verify_s11k4.cmd ended `=== S11K4 BOTH CI LANES OK ===`: (a) **windows-msvc** msvc-debug build warning-as-error CLEAN + ctest 4/4 100%; (b) **ubuntu-clang** clang-debug build on the VM CLEAN + ctest 4/4 100% (the VM was off-LAN initially — BLOCKED; after the user started it, clang's -Wunused-function caught an unused to_tile helper in the new raster seam that MSVC didn't flag, fixed + re-verified, BOTH lanes now end GREEN). 11k-4 is COMPLETE.

next_step: author the roadmap/progress step records (DONE — roadmap 11k-4 marked DONE; progress.md ACTIVE-CURRENT advanced to 11k-4) + commitit (standing directive — driver's seat is yours). Next sprint token = **11k-5 (EventTrigger element-level authoring)**.

notes: no git commands, no commits (standing directive — commitit is YOURS to
  invoke). MINIMAL REAL: none introduced (all 9 types + the abstract bases are
  real permanent Cat-1 surface; Geometry/Drawing are the WPF-faithful abstract
  contract, and the 11k-3 `<Geometry>` deferral is RESOLVED here at its own step —
  NO-GAP). ADR-011 held (core data; render stays vulkan), ADR-015 (single quad:
  raster once), ADR-018 (zero new locks: CPU raster single-threaded once at
  import). ZERO silent backfills: the element-count gates updated at THIS step;
  the Brush/Pen literal-color->brush-id conversion is a recorded-NO-GAP later
  surface.

ROADMAP-GOVERNANCE (11k-P0 PREAMBLE): per the user's directive, added a **11k-P0 PRE-11K
  DRIFT-CLOSURE MANDATE** token to docs/roadmap.md (after the FIDELITY CONTRACT / STATUS line,
  before 11k-0): pre-11k types (controls/framework/panel/shape/value/brush/hierarchy, all authored
  WITHOUT the WPF-source baseline -> drifted by construction) MUST be validated against
  E:\GitRepos\wpf by 11k-8/9/10/12/13, and a later step MUST NOT cite pre-baseline docs, an earlier
  pre-11k gate, or a scoped audit row to leave drift open (the "do not sight X" loophole closed).
  Also EXPANDED 11k-9 scope to include the pre-11k VALUE-TYPE + TYPE-HIERARCHY surfaces (CornerRadius/
  Point/Size/Vector/Matrix/KeyTime/Duration/RepeatBehavior/Stretch/TextTrimming/TextWrapping/
  GridUnitType/enums/Color-brush parse family + the Control-vs-FrameworkElement-vs-UIElement base
  re-audit, the Text-vs-TextBlock class) so they close AT 11k-9, never orphaned. STATUS line updated
  (11k-P0 DONE). This converts the user's two directives (unlisted drift gets tokens; no-citation
  loophole) into concrete, searchable, binding roadmap tokens. No code touched (governance-only).
  NEXT: 11k-5 (EventTrigger) is the next feature token; the pre-11k value/hierarchy closure is bound to
  land in 11k-9 under this mandate.

POST-GREEN GH-RUNNER REMASTER: while both lanes were green, the user surfaced a
  GH Actions failure on PR #162 (11k-2): `vaml-tools-tests` failed under
  `AppShellCompile_test.cpp(197)` — the S444 parity `ShouldBe(onDiskCore,
  inlineCore)` failed although BOTH strings render byte-identically in the log.
  ROOT CAUSE (inferred from the identical-display + line-count evidence): a
  working-tree LINE-ENDING artifact of `.gitattributes` `* text=auto`. The test
  reads `vaml_viewer/MainView.vaml` in `std::ios::binary` (byte-for-byte) and
  compares to the inline `kAppShellMainViewVaml` raw string (in `app_shell.h`).
  On a Windows checkout Git may leave one as CRLF and the other LF per-line
  (binary read preserves the checkout's CRLF; the compiled inline twin keeps LF),
  so the strings look identical but differ per line -> `ShouldBe` fails. FIX
  (test-only, S444-remaster): the parity intent is SEMANTIC app-shell equality,
  not a byte-encoding lock — added a `normalize_newlines` lambda (CRLF/lone-CR ->
  LF) applied to BOTH `onDiskCore` and `inlineCore` right after the existing
  comment-strip + trailing-trim. This is a strict superset (LF/LF still passes,
  CRLF/LF now passes), cannot regress the LF case, and makes the S444 parity
  robust to the checkout platform. VERIFIED: `vaml-tools-tests` all 151 cases /
  749 assertions green locally AND both full CI lanes re-greened via
  build/verify_s11k4.cmd (msvc-debug + clang-debug, 4/4 each, AppShellCompile
  now building+passing on both compilers). The 41->50 element-count gate updates
  are unrelated to this runner failure (that failed test is the tools target, not
  a unit gate).
