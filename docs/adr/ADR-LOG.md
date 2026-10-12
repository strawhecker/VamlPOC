# ADR Log — VAML

> Architecture Decision Records. Seeds recorded during planning; each becomes a full ADR
> (Context / Decision / Consequences) as the project proves or revises it. New ADRs append at the bottom.
> NOTE: per ADR-017 this is v2-as-v1 — a production first version, not a throwaway POC.

## ADR-001 — Two-path fidelity: build-time compiler is the BAML analogue; POC ships runtime parse first
- Status: ACCEPTED (seed)
- Context: XAML's value comes from a compile step (XamlCompiler → BAML) that the runtime reads, not the text.
  We need the same for VAML ("VBAML") to hide IP and start fast.
- Decision: Long-run there is a build-time compiler (`vamlcc`, text → binary VBAML). The POC ships the runtime
  parser (Sprint 2) first to move fast; the compiler is produced in Sprint 7 and becomes the release path.
- Consequences: Runtime parser = developer tool; compiled form = release. Intellectual-property and speed live
  in the compiler.

## ADR-002 — Threading: 1 render thread (GPU/command buffers) + N UI/work threads (scene graph), change ring
- Status: ACCEPTED (seed)
- Context: XAML/WPF splits render vs dispatcher. Single-threaded rendering is a dead end for GPU-driven UI.
- Decision: Render thread owns Vulkan device/swapchain/command buffers and draws a retained scene; UI/work
  threads own scene-graph shards and mutate via a change ring (messages + copy-on-write for hot nodes).
- Consequences: Input marshalling (dispatcher), per-thread change queues, thread-affinity enforcement all
  follow from this model. POC proves single-thread first, multi-thread arrives S14.

## ADR-003 — Distribution: consumer chooses shared (dll/so) or static (lib/.a); static default [REVISED]
- Status: ACCEPTED (seed, REVISED per user)
- Context: IP hiding via compiled static libs; but forcing static-only is wrong — consumers may want dll/so.
- Decision: `VAML_BUILD_SHARED` (default OFF) → shared outputs when ON, static when OFF. Consumer decides.
  Static is *recommended* for IP-hiding, not mandated. This is a build option, not a policy.
- Consequences: CMake artifact model must support both; packaging must offer both.

## ADR-004 — Language & error model: C++ latest, no exceptions in release hot paths, single allocator
- Status: ACCEPTED (seed)
- Context: C++ latest (MSVC VS 2026 / Clang via CMake). Predictable hot paths should not throw.
- Decision: C++ latest; release hot paths use error codes, exceptions allowed in non-hot/init paths; single
  allocator policy (see S14).

## ADR-005 — Events/callbacks: Signal<T> + token unsubscribe; XAML-faithful routed events (bubble/tunnel)
- Status: ACCEPTED (seed)
- Context: C# `event` must map to C++. Routed events are core to XAML-faithful behavior.
- Decision: `Signal<T>` with token-based removal; routed events (bubbling + tunneling + `handled`) built on it.
  Hot paths avoid type-erasure (typed function pointers / `std::move_only_function`).
- Consequences: Markup `Click="OnClick"` wiring relies on the factory/name registry (ties into ADR-006).

## ADR-006 — DI: static composition root for app deps + element-factory registry for UI composition; NO generic
##         string-driven setter injection from markup
- Status: ACCEPTED (seed)
- Context: C# DI relies on reflection; C++ has none. Generic "set property by name string" injection is a
  poor-man's reflection and not worth it.
- Decision: App business dependencies live in a STATIC composition root (user's existing style, Boost.DI or
  hand-rolled). The only dynamic-from-markup mechanism is an ELEMENT FACTORY REGISTRY (`"Button"` → factory
  fn) needed for UI composition. Generic setter injection from markup is explicitly avoided.
- Consequences: Ownership (unique/shared/raw) is decided explicitly at every DI seam; replaces GC semantics.

## ADR-007 — Build environment: Windows 11 + VS 2026 (MSVC) and/or CMake on Windows/Linux (Clang); CMake canonical
- Status: ACCEPTED (seed)

## ADR-008 — MVVM: VAML ships its OWN lightweight MVVM layer (ObservableObject, RelayCommand, Messenger,
##         static CompositionRoot, RoutedEvent) loosely modeled on MVVMLight/CommunityToolkit MVVM
- Status: ACCEPTED (seed)
- Context: Developers want MVVM niceties without C#/XAML. Prism-style runtime modularity is high-risk in C++
  (no reflection, static-link IP model).
- Decision: Ship the ViewModel-friendly 80% (ObservableObject, RelayCommand, Messenger, static
  CompositionRoot, RoutedEvent). Prism-style runtime modularity + region navigation: OUT for v1 (per ADR-017 this is v2-as-v1).
