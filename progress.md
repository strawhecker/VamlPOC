# VAML — PROGRESS / LLM CONTINUATION FILE  (SINGLE SOURCE OF TRUTH)

> ## HOW TO RESUME (an LLM must read this first, with no prior context)
> 1. Read ONLY this header block + the two `## CHECKPOINT` lines below.
> 2. That tells you: repo state, last completed step token, next step token, and the
>    surviving-state fingerprint. Do NOT re-read the history section to resume.
> 3. If a step is partially done (PENDING), read its SURVIVOR block directly.
> 4. Generated artifacts are never trusted; regenerate any derived file from the
>    surviving source files. Trust this file for WHERE things stand, trust source
>    files for WHAT the code is.

---

## HEADER — VERSION 1
- Framework: VAML (XAML-faithful, Vulkan-native, pure C++)
- Language/stack: C++ latest only. No C#, no Python, no managed code (includes build tooling).
- Distribution: consumer chooses shared (dll/so) or static (lib/.a); default = static for IP-hiding. [ADR-003]
- Repos: this `VamlPOC` repo IS the product — v1 built at v2 quality (ADR-017). NOT a throwaway POC;
  there is no separate production repo to open later. The repo is production-framed from here on.
- Build: MSVC (VS 2026) on Windows and Clang on Linux; CMake is the canonical driver. [ADR-007]
- Tests: doctest framework + adapted Shouldly-style fluent asserts (ported, MSTest-decoupled). [ADR-009]
- Async: Task/Executor continuation model, NOT compiler coroutine keywords. [ADR-010]
- MVVM: VAML ships its own lightweight MVVM layer (ObservableObject, RelayCommand, Messenger,
  static CompositionRoot, RoutedEvent). No Prism-style runtime modularity. [ADR-008]

## CHECKPOINT — DO NOT EDIT BY HAND (agents regenerate)
- Last completed step : S139 (SURVIVOR + checkpoint advance) [token: S139]
- Next step to do     : S140 (dep:S139) Sprint 3.5 — Intern property names / attribute strings to a canonical UTF-8 identity (Allocation & ownership discipline retrofit)
- PENDING work        : none (S139 complete; Sprint 3 CLOSED — S138 REFACTOR GATE(S3) verified both CI lanes green + clean grep-audit; S139 SURVIVOR + checkpoint advanced to S140.)
- Surviving-state digest: (computed when first build artifact exists; place SHA-256 of
  `build/state/surviving-nodes.ser` here once that pipeline is live)
- Key invariants intact: [ADR-001..012] [layer-rule: core has NO Vulkan/GLFW deps; only vulkan backend touches GPU]
  [no-managed] [no-stub rule] [MINIMAL REAL: vulkan→S161, tools→S287, Color→S089, Thickness→S089] [NO-GAP GUARANTEE] [NO LAZY LLM]

## GUARD RAILS (codified from S003 — mirror of docs/roadmap.md; survives roadmap trimming)
- **MINIMAL REAL**: permanent, linkable, forward-resolvable seed (kept+extended, NOT a stub);
  tagged with searchable sentinel `// [MINIMAL REAL] <comp> — RESOLVED BY <token>`; removable only
  by its declaring step. Active: vulkan→S161, tools→S287, built-in element seeds (S087,
  in `vaml/core/Element.h`) → Button→S401, Text→S400, TextBox→S402,
  CheckBox→S404, Slider→S403, ScrollViewer→S405, Window→S440; VamlReader event hooks (S092,
  `vaml/core/VamlReader.h`) → S322; UIElement transform-owner (S120, `vaml/core/UIElement.h`) → S208.
  RESOLVED: Color + Thickness (S089 — their real parsing
  semantics now live on the types via parse_color/parse_thickness); StackPanel (S129 — real panel);
  Grid (S131 — real panel: row/col definitions + Absolute/Auto/Star resolution + attached
  Grid.Row/Grid.Column; S132 adds attached Grid.RowSpan/Grid.ColumnSpan + span-aware
  measure/arrange); WrapPanel (S134 — REAL panel: UIElement-derived, WrapDirection DP default
  Horizontal, flow/wrap/align; a NEW element, not a seed); Canvas (S136 — REAL panel:
  UIElement-derived, attached Canvas.Left/Canvas.Top DPs (Double, default NaN = "not set"),
  absolute-position measure/arrange; a NEW element, not a seed).
- **NO-GAP GUARANTEE**: no feature/symbol left for silent backfill; every MINIMAL REAL has an
  explicit resolution authored at the same step that delivers real content.
- **NO LAZY LLM SYNDROME**: default posture is to reason the opposite of lazy; MINIMAL REAL is
  explicitly NOT lazy — it is strategic, documented-by-design.

## SURVIVING STATE (what a fresh LLM must assume is true)
- S000 DONE: directory skeleton created — docs/ (adr, design), src/ (core, vulkan, tools, cmake),
  test/ (unit, integration), VamlPOC/ with SoC subfolders {hmi, core, infra, viewmodels}/ for the
  sample APPLICATION (distinct from the library source tree in src/); tracking files present:
  progress.md (ROOT), docs/ROADMAP.md (S000–S999 tokenized), docs/architecture.md (seed),
  docs/adr/ADR-LOG.md (ADR-001..012), docs/design/GPU-DRIVEN-PRINCIPLE.md, docs/design/DESIGN-SEEDS.md.
- S001 DONE: top-level CMake (project(VAML) 0.1.0, C++20 CXX), vaml_apply_compiler_policy(),
  src/ + src/core CMake, vaml-core STATIC lib with version API (non-stub), CMakePresets.json,
  .gitignore, build/verify_msvc.cmd. VERIFIED: MSVC 19.51, warnings-as-errors ON, vaml-core.lib
  builds+links green. NOTE: no Vulkan/GLFW/vcpkg at S001 (correct — core has zero Vulkan deps,
  ADR-011); Next = S002.
- Build system: NOT started (S001 is the first code step).
- Property system, parser, Vulkan, binding, etc.: NOT started. See ROADMAP.md for the full S000–S999
  tokenized step list. The "no-stub" rule means a step is only ✅ when its behavior is real and tested.

---

---

## SPRINT 0 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S009 wrap)
- **Goal met (S000–S008): green skeleton, zero feature code.** Repo, toolchain, CI, and test
  harness are real and verified; no product feature (property system, parser, renderer, etc.) is
  started. All Sprint-0 gates are ✅.
- **Artifacts / libraries:** three STATIC libs — `vaml-core`, `vaml-vulkan`, `vaml-tools`
  (`VAML_BUILD_SHARED` default OFF; shared path proven but not the default). Artifact model +
  ADR-011 layer rule enforced in `cmake/artifact-model.cmake` (`vaml_add_library`, generate_export_header,
  `VAML_LAYER`, `vaml_validate_layer_rules`). Include-surface convention: internal impl headers under
  `src/<lib>/src`, ONLY public API under `src/<lib>/include/vaml/<lib>`.
- **Test harness:** vendored doctest single-header (v2.4.11) + ported `Shouldly.h`
  (`vaml::test` facade) with pluggable failure sink. `vaml-core-tests` EXECUTABLE =
  `smoke_test.cpp` + `shouldly_test.cpp`; `ctest` green 1/1.
- **Build lanes (both verified live):** MSVC VS 2026 (`msvc-debug` / `msvc-debug-shared`,
  warnings-as-errors ON) and real Ubuntu VM Clang 21.1.8 (`clang-debug`) via shared folder
  `/mnt/hgfs/VamlPOC` (cmake 4.2.3, ninja). Presets: `cmake/presets/{base,configure,build,test}.json`
  nested include chain; toolchains under `cmake/toolchains/`. MSVC lane driven via `build/verify_s*.cmd`
  (clean PATH + vcvars64). CI: `.github/workflows/ci.yml` (windows-msvc + ubuntu-clang).
- **MINIMAL REAL seeds carried forward UNRESOLVED (never backfilled in S0):**
  `vaml-vulkan` version marker → **S161**; `vaml-tools` version marker → **S287**. No other
  MINIMAL REAL active. Both carry searchable sentinel comments.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Next = S030** (Sprint 1, Property system core — first token of the real feature work).

---

## SPRINT 1 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S030..S048, wrapped at S049)
- **Goal met (S030–S048): the property-system core is complete, green on both compilers, and clean.
  The "heart" of VAML (full DP semantics) is real, tested, and refactor-gated.** Sprint-1 closing =
  S046 (value hook proofs) → S047 (suite-complete gate) → S048 (refactor gate) → S049 (wrap).
- **Full DP semantics delivered (real public symbols, none MINIMAL REAL / none STUB):**
  `Typeless Value` (11 tags, type-safe get) — S030; `PropertyKey` + `PropertyRegistry` (name→key,
  name-unique dedupe, pimpl) — S032; `PropertyId` (constexpr-safe name-token) — S033;
  `PropertyMetadata` (default/coerce/validate/changed, move-only pimpl) — S034;
  `DependencyObject` (local store, Get/Set with the S035 pipeline, thread-affinity) — S036;
  wrong-thread write QUEUING (marshalling) — S037; `ChangeRecorder` — S038;
  global `DependencyProperty::Register` + `GetEffectiveValue` (local ?? inherited ?? default,
  InheritOnly) + parent link — S039; inheritance cascade (proof) — S040;
  `ChangeQueue` + `ChangeProcessor` + registration-ordered subscribers + descendant push — S041;
  coalescing (proof) — S042; ATTACHED properties (owner≠class, metadata-provider borrowed) — S043;
  attached inheritance + notification (the passive-descendant gap-fix) — S044;
  ReadOnly enforcement + validation + all-flags — S045; coercion + per-set-validation value-hook
  proofs (attached/global axis + wrong-thread-queued-coercion) — S046.
- **Assertion vocabulary (standing convention):** the *Shouldly-for-doctest* vocabulary — the
  SHOULD/SHOULD_REQUIRE predicate seam + doctest StringMaker + `ShouldThrow<Ex>` — is adopted
  suite-wide from S036_5 onward; S047 certified ZERO unadapted feature `CHECK`/`CHECK_THROWS_AS`
  in every migrated feature file. Sanctioned non-feature exceptions: PropertyMetadata_semantics
  (the S035 spec harness), smoke_test (runner), StringMaker_test (stringifier test), shouldly_test
  (predicate/routing library test), REQUIRE null-deref precondition guards.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** internal store keys on an
  OWNED std::string, not the non-owning PropertyId view; coerce→validate(coerced)→fire-on-real-
  change→store pipeline contract (PropertyMetadata_semantics_test MiniStore is the reference spec);
  the move-only PropertyMetadata is NEVER copied into an object's entry (attached/borrowed
  PropEntry); descendant push carries each descendant's own effective-old→effective-new delta
  (local-wins respected through an intermediate); S046 REDIRECT (recorded): the registry-level
  "ValidateDependencies" graph is INERT for real WPF MVVM (VM composition is instance-level, not
  registry linkage) → replaced by WPF-faithful coercion + per-set validation value hooks.
- **Deferred to future sprints (all recorded, NOT silent):** BindsTwoWayByDefault runtime behavior →
  S242; owner-internal read-only write path → S120+ controls; visual tree / UIElement (derives
  DependencyObject) → S120; full store-race freedom (TSan) → S620/S723; genuine two-way binding →
  S242.
- **MINIMAL REAL carried forward RESOLVED/unresolved:** NONE resolved in Sprint 1. Still unresolved:
  `vaml-vulkan` → S161; `vaml-tools` → S287; `Color`/`Thickness` (S030) → S089. All carry searchable
  sentinel comments; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles):** windows-msvc (msvc-debug via build/verify_s*.cmd,
  VS 2026) + ubuntu-clang (clang-debug via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21) — configure/build
  warning-as-error clean + ctest 1/1 PASS. windows-clang-debug is NOT a lane.
- **Next = S080** (Sprint 2, VAML markup parser — XML scanner; the first token of the next sprint).

---

## SPRINT 2 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S080..S096, wrapped at S097)
- **Goal met (S080–S096): the VAML markup parser + runtime reader (text → ParsedElement tree →
  live DependencyObject tree) is complete, green on both compilers, and refactor-gated.** The
  runtime path is FIRST — Sprint 2 delivers text→tree→live objects before the compiled VBAML path
  (Sprint 7). Sprint-2 closing = S095 (golden-doc full suite) → S096 (refactor gate) → S097 (wrap).
- **Full parser/reader pipeline delivered (real public symbols, none MINIMAL REAL / none STUB):**
  `XmlScanner` (elements, attributes, self-closing, comments, CDATA, entities, nested tags, per-element
  namespace scope, x-language directive classification, attribute-value grammar) — S080–S084;
  `XmlScanException` structured scan-error taxonomy (19 categories, 1:1 per malformed path, line/col) —
  S081; `ParsedElement`/`ParsedAttribute` + `parse_vaml()` (tags + children + content tree) +
  `XmlParseException::UnterminatedElement` — S085; round-trip golden (S086); `ElementFactoryRegistry` +
  `Element` MINIMAL REAL seeds + `register_builtin_elements()` — S087; `resolve_element()` +
  `ElementResolutionException(UnknownType)` naming type+line — S088; `ValueConverterRegistry` +
  `register_builtin_converters()` (Int32/Thickness/Color/Bool) + `parse_color`/`parse_thickness`
  (RESOLVES the S030 Color/Thickness markers) — S089; `VamlReaderError::UnknownLiteral` +
  `try_infer_type` — S090; `VamlReader::Load(text/ParsedElement)` live tree builder + retained_objects
  + parent links + unknown-property baseline — S091; x:Name/x:Class registry + deferred markup
  extensions (S2 tag) + MINIMAL REAL event hooks + DuplicateName — S092; integration gates — S093/S094
  (S093 no-hooks pure-text; S094 error-taxonomy consolidation + `ClassOnNonRoot`); FULL-SUITE golden
  docs — S095.
- **Golden-doc full suite (S095, the capstone):** `GoldenDocs_test.cpp` asserts the ENTIRE S2 pipe
  (`text → parse_vaml → ParsedElement → Load → live tree`) on realistic documents across FOUR axes —
  parser golden canonical string, live reader tree (types/retained order/parent chain/x:Name registry/
  root_class/converted values incl. attached), deferred surface (MerkupExtension {Binding} retained
  verbatim, event handler routed, x:Key deferred), and live text-path==parsed-path parity — plus a
  golden B error-taxonomy gate (unknown element/attribute, malformed value, duplicate x:Name, x:Class
  on non-root each fire the RIGHT structured exception) and a clean-minimal no-hooks/no-extensions
  control. TEST-ONLY, zero new product symbols.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** the scanner is hand-rolled
  (no external XML lib — no manual XML dep, verified at S096); namespaces resolve on the URI not the
  letter "x" (any prefix mapped to kXamlLanguageNamespace is the x-language); attached-style dots
  (Grid.Row) are NOT namespace separators; markup-extension detection honors balanced braces;
  ParsedElement is tags+children+content (not a pseudo-element sentinel) so readers distinguish text
  from elements at the type level; element type resolution keyed on local_name (so `<vm:Button>`
  resolves "Button"); "carried, not re-scanned" — the tree carries the scanner's classified metadata;
  `resolve_element` is the SINGLE-ELEMENT primitive, consumed by build(); `ValueConversionError` has
  three orthogonal categories (UnknownType/MalformedValue/UnknownLiteral); the reader routes registered
  event keys to DeferredEventHandlers and records isExtension values as DeferredExtensions (the "S2
  tag" {Binding}, resolved by S252).
- **Deferred to future sprints (all recorded, NOT silent):** {Binding} markup resolution → S252/S253;
  x:Key resource-key → S361; x:Null → S084 (tagged, already deferred); event-handler wiring → S322;
  VBAML compiled path → Sprint 7; re-serializer/serializer → S282/S283; visual-tree children list →
  S121; real control logic (from the element seeds) → each control's sprint (S129/S131/S400-S405/S440).
- **MINIMAL REAL carried forward RESOLVED/unresolved:** RESOLVED in Sprint 2 — Color + Thickness
  (S089, parse_color/parse_thickness, sentinels removed). Introduced + still UNRESOLVED: built-in
  element seeds (S087, Element.h) → Button→S401, StackPanel→S129, Grid→S131, Text→S400, TextBox→S402,
  CheckBox→S404, Slider→S403, ScrollViewer→S405, Window→S440; VamlReader event hooks (S092) → S322.
  Still unresolved (pre-existing): vulkan→S161, tools→S287. All carry searchable sentinels; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles):** windows-msvc (msvc-debug via build/verify_s*.cmd,
  VS 2026) + ubuntu-clang (clang-debug via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21) — configure/build
  warning-as-error clean (except the pre-existing, downgraded upstream <ciso646> doctest deprecation) +
  ctest 1/1 PASS through the S096 gate.
- **Next = S120** (Sprint 3, element/scene graph + measure/arrange — UIElement base, first token).

---

## SPRINT 3 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S120..S138, wrapped at S139)
- **Goal met (S120–S138): the element/scene-graph + measure/arrange layout system is complete, green on
  both compilers, and refactor-gated. UIElement base + visual tree + a WPF-faithful infinity-aware
  Measure/Arrange contract + four REAL panels (StackPanel/Grid/WrapPanel/Canvas) + a dirty-flag
  LayoutManager driver, all real + tested + green on both CI lanes.** Sprint-3 closing = S136 (Canvas
  product) → S137 (Canvas tests) → S138 (refactor gate) → S139 (wrap).
