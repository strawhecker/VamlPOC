# VAML ROADMAP — S000..S999 (authoritative tokenized step list)

> This is THE step enumeration for the VAML POC. `progress.md` (repo root) is the continuation
> file that tracks completion. This file is the source of WHAT to do; each step has a token
> `S###`, a dependency (`dep:`), and a one-line description. The "no-stub" rule applies: a step
> is only ✅ when its behavior is real and verified by build + tests.
>
> Sprint headers list the sprint's goal. Token ranges reserve room; gaps mean reserved headroom
> for steps discovered mid-execution (append new steps into the nearest open token, then renumber
> the tail in one pass — do NOT edit the checkpoint by hand).

## GUARD RAILS (operative project contracts — codified from S003 forward)
### MINIMAL REAL (official term)
- A small, **permanent**, linkable public symbol that seeds a component's true interface (e.g. a
  version marker). Distinct from a **STUB**: a STUB is throwaway scaffold deleted later; MINIMAL
  REAL is **kept and extended** into the real implementation. It is NOT lazy — it is a deliberate,
  declared, forward-resolvable seed.
- Every MINIMAL REAL symbol carries a **searchable sentinel comment**:
  `// [MINIMAL REAL] <component> — RESOLVED BY <sprint-token>. Do NOT extend silently; do NOT backfill.`
- A MINIMAL REAL symbol may ONLY be removed or replaced by the step that **explicitly declares**
  its resolution.
- Active MINIMAL REAL marks (searchable): `vaml-vulkan` version marker → **RESOLVED BY S161**;
  `vaml-tools` version marker → **RESOLVED BY S287**; `Color` (S030) → **RESOLVED BY S089**;
  `Thickness` (S030) → **RESOLVED BY S089**.

### NO-GAP GUARANTEE (official term)
- Binding project contract: **no feature or symbol may be left behind for a silent backfill step.**
  Anything introduced as MINIMAL REAL must have an explicit, scheduled resolution point, recorded
  in the roadmap AND progress tracking, authored at the SAME step that delivers the real content.
- Any future step that uses MINIMAL REAL carries an obligation: it must be **searchable and
  resolvable** by its named sprint. Resolution is part of the real feature's own step, never a
  separate cleanup/backfill step.
- Forward-declared resolutions (contracts): **S161 must resolve the vulkan MINIMAL REAL marker;
  S287 must resolve the tools MINIMAL REAL marker. S089 must resolve the Color + Thickness
  MINIMAL REAL markers (introduced by S030).**

### NO LAZY LLM SYNDROME (official term)
- The project's default reasoning posture. When in doubt, **reason the opposite of lazy LLM**:
  prefer the explicit, the searchable, the contractually-bound, and the non-silent option.
- **MINIMAL REAL is explicitly NOT lazy-LLM** — it is a strategic choice made to keep resolution
  steps coherent **by design**, justified and recorded in this documentation, never a shortcut
  taken for convenience.