- Consequences: The v1 app itself is an MVVM app (bootstrap the hard binding/event pieces early).

## ADR-009 — Tests: doctest framework + adapted (ported) Shouldly-style fluent assertion DSL, MSPST-decoupled
- Status: ACCEPTED (seed)
- Context: User's Shouldly.h (fluent asserts) is locked to MSTest/VS-only and non-portable to Linux.
- Decision: Port Shouldly.h to `src/core/assert/Shouldly.h`: strip MSTest refs (`Assert::Fail`), `__forceinline`,
  Windows-only bits; pluggable failure sink (default throws `TestFailureException`); keep ALL predicate names
  identical. Pair with doctest (single-header, cross-platform MSVC+Clang, debug-friendly, fast).
- Consequences: Same fluent syntax the team likes; no VS-only lock-in; Linux CI works; stack stays debuggable.

## ADR-010 — Async: Task/Executor continuation model, NOT compiler coroutine keywords
- Status: ACCEPTED (seed)
- Context: C# async/await requires keywords; C++20 coroutines need compiler keyword support (MSVC has it, but
  the model we want is continuation-based and unifies with thread marshalling).
- Decision: Build a minimal `Task<T>` + `Executor` with `.then()` chaining, continuation scheduling, and
  cancellation. Coroutine sugar, if ever added, is an optional facade over this same machinery.
- Consequences: `co_await` is not required at the core; async unifies with the render/UI thread ring; a
  future coroutine layer can be layered on.

## ADR-011 — Layer rule: `src/core` NEVER depends on Vulkan/GLFW; only `src/vulkan` may
- Status: ACCEPTED (enforced by CMake from S001)
- Context: The framework language/runtime must stay pure and testable without a GPU.
- Decision: Core (property, parser, layout, scene, binding, style, controls) has zero Vulkan/GLFW deps.
  The `-vulkan` backend is the only layer that touches GPU. CMake enforces the include/export boundary.
- Consequences: Core is unit-testable without a GPU; the Vulkan backend can be swapped without touching core.

## ADR-012 — GPU-driven, retained-data rendering is the governing renderer principle
- Status: ADOPTED (codified before S001)
- Context: Legacy event-driven rerasterize-every-frame UI keeps the UI thread busy and re-issues
  unchanged content. The author's proven correction: put everything in the GPU, send only changes, keep
  the CPU calm. Vulkan (unlike OpenGL) structurally supports retained resources, reusable command buffers,
  multi-threaded recording, async queues, and GPU-driven (indirect) dispatch.
- Decision: VAML rendering MUST obey docs/design/GPU-DRIVEN-PRINCIPLE.md: retained-first, change-driven,
  calm threads, batching by a small fixed material set, explicit sync/lifetime, async queues where they pay,
  and (target) GPU-driven draw dispatch. Any step conflicting with it is wrong and must change, not the doc.
- Consequences: Renderer must meet R1–R9; ceremony contained in RAII (S168); validation gates (S214/S726);
  Sprint 14 delivers the full GPU-driven form. Added to the design/ADR cross-reference set.

## ADR-013 — User guide: `docs/guide/` is the commercial user guide (C#→VAML C++ application gap), authored
##         as a "For Dummies"-style hand-holding reference, tracked under Sprint 18 (S940–S953)
- Status: ACCEPTED (planning)
- Context: The open doc gap is the application-tier C++ that replaces C#/WPF/MVVM code — not C++ and not
  general VAML/XAML, both of which have ample materials. The project's own guard rails (evidence-not-spec,
  S480/S448) require the guide to cite exact roadmap tokens and never document unimplemented API.
- Decision: Author `docs/guide/*.md` (README front-matter + Parts I–V chapters + appendices) faithful to the
  "For Dummies" series conventions. Deletes/adds of guide files are frozen after Sprint 18 scaffold (the
  "no new md files" budget); future work edits in place. The guide CI gate is S951 (compile every snippet),
  ship/index S952, refactor gate + close-out S953.
- Consequences: Guide naming/prose follows the Dummies shape; every evidence token must resolve to a real
  roadmap step or a stabilized public-facade symbol (S920/S921); content is filled as the API stabilizes.

## ADR-014 — Cross-cutting "nothing fails silently" policy (icons, bindings, layout, threading)
- Status: ACCEPTED (seed)
- Context: Two unspoken goals of the project are the same disease — icons silently degrade/dither, bindings
  silently null-out. The cure is one principle the project's own guard rails (NO-GAP, NO-LAZY-LLM) already
  demand.