- **Full element/layout system delivered (real public symbols, none MINIMAL REAL / none STUB):**
  layout value primitives — `LayoutSlot.h` + `Size`/`Rect`/`layout_slot` + `compute_final_rect(slot,
  desired, margin, hAlign, vAlign)` (the arrange math: origin advances + slot shrinks by margin,
  non-Stretch pins inside) + `clip_to_bounds(content, clipBounds)` (rectangular render-slot clip) —
  S122/S124; `UIElement` base (derives DependencyObject; parent link, visual children + traversal,
  `Margin`/`HorizontalAlignment`/`VerticalAlignment`/`Width`/`Height`/`MinWidth`/`MaxWidth`/… DPs,
  infinity-aware `Measure(available)` finalizer `compute_measured_desired` — finite-fills / unbounded-
  wraps / non-Stretch clamps to min(natural,available), the S123 stretch-vs-wrap rule) —
  S120–S123/S126; the protected virtual `MeasureOverride(const Size&)` (natural content extent) +
  `ArrangeOverride(const Rect&)` (auto-drive: `Arrange` computes final_rect then auto-invokes
  ArrangeOverride) — S127; the `LayoutManager` dirty-flag driver (`InvalidateMeasure`/`InvalidateArrange`
  propagate up-to-root with short-circuit, `UpdateLayout` runs a measure pass iff root measure-dirty +
  arrange pass iff root arrange-dirty, TRUE no-op when clean) — S126; **REAL panels**: `StackPanel`
  (Orientation DP default Vertical, last-child-stretch, dirty-only re-measure/re-arrange) — S129/S130;
  `Grid` (GridLength {Auto/Pixel/Star} + RowDefinition/ColumnDefinition, resolve_grid_lengths, attached
  Grid.Row/Grid.Column, Absolute/Auto/Star resolution, attached Grid.RowSpan/Grid.ColumnSpan + span-aware
  two-pass D1/D2 math, dirty-only) — S131/S132/S133; `WrapPanel` (WrapDirection DP default Horizontal,
  natural-extent flow/wrap/align, aligned rows, dirty-only) — S134/S135; `Canvas` (attached Left/Top
  Double DPs default **NaN = "not set"**; absolute-position measure/arrange; bounding-extent DesiredSize
  clamped >= 0; dirty-only) — S136/S137.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** the Infinity sentinel is the
  "available is unconstrained" marker and is guarded by `is_infinite()` in the finalizers; `Measure` is
  WPF-faithful margin handling (shrink available by Margin → MeasureOverride(inner) → finalize → add
  Margin back into DesiredSize, clamped to available) and `compute_final_rect` strips the margin back
  out (margin never counted twice) — the S130_1 resolution; the Arrange auto-drive (ArrangeOverride
  receiving final_rect) is what lets LayoutManager arrange a whole subtree from the root alone; dirty-
  confinement: a child invalidation re-measures/re-arranges only that child's subtree-influence (leaf +
  ancestor chain), never friend subtrees or clean siblings (S128 proof on R>(P1>(A,B),P2>(C,D)) +
  4-level chain); each REAL panel adopts the dirty-only-subpass discipline inside MeasureOverride/
  ArrangeOverride (re-measure/re-arrange ONLY is_*_dirty() children, reuse clean desired_size/final_rect);
  four REAL panels share one pattern: measure offers each child, derives panel DesiredSize from children,
  arrange places each child into its slot honoring alignment+margin via child->Arrange.
- **Deferred to future sprints (all recorded, NOT silent):** `Canvas`'s and `ScrollViewer`'s render-slot
  clip consumption → S208 (renderer) / S405 (ScrollViewer); `Text`/`Button`/etc. real control logic from
  the S087 element seeds → each control's sprint (S400–S405/S440); visual-tree children list already
  delivered S121; transform-owner → S208; the S209 renderer consumes final_rect() as render slot +
  clip_to_bounds.
- **MINIMAL REAL carried forward RESOLVED/unresolved:** RESOLVED in Sprint 3 — `StackPanel` (S129, real
  panel, sentinel removed), `Grid` (S131, real panel + S132 spans, sentinel removed), `WrapPanel` (S134,
  real panel, NEW element no sentinel), `Canvas` (S136, real panel, NEW element no sentinel). Still
  UNRESOLVED (carried forward): `vulkan`→S161; `tools`→S287; built-in element seeds → Button→S401,
  Text→S400, TextBox→S402, CheckBox→S404, Slider→S403, ScrollViewer→S405, Window→S440; VamlReader event
  hooks→S322; UIElement transform-owner→S208. All carry searchable sentinels; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed, no-stub,
  NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles, at the S138 gate):** windows-msvc (msvc-debug via
  build/verify_s137.cmd, VS 2026, warning-as-error clean + ctest 1/1 PASS) + ubuntu-clang (clang-debug
  via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21, ctest 1/1 PASS; only pre-existing upstream <ciso646> doctest
  deprecation from third_party, non-error). windows-clang-debug is NOT a lane (not built/tested/flagged
  per standing rule).
- **Next = S140** (Sprint 3.5, Allocation & ownership discipline retrofit — intern property names to a
  canonical UTF-8 identity; the first token of the next sprint).

## STEP LOG  (append-only; new steps land at the BOTTOM)

### Sprint 0 — Repo, toolchain, CI, test harness   (S000–S019)
- [x] S000 repo skeleton + tracking files (this file, ROADMAP.md, ADR seeds, architecture.md) — ✅
  SURVIVOR: layout is docs/{adr,design}/, src/{core,vulkan,tools,cmake}/, test/{unit,integration}/,
  VamlPOC/{app+hmi,core,infra,viewmodels}/. progress.md lives at repo ROOT (lowercase). Next = S001.
- [x] S001 top-level CMake + compiler policy + vaml-core baseline — ✅
  SURVIVOR: CMakeLists.txt (project(VAML) 0.1.0, C++20, /W4 /permissive- /EHsc /WX MSVC;
  -Wall -Wextra -Wpedantic -Werror Clang), vaml_apply_compiler_policy() helper, src/CMakeLists.txt,
  src/core/CMakeLists.txt (vaml-core STATIC), include/vaml/version.h (Version + version() function),
  src/version.cpp, CMakePresets.json (msvc/clang), .gitignore, build/verify_msvc.cmd. Build verified
  MSVC 19.51 warnings-as-errors → vaml-core.lib links. No Vulkan at S001 (correct, ADR-011).
  GOTCHA: use vcvars64.bat before cl; use VS-bundled cmake.exe + ninja.exe (not on base PATH).
- [x] S002 cmake/ toolchain files + compiler presets (MSVC VS2026, Linux Clang) — ✅
  SURVIVOR: policy extracted to cmake/compiler-policy.cmake (vaml_apply_compiler_policy, flags
  byte-identical to S001: MSVC /W4 /permissive- /EHsc /Zc:* /WX + /wd4100; Clang -Wall -Wextra
  -Wpedantic -Werror). Toolchains: cmake/toolchains/{msvc-vs2026,linux-clang,windows-clang}.cmake.
  Presets split into cmake/presets/{base,configure,build,test}.json (Alt B) with a nested include
  CHAIN (base<-configure<-build<-test, root includes cmake/presets/test.json) — siblings do NOT
  cross-inherit in CMake. Root CMakePresets.json is a thin include stub. Presets anchored to
  toolchainFiles: msvc-debug/release->msvc-vs2026, clang-debug->linux-clang,
  windows-clang-debug->windows-clang. VERIFIED: clean configure --preset msvc-debug via include
  chain + toolchainFile -> MSVC 19.51, /WX active, vaml-core.lib builds green. Linux + windows-clang
  toolchains are static (no-build-on-host; review-validated only). Next = S003.
  GOTCHA: Ninja generator does NOT support CMAKE_GENERATOR_TOOLSET (MSVC toolchain file must NOT pin
  a toolset; vcvars64 selects it). GOTCHA: preset inheritance must be a nested include chain, not
  sibling includes.

### (remaining S003–S999 steps live in docs/ROADMAP.md, the authoritative tokenized list)

- [x] S003 static-library artifact model + guard rails — ✅
  SURVIVOR: cmake/artifact-model.cmake (NEW — vaml_add_library() helper, VAML_BUILD_SHARED
  default OFF, generate_export_header per lib, OUTPUT_NAME vaml-*, VAML_LAYER tag,
  vaml_validate_layer_rules() ADR-011 guard). Top-level CMakeLists includes artifact-model +
  calls vaml_validate_layer_rules() after add_subdirectory(src). src/core/CMakeLists.txt refactored
  to vaml_add_library(core). src/vulkan/CMakeLists.txt + src/tools/CMakeLists.txt NEW, un-commented
  in src/CMakeLists.txt; each builds a version-marker MINIMAL REAL lib. Libs now: vaml-core,
  vaml-vulkan, vaml-tools. Generated export headers committed at
  src/{core,vulkan,tools}/include/vaml/{core,vulkan,tools}/export.h (VAML_{CORE,VULKAN,TOOLS}_EXPORT;
  empty when static). Presets: msvc-debug-shared added (configure/build/test) to prove shared path.
  VERIFIED: static (`msvc-debug`) builds vaml-core.lib+vaml-vulkan.lib+vaml-tools.lib green, /WX ON;
  shared (`msvc-debug-shared`) builds vaml-core.dll/vulkan/tools green. ADR-011 guard negative-tested
  (vaml-core linking vaml-vulkan → configure FATAL "ADR-011 VIOLATION", then reverted to green).
  Linux + windows-clang static: review-validated only (no host build; consistent with S002).
  GUARD-RAIL CODIFICATION: MINIMAL REAL, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME terms added to both
  docs/roadmap.md (Guard Rails section) + progress.md (Guard Rails block); S003/S161/S287 lines in
  roadmap carry the forward-declared resolution contracts (vulkan→S161, tools→S287). No new md files.
  Next = S004.
  GOTCHA: layer guard introspects LINK_LIBRARIES (direct edges) — sufficient because GLFW/Vulkan are
  only reachable via vaml-vulkan in this project. GOTCHA: export.h generated into the PUBLIC include
  tree (absolute EXPORT_FILE_NAME) so targets consumers include via vaml/{lib}/export.h.
- [x] S004 CI matrices (.github/workflows) — ✅
  SURVIVOR: .github/workflows/ci.yml (NEW, sole deliverable of S004). Single matrix-driven file, two
  live jobs exactly matching ADR-007 core matrix: windows-msvc (windows-latest, VS 2026 cl.exe) ->
  preset msvc-debug; ubuntu-clang (ubuntu-latest, clang++) -> preset clang-debug. Both jobs run the
  identical checkout -> install toolchain -> cmake --preset -> cmake --build --preset -> ctest
  --preset --output-on-failure sequence, reusing S002/S003 presets verbatim (msvc-debug/clang-debug
  are named identically across configure/build/test). MSVC lane: microsoft/setup-msbuild (cl.exe +
  vcvars on PATH) + choco install ninja -y; Linux: apt clang + ninja-build. Guards: per-job
  timeout-minutes=30, warnings-as-errors ON via presets, no pinned runner labels (rolling -latest,
  keeps MSVC lane on same VS as local dev + follows platform). NO windows-clang lane in any form;
  NOT commented into YAML (toolchain seed stays unbuilt in cmake/toolchains/). Non-goals: no
  artifacts, no coverage, no packaging (future sprints own those). VERIFICATION: MSVC lane
  reproducible locally against the same msvc-debug preset (VS 2026) — re-ran clean, catches preset
  name typos pre-push. Ubuntu lane = no-build-on-host; acceptance is GH Actions on push/PR (free for
  public repos). All preset names in the workflow cross-checked against configure/build/test.json.
  Roadmap S004 line marked [DONE]. No build-system/feature code changed. Next = S005.
  GOTCHA: a workflow depends on preset NAMES being identical across configure/build/test JSON —
  CI calls only --preset <name>, never CMake directly, so a rename in presets silently breaks a lane
  (checked: msvc-debug + clang-debug are consistent). GOTCHA: MSVC lane needs the VS dev shell env
  (cl.exe), so setup-msbuild MUST precede cmake --preset; base.json sets Ninja generator so Ninja
  must be installed (choco) before configure.
- [x] S005 vendor doctest + test harness + 0-feature smoke test — ✅
  SURVIVOR: third_party/doctest/doctest.h (NEW — official doctest SINGLE HEADER, pinned v2.4.11,
  vendored VERBATIM, no upstream edits; SHA-256
  44FAA038E9C3F9728EFBDA143748D01124EA0A27F4BF78F35A15D8FAB2E039FB — regenerate from the
  https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h tag, never hand-edit).
  test/CMakeLists.txt + test/unit/CMakeLists.txt (+NEW): single EXECUTABLE vaml-core-tests
  from smoke_test.cpp, links vaml-core (proves consumer-link), vaml_apply_compiler_policy(),
  registered via add_test(NAME vaml-core-tests). test/unit/smoke_test.cpp (NEW):
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN + TEST_CASE("smoke") with 3 CHECKs (true, version().major==0,
  engine_name()) — exercises doctest + the vaml-core version symbol; 0-feature by design.
  Top-level CMakeLists.txt (MOD): enable_testing() + add_subdirectory(test) after
  vaml_validate_layer_rules(); test subtree contributes no library symbols. verify_s005.cmd +
  verify_s005_shared.cmd (NEW in build/, mirror S004 pattern; resets PATH so VS-bundled cmake/ninja
  win over Cygwin PATH). VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) configure+build+ctest green
  (1/1 PASS, doctest 2.4.11, 3/3 assertions), and REAL LINUX clang-debug on the Ubuntu VM
  (clang 21.1.8, cmake 4.2.3) via shared folder /mnt/hgfs/VamlPOC green (1/1 PASS). msvc-debug-shared
  still configures (no S003 regression). NOT a STUB / NOT MINIMAL REAL: doctest is permanent test
  infra, 0 product symbols introduced, so no sentinel + no resolution token (recorded here so it is
  not mistaken for a silent backfill). Per-case discovery via doctest_discover_tests deferred as a
  documented follow-up, not a silent omission. Next = S006.
  GOTCHA: vendored/3rd-party headers must be EXCLUDED from the strict /WX bar on both compilers.
  MSVC: mark third_party SYSTEM + /external:W0 (doctest triggers benign C5285 std::tuple
  specialization false-positive on new MSVC). Clang: -isystem already silences normal warnings but
  `#warning` directives are ALWAYS reported — upstream doctest includes compiler <ciso646> (carries
  a benign C++20 `#warning`; doctest uses it only for libc++ detection, we use libstdc++) so add
  -Wno-error=#warnings to the test target. GOTCHA: in CMakeLists, quote CMake arguments containing
  `#` (`"-Wno-error=#warnings"`) or CMake parses them as comments → "Function missing ending )".
  GOTCHA: build must be run from a clean PATH + vcvars64 (VS-bundled cmake/ninja); a stale
  msvc-debug cache created under a Cygwin PATH must be deleted before reconfigure (cygdrive path
  mismatch in CMakeCache.txt).