## Conventions
- `dep:S00X` = this step requires S00X complete first. Multiple deps separated by `+`.
- `dep:-` = no dependency (root step of its sprint).
- Gates: each sprint ends with a `REFACTOR GATE` step (force `-G` in the token's line) that removes
  dead code and re-verifies full build (MSVC + Clang) + ctest.
- Layer rule: `src/core` NEVER depends on Vulkan/GLFW; only `src/vulkan` may. Enforced by CMake.
- Distribution: consumer chooses shared/static; default static. [ADR-003]

---
## Sprint 0 — Repo, toolchain, CI, test harness   (goal: green skeleton, no feature code)
- S000 dep:-  Repo skeleton + tracking files (progress.md, ROADMAP.md, ADR seeds, architecture.md). [DONE]
- S001 dep:S000  Top-level CMake (project(VAML), C++ latest, /EHsc /permissive- MSVC, -Wall -Wextra -Wpedantic Clang, Werror on). [DONE — verified MSVC 19.51 via preset-driven verify scripts (S008 removed the superseded build/verify_msvc.cmd)]
- S002 dep:S001  cmake/ compiler presets + toolchain files (MSVC VS 2026, Linux Clang).
- S003 dep:S002  Static-library artifact model: VAML_BUILD_SHARED (default OFF); vaml-core, vaml-vulkan, vaml-tools targets. [MINIMAL REAL: vulkan→S161, tools→S287]
- S004 dep:S003  CI matrices (.github/workflows): windows-msvc + ubuntu-clang, configure+build+ctest green. [DONE — .github/workflows/ci.yml: rolling windows-latest (VS 2026)/ubuntu-latest, reuses msvc-debug + clang-debug presets]
- S005 dep:S003  Vendor doctest single-header into third_party; test target + smoke test (0-feature) passes. [DONE — third_party/doctest v2.4.11 single header; vaml-core-tests exec links vaml-core; smoke_test.cpp; CTest PASS on MSVC (VS 2026) + Clang (Ubuntu VM)]
- S006 dep:S005  Port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h (namespace vaml::test): strip MSTest refs, __forceinline, Windows bits; pluggable failure sink (throw TestFailureException). All predicate names preserved. [DONE — S006 is a no-build-by-contract step; compile gate belongs to S007]
- S007 dep:S006  Shouldly.h compiles under MSVC + Clang; assertion round-trip tests. [DONE — test/unit/shouldly_test.cpp exercises every predicate pass+fail; fixed ShouldBeNullOrEmpty to "null OR empty" bug; portability fixes in header (exception ctor, ignored-qualifiers, std::fpclassify/FP_ZERO/isnormal); verified MSVC + real Linux Clang, ctest green]
- S008 dep:S004  REFACTOR GATE(S0): no dead code, headers internal-only, static-link default verified, clean PR. [DONE — removed superseded build/verify_msvc.cmd; added include-surface convention + static-model STATUS log to cmake/artifact-model.cmake; MSVC + Linux Clang build + ctest green]
- S009 dep:S008  SURVIVOR block written to progress.md for Sprint 0. [DONE — consolidated SPRINT 0 SURVIVOR block + regenerated checkpoint (next = S030); docs-only, no build]

---
## Sprint 1 — Property system core  (goal: full DP semantics; the heart)  [token S030+]
- S030 dep:S009  Typeless Value: bool/int32/int64/float/double/string/Color/Thickness/Brush-id/untyped-ptr/Null + type tag + type-safe get(). [DONE — src/core/include/vaml/core/{Color,Thickness,Value}.h + src/core/src/Value.cpp; factories map to 11 tags; get<T> type-safe; equality + copy/move; msvc-debug build + ctest green]
- S031 dep:S030  Value round-trip + type-mismatch-throws tests. [DONE — test/unit/Value_roundtrip_test.cpp: every non-Null payload round-trips through get<T> exactly + re-factory==original; Null + every wrong-type get throws ValueTypeMismatch; BrushId reads as int32_t only; mismatch never corrupts state (valid re-read still succeeds); msvc-debug + clang-debug ctest green]
- S032 dep:S031  PropertyKey (name, type, owner-type id, flags) + PropertyRegistry (name→key, dedupe detection). [DONE — src/core/include/vaml/core/{PropertyFlags,PropertyKey,PropertyRegistry}.h + src/core/src/{PropertyFlags,PropertyKey,PropertyRegistry}.cpp + test/unit/PropertyKeyRegistry_test.cpp; real public symbols (no MINIMAL REAL); flags = inherits|readonly|bindsTwoWayByDefault; owner-id = string type-name; registry instance-based pimpl, name-unique + dedupe-throws; msvc-debug + clang-debug ctest green]
- S033 dep:S032  Registry: register/lookup/duplicate-throws tests; constexpr-safe static id. [DONE — src/core/include/vaml/core/PropertyId.h (header-only constexpr-safe name-token id: of(), name(), valid(), ==/!=, std::hash) + PropertyRegistry id_of/try_id bridge + test/unit/{PropertyId,PropertyRegistryExhaustive}_test.cpp (static_assert compile-time proof + exhaustive bulk register/lookup tr/dedupe-throws-unchanged + missing-lookup + move semantics); global static registry instance deferred to S034 (no consumer yet — recorded, not silent); msvc-debug + clang-debug ctest green]
- S034 dep:S033  PropertyMetadata { defaultValue; flags(inherits|readonly|bindsTwoWayByDefault); changed cb; coerce; validate }.
- S035 dep:S034  Metadata tests: default applied, cb fires only on real change, coerce applied, validate rejects.
- S036 dep:S035  DependencyObject base: local value store, Get/Set, per-prop mastery, thread-affinity marker.
- S037 dep:S036  Set/Get local values; wrong-thread write queued-not-applied tests (marshalling foundation).
- S038 dep:S037  ChangeRecorder: pending per-property changes for deferred notification.
- S039 dep:S038  Meffective-value resolution: local ?? inherited-from-parent ?? default; inherit-only flags.
- S040 dep:S039  Inheritance cascade: parent change recomputes child effective value without child write (test).
- S041 dep:S040  Change notification pipeline: ChangeQueue (per-thread) + processor drains in registration order.
- S042 dep:S041  Coalescing: two sets of same prop before drain == one notification (test).
- S043 dep:S042  Attached properties: key owned by A settable on B; registry supports owner≠class.
- S044 dep:S043  Attached set/get + inheritance + notification tests.
- S045 dep:S044  ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored.
- S046 dep:S045  DependencyProperty registration-time dependency validation (ValidateDependencies).
- S047 dep:S046  Property-system unit-test suite complete; every numbered predicate via adapted Shouldly.
- S048 dep:S047  REFACTOR GATE(S1): unused removed, build+ctest green both compilers.
- S049 dep:S048  SURVIVOR + checkpoint advance to S080.

---
## Sprint 2 — VAML markup parser (runtime path first)  (goal: text → ParsedElement tree)  [token S080+]
- S080 dep:S049  XML scanner (hand-rolled): elements, attributes, self-closing, comments, CDATA, entities, nested tags.
- S081 dep:S080  Scanner line:col error reporting; malformed inputs → structured errors.
- S082 dep:S081  Namespaces + local/prefixed names + default ns; clr-namespace-style mapping tokenized.
- S083 dep:S082  x: directives (Name, Class, Key, Null) tokenized.
- S084 dep:S083  Attribute value grammar: quoted literal, markup-extension {Binding...} tagged, numeric detection.
- S085 dep:S084  ParsedElement {tagName, ns, attributes, children} + ParsedAttribute {key, value, isExtension}.
- S086 dep:S085  Parser round-trips a realistic VAML document into the tree (tests).
- S087 dep:S086  ElementFactoryRegistry: typeName→factory<unique_ptr<DependencyObject>>; startup registration.
- S088 dep:S087  Resolve "Button"→instance; unknown type → structured error naming type+line.
- S089 dep:S088  Value converter registry: "42"→int, "10,20"→Thickness, "#FF0000"→Color, "true"→bool. [RESOLVES S030 Color + Thickness MINIMAL REAL markers → give them full string-parsing semantics + remove their sentinel tags]
- S090 dep:S089  Unknown literal → error; converter registry tests.
- S091 dep:S090  Runtime VamlReader::Load(text/ParsedElement→live tree): build children, set props.
- S092 dep:S091  VamlReader defers markup-extension values (S2 tag) + event hooks (S8); records x:Name registry.
- S093 dep:S092  Load a StackPanel+Button with a property set from pure text — tree correct, no hooks.
- S094 dep:S093  VamlReader error cases: unknown attr, unknown element, bad type name, malformed value.
- S095 dep:S094  Parser + reader full test suite (golden VAML docs).
- S096 dep:S095  REFACTOR GATE(S2): full parser green, no manual XML dep, no stubs.
- S097 dep:S096  SURVIVOR + checkpoint advance to S120.

---
## Sprint 3 — Element / scene graph + measure/arrange layout  (goal: full layout contract)  [token S120+]
- S120 dep:S097  UIElement base (derives DependencyObject): visibility, opacity, transform owner slot, layout slot.
- S121 dep:S120  Create/attach/detach children; parent traversal; visual-tree-walk tests.
- S122 dep:S121  LayoutSlot + final rect + DesiredSize + Margin slot + alignment.
- S123 dep:S122  Measure contract (infinity-aware): Measure(available)->DesiredSize; stretch vs wrap.
- S124 dep:S123  Arrange contract + clip: Arrange(finalRect); clip bounds honored by render slot.
- S125 dep:S124  Leaf elements (Text) measure to content; panels measure children.
- S126 dep:S125  InvalidateMeasure/InvalidateArrange; LayoutManager drives measure+arrange over dirty subgraph.
- S127 dep:S126  MeasureOverride/ArrangeOverride virtuals for panels.
- S128 dep:S127  Dirty-only subpass: changing a child re-measures only subtree (test).
- S129 dep:S128  StackPanel (vertical+horizontal; orientation prop; last-child-stretch).
- S130 dep:S129  StackPanel DesiredSize + stretch correctness tests.
- S131 dep:S130  Grid: row/col definitions; Absolute/Auto/Star resolution.
- S132 dep:S131  Grid ColumnSpan/RowSpan + measure+arrange math.
- S133 dep:S132  Grid star-sizing + span tests.
- S134 dep:S133  WrapPanel: flow, wraps at boundary, aligned rows.
- S135 dep:S134  WrapPanel tests.
- S136 dep:S135  Canvas: explicit position via attached Left/Top (attached props via S1).
- S137 dep:S136  Canvas tests.
- S138 dep:S137  Layout suite green both compilers; REFACTOR GATE(S3).
- S139 dep:S138  SURVIVOR + checkpoint advance to S160.

---
## Sprint 4 — Vulkan window + swapchain + first pixels (single-thread proof)  [token S160+]
- S160 dep:S139  Platform window via GLFW (Win32 + X11); resize + input callback stub surface.
- S161 dep:S160  CMake finds glfw3 + vulkan + VMA; link vaml-vulkan. [RESOLVES S003 vulkan MINIMAL REAL marker → replace version marker with real surface + remove its sentinel tag]
- S162 dep:S161  Vulkan instance + surface + physical device + queue family selection; validation layers debug-only.
- S163 dep:S162  Best-GPU auto-selection: score ALL physical devices (DISCRETE>INTEGRATED>OTHER; device-local memory; apiVersion tiebreak); filter to present-capable families; VAML_PREFERRED_GPU name override; log full ranked list at init. Identical code in VamlPOC.cpp + VamlPOC-linux/main.cpp; two-GPU Windows proves multi-adapter selection; single-GPU Linux proves ranking+override path. [both apps, not CI]
- S164 dep:S163  Swapchain (VK_KHR_swapchain) + image views + depth; format/BPP negotiation.
- S165 dep:S164  Render pass + single-triangle graphics pipeline + framebuffers (hardcoded MVP).
- S166 dep:S165  Command buffers + sync (semaphores/fences) + main-thread draw loop; clear + triangle visible.
- S167 dep:S166  Resize-safe swapchain recreation; no validation errors in debug.
- S168 dep:S167  MSAA sample-count negotiation + multisampled triangle.
- S169 dep:S168  Vulkan RAII wrappers (device/queue/swapchain/command-pool/framebuffer) — no raw vk* owned outside device.cpp.
- S170 dep:S169  RAII destruction-order clean at close; no leaks, no staging.
- S171 dep:S170  REFACTOR GATE(S4): triangle renders, no validation errors, resize-safe, RAII clean.
- S172 dep:S171  SURVIVOR + checkpoint advance to S200.

---
## Sprint 5 — Vulkan UI renderer (retained scene → GPU)  (goal: VAML-declared UI on screen)  [token S200+]
- S200 dep:S172  Quad renderer: vertex/index buffers, textured quads, uniform MVP/UI scale, alpha blend, SRGB.
- S201 dep:S200  Draw N quads from CPU list each frame; blend + SRGB tests.
- S202 dep:S201  Texture atlas + image upload (staging → device-local); sub-region invalidation.
- S203 dep:S202  Embedded bitmap-font loader + ASCII 32–126 glyph atlas build.
- S204 dep:S203  SDF rasterizer: glyphs → SDF into atlas; soft-edge sampling.
- S205 dep:S204  UI shaders (SDF text + solid color + border) + descriptor sets; shared layout.
- S206 dep:S205  Draw text + rect from shared descriptor layout; batch same-shader draws.
- S207 dep:S206  Scene→command-buffer encoder: walk visual tree → draw list by material; z-order via tree order.
- S208 dep:S207  Encoder honors Opacity + Clip (draw grouping).
- S209 dep:S208  GPU batching by texture+material; minimal pipeline switches; retired-buffer lifetime via fence.
- S210 dep:S209  Render-thread loop end-to-end (single thread first): retained scene drawn each frame.
- S211 dep:S210  Marshalled-change consumption per frame (UI thread mutates, render thread draws) — first wire.
- S212 dep:S211  MERGE POINT: UI declared in VAML renders to window via full S5 path (first real vertical slice).
- S213 dep:S212  DPI + framebuffer scale: logical px layout, render scales by framebufferScale; high-DPI crisp.
- S214 dep:S213  REFACTOR GATE(S5): retained UI renders via Vulkan; no stubs; no validation errors.
- S215 dep:S214  SURVIVOR + checkpoint advance to S240.

---
## Sprint 6 — Binding engine + MVVM core  (goal: POC is an MVVM app)  [token S240+]
- S240 dep:S215  PropertyPath parser + evaluator (dot-path; indexers; null-safe; errors reported).
- S241 dep:S240  ObservableObject: Set() + typed PropertyChanged signal (INotifyPropertyChanged-analogue).
- S242 dep:S241  BindingExpression: source/target, mode one/one-time/two-way, converter, fallback value.
- S243 dep:S242  Binding engine one-way: source→target on change; DP invalidation coalescing.
- S244 dep:S243  Two-way: target→source; TextBox edit → VM updates; converter both directions.
- S245 dep:S244  Height/glide target (fallback) + expression lifecycle (detach on element destroy).
- S246 dep:S245  RelayCommand (ICommand-analogue): invoke + CanExecute + CanExecuteChanged (full, no stub).
- S247 dep:S246  RelayCommand tests.
- S248 dep:S247  Messenger (typed loosely-coupled bus): publish/subscribe + token unsubscribe, no leaks.
- S249 dep:S248  Messenger tests.
- S250 dep:S249  Static CompositionRoot (user's DI style): resolves VM + service; no runtime container.
- S251 dep:S250  CompositionRoot + VM-with-injected-service tests.
- S252 dep:S251  Markup {Binding} resolved in VamlReader (deferred extension now bound): <Text Text="{Binding Title}"/>.
- S253 dep:S252  Command via {Binding DoIt}; one/two-way markup binding end-to-end.
- S254 dep:S253  REFACTOR GATE(S6): full MVVM works in POC, all modes + converters, no stubs.
- S255 dep:S254  SURVIVOR + checkpoint advance to S280.

---
## Sprint 7 — VBAML compiler (BAML analogue; FIRST full pipeline)  (goal: text→vbaml→tree)  [token S280+]
- S280 dep:S255  vamlcc CLI skeleton: read .vaml → write compiled form; parses via S2 scanner + tables.
- S281 dep:S280  Binary record set: versioned schema (element-id/type-token/prop-token/event-token/resource-token/string-table).
- S282 dep:S281  Serializer round-trips a sample tree to bytes and decode-back equal (byte-identical).
- S283 dep:S282  Parser → record-emit (reuse scanner/factory/value tables; no stubs).
- S284 dep:S283  Goldens: a full VAML file compiles to a .vbaml with correct token stream.
- S285 dep:S284  Runtime VBamlReader: bytes → DependencyObject tree (InitializeComponent-style).
- S286 dep:S285  Reader instantiates via factory registry, sets props, attaches deferred events (S8).
- S287 dep:S286  vamlcc integrated in CMake: custom command per .vaml → .vbaml embedded or sidecar. [RESOLVES S003 tools MINIMAL REAL marker → extend version marker into real tooling surface + remove its sentinel tag]
- S288 dep:S287  Dual-path parity harness: runtime-parsed vs compiled trees behaviorally identical (asserted).
- S289 dep:S288  Pack/identity: x:Class name mapping; embedded resource lookup; release uses compiled only.
- S290 dep:S289  Refactor-gated dual path: S2 reader = dev tool, S7 reader = release.
- S291 dep:S290  REFACTOR GATE(S7): FULL text→vbaml→tree pipeline, parity proven, no stubs.
- S292 dep:S291  SURVIVOR + checkpoint advance to S320.

---
## Sprint 8 — Routed events + input + async  (goal: real events + input + dispatcher)  [token S320+]
- S320 dep:S292  RoutedEvent primitive: Signal<T> + token + route-policy hook; reentrancy-safe add/remove.
- S321 dep:S320  Bubbling + tunneling + handled: child click bubbles to root unless handled; preview tunnel.
- S322 dep:S321  Markup event wiring: Button Click="OnClick" → handler on x:Class owner via registry.
- S323 dep:S322  Mouse/keyboard input → routed events via GLFW callbacks + hit-testing.
- S324 dep:S323  Click hit-tests element, raises bubbling routed event with correct source.
- S325 dep:S324  Keyboard focus + routed key events (bubble).
- S326 dep:S325  Dispatcher: input→UI-thread marshalling; continuations scheduled (first Task usage) [ADR-010].
- S327 dep:S326  Async continuation updates VM prop → binding updates UI; no races.
- S328 dep:S327  REFACTOR GATE(S8): events, routing, markup wiring, input, async dispatcher all real.
- S329 dep:S328  SURVIVOR + checkpoint advance to S360.

---
## Sprint 9 — Styles / resources / templates  (goal: full styling)  [token S360+]
- S360 dep:S329  ResourceDictionary + merged dictionaries + lookup (tree + app + system).
- S361 dep:S360  StaticResource/DynamicResource resolution; merge precedence; missing-key errors.
- S362 dep:S361  Style + Setter + BasedOn + PropertyTrigger + DataTrigger.
- S363 dep:S362  Style applied to type; property setters; BasedOn chain; triggers fire.
- S364 dep:S363  ControlTemplate + visual states (normal/hover/pressed swap visuals via brushes).
- S365 dep:S364  TemplateBinding to control props works; templatable Button restyle.
- S366 dep:S365  Template lookup from markup + compiled (S2 and S7 readers agree).
- S367 dep:S366  REFACTOR GATE(S9): resources/styles/templates fully functional; no stubs.
- S368 dep:S367  SURVIVOR + checkpoint advance to S400.

---
## Sprint 10 — Core controls  (goal: usable control set)  [token S400+]
- S400 dep:S368  Text control: content, font size/weight/color, wrapping via measure, SDF render.
- S401 dep:S400  Button: template, hover/pressed visual, Click routed event, IsEnabled.
- S402 dep:S401  TextBox: single+multi line, caret, editing, two-way binding, selection, focus.
- S403 dep:S402  Slider: value binding, thumb drag, routed change event.
- S404 dep:S403  CheckBox: toggle + IsChecked two-way binding.
- S405 dep:S404  ScrollViewer: scroll + clipping + wheel/scrollbar.
- S406 dep:S405  Focus + tab order + Enter-activates-default + routed key events.
- S407 dep:S406  Milestone: all controls fully implemented; each has test; no stubs.
- S408 dep:S407  REFACTOR GATE(S10) + SURVIVOR.

---
## Sprint 11 — POC app vertical slice  (goal: complete MVVM app on VAML+Vulkan)  [token S440+]
- S440 dep:S408  app/hmi MainView.vaml: Grid+StackPanel+Button+Text+TextBox+Slider+CheckBox layout.
- S441 dep:S440  MainViewModel: observable props + RelayCommands wiring to a domain service.
- S442 dep:S441  app/infra service (filesystem/data demo service) + CompositionRoot wiring.
- S443 dep:S442  Wire MainView to MainViewModel via markup {Binding} + {Command}.
- S444 dep:S443  Compile MainView.vaml→.vbaml via vamlcc in the CMake custom command (release path).
- S445 dep:S444  Launch exe → full declared UI renders; click/type/slide updates VM-driven UI end to end.
- S446 dep:S445  Zero validation-layer errors; no leaks over 10k frames; no layout thrash.
- S447 dep:S446  POC E2E integration test (headless/offscreen where feasible).
- S448 dep:S447  POC acceptance doc written from real behavior (not spec).
- S449 dep:S448  REFACTOR GATE(S11) — POC COMPLETE. SURVIVOR.

---
## Sprint 12 — Docs from evidence + open production repo  (goal: promote to vaml)  [token S480+]
- S480 dep:S449  Write architecture.md FINAL from real POC code (not the seed).
- S481 dep:S480  Write design docs (measure/arrange, renderer, binding, VBAML schema) from evidence.
- S482 dep:S481  Complete ADR log (all standing decisions + lessons learned with rationale).
- S483 dep:S482  POC lessons document: what to keep/change for production.
- S484 dep:S483  Create the public `vaml` production repo from POC lessons; seed ROADMAP + progress.
- S485 dep:S484  Port POC core→vaml production structure (clean, not copy/paste).
- S486 dep:S485  REFACTOR GATE(S12) + SURVIVOR. Production repo initialized.

---
## Sprint 13 — Test coverage expansion  (goal: CI-grade coverage, property-based, fuzz)  [token S520+]
- S520 dep:S486  Coverage gate (gcov/lcov + llvm-cov) wired into CI on core.
- S521 dep:S520  Property-system fuzz harness (random value sets → invariants).
- S522 dep:S521  Parser fuzz harness (malformed VAML → no crash, structure errors only).
- S523 dep:S522  VBamlReader fuzz harness (random byte stream → no crash).
- S524 dep:S523  Layout property-based tests (random trees → no negative size, terminate).
- S525 dep:S524  Binding stress tests (rapid invalidation → coalesced, correct final).
- S526 dep:S525  Event routing random-tree tests (hit path + handled correctness).
- S527 dep:S526  Style/resource lookup precedence fuzz.
- S528 dep:S527  Renderer offscreen correctness tests (pixel-snapshot on fixtures).
- S529 dep:S528  REFACTOR GATE(S13) + SURVIVOR. Coverage + fuzz green CI.

---
## Sprint 14 — Performance & GPU-driven rendering  (goal: production perf)  [token S620+]
- S620 dep:S529  Async layout: measure/arrange on worker threads, single commit to render; TSan-clean.
- S621 dep:S620  Async layout correctness == single-threaded (diff test).
- S622 dep:S621  GPU-driven batching: indirect/draw-list, instance buffer, minimal pipeline switches.
- S623 dep:S622  Draw-call count near-constant under 10k elements; no correctness drift.
- S624 dep:S623  Per-frame arena allocator; element pool; no heap churn in steady state.
- S625 dep:S624  Zero-copy texture update + cache-resident font/brush cache.
- S626 dep:S625  Thread vs render ring scale-out: multiple UI threads; copy-on-write hot nodes.
- S627 dep:S626  Perf smoke harness: 60fps @ 10k elements target; profiling budget in CI (event).
- S628 dep:S627  MEMORY profile gate: bounded memory, no leaks (ASan).
- S629 dep:S628  REFACTOR GATE(S14) + SURVIVOR. Perf gates met.

---
## Sprint 15 — Production hardening  (goal: release-quality robustness)  [token S720+]
- S720 dep:S629  Cancellation tokens through Task/Executor; no deadlocks; deterministic ordering.
- S721 dep:S720  Reentrancy audit across property/binding/event paths.
- S722 dep:S721  Allocator policy finalization (arena/pool rules in ADR).
- S723 dep:S722  TSan + ASan clean across suite; Valgrind (Linux) spot-check.
- S724 dep:S723  Thread-affinity enforcement; wrong-thread writes blocked with clear error.
- S725 dep:S724  Graceful shutdown: device/swapchain teardown ordering, zero validation errors on exit.
- S726 dep:S725  Error taxonomy: structured codes + diagnostics channel (no exceptions in release hot paths).
- S727 dep:S726  REFACTOR GATE(S15) + SURVIVOR.

---
## Sprint 16 — DPI, i18n, accessibility, tooling  (goal: broad applicability)  [token S820+]
- S820 dep:S727  Per-monitor DPI correctness across multi-monitor.
- S821 dep:S820  i18n: resource fallback (culture→neutral), string tables from VAML resources.
- S822 dep:S821  RTL mirroring + glyph shaping for localized text.
- S823 dep:S822  Culture-aware number/date formatting in bindings (converters).
- S824 dep:S823  Accessibility: text alternatives + keyboard navigation completeness.
- S825 dep:S824  Hot-reload dev loop for .vaml (S2 reader) in POC tools.
- S826 dep:S825  vamlcc diagnostics (nice errors: source line/col, suggested fix).
- S827 dep:S826  Packaging: single-exe consumer link demo + install rules.
- S828 dep:S827  REFACTOR GATE(S16) + SURVIVOR.

---
## Sprint 17 — Release readiness + production roadmap  (goal: v1 control set + release)  [token S920+]
- S920 dep:S828  Documented v1 control set + API stabilization pass.
- S921 dep:S920  Public headers/export facade audit (IP behind libs) [ADR-003].
- S922 dep:S921  Licensing + dual-license docs (OSS POC / prod options).
- S923 dep:S922  Sample gallery + tutorial VAML docs.
- S924 dep:S923  Full end-to-end release pipeline (tag → build both CI → package → notes).
- S925 dep:S924  v1 release candidate; regression suite green.
- S926 dep:S925  REFACTOR GATE(S17) — FINAL. SURVIVOR + close-out.
- S927 dep:S926  Post-1.0 roadmap drafted (advanced controls, GPU compute, etc.).
- S928 dep:S927  Documentation site skeleton (from docs/).
- S929 dep:S928  Community/contribution guide.
- S930 dep:S929  Roadmap v1.1 planning; guide sprint is Sprint 18 (S940+); reserve S990–S999 for post-release work.

---
## Sprint 18 — Commercial user guide: authoring a VAML app in C++ (goal: v1.0 commercial guide covering ONLY the C#→C++ application gap)  [token S940+]
- S940 dep:S930  Guide charter + outline + audience + style guide; docs/guide/README.md.
      Rules: every chapter declares its evidence source = an exact ROADMAP token; no chapter cites an
      unimplemented API (NO-LAZY-LLM gate).
- S941 dep:S940  Ch.1 Hello VAML (01-hello-vaml.md): bootstrap a consumer app from scratch (CMake +
      preset, link vaml-core/vulkan, static vs shared). C# analog: "New WPF project". Full compilable TU.
- S942 dep:S941  Ch.2 Markup (02-markup.md): the exact VAML dialect an author writes — elements, properties,
      attached props, resources, styles, templates, x:Name/x:Class, {Binding}/{StaticResource}. Snippets must
      pass vamlcc.
- S943 dep:S942  Ch.3 MVVM core in C++ — THE HEART OF THE GAP (03-mvvm-core.md): ObservableObject::Set +
      PropertyChanged vs INotifyPropertyChanged; RelayCommand CanExecute/Changed vs ICommand; header/cpp
      split; value vs reference semantics; ownership (unique_ptr/shared_ptr/raw) vs GC at DI seams; move-only
      discipline. Gate: every class snippet is a full TU compiled in CI.
- S944 dep:S943  Ch.4 Bindings & commands (04-bindings-commands.md): PropertyPath syntax, one/one-time/two-way,
      C++ converter vs IValueConverter, fallback, two-way TextBox→VM, detach/lifetime.
- S945 dep:S944  Ch.5 Events & input (05-events-input.md): Click="OnClick" wiring + handler declaration on the
      x:Class owner, routed/bubbling-tunnel/handled, focus/keyboard, async continuation vs async/await (Task/Executor).
- S946 dep:S945  Ch.6 Layout, styles, templates, controls (06-layout-styles-controls.md): shipped control set,
      styling, ControlTemplate authored in VAML.
- S947 dep:S946  Ch.7 Compiled path & release (07-compiled-vbaml-release.md): vamlcc in CMake, InitializeComponent
      analog, dual path, static vs shared, single-exe shipping.
- S948 dep:S947  Ch.8 Threading model (08-threading.md): render thread vs UI thread, marshalling, thread-affinity,
      and why lifetime/ownership differs from C#.
- S949 dep:S948  Ch.9 Full worked example (09-worked-example.md): the POC/sample app (mirrors S440–445) as a
      walkthrough, C# sketch vs VAML C++ side-by-side per section.
- S950 dep:S949  Appendices (appendix-a-glossary.md, appendix-b-cheatsheet.md): C#/XAML term → VAML C++ term
      mapping table + one-page quick reference.
- S951 dep:S950  GUIDE GATE: doc→CI job extracts every code block, compiles+runs it, resolves every API reference
      against a real public header symbol; zero snippets using unimplemented features.
- S952 dep:S951  Ship: mount guide into S928 docs-site skeleton, version-tag with v1.0, index from S923 tutorial docs.
- S953 dep:S952  REFACTOR GATE(S18): docs-only sprint — no dead code; guide build green both platforms; link-check
      green. SURVIVOR + close-out.
- S954–S989  Reserved for guide v2 / extra appendix material.

---
## Reserved / tail (S990–S999)
- Reserved tokens for post-1.0 discoveries and next-plan increments. Append new steps here in
  dependency order as work is scoped; never hand-edit the checkpoint in progress.md.

## End of ROADMAP