- Decision: **Nothing fails silently in debug; every failure is structured data the app may route; the UI
  always degrades to a fallback and never crashes.** A failure the programmer can't see is a bug; a failure
  the user sees as a crash is worse. The **diagnostics channel and the control-flow/fallback channel are
  always separate** — a subscriber gets information, never control. Formalized as the S726 (Sprint 15)
  error-taxonomy/diagnostics-channel specialization.
- Consequences: ADR-015 (icons) and ADR-016 (bindings) are concrete applications of this rule.

## ADR-015 — Icon rendering: single quad engine; PNG/SVG are source raster formats, not vector-on-screen
- Status: ACCEPTED (seed)
- Context: WPF is poor at transparent icons because it treats them as bitmaps re-scaled per target, and
  multicolor/vector assumptions risked dragging in a second renderer. A "vector vs raster engines" premise
  was entertained but is FALSE here — Vulkan in this project is a quad/texture renderer (S200) with no
  vector path, and fonts are already rasterized once into the atlas (S203/S204).
- Decision: ONE quad renderer. PNG and SVG are BOTH **source asset formats**, each rasterized ONCE into
  the texture atlas (S202) exactly like fonts. Monochrome icons = single-channel alpha mask, tinted at
  sample in the quad shader (S205) with Foreground (no bitmap color-effect recompositing); multicolor =
  rasterized color texture. `TintMode` = { Preserve | Monochrome }. Resolution restricted to the packaged
  resource set (reject external paths/scripts — C++ has no safe runtime, so this is a security rule, not
  hygiene). Missing/unsupported → deliberate fallback glyph + diagnostic (NO-GAP).
- Consequences: Icons are fixed-resolution after rasterization; runtime re-scale/retint uses a DPI texel
  size family (S213_1), not true vector. Image scaling policy { none | down-only | both } (default none):
  N_physical = logical × dpiScale; "none" picks max authored texels ≤ N_physical and never resamples —
  "pixels are pixels" is TRUE once measured in physical pixels. True vector-on-screen is a post-1.0
  production path renderer feature, deferred deliberately. Tokens: S202_1 (import), S205_1 (mask tint),
  S213_1 (size family), S360_1 (IconKey resource), S400_1 (Icon control), S528_1 (gallery).

## ADR-016 — Binding failures: classed (Class A fails fast, Class B degrades) + programmable diagnostics sink
- Status: ACCEPTED (seed)
- Context: WPF binding errors are silent-dynamic (reflection): trace-only, unreachable by app code, and it
  "must not throw" because resolution is dynamic. VAML is deterministic and statically typed, so it can do
  strictly better.
- Decision: Bindings are **classed**:
  - **Class A** (static/authoring error — typo, wrong hop type, converter signature) → fail fast at
    **compile-time** (vamlcc, S280_1/S826_1) or **bind-time** (S242_1 `Strict`); loud by default in debug.
  - **Class B** (defined runtime data outcome — null chain, converter rejects a value, two-way write
    rejected, expression detached) → **NEVER throws**; falls back + emits a structured diagnostic.
  Every failure point emits a `BindingDiagnostic` (S240_1) to an `IBindingDiagnostics` sink: **diagnostics
  ≠ control-flow** — the subscriber gets information, never control. Default sink is NO-OP in release
  (opt-in; "the receipt is on them"). One channel, two consumers (VM + view, via S250_1). Static binding is
  the rule (per-binding path validated against a declared root type via S032–S039); dynamic-root binding is
  the documented, Strict-excludable escape hatch. Compile-time coverage ratio (S826_1) proves the static
  guarantee holds at scale (e.g. a 500k-line codebase).
- Consequences: Fail-fast on authoring bugs (caught at startup/build, not years later); graceful
  degradation on runtime data outcomes; a programmable, testable diagnostics channel. The record is the
  S726 (Sprint 15) diagnostics-channel specialization.

## ADR-017 — v2-as-v1: this project is the production first version, not a POC
- Status: ACCEPTED (user decision, supersedes POC framing)
- Context: The term "POC" is historical (the repo was named VamlPOC before the roadmap). It stopped being
  a POC before S001 — it is v1 built at v2 quality ("version 2 as our first version"). Sprint 12's original
  "open a separate production repo and port the POC" framing is invalid under this decision.
- Decision: The code under this repo IS the product — v1 at v2 quality. No throwaway-first, no "port core
  to a production repo later." Sprint 12 (S480–S486) hardens and packages the repo *in place*; the "open
  production repo / port" steps dissolve into that hardening. Dev contract is professional commercial
  quality from the start.
- Consequences: Every decision is production-quality from day one (ADR-015 icons, ADR-016 bindings); roadmap
  wording updated to "v1" not "POC"; the S949 sample-app worked example is "the sample app," not "the POC app."

## END OF ADR LOG (seed)