- [x] S006 port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h (namespace vaml::test) — ✅
  SURVIVOR: src/core/include/vaml/assert/Shouldly.h (NEW — header-only port of the user's
  PiuGlobalsCpp::NsShouldly Shouldly file; the spec's "src/core/assert/Shouldly.h" lands at
  src/core/include/vaml/assert/Shouldly.h to match the project's public-include convention).
  Namespace vaml::test (NOT vaml::assert — `assert` is a function-like macro from <cassert>, so
  `namespace assert {}` is ill-formed if <cassert> loads; `assert` as a dir name is fine).
  STRIPPED: MSTest (Microsoft::...::Assert::Fail + wide-string AssertStdString path), Windows bits
  (_wcsdup/wchar_t/free conversions + SHOULDLY_USES_AWESOMELY toggle -> failure now unconditional),
  __forceinline/__inline hints (plain inline), dead/commented code (commented-out Actual overloads,
  unused DynString + is_str helpers, AwesomelyException superseded). KEPT: portable
  #pragma push_macro("max")/#undef max guard. ADDED (only API): TestFailureException : std::exception,
  FailureSink, DefaultFailureSink (throw TestFailureException), failure_sink() (thread_local-static-
  backed settable singleton ref), ReportFailure(string) -> routes to failure_sink()(). All predicate
  names preserved: ShouldBe, ShouldNotBe, ShouldBeGreaterThan, ShouldBeLessThan,
  ShouldBeGreaterThanOrEqualTo, ShouldBeLessThanOrEqualTo, ShouldBeInRangeTo, ShouldBeNull,
  ShouldNotBeNull, ShouldBeNullOrEmpty, ShouldNotBeNullOrEmpty, ShouldNotBeNullOrEmptyOrBe,
  ShouldNotBeNullOrEmptyButBe, ShouldNotBeNullOrEmptyButBeNearlyEqual, ShouldNotBeNullOrEmptyButMatchContents
  + Diff; factories Integral/String/Pointer/Array + macros ShouldlyIntegral/ShouldlyString/
  ShouldlyPointer/ShouldlyArray (retargeted to vaml::test). No CMake change (header lands in existing
  public include root already on vaml-core-tests' include path; header-only -> nothing links).
  S006 is a NO-BUILD-BY-CONTRACT step (compile gate belongs to S007); still SILENT-COMPILE-CHECKED on
  MSVC (VS 2026 cl 19.51, /W4 /permissive- /EHsc /WX /std:c++20, throwaway TU incl. failing-predicate
  throw path + custom-sink path) -> W4/WX clean; throwaway files deleted. Next = S007.
  GOTCHA: the Shouldly factory helper functions REQUIRE the `line` argument (the *_macros supply it via
  `#stuff`); calling Integral/String/Pointer/Array directly without a line fails to overload — use
  ShouldlyIntegral/ShouldlyString/ShouldlyPointer/ShouldlyArray or pass a line explicitly.
  GOTCHA: on Windows, `cl` requires the VS dev shell (vcvars64.bat) — call it inside the same cmd that
  runs cl (a bare `cmd /c "... && cl"` does NOT persist the vcvars env across separate cmd invocations).
- [x] S007 Shouldly.h compiles under MSVC + Clang; assertion round-trip tests — ✅
  SURVIVOR: test/unit/shouldly_test.cpp (NEW — the compile-gate + round-trip test deliverable).
  Includes <vaml/assert/Shouldly.h> + doctest (main/impl owned by smoke_test.cpp; DOCTEST_CONFIG_
  IMPLEMENT_WITH_MAIN lives ONLY there to avoid duplicate-main at link). TEST STRUCTURE: a
  thread-local capture-sink RAII helper `CaptureOn` (installs a collecting FailureSink via
  failure_sink(), restores the prior sink on scope exit, clears the thread-local buffer) -> the
  "round-trip" (predicate -> ReportFailure -> sink -> captured formatted message). 14 test cases:
  (A) pass-vectors for Integral/String/Pointer/Array asserting NOTHING is captured; (B) fail-vectors
  per predicate asserting the captured message carries the expected fragment (e.g. "should be equal",
  "greater than", "range", "[i]" for elementwise); (C) Diff helper both directions on a double array;
  (D) default-sink contract: with NO custom sink a failing predicate throws TestFailureException
  (CHECK_THROWS_AS). Covered predicates: ShouldBe, ShouldNotBe, ShouldBeGreater/LessThan,
  Greater/LessThanOrEqualTo, InRangeTo, BeNull, NotBeNull, BeNullOrEmpty, NotBeNullOrEmpty,
  NotBeNullOrEmptyOrBe/ButBe/ButBeNearlyEqual(value+pointer)/ButMatchContents, Diff.
  test/unit/CMakeLists.txt (MOD): added shouldly_test.cpp to vaml-core-tests sources (no new flags;
  doctest already SYSTEM-excluded + -Wno-error=#warnings). build/verify_s007.cmd (NEW): clone of
  verify_s005 — clean PATH -> vcvars64 -> cmake --preset msvc-debug -> build -> ctest.
  HEADER FIXES (S007 DOES touch Shouldly.h — sanctioned plan-scoped portability+bug fixes, each the
  "compile gate S006 deferred"): (1) BUG: ShouldBeNullOrEmpty required BOTH null AND len==0; intent
  is "null OR empty" — collapsed the two sequential ifs to one `if (!(actual==nullptr || len==0))`.
  (2) TestFailureException(const char*) relied on MSVC-only std::exception(char const*) ctor — now
  stores std::string message_ + overrides what() (portable to libstdc++). (3) Remove meaningless
  top-level const on `char const* const` RETURN types (Clang -Wignored-qualifiers). (4) Qualified
  std::fpclassify + std::isnormal (MSVC leaked cmath names into global scope; libstdc++ does not);
  FP_ZERO is a C MACRO (keep bare, not std::FP_ZERO — naive std:: prefix broke both compilers).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN (1/1 ctest PASS, 15 test
  cases listed = 14 Shouldly + smoke, all green); REAL LINUX clang-debug on Ubuntu VM via shared
  folder /mnt/hgfs/VamlPOC (clang 21.1.8) GREEN (1/1 ctest PASS). No new product symbols; this test
  file is permanent test infra (NOT a STUB / NOT MINIMAL REAL, recorded). Next = S008.
  GOTCHA: cannot declare a zero-length array in standard C++ (`int x[0]` ill-formed) — use a non-null
  pointer with len 0 to exercise the empty/len==0 path. GOTCHA: `ShouldBeNullOrEmpty` is "null OR
  empty", NOT "null AND empty" — my first pass-case wrongly assumed AND; the OR semantics is the
  correct original intent. GOTCHA: FP_ZERO is a preprocessor macro (math.h: `#define FP_ZERO 2`), NOT
  a std:: member; `std::FP_ZERO` is ill-formed everywhere (both MSVC C2589 and Clang error) — only
  fpclassify/isnormal take the std:: qualification. GOTCHA: MSVC accepted unqualified fpclassify/
  isnormal (they leak into global scope), so /W4 /WX was blind to the non-portable call — the Linux
  Clang lane is what surfaced it; keep the Clang lane authoritative for portability.
- [x] S008 REFACTOR GATE(S0): no dead code, headers internal-only, static-link default verified, clean PR — ✅
  SURVIVOR: DELETED build/verify_msvc.cmd (superseded S001-era raw `-G Ninja` + direct-flag harness,
  fully duplicated by the preset-driven verify_s004/s005/s005_shared/s007.cmd; was gitignored/untracked).
  MODIFIED cmake/artifact-model.cmake (S008): resolve VAML_ARTIFACT_MODEL (STATIC/SHARED) + `message(
  STATUS "VAML artifact model: ..." )` so the static-by-default is provable from configure output (no
  hard FATAL); added include-surface convention comment — internal impl headers live under
  src/<lib>/src, ONLY public API under src/<lib>/include/vaml/<lib> (write-rule ONLY, NO header moves
  in S0; Shouldly.h stays the public vaml::test facade; vulkan/tools keep MINIMAL REAL version.h seeds
  until S161/S287). MODIFIED docs/roadmap.md: S008 [DONE]; S001 note now cites preset verify scripts.
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN — configure prints
  "VAML artifact model: STATIC", static vaml-{core,vulkan,tools}.lib, ctest 1/1 PASS; REAL LINUX
  clang-debug on Ubuntu VM (clang 21.1.8) via /mnt/hgfs/VamlPOC GREEN — STATIC .a, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> deprecation from vendored
  third_party/doctest under GCC-15 libstdc++ (not VamlPOC source, not introduced here). Tracked
  change-set = cmake/artifact-model.cmake + docs/roadmap.md only (export.h dirty flags are pre-existing
  CRLF/LF autocrlf noise — zero content diff vs HEAD). NO new md files (directive). Sprint-0
  SURVIVOR/checkpoint wrap is S009's job; this entry is a minimal non-duplicative record. Next = S009.
  GOTCHA: drive the MSVC lane through build/verify_s*.cmd (clean PATH + vcvars64 -> VS-bundled
  cmake/ninja win over any Cygwin PATH); a stale msvc-debug CMakeCache containing a /cygdrive path
  must be wiped before reconfigure.
- [x] S009 SURVIVOR wrap (Sprint 0) — ✅
  SURVIVOR: this step is documentation-only (no build, per plan). Added the consolidated
  `## SPRINT 0 — SURVIVOR` block above (distills S000–S008 into the surviving-state picture a fresh
  LLM resumes from: green skeleton, 3 STATIC libs, ADR-011 artifact model, vendored doctest+vaml::test
  harness, MSVC+Linux Clang lanes both verified, MINIMAL REAL vulkan→S161 + tools→S287 unresolved,
  invariants intact). Regenerated `## CHECKPOINT` (last completed = S009; next = S030; PENDING none).
  Marked roadmap S009 `[DONE]`. No new md files (directive); no MINIMAL REAL introduced or resolved;
  NO-GAP GUARANTEE remains satisfied (both active seeds still carry explicit resolution tokens).
  Next = S030.

### Sprint 1 — Property system core   (S030–S049)
- [x] S030 Typeless Value — ✅ (see roadmap S030 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/{Color,Thickness,Value}.h` + `src/core/src/Value.cpp`.
  Color = uint8 RGBA struct; Thickness = float l,t,r,b struct — BOTH tagged
  `// [MINIMAL REAL] ... RESOLVED BY S089`. Value = `ValueType:uint8_t` enum (11 tags in roadmap
  order Null..UntypedPtr), hand-rolled tagged union (NOT std::variant), static factories for all 11
  payloads, `type()`/`isNull()`, template `get<T>()` throwing `vaml::ValueTypeMismatch` on tag/T
  mismatch, `operator==`, explicit copy/move/dtor for the std::string member.
  `ValueTypeOf<T>` trait maps payload type → accepted tags; `get<int32_t>` accepts BOTH Int32 and
  BrushId (BrushId is a tagged int32 scalar id). Test TU `test/unit/Value_test.cpp` (S030 scope:
  factory→tag, equality, copy/move). Build fixes: enum/Storage forward-ref order (MSVC); read<T>()
  noexcept mismatch from string-copy-throws (Clang) → primary+all specializations non-noexcept;
  dropped duplicate doctest main. VERIFIED msvc-debug (25/25 cases, 91/91 asserts) + clang-debug.
  NO-GAP: Color/Thickness resolution authored at same step (→S089). Next = S031.
- [x] S031 Value round-trip + type-mismatch-throws tests — ✅
  SURVIVOR: `test/unit/Value_roundtrip_test.cpp` (NEW; doctest, NO IMPLEMENT_WITH_MAIN — shares
  smoke_test's main). Complementary to S030's Value_test.cpp (left untouched). Scope:
  (A) ROUND-TRIP — every non-Null tag's payload round-trips through `get<T>()` EXACTLY and the
  re-factory `Value::X(v.get<P>()) == v` (operator== round-trip). String verified to round-trip by
  VALUE (copy), mutation of the read-back copy does not affect the stored value. BrushId/UntypedPtr
  included. (B) TYPE-MISMATCH-THROWS — Null has no payload so EVERY get<T> throws (gesture not
  corrupting: Null stays Null); each non-Null tag throws `ValueTypeMismatch` for every NON-valid
  payload type (exhaustive per-tag, incl. no cross-width/no-promotion: Int32 not int64/float/double,
  Bool not int32, Float not double, BrushId reads via int32_t ONLY but tag stays BrushId ≠ Int32 and
  BrushId(1) != Int32(1)); String mismatches every scalar and vice-versa; constraint — after a
  throw the SAME value still re-reads its valid payload and still equals its original (throw path is
  a read-only no-op on the union). CMakeLists (MOD): + Value_roundtrip_test.cpp to vaml-core-tests.
  build/verify_s031.cmd (NEW): clone of verify_s007 pattern (clean PATH + vcvars64 → msvc-debug).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) exe 33/33 cases, 237/237 asserts PASS (was
  25/91 at S030); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (not this TU).
  No MINIMAL REAL introduced/resolved; NO-GAP unchanged. Next = S032.
  GOTCHA: `get<T>()` is `[[nodiscard]]` (S030) — `CHECK_THROWS_AS(a.get<T>(), ...)` discards the
  return → MSVC C4834 (warnings-as-errors). Fix: wrap the expression `(void)a.get<T>()` inside
  CHECK_THROWS_AS; applied to all throw-paths.
- [x] S032 PropertyKey + PropertyRegistry — ✅ (see roadmap S032 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/{PropertyFlags,PropertyKey,PropertyRegistry}.h` +
  `src/core/src/{PropertyFlags,PropertyKey,PropertyRegistry}.cpp` + `test/unit/PropertyKeyRegistry_test.cpp`.
  All three are REAL public symbols (NOT MINIMAL REAL — no sentinel, no resolution token). PropertyFlags =
  enum class:uint32_t with None|Inherits(1<<0)|ReadOnly(1<<1)|BindsTwoWayByDefault(1<<2) — exactly the
  roadmap's S032/S034 flag set; free functions `operator|` + `has_flag` (independent-bit semantics),
  defined out-of-line in PropertyFlags.cpp. PropertyKey = immutable (name, type:ValueType, ownerTypeId:string,
  flags) tuple; getters; operator==/!= on all four fields; describe() renders a human-readable line with a
  stable type tag name (mirrors Value.cpp enum order). Design decisions recorded: owner-id is a std::string
  type-name (no cheap portable runtime type_of in C++; supports S043 attached-prop owner≠class; S033's
  constexpr-safe static id layers on without changing fields); unique on NAME globally (WPF-style), ownerTypeId
  stays identity metadata inside the key, not a second registry dimension. PropertyRegistry = instance-based,
  pimpl'd (class Impl behind unique_ptr hides std::unordered_map — ABI + include-surface stable); register_key
  (uniques by name; throws PropertyAlreadyRegistered:logic_error on collision, map unchanged, returns stored
  const ref), try_find (→ptr or nullptr, noexcept), find (→stored key or throws PropertyNotFound:out_of_range),
  size/empty. Exceptions declared in header, definitions in .cpp. A global static registry + constexpr-safe
  static id/lookup is an S033 concern, NOT baked here (recorded, not silent). Test TU covers: PropertyFlags
  OR+has_flag; PropertyKey getters + per-field equality + different ValueType tags (Int32/Thickness/String) +
  describe(); Registry register/find/try_find/size; dedupe — duplicate NAME throws PropertyAlreadyRegistered
  and leaves the map unchanged (size stable, original still findable); multiple distinct names coexist.
  CMakeLists: +3 core sources; test/unit +PropertyKeyRegistry_test.cpp. build/verify_s032.cmd (NEW): clone of
  verify_s031 pattern. VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) ctest 1/1 PASS (was blocked by discarded
  [[nodiscard]] register_key return → `(void)` cast fix); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8)
  build green, ctest 1/1 PASS. Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation.
  NO-GAP accounting: S033 = exhaustive register/lookup/dedupe-throws + constexpr-safe static id; S034 consumes
  flags + adds metadata (defaults/callbacks); S039/S043 consume owner-id + Inherits. Two deferred design
  decisions recorded (not silent): (a) global name-uniqueness scope, (b) global static registry instance →
  both S033. No MINIMAL REAL introduced/resolved. Next = S033.
- [x] S035 Metadata apply-semantics tests (default applied, cb-only-on-real-change, coerce applied, validate rejects) — ✅
  SURVIVOR: `test/unit/PropertyMetadata_semantics_test.cpp` (NEW — pure TEST step, NO product symbol).
  A doctest-scope `MiniStore<T>` harness models the Set-time apply pipeline that S036's DependencyObject
  must reproduce: construction seeds `current = defaultValue()` ("default applied"); `trySet(proposed)`
  runs coerce → validate(coerced) → fire-changed-cb-only-on-real-change → store, and returns
  APPLIED/REJECTED/NO_CHANGE. The harness records every cb fire + (old,new) so exact call behavior is
  assertable. Tests cover exactly S035's four title behaviors + ordering + passthrough, each in its own
  TEST_CASE: (1) **default applied** — fresh holder reads defaultValue() (7); setting a value equal to the
  default → NO_CHANGE, no cb. (2) **coerce applied** — negative raw →5 stored (coerced value is what's
  validated+stored), cb receives coerced new; positive passthrough stores as-given with correct (old,new).
  (3) **coerce-before-validate ordering** — raw -3 would FAIL `validate(v>=0)` as-is, but coerce runs FIRST
  turning it into 5, so validate(5) passes → APPLIED (default 0→5, cb (0,5)); proves validate sees the
  COERCED value. (4) **validate rejects** — rejected value → REJECTED, current unchanged, cb NOT fired
  (checked for two distinct rejections). (5) **cb fires ONLY on real change** — first set fires once;
  setting the SAME value repeatedly → NO_CHANGE, fireCount unchanged; N distinct values → N fires each with
  correct (old,new); reverting to a previously-held value still fires (it IS a real change). (6) **absent
  callbacks = passthrough** — no coerce/validate/changed → stores as-given, one fire on first set, NO_CHANGE
  on repeat. (7) harness stays **type-bound** — a wrong-tag proposed value throws vaml::ValueTypeMismatch
  through the pipeline before any accept decision, store untouched. The MiniStore + test file are PERMANENT
  TEST INFRASTRUCTURE (same class as the vendored doctest/Shouldly harness), NOT a STUB and NOT MINIMAL
  REAL — no sentinel, never shipped, because the real value-holder is S036's DependencyObject (recorded,
  not silent; NO-GAP intact: the feature is already scheduled at S036). CMakeLists (MOD): test/unit
  +PropertyMetadata_semantics_test.cpp to vaml-core-tests. build/verify_s035.cmd (NEW): verify_s034-pattern
  msvc-debug lane (clean PATH → vcvars64 → msvc-debug configure+build+ctest). VERIFIED: msvc-debug (VS 2026)
  ctest 1/1 PASS (66 cases total incl. S034/S033...; this TU adds 7). One in-development test-bookkeeping
  fix: the first ordering draft coerced -5→0 while the default was already 0 (would be NO_CHANGE, not
  APPLIED) — corrected to coerce -3→5 against default 0 so the ordering proof is a real change (fixed
  pre-build, not a shipped defect). DESIGN CONTRACT hand-off to S036 (recorded searchable in roadmap +
  progress): DependencyObject::Set MUST reproduce coerce → validate(on coerced) → fire changed cb only on
  real change → store; `PropertyMetadata_semantics_test.cpp::MiniStore` is the reference spec.
  Roadmap S035 marked [DONE]; S036 line annotated with the S035→S036 apply-pipeline contract.
  No MINIMAL REAL introduced/resolved. Next = S036.

- [x] S034 PropertyMetadata { defaultValue; flags; changed cb; coerce; validate } — ✅
  SURVIVOR: `src/core/include/vaml/core/PropertyMetadata.h` (NEW) + `src/core/src/PropertyMetadata.cpp`
  (NEW) + `test/unit/PropertyMetadata_test.cpp` (NEW). Move-only (copy deleted, proven by static_assert)
  metadata bundle under an Impl pimpl (hides std::function ABI + enforces move-only at the public
  boundary). Field surface mirrors the roadmap exactly: defaultValue (Value) fixes BOTH S039's fallback
  AND the metadata's bound type = defaultValue.type(); flags (PropertyFlags recomposes S032's
  inherits|readonly|bindsTwoWayByDefault as a validating carrier, never a competing source of truth);
  changed cb (old,new); coerce Value(Value); validate Value->bool. Construction: canonical factory
  `WithDefault(Value, PropertyFlags)` (REQUIRED — no default-constructible empty metadata), then
  &&-qualified return-by-value builder hooks `with_coerce` / `with_validate` / `with_changed` (each
  installs one callback and returns a NEW metadata, leaving the source untouched). Passthrough defaults:
  absent coerce=identity, absent validate=always-accept, absent changed=no-op (branch-free common path
  for S036). TYPE-BOUND: coerce/validate/notifyChanged throw vaml::ValueTypeMismatch (S030 contract) when
  passed a Value whose tag != boundType — NO-LAZY guard against a callback receiving a wrong-type Value.
  Test TU: move-only static_asserts + move ctor/assign; WithDefault seeds default+bound type+flags;
  flags carrier round-trips each of the 3 bits; coerce absent=identity / present=transforms; validate
  absent=accept / present=accept+reject; changed cb no-op when absent / fires (old,new) when installed;
  single builder chain composes all three hooks; wrong-tag throws from all three entry points + correct
  tag still passes post-throw (throw path is read-only); builder return-by-value leaves source untouched.
  CMakeLists (MOD): core +PropertyMetadata.cpp; test/unit +PropertyMetadata_test.cpp.
  build/verify_s034.cmd (NEW): verify_s033-pattern msvc-debug lane. VERIFIED BOTH COMPILERS: msvc-debug
  (VS 2026) ctest 1/1 PASS; REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (degraded by design, not this
  TU). NO-GAP accounting: S034 delivers the metadata TYPE + its own unit semantics; the actual
  coercion/validation/changed INVOCATION at Set-time and the global static registry +
  DependencyProperty::Register LANDING SITE are S036's concern (S036 = DependencyObject with a local
  value store that needs a registered DP) — recorded non-silently, deliberately resolving the S032(b)/S033
  recorded deferral. S035 tests every field (default applied, cb-only-on-real-change, coerce applied,
  validate rejects). No MINIMAL REAL introduced/resolved. Next = S035.

- [x] S033 Registry exhaustive tests + constexpr-safe static id — ✅ (see roadmap S033 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/PropertyId.h` (NEW — header-only constexpr-safe static id).
  PropertyId wraps the property NAME as a `std::string_view` bound to a string literal: `constexpr`-safe
  (fully compile-time, proven by `static_assert` in tests), collision-free by construction (carries the
  name, NOT a hash → no collision class, no cross-TU nondeterminism, no static-init-order fiasco), stable +
  interconvertible with the registry's name-keyed store. API: `of(sv)`, `name() constexpr`, `valid()`,
  `constexpr ==/!=`, `std::hash<PropertyId>` specialization (feeds S036's local-value store as map key).
  Deliberate name-token-over-hash choice recorded (a compile-time hash also compiles but reintroduces a
  collision class + opaque proof). NOT a STUB / NOT MINIMAL REAL — complete on delivery. Lifetime note:
  owner must pass a string literal (wrapping a temp std::string is a lifetime bug), mirroring the registry
  name contract. `PropertyRegistry` (MOD PropertyRegistry.h/.cpp): + `id_of(name)` (→PropertyId, throws
  PropertyNotFound if unregistered) + `try_id(name) const noexcept` (→id or invalid empty id) — the
  runtime→compile-time bridge = the canonical key S036's local-value store uses. Test TUs (NEW):
  `test/unit/PropertyId_test.cpp` (static_assert compile-time proof; equality by name; default-id invalid;
  std::hash consistent with equality; id_of/try_id round the name-keyed store + unregistered-name path)
  and `test/unit/PropertyRegistryExhaustive_test.cpp` (exhaustive bulk register across types/flags/owners +
  exact lookup; all four duplicate-throws rejection forms — diff type / diff owner / diff flags / identical
  key — each leaves the registry unchanged: size, stored-pointer identity, and id stable; missing-lookup on
  empty + non-empty; move-construct/move-assign preserve mappings + dedupe). CMakeLists (MOD): +2 test TUs.
  build/verify_s033.cmd (NEW): verify_s032-pattern msvc-debug lane. VERIFIED BOTH COMPILERS: msvc-debug
  (VS 2026) ctest 1/1 PASS; REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation. GOTCHA: doctest stringifies
  CHECK operands via ostream — comparing a raw `std::string_view` operand fails to compile on MSVC (undefined
  `basic_ostream`) unless wrapped (e.g. `std::string(id.name())`); check string_view-wide values via
  `.size()`/`.empty()` or convert. DESIGN re-deferral (S032 item (b)): global static registry instance NOT
  added in S033 — no consuming static-Register site exists until S034's real `DependencyProperty::Register`;
  recorded non-silently as an explicit S034 concern, resolving the S032 recording. NO-GAP: PropertyId is
  real + fully tested (no sentinel); nothing left for silent backfill; deferral searchable in roadmap+progress.
  No MINIMAL REAL introduced/resolved. Next = S034.
 
- [x] S036 DependencyObject base: local value store, Get/Set, per-prop mastery, thread-affinity marker. — ✅
  SURVIVOR: `src/core/include/vaml/core/DependencyObject.h` (NEW) + `src/core/src/DependencyObject.cpp`
  (NEW) + `test/unit/DependencyObject_test.cpp` (NEW) + `build/verify_s036.cmd` (NEW, clone of
  verify_s035-pattern msvc-debug lane). REAL public symbol `vaml::DependencyObject` (NOT MINIMAL REAL,
  no sentinel — every member is genuine permanent surface with scheduled consumers). API: (a)
  `register_property(PropertyKey, PropertyMetadata)` → returns const ref to STORED key; installs
  (metadata, defaultValue) keyed by the property NAME; name dedupe throws `PropertyAlreadyRegistered`
  leaving store unchanged; type-coherence guard key.type()≠metadata.boundType() → `ValueTypeMismatch`.
  (b) `Get(PropertyId)` → Value BY VALUE (no ref-invalidation hazard for S037 marshalling/S039
  effective-value); unregistered id throws `PropertyNotFound`; absent local = "default applied" → falls
  back to metadata.defaultValue(). (c) `Set(PropertyId, const Value&)` → `SetResult::{APPLIED,REJECTED,
  NO_CHANGE}` reproducing the S035 pipeline VERBATIM: coerce → validate(on the COERCED value) → fire
  changed cb (oldEffective, coerced) ONLY on a real change → store; oldEffective = local-override-else-
  default; wrong-tag input throws `ValueTypeMismatch`. (d) thread-affinity marker: ctor records
  `std::this_thread::get_id()`; exposes `thread_affinity_id()` + `is_affine_to_current_thread()` (real,
  consumed by S037 wrong-thread detection). Storage pimpl'd (class Impl + unique_ptr → header exposes no
  unordered_map / no move-only PropertyMetadata). GOTCHA (recorded): the internal store keys on an OWNED
  std::string (copied name), NOT on `PropertyId` — a PropertyId is a non-owning std::string_view that
  dangles as soon as the registering PropertyKey (its storage source) dies; SSO+heap names both failed
  until the owned-string key. This is why Get/Set re-derive `std::string(id.name())` for lookup. Tests
  (doctest, 11 S036 TEST_CASEs): default-applied; coerce applied (coerced value validated+stored, cb sees
  coerced new); coerce-BEFORE-validate ordering; validate rejects (store unchanged, no cb); cb-fires-ONLY-
  on-real-change (same-value → NO_CHANGE, distinct → fires once each); passthrough; type-bound wrong-tag
  throws (store untouched); per-prop mastery (two props coexist independently); dedupe throws-unchanged
  (local override preserved); Get-by-value (string copies independent); thread-affinity (affine on
  creating thread; a worker-created object records the worker id ≠ this thread). CMakeLists (MOD): core
  +`src/DependencyObject.cpp`; test/unit +`DependencyObject_test.cpp`. GOTCHA: `Value` has NO public
  default ctor → the `PropEntry::local` member initializes to `Value::Null()` (not `{}`). GOTCHA: `Set`
  is `[[nodiscard]]` by design (result must be read) → tests that discard it cast to `(void)` to satisfy
  MSVC C4834 under /WX. DESIGN re-scope (recorded, NOT silent): the global static registry +
  `DependencyProperty::Register` landing site (S034 recorded it as "S036's concern") is authored at S039
  instead — S036 delivers OBJECT-SCOPED registration only, because a global static registry in S036 would
  be speculative surface with no consumer until S039's effective-value resolution; roadmap S039 line
  carries this response note + S037 line annotated with its S036 thread-affinity consumer contract.
  VERIFIED: msvc-debug (VS 2026) via verify_s036.cmd — configure/build warning-clean, ctest 1/1 PASS
  (100% tests passed; 79 total cases / 529 asserts across the suite incl. 11 new S036 cases). The
  windows-clang-debug preset was attempted but fails on EVERY TU (incl. untouched version.cpp) on a
  pre-existing `/Zc:preprocessor`-unused clang-cl incompatibility — a cross-cutting preset issue wholly
  out of S036 scope, NOT introduced here; the maintained verify lane for this step is msvc-debug (green).
  No MINIMAL REAL introduced/resolved. Next = S037.
 
- [x] S036_1 Shouldly routing reform: predicates keep the S006 default-sink THROW contract; new
  doctest-aware macro seam `SHOULD`/`SHOULD_REQUIRE` routes failures into doctest's ResultBuilder.
  — ✅
  SURVIVOR: `src/core/include/vaml/assert/Shouldly.h` (MOD) + `test/unit/shouldly_test.cpp` (MOD).
  DESIGN/SEAM (the genuinely hard part, resolved): a fluent Shouldly predicate call
  (`ShouldlyIntegral(x).ShouldBe(y)`) cannot by itself terminate inside doctest — doctest's
  `detail::ResultBuilder` is only valid while a test is running (it dereferences g_cs) and a fluent
  call evaluated inside an expression has no scoped ResultBuilder to hand its result to. So predicates
  were NOT rewritten; they keep throwing `vaml::test::TestFailureException` on failure (the S006
  default-sink contract). The integration lives in a MACRO that opens the RB inside a test context:
  `SHOULD( ShouldlyIntegral(Get()).ShouldBe(3) )` -> opens RB(file,line,#expr), runs the predicate,
  catches the thrown TestFailureException, routes its message into rb.m_decomp + rb.m_failed=true,
  then rb.log()/rb.react() (log() feeds doctest counters/summary, react() honors severity). Severity
  explicit + correct: `SHOULD` = DT_CHECK (continue-on-failure), `SHOULD_REQUIRE` = DT_REQUIRE (abort
  case). Core stays doctest-free: the macros are gated `#ifdef DOCTEST_LIBRARY_INCLUDED`, so any non-
  test TU gets the canonical DSL with no doctest dependency; test TUs include <doctest/doctest.h>
  before <vaml/assert/Shouldly.h>. FAILURE-PROOF note: because predicates still throw, a failing
  predicate can be asserted bare via `CHECK_THROWS_AS(..., vaml::test::TestFailureException)` — no
  sink capture needed. shouldly_test.cpp (MOD): KEPT the default-sink-throws contract test and now
  documents it as the exact contract SHOULD consumes; ADDED SHOULD success-path cases (integral decay
  great/etc, string ShouldBe/ShouldNotBe, pointer ShouldBeNull/ShouldNotBeNull), a string+pointer
  success case, and a `should_fail()` decorated demonstration proving a FAILING SHOULD is swallowed
  and routed into doctest (had it rethrown TestFailureException it would surface as a test-case
  exception/crash instead of a normal AssertFailure -> suite would fail, not pass). The golden rule
  that forced S036_1 to also rewrite shouldly_test.cpp in the SAME step: the reform invalidates the
  old throw-contract assertions, so deferring the rewrite would leave an unbuildable suite window.
  GOTCHA (fixed at build): `TEST_CASE("...") * doctest::should_fail()` must place the decorator INSIDE
  the TEST_CASE argument list, not on the following line (MSVC C2143/C2063 — decorator is a chained
  `*` onto the macro arg). GOTCHA: this was ON-STEP (S036_1 only); feature-test files are NOT
  migrated here — that is S036_4/S036_5 by plan. VERIFIED: msvc-debug (VS 2026) configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed). The maintained verify lane for this step is
  msvc-debug (green); windows-clang-debug preset remains broken on EVERY TU by the pre-existing
  /Zc:preprocessor clang-cl incompatibility (cross-cutting, out of scope, NOT introduced here). No
  MINIMAL REAL introduced/resolved. Next = S036_2 (StringMaker domain exposure).
 
- [x] S036_2 StringMaker domain exposure: doctest::StringMaker specializations for VAML core types so
  failed assertions stringify as readable source-level text. — ✅
  SURVIVOR: `test/unit/vaml_test_stringmakers.h` (NEW, TEST-ONLY header) + `test/unit/StringMaker_test.cpp`
  (NEW) + `test/unit/CMakeLists.txt` (MOD +StringMaker_test.cpp). SPECIALIZATIONS (namespace doctest):
  `StringMaker<vaml::Value>` -> tag-first "<Tag payload>" for ALL 11 payload kinds: Null "null", Bool
  "true"/"false", Int32/Int64/Float/Double scalar, String "\"..\"", Color "Color", Thickness "Thickness",
  BrushId scalar, UntypedPtr address; helper `detail::vaml_value_type_name(ValueType)` maps tag->name.
  `StringMaker<vaml::SetResult>` -> "SetResult::APPLIED"/"REJECTED"/"NO_CHANGE". `StringMaker<vaml::PropertyId>` ->
  "PropertyId(Width)" or "PropertyId(empty)". WHY test-side header (layering): these live in
  `namespace doctest`, so they CANNOT live in core — core must stay doctest-free (S036 layering rule);
  the header is TEST-ONLY and each test TU includes it. GOTCHA (doctest internals, two build fixes):
  `doctest::String` has a `const char*` ctor but NO `std::string` ctor and NO `operator+(const char*)` —
  conversions must go through `.c_str()` into a `const char*`; `<sstream>` must be included explicitly
  (ostringstream is not transitively available). GOTCHA (link, fixed): StringMaker_test.cpp must NOT
  define `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` (only smoke_test.cpp owns the main) — doing so causes
  ~200 LNK2005 multiply-defined-symbol failures (main + every doctest global). SYSTEM benefit: doctest
  auto-uses `doctest::StringMaker` when stringifying a failed expression's operands, so ANY later
  `CHECK(a == b)` (or SHOULD-equivalent) on Value/SetResult/PropertyId renders readable output
  automatically — the S036_2 payoff is system-wide, not confined to StringMaker_test. This is the
  framework-native "source-tool readability scaffold" the re-formulated Shouldly-for-doctest needs for
  raw stringified fallback. VERIFIED: msvc-debug (VS 2026) configure/build warning-clean, ctest 1/1
  PASS (100% tests passed; 3 new S036_2 cases). Maintained verify lane msvc-debug (green); windows-clang
  -debug preset remains broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl incompatibility,
  cross-cutting, out of scope, NOT introduced here). No MINIMAL REAL introduced/resolved. Next =
  S036_3 (ShouldThrow<Ex> typed-exception assertion category).
 
- [x] S036_3 ShouldThrow<Ex> typed-exception assertion category — first-class replacement for
  CHECK_THROWS_AS/_WITH/_WITH_AS that OUT-EXPOSES doctest (reports actual thrown type + message). — ✅
  SURVIVOR: `src/core/include/vaml/assert/Shouldly.h` (MOD) + `test/unit/shouldly_test.cpp` (MOD).
  API (namespace vaml::test, template resolution): `ShouldThrow<Ex>(Fn&&)` — asserts fn() throws Ex;
  `ShouldThrow<Ex>(Fn&&, std::string const&)` and `ShouldThrow<Ex>(Fn&&, char const*)` — message-
  matcher overloads (Ex.what() must equal the expected string). Tail `static_assert(is_base_of<std::
  exception,Ex>)` requires a std::exception-derived Ex (so a message exists). FAILURE MODES (through
  the SAME pluggable sink as every predicate -> composes with SHOULD(...) seam): (1) nothing thrown ->
  "expected <Ex> but nothing was thrown"; (2) wrong std::exception type -> "expected <Ex> but got
  <typeid(e).name()> '<e.what()>'" (ACTUAL type + message surfaced — the ShouldThrow value-add over
  CHECK_THROWS_AS, which cannot report the actual type at all); (3) right type + wrong message ->
  "<Ex> with message '<expected>' but message was '<actual>'"; non-std throws -> "<Ex> but a non-std::
  exception of unknown type was thrown". DESIGN/FIX (three builds): the Ex catch is IDENTICAL to
  catch(std::exception) when Ex==std::exception -> MSVC C2312 duplicate-handler. Resolved with
  `if constexpr(is_same<Ex,std::exception>)` that emits TWO handlers (catch(std::exception) passes,
  catch(...) fails) vs THREE handlers (Ex / std::exception-with-typeinfo / catch(...)) for derived Ex.
  The message-matcher overload adds `static_assert(!is_same<Ex,std::exception>)` (ambiguous semantics).
  Any non-returning path falls to the trailing "nothing was thrown" report (a GOTCHA: an earlier draft
  dropped it under catch(...) for derived Ex, silently losing the wrong-type report + failing one test).
  Tests (6 new S036_3 TEST_CASEs, using vaml::test::ShouldThrow + name-scoped
  DomainError/throwDomainError/throwLogicError/throwNothing fixtures + <stdexcept>): pass on right
  type; pass on base-Ex catching a derived throw (ShouldThrow<std::exception>(throwDomainError) via
  the if constexpr two-handler branch); fail on nothing-thrown; fail on wrong type routes ACTUAL
  message ("logic boom"); pass on message match (std::string + const char* overloads); fail on wrong
  message routes ACTUAL message. THIS STEP proves the category compiles + behaves; the suite-wide
  CHECK_THROWS_AS->ShouldThrow migration is S036_4/S036_5 (NOT here, per plan). VERIFIED: msvc-debug
  (VS 2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed, 90 cases / 558 asserts).
  Maintained verify lane msvc-debug (green); windows-clang-debug preset remains broken on EVERY TU
  (pre-existing /Zc:preprocessor clang-cl incompatibility, cross-cutting, out of scope, NOT introduced
  here). No MINIMAL REAL introduced/resolved. Next = S036_4 (migrate feature tests).
 
- [x] S036_4 Migrate Value / roundtrip / PropertyId / PropertyKeyRegistry / PropertyRegistryExhaustive /
  PropertyMetadata feature tests from CHECK* to the Shouldly-for-doctest vocabulary. — ✅
  SURVIVOR: `test/unit/vaml_test_stringmakers.h` (MOD, +operator<< overloads) + the six migrated
  test files (Value_test, Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test,
  PropertyRegistryExhaustive_test, PropertyMetadata_test) all rewritten to the Shouldly vocabulary.
  PRE-REQ (the IMPLEMENT BLOCKER surfaced + user ACCEPTED): the plan's literal "migrate CHECK== to
  SHOULD(Shouldly...)" is NOT buildable without formatter support — Shouldly's stream-path formatter
  (ShouldlyStr -> ThrowAwesomelyStr) renders a failure via `std::stringstream << actual`, and the
  domain types (Value/ValueType/Color/Thickness/SetResult/PropertyId) had no operator<<. Resolution:
  added test-side `operator<<` overloads in `vaml_test_stringmakers.h` (namespace vaml so ADL finds
  them) reusing the exact S036_2 readable-text intent ("<Int32 42>", "SetResult::APPLIED",
  "PropertyId(Width)", "RGBA(...)", "Insets(...)"); +`operator<<(ostream, PropertyKey)` delegating to
  `k.describe()` (PropertyKey had none). Kept TEST-SIDE (not core): Value/Color/Thickness headers stay
  lean + S089 Color/Thickness are MINIMAL REAL ("do NOT extend silently") — the formatting surface is
  deliberately out of production headers. MIGRATION PATTERN (uniform across all 6 files): equality
  `CHECK(a==b)` -> `SHOULD(ShouldlyString(a).ShouldBe(b))`; inequality `CHECK_FALSE(a)`/`CHECK(x!=y)`
  -> `SHOULD(ShouldlyString(x).ShouldNotBe(y))` / bool -> `ShouldNotBe(true)`; flag checks
  `CHECK(has_flag(...))` -> `SHOULD(ShouldlyString(has_flag(...)).ShouldBe(true))`; typed-exception
  `CHECK_THROWS_AS((void)f(),Ex)` -> `SHOULD(ShouldThrow<Ex>([&]{ (void)f(); }))`; missing-string
  contains-checks -> `ShouldlyString(d.find("X")!=npos).ShouldBe(true)`. SAMCTIONED non-migrated
  survivors (deliberate, documented in each file): (a) 3 REQUIRE precondition guards (null-deref
  gates BEFORE dereferencing a try_find pointer in PropertyKeyRegistry_test x1 + PropertyRegistry
  Exhaustive_test x2) — these MUST abort the case on null, so REQUIRE is the correct severity;
  (b) StringMaker_test.cpp's CHECKs — they assert the raw doctest::String text produced by the S036_2
  StringMaker itself (an infrastructure test of the stringifier; migrating it to Shouldly would change
  the very output under test). smoke_test.cpp (the doctest runner/main) untouched. PROCESS (the user's
  option-B restart, honored): the FIRST S036_4 attempt used regex bulk-edits on Value_roundtrip_test
  .cpp and was discovered unverified + rolled back (git checkout) at user direction; this completed
  run is the CONSERVATIVE approach — one file at a time, full read then full rewrite (not regex
  surgery), each verified by a build gate; PropertyId_test.cpp (22 sites, simplest types) migrated
  FIRST as the proven template before scaling. GOTCHA (build-fixed): operator<<(Color/Thickness) must
  be DEFINED ABOVE operator<<(Value) in the header because operator<<(Value) calls v.get<Color>() /
  v.get<Thickness>() (C3861 referencing a later definition) — reordered so Color/Thickness precede
  Value. GOTCHA: ShouldlyStr<T> requires actual + expected to be the SAME type T (both T const&), so
  `ShouldlyString(std::string(x)).ShouldBe(std::string("..."))` (NOT a const char* rhs), and pointer
  null-checks became `ShouldlyString(p == nullptr).ShouldBe(true)` / `!= nullptr` rather than
  ShouldNotBe(nullptr) (nullptr_t vs T* mismatch). Status of migration target (S036_4 goal): ZERO
  CHECK* remain in the 6 feature files except the sanctioned REQUIRE guards; no CHECK_THROWS_AS /
  CHECK_NOTHROW / CHECK_THROWS_WITH anywhere in the migrated files; verbose failure info now carried
  by the ShouldThrow<Ex> (actual-type+message) + ShouldlyString predicates. VERIFIED: msvc-debug (VS
  2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed) after EACH file and after
  the final gate. Maintained verify lane msvc-debug (green); windows-clang-debug preset remains
  broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl incompatibility, cross-cutting, out of
  scope, NOT introduced here). No MINIMAL REAL introduced/resolved. Next = S036_5 (migrate
  DependencyObject_test.cpp — the S036 proof — as the capstone + final gate).


- [x] S036_5 Migrate `DependencyObject_test.cpp` (the S036 proof) as the capstone + final gate of the
  Shouldly-for-doctest convention effort. — ✅ (S036_1..S036_5 PLAN COMPLETE)
  SURVIVOR: `test/unit/DependencyObject_test.cpp` (MOD, full rewrite) + `docs/roadmap.md` (MOD,
  S036_1..S036_5 all [DONE]) + `session-context.md` (MOD). MIGRATION: all 11 S036 TEST_CASEs converted
  with zero semantic change — default-applied -> `SHOULD(ShouldlyString(obj.Get(...)).ShouldBe(Value::
  Int32(7)))`; SetResult outcomes -> `SHOULD(ShouldlyString(obj.Set(...)).ShouldBe(SetResult::APPLIED/
  NO_CHANGE/REJECTED))`; fire-count / oldV / newV -> `ShouldlyString(fire).ShouldBe(n)`; type-coherence
  + PropertyNotFound + value-type-mismatch + dedupe throws -> `SHOULD(ShouldThrow<Ex>([&]{...}))`;
  thread-affinity -> `SHOULD(ShouldlyString(bool).ShouldBe(true))` (is_affine_to_current_thread /
  affinity==current / !=default / worker-records-other-thread). Includes wired: vaml_test_stringmakers.h
  + <vaml/assert/Shouldly.h>; `using vaml::test::ShouldThrow`. FINAL GATE (the S036_5 contract): zero
  CHECK*/CHECK_THROWS_AS across all 7 migrated feature files (Value_test, Value_roundtrip_test,
  PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test, PropertyMetadata_test,
  DependencyObject_test) — verified by grep (all 0). SANCTIONED non-migrated asserts (deliberate,
  documented in files + roadmap S036_5 [DONE]): (1) PropertyMetadata_semantics_test.cpp = the S035
  reference SPEC harness, intentionally NOT migrated — it IS the spec S036 must satisfy, rewriting it
  would alter the exact contract under test (its single CHECK_THROWS_AS stays); this is the one place
  where "no CHECK_THROWS_AS remains" is intentionally carved out, and it is recorded, NOT silent.
  (2) smoke_test.cpp = the doctest runner/main harness. (3) StringMaker_test.cpp = asserts the raw
  doctest::String output of the S036_2 StringMaker itself (infrastructure test of the stringifier,
  not a vaml domain assertion). (4) 3 REQUIRE precondition guards (null-deref gates). S047 annotation:
  "every numbered predicate via adapted Shouldly" is now the standing vocabulary from S036_5 onward —
  S047 remains the formal suite-complete gate against the established vocabulary. VERIFIED: msvc-debug
  (VS 2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed) after the capstone rewrite
  and at the final gate. Maintained verify lane msvc-debug (green); windows-clang-debug preset remains
  broken on EVERY TU by the pre-existing /Zc:preprocessor clang-cl incompatibility (cross-cutting, out
  of scope, not introduced here) — full dual-compiler verification of the S036_* suite is deferred to
  S041+ gates / an explicit later dual-compiler run (recorded, not silent). No MINIMAL REAL introduced
  or resolved. Next = S037.

- [x] S038 ChangeRecorder: pending per-property changes for deferred notification — ✅
  SURVIVOR: `src/core/include/vaml/core/ChangeRecorder.h` (NEW) + `src/core/src/ChangeRecorder.cpp`
  (NEW) + `test/unit/ChangeRecorder_test.cpp` (NEW) + `build/verify_s038.cmd` (NEW, clone of
  verify_s037-pattern msvc-debug lane) + `src/core/CMakeLists.txt` (MOD +ChangeRecorder.cpp) +
  `test/unit/CMakeLists.txt` (MOD +ChangeRecorder_test.cpp) + `docs/roadmap.md` (MOD S038 [DONE],
  S042 REALIZED-EARLY annotation). REAL public symbol `vaml::ChangeRecorder` (NOT MINIMAL REAL, NOT a
  stub — complete, fully-tested, with an explicitly scheduled consumer S041; recorded NO-GAP). Pimpl'd
  (Impl + unique_ptr, move-only copy-deleted) so the header exposes no std::unordered_map / std::function
  value. API: `record(PropertyId, Value old, Value new)` — pending per-property notification; a repeat
  for the SAME property COALESCES (first `old` kept, latest `new` adopted, one notification per property);
  `has_pending(id)`; `pending_count()` (distinct-property depth); `flush(NotifyFn)` — emit one
  notify(id,old,new) per pending property in FIRST-RECORD order, then clear, return the emit count;
  `clear()`. DESIGN (recorded in header + roadmap, NO LAZY LLM): DELIBERATELY SINGLE-THREADED (a
  pipeline-internal deferral device — the cross-thread ChangeQueue/mutex is S041's job, NOT baked here);
  S038 stays out of S041's scope and does NOT rewire DependencyObject's synchronous callbacks (would
  break the S037 marshalling tests, which is S041's work). COALESCING realized-early (S042 outcome
  delivered at the primitive here; S042 annotated REALIZED-EARLY per the S036_1..S036_5 → S047 precedent
  and remains the formal end-to-end integration gate). Owned-name gotcha honored (S036/S037): records key
  on an OWNED std::string copy of PropertyId::name(), never the non-owning string_view; insertion-ordered
  vector+name-map (never unordered_map iteration order) → stable first-record flush order. flush is
  snapshot-then-emit (a notify handler that re-record()s lands in a fresh recorder, NOT double-emitted
  this pass; mirrors S037's swap-out). Cross-thread contract unchanged (S037 owns the mutex queue). Tests
  (doctest, Shouldly vocabulary, 8 TEST_CASEs): record — pending_count increments, has_pending true,
  distinct props coexist; per-property coalescing — two records same prop => 1 pending, first old (1) +
  latest new (8); three records same prop => earliest old (0) + newest new (3); flush order — once per
  property in first-record order (Width/Height/Depth) with correct (old,new), returns count, recorder
  cleared, 2nd flush = 0; record-during-flush — re-record lands in fresh state (1 emit this pass) then
  flushes next call; clear — drops all, flush after clear = 0; empty flush — 0, callback invoked for
  nothing; owned-name — record survives the registering key's death + same-name value coalesces.
  VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s038.cmd — configure/build warning-clean,
  ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder /mnt/hgfs/VamlPOC)
  configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646> doctest deprecation warning
  from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). No MINIMAL REAL introduced/resolved (nothing to backfill; ChangeRecorder's S041
  consumer is S041's own step). No NEW md files (directive honored). Next = S039.

- [x] S039 Effective-value resolution: local ?? inherited-from-parent ?? default; inherit-only flags — ✅
  SURVIVOR: `src/core/include/vaml/core/DependencyProperty.h` (NEW) + `src/core/src/DependencyProperty.cpp`
  (NEW) + `src/core/include/vaml/core/PropertyRegistry.h` (MOD +global()) +
  `src/core/src/PropertyRegistry.cpp` (MOD +global() Meyers singleton) +
  `src/core/include/vaml/core/PropertyFlags.h` (MOD +InheritOnly=1<<3) +
  `src/core/src/PropertyKey.cpp` (MOD describe() +InheritOnly) +
  `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD:
  GetEffectiveValue + parent link + Get delegates) + `test/unit/DependencyProperty_test.cpp` (NEW) +
  `test/unit/EffectiveValue_test.cpp` (NEW) + `build/verify_s039.cmd` (NEW, verify_s038-pattern
  msvc-debug lane) + src/core/CMakeLists.txt + test/unit/CMakeLists.txt (MOD) + docs/roadmap.md (MOD
  S039 [DONE]) + session-context.md (MOD). AUTHORS the recorded S036 RE-SCOPE: the global static
  registry + `DependencyProperty::Register` landing site (S034-recorded, previously "S036's concern").
  New REAL public symbol `vaml::DependencyProperty` (NOT MINIMAL REAL / NOT a stub — fully implemented,
  tested, first real consumer is S039's own effective-value resolution): pimpl'd (class Impl +
  unique_ptr), `Register(std::string_view name, ValueType type, std::string_view ownerTypeId,
  PropertyMetadata)` inserts a PropertyKey into the new `PropertyRegistry::global()` (static-local
  Meyers singleton, thread-safe init, name-unique → PropertyAlreadyRegistered on duplicate, store
  unchanged) + a companion function-local-static table holding the full (key, metadata) definition
  incl. flags (Inherits / InheritOnly) + default; `TryGet(name)` (null) / `Get(name)` (throws
  PropertyNotFound); `property_id()` (wraps the OWNED name → lifetime-safe view),
  `metadata()` / `flags()`. Type coherence guard: Register() throws ValueTypeMismatch when
  type != metadata.boundType(). `PropertyFlags` gains `InheritOnly = 1 << 3` (independent bit;
  meaningful only with Inherits) + describe() lists it. `DependencyObject::GetEffectiveValue(PropertyId)`
  (READ-SIDE pull model; the real S039 primitive): NOT Inherits → local ?? default (unchanged S036
  semantics); Inherits (no InheritOnly) → local ?? inherited-from-parent ?? default; InheritOnly →
  inherited-from-parent ?? default (own local override does NOT contribute). Inheritance descriptor
  (flags+default) sourced from the GLOBAL DependencyProperty where globally registered, falling back
  to the object's own metadata for object-scoped S036 registration; local value always from the
  object's per-object store. `Get` delegates to GetEffectiveValue (backward compatible — no existing
  test uses Inherits/InheritOnly). Internal `nearest_ancestor_local` is a private static member
  (same-class access to pimpl) — a straight upward scan for the nearest ancestor with a LOCAL
  override, which is exactly equivalent to recursing each ancestor's effective value (an intermediate
  ancestor with no local defers to ITS ancestor) without the throw hazard; TOLERANT by contract (an
  ancestor that does not register the property, or has no local, contributes nothing — never throws);
  public GetEffectiveValue still throws PropertyNotFound for an unregistered id. Added single
  non-owning `set_parent(DependencyObject*)/parent()` link (read-side seed, NOT lazy, NO-GAP: S040
  inheritance cascade + S120 UIElement/visual tree consume it; setting it triggers NO cascade — write/
  cascade side is S040, recorded NOT silent). Tests (doctest, Shouldly vocabulary):
  DependencyProperty_test.cpp (5 TEST_CASEs): Register installs globally + TryGet/Get retrieve it +
  property_id/flags/default round-trip; name-unique re-register throws PropertyAlreadyRegistered,
  original kept; type-coherence throws ValueTypeMismatch, nothing stored; TryGet null / Get throws
  PropertyNotFound for unregistered; flags() round-trips Inherits|InheritOnly.
  EffectiveValue_test.cpp (8 TEST_CASEs): non-inheriting local ?? default (Get delegates);
  plain Inherits 1-level child inherits parent's local; multi-level inherits great-grandparent;
  nearest-ancestor-with-value (intermediate empty local defers to IT ancestor); InheritOnly ignores
  OWN local override + still inherits from chain; InheritOnly with no ancestor → default;
  empty parent chain → default; GetEffectiveValue throws PropertyNotFound for unregistered. GOTCHA
  (fixed pre-build): DependencyProperty::Register is GLOBALLY name-unique, so the test helper
  registers each inherited property's global descriptor EXACTLY ONCE (tracked via a suite-local
  static set) and only mirrors onto each object's own store — repeated hostInherited() across
  objects no longer re-throws PropertyAlreadyRegistered. GOTCHA (fixed at build): header declares
  `class Impl;` but cpp defined `struct Impl` → MSVC C4099 (warnings-as-errors) — cpp now uses
  `class Impl`. VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s039.cmd — configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed; 121 cases / 674 asserts); ubuntu-clang (Ubuntu VM
  clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing
  upstream <ciso646> doctest deprecation from third_party — not this TU, already non-error in test
  CMake). windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule). NO-GAP
  accounting: S040 = write/cascade side (parent change → recompute child effective value without
  child write; child Set vs inherited old) consumes the parent-link seed + global table —
  authored at S040, never a silent backfill; S120 (visual tree) consumes the parent link. No MINIMAL
  REAL introduced/resolved. No NEW md files (directive honored). Next = S040.

- [x] S040 Inheritance cascade: parent change recomputes child effective value without child write — ✅ (test)
  SURVIVOR: `test/unit/InheritanceCascade_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +InheritanceCascade_test.cpp) + `build/verify_s040.cmd` (NEW, verify_s039-pattern msvc-debug lane)
  + `docs/roadmap.md` (MOD S040 [DONE]) + `session-context.md` (MOD). TEST-ONLY step (ZERO new product
  symbols) — the S039 pull model already delivers correct cascade semantics; S040 PROVES the S039-
  deferred write/cascade side is real and exercises the genuinely new dimensions S039 did not show:
  (1) dynamic ordering — parent change AFTER the child is attached (S039 only set the chain up then
  read once, static); (2) local-vs-inherited interaction ("child Set vs inherited old"). CONTRACT
  boundary (recorded NO-GAP, NOT silent): S040 does NOT build descendant tracking / notifications —
  that is S041's explicit scope (ChangeQueue + notification processor). Test harness mirrors
  EffectiveValue_test.cpp: hostInherited() registers the GLOBAL DependencyProperty descriptor once per
  name (name-unique → PropertyAlreadyRegistered avoided via suite-local static set) + mirrors onto each
  object's own store; kInh = PropertyFlags::Inherits. 6 TEST_CASEs (doctest + Shouldly vocabulary
  SHOULD/ShouldThrow):
  (1) CASCADE AFTER ATTACH — child.set_parent(parent) FIRST, then parent.Set(11) → child effective 11,
  then parent.Set(22) → child effective 22; no child Set ever called (recompute is purely the read-side
  pull over an empty child local store).
  (2) MULTI-LEVEL — 3-level chain wired in place (leaf→g3→g2→g1), THEN g1.Set(5) → leaf 5; g1.Set(9)
  → leaf 9 (intermediate g2/g3 hold no local, leaf follows; no child write).
  (3) LOCAL-WINS — child sets its own local 7 BEFORE parent changes; parent then Set(99) → child
  effective STAYS 7 (cascade respects a real local override, does not sweep it).
  (4) INHERITONLY RECOMPUTE — Inherits|InheritOnly property; child Set(100) is IGNORED; parent.Set(3) →
  child 3; parent.Set(4) → child 4 (own local never contributes, parent change DOES recompute).
  (5) CHILD SET VS INHERITED OLD — child first inherits parent 20, then writes its own local 31 (child
  Set vs inherited old); a LATER parent.Set(44) must NOT clobber the child write → child effective STAYS
  31.
  (6) PARENT REVERTS — parent Set(8) → child 8, parent Set(0) → child 0, parent Set(6) → child 6; child
  tracks both moves purely through the read-side pull, no child Set anywhere.
  VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s040.cmd — configure/build warning-clean,
  ctest 1/1 PASS (100% tests passed; 127 cases / 684 asserts); ubuntu-clang (Ubuntu VM clang 21, shared
  folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646>
  doctest deprecation from third_party — not this TU, already non-error in test CMake).
  windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule). NO-GAP accounting:
  S040 consumes the S039 parent-link seed + global DependencyProperty table and verifies the
  S039-deferred "write/cascade side" is real — resolved here by proof, not silently dropped. Push
  notification of descendants is explicitly S041's scope (recorded, NOT silent). No MINIMAL REAL
  introduced/resolved. No NEW md files (directive honored). Next = S041.

