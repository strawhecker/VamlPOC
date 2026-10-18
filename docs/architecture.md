# VAML Architecture — v0.1 (SEED; final version written from evidence in Sprint 12)

> This is the planning seed. It records the intended architecture and the standing decisions.
> The FINAL architecture.md (Sprint 12, S480) is rewritten from real code, not this text.
> Pairs with: docs/ROADMAP.md (steps), docs/adr/ADR-LOG.md (decisions), progress.md (state).
> GOVERNING RENDER PRINCIPLE: docs/design/GPU-DRIVEN-PRINCIPLE.md (ADR-012). All renderer work must serve it.
> Per ADR-017: this is v1 built at v2 quality — a production first version, not a throwaway POC.

## 1. Goals & Non-goals
Goals: XAML-faithful markup ("VAML"); GPU-native retained UI via Vulkan; pure C++ (latest); multi-threaded
render; consumer-driven static-or-shared distribution as a BUILD-MODEL OPTION (static preferred for
determinism + LTO, not IP hiding — this lib is MIT-public); no C#/managed/Python anywhere.
Non-goals (v1): accessibility, i18n, GPU-driven advanced batching, huge control set, XAML binary compat.

## 2. Repo layout (see also the app structure dictated by the user)
- docs/        — ROADMAP.md (S000–S999), adr/, design/, architecture seed (this file),
                 guide/ (commercial user guide: authoring a VAML app in C++, Sprint 18, S940–S953).
- src/         — the reusable C++ framework + tools (MIT-public, no protected IP); subfolders split libs/tools.
  - core/      — vaml-core: property, markup, layout, scene, binding, style, controls, assert (no Vulkan).
  - vulkan/    — vaml-vulkan: window, swapchain, renderer backend (only layer touching Vulkan/GLFW).
  - tools/     — vamlcc (VBAML compiler), other build/dev tools.
  - cmake/     — presets, toolchains, artifact/CI helpers.
- test/        — unit/ (doctest), integration/ (E2E/headless where feasible).
- VamlPOC/     — the sample APPLICATION (app-specific, by SoC subfolders), separate from the library source.
  - app/       — hmi/ (views), core/ (domain), infra/ (services), viewmodels/ (VM layer).

## 3. Dependency / layer rule
App (VamlPOC) ─► vaml-core ─► vaml-vulkan ─► Vulkan/GLFW/VMA
                  │ (no Vulkan here)
                  ▼
            vamlcc (build-time compiler: .vaml → .vbaml)

## 4. Core subsystems (each a module; see ROADMAP sprints)
- property/  : Value, DependencyObject, DependencyProperty, registry, inheritance, coercion, change
               notification, attached props, read-only enforcement.   [Sprint 1]
- markup/    : XML scanner, namespaces, x: directives, attribute grammar, ParsedElement model,
               ElementFactoryRegistry, value converter registry, runtime VamlReader.   [Sprint 2]
- layout/    : UIElement, LayoutSlot/DesiredSize, Measure/Arrange contract, LayoutManager (dirty-only),
               StackPanel, Grid, WrapPanel, Canvas.   [Sprint 3]
- scene/     : scene graph, invalidation → render encoder, hit-testing, event routing (routed events).
- binding/   : PropertyPath, ObservableObject, BindingExpression, one/two-way, converters, RelayCommand,
               Messenger, static CompositionRoot. Classed binding diagnostics (Class A fails fast at compile/
               bind; Class B degrades + structured sink). [Sprint 6] [ADR-016]
- runtime/   : VBamlReader + InitializeComponent (compiled-form consumer).   [Sprint 7]
- style/     : ResourceDictionary, Style/Setter/BasedOn/Triggers, ControlTemplate, visual states. [Sprint 9]
- controls/  : Text, Button, TextBox, Slider, CheckBox, ScrollViewer, focus/tab.   [Sprint 10]

## 5. Rendering & threading (multi-threaded retained UI)
- Render thread: owns Vulkan device/swapchain/command buffers; draws retained scene each frame.
- UI threads: own scene-graph shards; mutate via change ring (messages + copy-on-write). Dispatcher
  marshals input UI→render ready state. Mirrors WPF render/dispatcher split. [ADR-002]
- Single-thread render first (Sprint 4–5), multi-thread scale-out in Sprint 14.
- Backend renderer: texture atlas, SDF text, quad/border shaders, batching by material. [Sprint 5]
- Icons: single quad engine; PNG/SVG are source raster formats rasterized once into the atlas; monochrome
  tinted at sample in the shader. [ADR-015]
- GOVERNED BY docs/design/GPU-DRIVEN-PRINCIPLE.md (ADR-012): retained-first, change-driven, calm threads,
  small fixed material set, explicit sync/lifetime, indirect dispatch target (Sprint 14).

## 6. The BAML analogue (two-path)
- Option A (long run): vamlcc compiles .vaml → .vbaml (versioned token stream: element-id, type-token,
  prop-token, event-token, resource-token, string-table). Runtime reads via VBamlReader. [ADR-001]
- Option B (dev tool): runtime VamlReader parses text at load.
- Sprint 7 builds A and proves dual-path parity; release uses compiled form.

## 7. Distribution / build model
- vaml-core + vaml-vulkan built per VAML_BUILD_SHARED (default static). Consumer links → single exe.
- Static is a BUILD-MODEL PREFERENCE (no DLLs/leaks, determinism, whole-program optimization/LTO),
  NOT IP hiding: this library is MIT-public with no IP to protect. A consumer APP may statically link
  to compile ITS OWN IP into a single exe — an app-level concern, not this repo's. [ADR-003 — revised]
- Public headers are the consumer's API surface; they are NOT an "ABI facade" — the static model has
  no runtime ABI boundary to defend (consumer *is* the exe). Export machinery is build-model plumbing
  only. [ADR-003 — revised]
- vamlcc ships prebuilt (binary). VAML source OSS; production licensing options. [Sprint 17]

## 7a. CPU/GPU line of responsibility (frame-critical)
- CPU owns (permanent): scene graph + parent links; inheritance resolution; cascade/layout DECISIONS;
  change coalescing; pipeline-BUILDING (flatten resolved effective state into POD arrays).
- GPU owns (per-primitive, per-frame rendering of KNOWN structures): shaders, compute, buffers,
  retained resources — driven only by the flat data the CPU produces.
- The fence = the flattened pipeline buffers are the SOLE hand-off; the GPU never touches the live
  scene graph. We do NOT use the GPU for what the CPU must do, and no future step reverses this.
  [ADR-019]

## 8. Test/async policy
- Tests: doctest + adapted Shouldly-style fluent asserts (ported, MSTest-decoupled). [ADR-009]
- Async: Task/Executor continuation model; coroutine keywords optional facade only. [ADR-010]

## See Also
- ROADMAP.md for the tokenized execution plan.
- ADR-LOG.md for every standing decision.
- progress.md for live completion state.