- [x] S041 Change notification pipeline: ChangeQueue (per-thread) + processor drains in registration order — ✅
  SURVIVOR: `src/core/include/vaml/core/{ChangeQueue,ChangeProcessor}.h` (NEW) +
  `src/core/src/{ChangeQueue,ChangeProcessor}.cpp` (NEW) + `test/unit/{ChangeQueue,ChangeNotificationPipeline}_test.cpp`
  (NEW) + `build/verify_s041.cmd` (NEW, verify_s040-pattern msvc-debug lane) + MOD
  `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` +
  `src/core/CMakeLists.txt` (+ChangeQueue.cpp +ChangeProcessor.cpp) + `test/unit/CMakeLists.txt`
  (+2 test TUs) + `docs/roadmap.md` (S041 [DONE]) + `session-context.md` (MOD). Delivers the S041
  headline as THREE REAL public symbols (none MINIMAL REAL, none STUB):
  (1) `vaml::ChangeQueue` — per-thread pending NOTIFICATION queue; S037's per-object VALUE queue
  generalized onto the notification axis (S037's OWN resolution point — NO backfill). Cross-thread
  `enqueue(id,old,new)` / `pending_count()` (mutex); affinity-thread-only `process(NotifyFn)` /
  `clear()` (throw WrongThreadException from a non-affine thread, mirroring S037's ProcessPendingChanges
  D3 NO-LAZY guard); FIFO swap-out (a record during process lands in a fresh queue, never double-emitted
  this pass); owned-name gotcha honored (each item keys on an OWNED std::string, never the PropertyId
  view).
  (2) `vaml::ChangeProcessor` — "drains in registration order" per the headline: owns a ChangeQueue and
  CONSUMES S038's ChangeRecorder INSIDE the drain, so two enqueues of the same property before a drain
  collapse to ONE notification (first old kept, latest new adopted — S042's coalescing outcome realized
  end-to-end against live objects). `process(NotifyFn)` drains the snapshot then flushes the recorder;
  affinity-thread-only.
  (3) `DependencyObject` (MOD) — three additions: (a) registration-ordered subscriber registry —
  `using PropertyChangedHandler = std::function<void(PropertyId,const Value&,const Value&)>;`
  `add_property_changed(PropertyId, handler)` -> monotonically-increasing `PropertyChangedToken` (never
  reused), `remove_property_changed(token)` (O(n), preserves registration order of what remains; a re-add
  appends at the end); fired ONLY on an actually applied change (additive dispatch — the existing metadata
  `with_changed` cb + S037 SetResult::{APPLIED,REJECTED,NO_CHANGE,QUEUED} + marshalling tests all stay
  green). (b) per-object notification queue + `ProcessNotificationQueue()` (affinity-thread-only,
  WrongThreadException guard; drains via ChangeQueue + coalesces through ChangeRecorder at drain; fires
  this object's registration-ordered subscribers) + `pending_notification_count()`. (c) DESCENDANT PUSH —
  the S040-RECORDED obligation, delivered here NOT silent: `set_parent()` now ALSO registers the child
  into the parent's non-owning inheritance-scope (the INVERSE edge of the S039 parent link; `std::vector<
  DependencyObject*>` in Impl; unwind-on-reparent + dedupe guard). On an APPLIED Inherits/InheritOnly
  change (on-affine Set OR a drain-applied queued write via ProcessPendingChanges), the TWO-PHASE pull
  model runs: `collect_descendant_effectives` captures each registering descendant's CURRENT effective
  value BEFORE the store (recursive walk via `visit_scope`), and `notify_descendants` recomputes each
  descendant's NEW effective value AFTER the store and enqueues into that descendant's OWN notification
  queue ONLY when old != new. This means each descendant receives its OWN effective-old -> effective-new
  delta and is notified only when it ACTUALLY changes — S040 local-wins is respected THROUGH an
  intermediate (a local-wins node shields its entire subtree; a parent move that changes no descendant's
  effective pushes nothing). No visual tree is built: the inheritance-scope is DISTINCT from the visual
  children list (S121 create/attach/detach + walk, which composes with set_parent) — recorded, no
  overlap, no gap. Tests (doctest + Shouldly vocabulary): ChangeQueue_test.cpp (8 TEST_CASEs: cross-thread
  enqueue/owner drain; FIFO order; empty drain=0; process-from-non-affine throws WrongThreadException;
  clear-from-non-affine throws; record-during-process fresh-queue (swap-out); clear; owned-name survival);
  ChangeNotificationPipeline_test.cpp (11 TEST_CASEs: ChangeProcessor coalescing 2-in->1-out first-old+
  latest-new; distinct props once each in first-record order; empty=0; DependencyObject registration-ordered
  dispatch via real descendant push; remove-token detach + re-add-appends-at-end; descendant push carries
  oldEffective->newEffective delta via ProcessNotificationQueue; multi-level great-grandparent->leaf; local-
  wins intermediate shields BOTH itself and its subtree (no push) + sanity grandchild notified when the child
  (the true source) changes; InheritOnly always inherits; unregistered descendant -> nothing pending; single
  subscriber fires once per push). VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via
  verify_s041.cmd — configure/build warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM
  clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (146 cases; the single in-suite
  "failed" assertion is the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing
  upstream <ciso646> doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane
  (not built/tested/flagged per standing rule). NO-GAP accounting: S037's "S041 is its OWN resolution point
  (NO backfill)" is satisfied (ChangeQueue generalizes the per-object value queue); S038's "ChangeRecorder is
  S041's building block; S041 consumes that coalescing end-to-end" is satisfied (ChangeProcessor + the object's
  ProcessNotificationQueue both drain via ChangeRecorder); S040's "push notification of descendants is
  explicitly S041's scope" is satisfied (descendant push delivered above). S042 remains the formal end-to-end
  coalescing-vs-live-DependencyObject integration gate — its primitive outcome is already REALIZED-EARLY by
  S038 and now additionally integrated here (recorded NOT silent). No MINIMAL REAL introduced/resolved. No
  NEW md files (directive honored). Next = S042.

- [x] S042 Coalescing: two sets of same prop before drain == one notification (test) — ✅
  SURVIVOR: `test/unit/Coalescing_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD +Coalescing_test.cpp) +
  `build/verify_s042.cmd` (NEW, verify_s041-pattern msvc-debug lane) + `docs/roadmap.md` (MOD S042 [DONE],
  REALIZED-EARLY annotation refined to record the formal integration is now delivered at S042) +
  `session-context.md` (MOD). TEST-ONLY step (ZERO new product symbols) — the formal LIVE-OBJECT
  coalescing integration gate the S042 headline names. The coalescing PRIMITIVE was REALIZED-EARLY by S038
  (ChangeRecorder coalesces per-property by design — first old kept, latest new adopted — tested in
  ChangeRecorder_test.cpp), and S041 wired it into ChangeProcessor + the live-object per-object notification
  queue + ProcessNotificationQueue. S042 closes the ONE remaining gap: S041 proved coalescing only on the
  STANDALONE ChangeProcessor and proved live descendant-push with a DRAIN BETWEEN Sets (two *sequential*
  notifications) — it never proved the exact S042 headline: **two live Sets of the same prop before ONE
  drain == ONE notification**, first old kept + latest new adopted. New test TU (dedicated, mirroring the
  S040 test-step precedent of one proof file per step; reuses the ChangeNotificationPipeline hostInherited()
  global-descriptor harness verbatim, with S042-unique property names `Coalesce_S042_*` so the global
  name-unique DependencyProperty table cannot collide across TUs). 6 TEST_CASEs, all against live
  DependencyObjects through the real descendant-push → per-object notification queue →
  ProcessNotificationQueue path:
  (1) HEADLINE — parent/child Inherits (default 0); child subscriber registered; parent.Set(5) then
  parent.Set(8); raw queue depth == 2 (proves coalescing happens AT DRAIN, not enqueue); ONE
  ProcessNotificationQueue() → subscriber fired EXACTLY ONCE with (old=0 first-kept, new=8 latest-adopted),
  queue depth 0.
  (2) THREE SETS → ONE notification — Set 5,7,9 → one notif (old=0, new=9): earliest old + newest new.
  (3) DRAIN IS THE BOUNDARY — Set 5 → drain (one notif 0→5), Set 8 → drain (one notif 5→8): two sequential
  notifications, each its own delta, no cross-drain collapse.
  (4) DISTINCT PROPS DO NOT COALESCE — two Inherits props A,B on one parent; interleave A=1,B=2,A=3 then one
  drain → exactly TWO notifications, A (0→3) then B (0→2), in FIRST-RECORD order.
  (5) NO_CHANGE SET DOES NOT INFLATE — parent.Set(5) then parent.Set(5): the second is NO_CHANGE (no push),
  queue stays depth 1, one notification (0→5).
  (6) AGGREGATE CAPSTONE — mixed interleaved A=5,B=2,A=7,B=3 then one drain → exactly TWO notifications
  (per-property coalesced, each adopting its LATEST new: A 0→7, B 0→3), never four.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s042.cmd — configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (152 cases; the single in-suite "failed" assertion is
  the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646>
  doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/
  flagged per standing rule). NO-GAP accounting: S042 delivered its own outcome + proof at its own step —
  nothing deferred to a silent backfill; NO-GAP satisfied by construction. No MINIMAL REAL introduced/
  resolved. No NEW md files (directive honored). Next = S043.

- [x] S043 Attached properties: key owned by A settable on B; registry supports owner≠class — ✅
  SURVIVOR: `src/core/src/DependencyObject.cpp` (MOD — attached-aware global resolution) +
  `src/core/include/vaml/core/DependencyProperty.h` + `src/core/src/DependencyProperty.cpp` (MOD
  +owner_type_id()) + `test/unit/AttachedProperty_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +AttachedProperty_test.cpp) + `build/verify_s043.cmd` (NEW, verify_s042-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S043 [DONE]) + `session-context.md` (MOD). Makes DependencyObject ATTACHED-AWARE
  by GLOBAL RESOLUTION (the S043 headline): a globally-registered DependencyProperty (key owned by A) is
  now settable + gettable on a plain object B that NEVER calls register_property. Core mechanics:
  (1) PROPERTY-METADATA PROVIDER refactor — `Impl::PropEntry` now has TWO modes (exactly one active):
  OWNED (object-scoped register_property owns its move-only PropertyMetadata in a `unique_ptr<PropertyMetadata>`
  field `owned`) or BORROWED (an attached property materialized onto the object holds a NON-OWNING
  `const PropertyMetadata* borrowed` pointer to the GLOBAL descriptor). `PropEntry::metadata()` returns
  whichever is active. This is the S043 KEY DESIGN DECISION: the move-only PropertyMetadata is NEVER copied
  into an object's entry — the global DependencyProperty (process-lifetime companion table) is the single
  owner; per-object attached entries borrow it by pointer (no dangle: process-lifetime table). (2)
  GetEffectiveValue()/Get() — three-way resolution: in-store → stored entry metadata + local; not-in-store
  but GLOBALLY registered → global metadata + empty local (`inherited ?? global-default`, object needs NO
  local entry to read an attached property); NEITHER → PropertyNotFound (unchanged — unregistered names
  still throw). (3) Set() — resolves the metadata PROVIDER non-owning WITHOUT mutating the store first
  (S037 cross-thread contract: coerce+validate run BEFORE any store touch and a NON-affine Set NEVER
  mutates the affinity-owned store); wrong-thread POSTS to the pending queue (no materialization);
  ON THE AFFINE THREAD it LAZILY MATERIALIZES a BORROWED store entry for the attached property (so the
  local override has a home) then runs the UNCHANGED S035 pipeline (coerce → validate(coerced) →
  fire-on-real-change → store → S041 descendant push). (4) ProcessPendingChanges() — a queued write whose
  name is globally-registered-but-not-materialized now materializes a BORROWED entry (affinity-thread drain,
  legal) and applies, instead of skipping silently (SKIP only when NOT globally registered). (5)
  `DependencyProperty::owner_type_id()` (new public accessor returning `const std::string&` from the stored
  key's ownerTypeId, threaded through the DependencyProperty::Impl + Register) surfaces the DECLARING owner
  so "owner ≠ class" is provable. Make-existing-Set/Get-attached-aware (no new SetValue/GetValue surface) is
  the minimal, roadmap-faithful choice (WPF-faithful: value stored per-object, definition owned once by A).
  Name-keyed, owner-as-metadata design is PRESERVED (S032) — S043 does NOT introduce same-name-multi-owner
  (would break global name-uniqueness). Tests (doctest, Shouldly vocabulary; S043-UNIQUE global names
  `{Left,Theme,Dock,CanvasTop,Margin}_S043_*` via a suite-local `global_seen()` set so the name-unique
  global DependencyProperty table cannot collide):
  (1) HEADLINE — attached Set(5) -> APPLIED; Get -> 5; isolated per-object; etc. (see roadmap S043 line).

- [x] S044 Attached set/get + inheritance + notification tests — ✅
  SURVIVOR: `test/unit/AttachedInheritanceNotification_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +AttachedInheritanceNotification_test.cpp) + `build/verify_s044.cmd` (NEW, verify_s043-pattern msvc-debug
  lane) + `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD —
  the S044 push-walk broadening) + `docs/roadmap.md` (MOD S044 [DONE]) + `session-context.md` (MOD).
  Delivers the S044 headline end-to-end against live DependencyObjects AND closes a REAL GAP discovered
  while tracing the headline (NOT a silent omission — a sanctioned product extension authored at the SAME
  step as the tests, per NO-GAP): **passive attached-Inherits descendants were never notified**.
  THE GAP: S041's descendant push (`visit_scope`) captured a descendant ONLY when it already had a local
  store entry (`store.find(name) != end()`). For an attached Inherits property, a PURELY-PASSIVE inheriting
  child (never registers, never Set, never materializes a borrowed entry) reads the inherited value
  correctly via S043's global-resolved `GetEffectiveValue`, but was NEVER captured -> NEVER notified when
  the parent's attached Inherits value changed. Only descendants that artificially materialized a store
  entry (which makes them local-wins, so they never inherit-and-notify) were notified — leaving S044's
  "inheritance + notification" headline hollow.
  PRODUCT CHANGE (minimal, scoped to the S041 push walk): `collect_descendant_effectives` /
  `visit_scope` gain a `captureAttachedInherits` flag (threaded from BOTH Set() and ProcessPendingChanges()).
  It is computed as: the property carries the GLOBAL `Inherits` flag (from `DependencyProperty::TryGet(id.name())
  ->flags()`, already resolved as `dp`) AND the ORIGIN's own store entry is BORROWED (`entry.owned == nullptr`
  — the precise "attached" signal: this object hosts the value but does NOT own the property, S043 design).
  When set, `visit_scope` captures a descendant even if it has NO local store entry. `notify_descendants`
  still filters to real change (`old != new`), so over-capturing a passive descendant that ends up unchanged
  is harmless — shielded / local-wins / non-participating descendants still produce zero notifications. An
  object-scoped Inherits property keeps the EXACT S041 behavior (the origin OWNS its entry, so
  captureAttachedInherits stays false) — verified by the existing S040/S041/S042/S043 tests staying green.
  No new public symbol introduced.
  Tests (doctest, Shouldly vocabulary; S044-UNIQUE global names `{AttachedTheme,AttachedOpacity,AttachedBrush,
  AttachedCoalesce,AttachedMarshalled,AttachedAccent}_S044_*` via a suite-local `global_seen()` set so the
  name-unique global DependencyProperty table cannot collide; `registerAttachedGlobal` registers ONLY on the
  global registry with owner A — objects do NOT locally register, the true attached premise):
  (1) ATTACHED-INHERITS CASCADE (headline) — attached Inherits (owner AppOwner_A) never locally registered on
  parent/child; orphan read -> global default 0; parent.Set(5) -> APPLIED, child (unattached read) inherits 5;
  parent.Set(9) -> child follows 9 (read-side cascade on the attached axis).
  (2) ATTACHED INHERITED + LOCAL-WINS — child Set its own attached 7 first (local-wins, borrowed entry
  materialized); later parent.Set(44) does NOT clobber -> child effective stays 7.
  (3) PASSIVE DESCENDANT IS NOTIFIED (the product gap-fix under test) — parent/child attached Inherits; child is
  purely passive (no Set ever), subscribes; parent.Set(8) -> child.pending_notification_count()==1; child.
  ProcessNotificationQueue() fires EXACTLY once (old=0 inherited default, new=8). This FAILS on the pre-S044
  walk (never captured) and PASSES post-S044.
  (4) ATTACHED COALESCING — parent.Set(5) then parent.Set(8) before one passive-child drain -> raw queue depth
  2 (coalescing at drain), ONE notification (old=0 first-kept, new=8 latest-adopted) — S042 on the attached axis.
  (5) WRONG-THREAD ATTACHED INHERITS -> QUEUED -> DRAIN -> NOTIFIED — non-affine parent.Set(9) -> QUEUED, pending
  1, parent.Get still 0, child.pending_notification_count()==0; affinity parent.ProcessPendingChanges() -> applied
  (parent.Get 9, child pending 1) -> child.ProcessNotificationQueue() fires (0->9).
  (6) OWNER != CLASS + SHADOWING + THROW — attached owner AccentOwner_A read back via dp.owner_type_id() while
  plain objects store (owner != class preserved on the inheriting axis too); a locally-registered same-name
  SHADOWS the global path (local default 99 governs, not global 0); an unregistered name still throws
  PropertyNotFound from both Get and Set.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s044.cmd — configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (163 cases; the single in-suite "failed" assertion is the
  intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646> doctest
  deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). NO-GAP accounting: the attached-Inherits notification gap was found, DECIDED in the plan as a
  sanctioned same-step product extension (not silent backfill), and its resolution is authored at S044 — no
  feature left for a silent future step. No MINIMAL REAL introduced/resolved. No NEW md files (directive
  honored). Next = S045.
  (1) HEADLINE — attached Set/Get on B WITHOUT B calling register_property (key owned by CanvasOwner_A);
  Get resolves the global default (0) before any write; Set(5) → APPLIED; Get → 5; a SECOND independent B
  object does NOT see the first B's value (value stored PER-OBJECT, definition owned once by A).
  (2) EFFECTIVE = local ?? inherited ?? global-default — orphan (no parent, no local registration) reads the
  global default; parent→child chain neither locally registers, parent attached-Set(7) → APPLIED, child
  (unattached read) INHERITS 7 through the global-resolved pull; child local Set(9) → 9 wins (local ??).
  (3) OWNER ≠ CLASS — key registered under owner DockPanelOwner_A is settable+gettable on plain object B;
  `dp.owner_type_id()` reads back "DockPanelOwner_A" BOTH before and after B stores a value.
  (4) WRONG-THREAD ATTACHED SET → QUEUED (marshalling parity) — affine attached Set applies + pending 0;
  a non-affine attached Set(8) → QUEUED, pending 1, Get still 1 (not applied); affinity-thread drain →
  pending 0, Get 8.
  (5) BACKWARD COMPAT — a property both globally registered AND then locally registered on B with a
  DIFFERENT default: B's LOCAL registration SHADOWS the global descriptor (Get returns B's local default
  99, not the global 0; local Set path governs). An UNREGISTERED (not local, not global) name still throws
  PropertyNotFound from BOTH Get and Set.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s043.cmd — configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (157 cases; the single in-suite "failed" assertion is
  the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646>
  doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/
  flagged per standing rule). NO-GAP accounting (recorded, NOT silent): S044 = the DEDICATED attached
  inheritance + notification test gate (attached Inherits cascade + descendant notification on live objects)
  — S043 delivers the enabling resolution + its own basic attached tests; S044 is the separate formal proof
  of the inheritance/notification axes (mirroring S042's test-step precedent), NEVER a silent backfill.
  No MINIMAL REAL introduced/resolved. No NEW md files (directive honored). Next = S044.

- [x] S045 ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored — ✅
  SURVIVOR: `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD) +
  `test/unit/ReadOnlyAndValidation_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +ReadOnlyAndValidation_test.cpp) + `build/verify_s045.cmd` (NEW, verify_s044-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S045 [DONE]) + `session-context.md` (MOD). Delivers the S045 headline end-to-end:
  **ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored.**
  PRODUCT CHANGES (both authored at the same step as the tests, NOT silent backfills):
  (1) NEW `vaml::PropertyReadOnlyException : std::logic_error` (declared in DependencyObject.h alongside
  WrongThreadException) — catchable specifically so a caller can distinguish a read-only rejection from a
  wrong-thread QUEUED result.
  (2) Set() READ-ONLY GUARD — placed IMMEDIATELY after metadata-provider resolution and BEFORE
  coerce/validate/affinity:
  `if (has_flag(md.flags(), PropertyFlags::ReadOnly)) throw PropertyReadOnlyException(...)`.
  Structural + VALUE-INDEPENDENT: throws even when `input` equals the current/default value (never
  REJECTED/NO_CHANGE); fires on ANY thread — the guard precedes the S037 affinity branch, so a wrong-thread
  read-only Set THROWS, never returns QUEUED/deferred to a drain; and a refused read-only write NEVER
  materializes a borrowed store entry (Get still reads the default). Rationale (recorded): a read-only
  refusal is structural and value-independent, so it must precede the S035 pipeline AND the marshalling
  branch by construction.
  (3) ProcessPendingChanges() DEFENSIVE apply-time guard — resolves the metadata provider BEFORE
  materializing an entry and SKIPS a queued write whose property is now read-only at apply time (covers the
  narrow edge where metadata became read-only between queue-time and drain; NO-LAZY — never apply a write to
  a read-only property, never materialize a borrowed entry for a refused read-only write). The Set() guard
  already blocks read-only writes from ever being queued, so this is defense-in-depth, not a hot path.
  SCOPE DECISION (recorded): read-only => Set throws UNCONDITIONALLY for any caller on any thread — an
  OWNER-INTERNAL write path (WPF lets the owning class set read-only DPs internally; our public surface has
  no owner gate) is deliberately DEFERRED + recorded as a S120+ controls concern, NOT silently omitted.
  ALL-FLAGS-HONORED scope (recorded, honest): Inherits (effective resolution) + InheritOnly + ReadOnly
  (throws) behave in this step; BindsTwoWayByDefault is CARRIED through key + metadata flags but has NO
  runtime behavior yet — genuine two-way binding is S242's explicit scope (recorded NO-GAP, not faked).
  Tests (doctest + Shouldly vocabulary; S045-UNIQUE global names via a suite-local `global_seen()` set +
  `registerObject`/`registerGlobal` harness; 7 TEST_CASEs):
  (1) READ-ONLY SET THROWS (object-scoped) — register `PropertyFlags::ReadOnly`, Set(5) -> throws
  PropertyReadOnlyException; Get still reads the default 7; store unchanged (refused write never
  materialized a local override).
  (2) READ-ONLY THROWS EVEN FOR VALUE == CURRENT — Set(7) (equals current/default) -> THROWS, not NO_CHANGE
  (structural, value-independent).
  (3) READ-ONLY WRONG-THREAD THROWS (NEVER QUEUED) — a non-affine Set(9) on a read-only property -> throws
  PropertyReadOnlyException (caught + rethrown via std::exception_ptr), pending_change_count()==0 (never
  queued), Get unchanged 0. Proves the guard precedes the S037 affinity branch.
  (4) GLOBALLY-REGISTERED READ-ONLY ATTACHED — global read-only (owner A), Set on B (no local registration)
  -> throws; Get works (global default 2); no borrowed entry materialized (Get still 2).
  (5) READ-ONLY THROW PRECEDES THE PIPELINE — a read-only property that ALSO has with_coerce (doubles) +
  with_validate (rejects all): Set(-3) -> throws PropertyReadOnlyException (if coerce ran -3->-6, if validate
  ran it would REJECT — the throw fires first), Get stays default 0.
  (6) VALIDATE OUT-OF-RANGE REJECTS (read-write) — bounded validate [0,10]: in-range 8 -> APPLIED, store 8,
  changed cb fired once; out-of-range 42 and -3 -> each REJECTED, store unchanged 8, cb NOT fired.
  (7) ALL FLAGS HONORED (consolidated) — (a) ReadOnly|BindsTwoWayByDefault on one key: Set throws +
  BindsTwoWayByDefault carried through key+metadata flags (no runtime behavior — S242 scope); (b)
  Inherits|InheritOnly global descriptor: parent.Set(5) -> child effective inherits 5 (effective resolution;
  child's own local does NOT contribute under InheritOnly).
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s045.cmd — configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646> doctest
  deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). NO-GAP accounting: BindsTwoWayByDefault behavior is explicitly S242's scope (recorded, NOT
  silent); owner-internal read-only write is a recorded S120+ controls concern (NOT silent); nothing deferred
  to a silent backfill. No MINIMAL REAL introduced/resolved. No NEW md files (directive honored). Next = S046.

- [x] S046 DependencyProperty registration-time dependency validation (ValidateDependencies). — ✅ (REDIRECTED)
  SURVIVOR: `test/unit/DependencyCallbacks_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +DependencyCallbacks_test.cpp) + `build/verify_s046.cmd` (NEW, verify_s045-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S046 [DONE]). REDIRECTED (user-accepted, ADR-style decision recorded in roadmap):
  the original title — a registry-level "ValidateDependencies" dependency-graph gate — was reviewed against
  real commercial WPF MVVM and judged INERT: the "children VM depending on a hasa DP" relationship is
  INSTANCE COMPOSITION, not static registry linkage (WPF itself declares property coupling inline via
  coercion and NEVER validates a dependency graph at registration). S046 instead delivers the WPF-faithful
  value hooks — coercion + per-set validation — end-to-end. SCOPE DECISION (recorded, NO-LAZY-LLM): the
  coerce → validate(coerced) → fire-on-real-change → store pipeline is ALREADY implemented in
  DependencyObject::Set/ProcessPendingChanges (S036/S037/S045) and the callbacks already live in
  PropertyMetadata (S034). Therefore S046 introduces NO new product symbol — a standalone
  DependencyProperty::CoerceValue seam or a duplicated CoerceValueCallback alias would be DEAD/redundant
  surface (precisely what S048's refactor gate removes). S046 instead proves the two dimensions previous
  steps left UNCOVERED, fully in the Shouldly vocabulary:
  (1) ATTACHED/GLOBAL axis — a globally-registered DependencyProperty (owner A) carrying with_coerce +
  with_validate, set on an object B that never locally registers it (the S043 attached premise): raw 150 →
  coerced 100 → validate(100) passes → APPLIED, Get 100 (the COERCED value stored); raw -5 → coerced 0 →
  APPLIED; raw 7 → coerced 7 → validate(7) rejects → REJECTED, store unchanged. Wrong-tag input on the
  global axis throws ValueTypeMismatch, store unchanged.
  (2) BACKWARD COMPAT — a globally-registered property with NO hooks stores as-given on the attached axis
  (APPLIED, then NO_CHANGE on repeat) — S036 passthrough proven on the global/attached path.
  (3) WRONG-THREAD QUEUED COERCION — coerce+validate run AT QUEUE TIME (S037), so what is POSTED and later
  DRAINED is the COERCED value, not the raw input: non-affine Set(150) → QUEUED (coerced 100), drain stores
  100 (not 150) + cb(50→100); non-affine Set(-5) → QUEUED (coerced 0), drain stores 0 (not -5) + cb(10→0).
  Both global-axis coercion and coerced-value-at-drain were previously UNCOVERED (no existing test registered
  a global DependencyProperty with a coerce hook set on a live object, and none asserted the coerced value is
  what the wrong-thread drain stores). VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via
  verify_s046.cmd — configure/build warning-clean, ctest 1/1 PASS (100% tests passed; 179 cases / 956 asserts);
  ubuntu-clang (Ubuntu VM clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only
  pre-existing upstream <ciso646> doctest deprecation from third_party, not this TU). windows-clang-debug is
  NOT a lane (not built/tested/flagged per standing rule). NO-GAP: S046 resolved its recorded obligation
  (the registered dependency-validation intent) via the user-accepted redirect — nothing deferred to a silent
  backfill; the redirect itself IS the recorded resolution. No MINIMAL REAL introduced/resolved. No NEW md
  files (directive honored). Next = S047.

- [x] S047 Property-system unit-test suite complete; every numbered predicate via adapted Shouldly. — ✅ (GATE)
  SURVIVOR: `docs/roadmap.md` (MOD S047 [DONE]) — DOCS-ONLY, no code change, both CI lanes already green from
  the S046 run. S047 is the FORMAL suite-complete gate, the vocabulary REALIZED EARLY by S036_1..S036_5 and
  now CERTIFIED against the established convention. GATE MET (verified by grep across test/unit): ZERO
  unadapted feature `CHECK`/`CHECK_THROWS_AS` across every migrated property feature file (Value_test,
  Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test,
  PropertyMetadata_test, DependencyObject_test) plus the NEW S046 file DependencyCallbacks_test.cpp — all
  pure Shouldly (SHOULD/ShouldThrow/ShouldlyString). SANCTIONED non-feature exceptions (deliberate,
  documented, NOT count as violations): PropertyMetadata_semantics_test.cpp (the S035 reference SPEC harness —
  rewriting it would alter the exact contract under test; its CHECKs + single CHECK_THROWS_AS stay),
  smoke_test.cpp (doctest runner/main), StringMaker_test.cpp (asserts the raw doctest::String output of the
  S036_2 stringifier itself), shouldly_test.cpp (tests the Shouldly predicate/routing library + the S036_1
  SHOULD/SHOULD_REQUIRE routing seam + the intentional should_fail demonstration that must stay green), and
  the REQUIRE precondition guards (null-deref gates in PropertyKeyRegistry/PropertyRegistryExhaustive). The
  FULL Sprint-1 property suite — now including DependencyCallbacks_test.cpp — is green on BOTH ci lanes
  (msvc-debug via verify_s046.cmd + ubuntu-clang via /mnt/hgfs/VamlPOC). No new product symbols. No MINIMAL
  REAL introduced/resolved. No NEW md files (directive honored). Next = S048.

- [x] S048 REFACTOR GATE(S1): unused removed, build+ctest green both compilers. — ✅
  SURVIVOR: `docs/roadmap.md` (MOD S048 [DONE]) — GATE step, no code change required. Full build + ctest
  green on BOTH ci lanes (windows-msvc via verify_s046.cmd's msvc-debug lane + ubuntu-clang via
  /mnt/hgfs/VamlPOC clang-debug) — both verified from first principles in the S046 run. No dead/unused code to
  remove: S046 deliberately added NO product symbol (a standalone CoerceValue seam / duplicated
  CoerceValueCallback alias would have been dead surface — excluded by design at S046, NO-LAZY-LLM), and no
  documented public surface or MINIMAL REAL marker was touched across Sprint-1 closing. vaml-core builds
  warning-as-error CLEAN on both compilers (the only Clang warning remains the pre-existing upstream <ciso646>
  doctest deprecation from third_party, not VamlPOC source). ctest 1/1 PASS both lanes. Sprint-1 property
  system core is clean + green. No MINIMAL REAL introduced/resolved. No NEW md files (directive honored).
  Next = S049 (SURVIVOR + checkpoint advance to S080).

- [x] S049 SURVIVOR + checkpoint advance to S080. — ✅
  SURVIVOR: `progress.md` (MOD — consolidated `## SPRINT 1 — SURVIVOR` block + regenerated checkpoint to
  last=S049/next=S080) + `docs/roadmap.md` (MOD S049 [DONE]) — DOCS-ONLY, no build (per plan). The
  consolidated Sprint 1 SURVIVOR block distills S030..S048 into the surviving-state picture a fresh LLM
  resumes from: full DP semantics (Value/PropertyKey/PropertyRegistry/PropertyId/PropertyMetadata/
  DependencyObject + marshalling + ChangeRecorder/ChangeQueue/ChangeProcessor + effective-value resolution +
  attached properties + read-only/validation + coercion value-hook proofs) all real + tested + refactor-gated;
  the Shouldly-for-doctest vocabulary is the standing suite convention (S047 certified zero unadapted feature
  CHECK); key design decisions recorded (owned-name store key, the S035 pipeline spec, move-only metadata
  never copied, descendant-push per-delta, the S046 ValidateDependencies→value-hooks redirect); deferred items
  all recorded NOT silent (BindsTwoWayByDefault→S242, owner-internal read-only→S120+ controls, UIElement→S120,
  store-race freedom→S620/S723, two-way binding→S242). NO MINIMAL REAL resolved in Sprint 1 — all four active
  seeds (vulkan→S161, tools→S287, Color→S089, Thickness→S089) carry forward unresolved, NO-GAP intact. Checkpoint
  advanced: Sprint 1 CLOSED, next = S080 (Sprint 2 — VAML markup parser, XML scanner). No NEW md files
  (directive honored), no build (docs-only per plan S049 bound). Next = S080.

### Sprint 3 — Element / scene graph + measure/arrange layout  (S120–S139)

- [x] S132 Grid ColumnSpan/RowSpan + span-aware measure+arrange math — ✅
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + ADDED
  test/unit/GridSpan_test.cpp + MOD test/unit/CMakeLists.txt (+GridSpan_test.cpp) + ADDED
  build/verify_s132.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md files).
  RESOLVES the S131-recorded NO-GAP hand-off (the span dimension). PRODUCT:
  (1) ATTACHED `Grid.RowSpan`/`Grid.ColumnSpan` Int32 DPs (default 1, clamped ≥ 1 and to grid bounds)
  registered globally under owner "Grid" via the S043 attached-aware path (mirroring Grid.Row/
  Grid.Column), with typed statics `Grid::set_row_span/set_column_span(row_span/column_span)`.
  (2) Span-aware two-pass measure/arrange in Element.cpp via a new static `resolve_grid_with_spans`
  (shared by MeasureOverride + ArrangeOverride): D2 Auto equal-division (a spanning child contributes
  desired/span-count to EACH spanned Auto track; single cell = span 1 = the S131 no-span behavior,
  NO-GAP preserved); D1 star min/re-award (a Star track that received a POSITIVE proportional share
  grows to at least its content minimum, so a spanning/single-cell child whose content exceeds the
  share re-awards the track to fit; a no-share star — totalStar==0 or zero weight — stays 0, keeping
  the S131 resolve contract); merged-span-cell re-measure of ONLY dirty children (S128); merged-span
  rect placement {anchor origin, Σ spanned extents} honoring child alignment+margin, arranging ONLY
  dirty children (S128). Panel DesiredSize = Σ(non-star resolved) + Σ(star content-minima) — the
  content-natural extent (upgrades S131's star=0 minimum, NO-GAP). No MINIMAL REAL introduced.
  TESTS (test/unit/GridSpan_test.cpp, 7 TEST_CASEs, Shouldly vocabulary, real "Grid.RowSpan"/
  "Grid.ColumnSpan" names): span DPs default 1 + set/read on a plain DependencyObject + raw S043
  Set/Get path; D2 auto equal-division (2-wide 50/50; 3-row 1/3); merged placement (origin at anchor,
  size = Σ tracks); span clamp (huge → bounds, negative → 1); Stretch-in-merged-cell; D1 star
  re-award with spans. NO-GAP clean: S133 = the exhaustive star+span suite (recorded, not silent);
  S132 resolves the S131 span hand-off; no new MINIMAL REAL. Both CI lanes from first principles green:
  windows-msvc (build/verify_s132.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S133.

- [x] S133 Grid star-sizing + span tests (exhaustive) — ✅
  SURVIVOR: ADDED test/unit/Grid_stretch_test.cpp + MOD test/unit/CMakeLists.txt
  (+Grid_stretch_test.cpp) + ADDED build/verify_s133.cmd + MOD docs/roadmap.md + session-context.md
  (no commits; no NEW md files). TEST-ONLY — zero new product symbols (the S040/S042/S044/S125/
  S128/S130 test-only precedent). S131/S132 recorded S133 as the named exhaustive star+span gate
  (NO-GAP, not a silent backfill): it asserts the REAL S132 Grid (span-aware two-pass +
  Grid.RowSpan/Grid.ColumnSpan) over the D1/D2 decisions settled at S132 (D2 = auto equal-division;
  D1 = star min/re-award). Instrument = a test-local RecordLeaf (S125/S130 pattern) recording the
  exact measure offer + final arrange slot on both axes, so Stretch-vs-Wrap and the merged-cell span
  extents are observable. 17 TEST_CASEs in five parts: PART A — star proportional sizing (1:1 equal;
  3:1 by weight; star after absolute+auto taken with clean-exact numbers; star clamp when fixed >
  available; totalStar==0 all-zero; zero-weight star beside a positive sibling; star never negative +
  D1 single-cell re-award). PART B — star+span (a spanning child re-awards the spanned star track D1;
  span across star+auto mixed; Stretch fills a merged star cell). PART C — auto+span D2 (2-wide 50/50;
  content-driven auto grows for a spanning child). PART D — span placement (mixed rowspan+columnspan
  merged rect; whole-grid span fills the grid; alignment honored inside the merged cell; span clamps).
  PART E — dirty-respect with spans (re-arranging ONLY a dirty spanning child leaves a clean sibling
  untouched). NO-GAP clean: no MINIMAL REAL introduced/resolved; S132 resolved the span surface at its
  own step, S133 is the recorded exhaustive suite, not a backfill. Both CI lanes from first principles
  green: windows-msvc (build/verify_s133.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S134 (WrapPanel:
  flow, wraps at boundary, aligned rows).

- [x] S134 WrapPanel: flow, wraps at boundary, aligned rows — ✅
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + MOD
  test/unit/ElementFactoryRegistry_test.cpp (9→10 built-in count + WrapPanel resolve) + ADDED
  build/verify_s134.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md files).
  REAL panel — NEW element (no S087 seed existed). PRODUCT: `WrapPanel final : UIElement`
  (visual-tree + Measure/Arrange) with a real `WrapDirection` DP (reuses the S129 `Orientation`
  enum: Vertical=0, Horizontal=1), default HORIZONTAL (WPF-faithful), NOT inherited, self-registered
  via the S043 attached-aware global path (Meyers-static `ensure_wrappanel_properties_registered()`).
  Property NAME "WrapDirection" (owner "WrapPanel") — deliberately NOT "Orientation" (StackPanel owns
  it in the global registry; a same-name pair under a different owner would be ambiguous to the S091
  dotted-key reader). REAL flow/wrap/align algorithm in Element.cpp: MEASURE offers every child
  `Size::Infinite()` on BOTH axes (S123 infinity rule — a Stretch child given infinite wraps to
  content; CROSS axis ALSO unbounded so rows are CONTENT-HEIGHTED, refining the accepted "natural-
  width basis" to a "natural-extent basis" — recorded in Element.cpp) and re-measures ONLY
  is_measure_dirty() children (S128); Horizontal flows left-to-right, WRAPS when `rowWidth +
  childWidth > available.width` (checked ONLY when the row is non-empty, so a child wider than the
  whole boundary still gets its own row), DesiredSize = {max row width, Σ row heights}; Vertical is
  the mirror (top-to-bottom columns, wrap on height, DesiredSize = {Σ column widths, max column
  height}). ARRANGE reproduces the same wrap decisions (boundary = final_rect extent), gives each row
  its extent (row height = tallest child on Horizontal), places each child into its row slot
  {cursor, rowY, childDesiredW, rowHeight} via child->Arrange (honoring the child's OWN alignment +
  margin inside the slot via S124 compute_final_rect — a default-Stretch child fills the row's cross
  extent while keeping its natural flow extent = the "aligned rows" headline), re-arranging ONLY
  is_arrange_dirty() children (S128). Registered as a built-in (`register_builtin_elements()`
  +"WrapPanel") — count 9→10, so the S087 ElementFactoryRegistry_test.cpp size assertions updated at
  THIS step + a WrapPanel resolve assertion added HERE. NO ItemWidth/ItemHeight (fixed-item mode) —
  not in the S134 title, recorded NO-GAP (deferred, not fabricated). NO MINIMAL REAL introduced.
  NO-GAP hand-off: S135 = the dedicated WrapPanel test suite (recorded, not silent). Both CI lanes
  from first principles green: windows-msvc (build/verify_s134.cmd, msvc-debug warning-clean + ctest
  1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only
  Clang warnings = pre-existing upstream <ciso646> doctest deprecation (non-error, already
  downgraded). Next = S135.

- [x] S135 WrapPanel tests — ✅
  SURVIVOR: ADDED test/unit/WrapPanel_test.cpp + MOD test/unit/CMakeLists.txt (+WrapPanel_test.cpp) +
  ADDED build/verify_s135.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md
  files). TEST-ONLY — zero new product symbols beyond S134 (the S040/S042/S044/S125/S128/S130/S133
  test-only precedent). S134 recorded S135 as its dedicated test suite (NO-GAP, not a silent
  backfill): it asserts the REAL S134 WrapPanel (flow/wrap/align + WrapDirection) over the S123
  infinity-aware finalizer, the S124 arrange math, and the S126/S127 dirty-driver. Instrument = a
  test-local RecorderLeaf (S125/S130 pattern) recording the exact measure offer + final arrange slot
  on both axes, so the wrap decision's natural-extent basis and the aligned-row slots are observable.
  14 TEST_CASEs, Shouldly vocabulary: derives UIElement via dynamic_cast; WrapDirection defaults
  Horizontal; typed round-trip Horizontal<->Vertical; WrapDirection resolves globally on a plain
  DependencyObject (S043 attached-aware); default-Horizontal overflow WRAPS to a new row; a child
  wider than the whole boundary still gets its own row; DesiredSize Horizontal = max row width / Σ
  row heights; children measured at NATURAL extent on BOTH axes (+inf offer); aligned rows — row
  height is the tallest child + a default-Stretch child fills the row height, per-child
  VerticalAlignment pins within its row (Top/Center/Bottom); vertical mirror — flows top-to-bottom +
  WRAPS to a new column + DesiredSize Σ col widths / max col height; empty panel -> DesiredSize 0 +
  arrange no-op; dirty-respect (a clean child is NOT re-measured nor re-arranged; an
  arrange-invalidated child IS re-arranged but a clean one is not) + margin enters DesiredSize
  (S130_1) and is honored at arrange. NO-GAP clean: no MINIMAL REAL introduced/resolved; S134
  resolved the WrapPanel surface at its own step, S135 is the recorded suite, not a backfill. Both CI
  lanes from first principles green: windows-msvc (build/verify_s135.cmd, msvc-debug warning-clean +
  ctest 1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS).
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (non-error, already
  downgraded).

- [x] S136 Canvas: explicit position via attached Left/Top (attached props via S1) — ✅
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + MOD
  test/unit/ElementFactoryRegistry_test.cpp (10→11 built-in count + Canvas resolve) + MOD
  docs/roadmap.md + session-context.md (no commits; no NEW md files). REAL panel — NEW element (no
  S087 seed existed). PRODUCT: `Canvas final : UIElement` (visual-tree + Measure/Arrange) with TWO
  REAL ATTACHED DPs, `Canvas.Left` + `Canvas.Top` (Double value path), default **NaN** = "not set"
  (the NaN contract introduced explicitly at THIS step — the S1 value path had no NaN convention
  before S136; user-directed at review to mirror WPF), NOT inherited, self-registered via the S043
  attached-aware global path (Meyers-static `ensure_canvas_properties_registered()`, registered
  globally under owner "Canvas" so they resolve on ANY DependencyObject). Typed statics
  `Canvas::set_left/set_top/left/top` + raw S043 Set/Get(PropertyId) path. REAL absolute-position
  panel algorithm in Element.cpp: MEASURE offers every child `Size::Infinite()` on BOTH axes (S123
  infinity rule, same basis as WrapPanel S134 — a child measures at its NATURAL content extent
  regardless of explicit position) and re-measures ONLY is_measure_dirty() children (S128);
  DesiredSize = the BOUNDING EXTENT of the arranged children — per child `rightEdge = effLeft +
  child.desired.w`, `bottomEdge = effTop + child.desired.h` where `effX = is_nan(v) ? 0 : v`
  (NaN = "not set" -> effective 0), DesiredSize axis = max over children of that edge, clamped >= 0
  (never negative, never NaN — NaN axes zeroed BEFORE the max fold so no NaN reaches the bounding
  math). ARRANGE places each child at its EXPLICIT origin {effLeft, effTop} (NaN collapses to 0 =
  the canvas origin, effective top-left) at the child's measured NATURAL size via child->Arrange —
  honoring the child's OWN alignment + margin inside that slot through compute_final_rect (S124/
  S130_1 contract) — so children at distinct origins are ABSOLUTE (no panel flow/stacking);
  re-arranges ONLY is_arrange_dirty() children (S128). Registered as a built-in
  (`register_builtin_elements()` +"Canvas") — count 10→11, so the S087 ElementFactoryRegistry_test
  .cpp size assertions updated at THIS step + a Canvas resolve assertion added HERE. NO MINIMAL REAL
  introduced. NO-GAP hand-off: S137 = the dedicated Canvas test suite (recorded, not silent). Both CI
  lanes from first principles green (the S136 product is most directly verified by S137's
  build/verify_s137.cmd, which runs the product + S137 tests): windows-msvc (build/verify_s137.cmd,
  msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug
  configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing upstream <ciso646> doctest
  deprecation (non-error, already downgraded). Next = S137.

- [x] S137 Canvas tests — ✅
  SURVIVOR: ADDED test/unit/Canvas_test.cpp + MOD test/unit/CMakeLists.txt (+Canvas_test.cpp) +
  ADDED build/verify_s137.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md
  files). TEST-ONLY — zero new product symbols beyond S136 (the S040/S042/S044/S125/S128/S130/S133/
  S135 test-only precedent). S136 recorded S137 as its dedicated test suite (NO-GAP, not a silent
  backfill): it asserts the REAL S136 Canvas (explicit Left/Top via attached DPs + absolute
  placement) over the S123 infinity-aware finalizer, the S124 arrange math, and the S126/S127
  dirty-driver. Instrument = a test-local RecorderLeaf (S125/S130 pattern) recording the exact
  measure offer + final arrange slot on both axes. 14 TEST_CASEs, Shouldly vocabulary: derives
  UIElement via dynamic_cast; Canvas.Left/Top default to NaN (not set); typed round-trip on finite
  values; explicit 0 is DISTINCT from unset (NaN) — round-trips as 0; Left/Top resolve globally on a
  plain DependencyObject (S043 attached-aware, default NaN); children measured at NATURAL extent on
  BOTH axes (+inf offer); empty canvas -> DesiredSize 0 + arrange no-op; DesiredSize = bounding
  extent incl. finite offsets + NaN axes ignored as 0; DesiredSize clamps >= 0 for negative coords;
  a child at Left=10,Top=20 lands exactly there at measured size; an un-set axis (NaN) collapses to
  the origin (0,0); multiple children at distinct origins are ABSOLUTE (no panel flow); negative
  Left/Top honored at the origin; a margin'd child lands at its origin with margin honored (S130_1),
  a default-Stretch child keeps its natural extent (nothing to stretch into), dirty-respect (a clean
  child is NOT re-measured nor re-arranged; an arrange-invalidated child IS re-arranged but a clean
  one is not). NO-GAP clean: no MINIMAL REAL introduced/resolved; S136 resolved the Canvas surface at
  its own step, S137 is the recorded suite, not a backfill. Both CI lanes from first principles green:
  windows-msvc (build/verify_s137.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S138.
